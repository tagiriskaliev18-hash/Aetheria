#include "AetheriaWorldDirector.h"
#include "Engine/LevelStreamingDynamic.h"
#include "AetheriaSanctumGate.h"
#include "AetheriaPackVFX.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "GameFramework/Pawn.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Components/LocalLightComponent.h"
#include "Components/ShapeComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/PointLight.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "Engine/TextureLightProfile.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/CollisionProfile.h"

DEFINE_LOG_CATEGORY_STATIC(LogAetheriaWorld, Log, All);

static const FName GDressTag(TEXT("AetheriaBastionDressing"));
static const FName GArenaDressTag(TEXT("AetheriaWorldArenaDress"));
static const FName GFallbackLightTag(TEXT("AetheriaFallbackRig"));

namespace
{
	UStaticMesh* LoadSM(const TCHAR* Pkg)
	{
		if (!Pkg) return nullptr;
		const FString Obj = FString::Printf(TEXT("%s.%s"), Pkg, *FPackageName::GetShortName(Pkg));
		return LoadObject<UStaticMesh>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UMaterialInterface* LoadMat(const TArray<const TCHAR*>& Candidates)
	{
		for (const TCHAR* Pkg : Candidates)
		{
			if (!Pkg) continue;
			const FString Obj = FString::Printf(TEXT("%s.%s"), Pkg, *FPackageName::GetShortName(Pkg));
			if (UMaterialInterface* MI = LoadObject<UMaterialInterface>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet))
			{
				return MI;
			}
		}
		return nullptr;
	}

	AStaticMeshActor* SpawnMesh(UWorld* World, UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& Scale,
		UMaterialInterface* OverrideMat = nullptr, FName Tag = GArenaDressTag, bool bCollision = true)
	{
		if (!World || !Mesh) return nullptr;
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Loc, Rot, Sp);
		if (!Actor) return nullptr;
		Actor->Tags.Add(GDressTag);
		if (Tag != NAME_None && Tag != GDressTag)
		{
			Actor->Tags.Add(Tag);
		}
		if (UStaticMeshComponent* C = Actor->GetStaticMeshComponent())
		{
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Mesh);
			C->SetWorldScale3D(Scale);
			C->SetCollisionEnabled(bCollision ? ECollisionEnabled::QueryAndPhysics : ECollisionEnabled::NoCollision);
			if (bCollision)
			{
				C->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
			}
			C->SetVisibility(true, true);
			C->SetHiddenInGame(false, true);
			if (OverrideMat)
			{
				const int32 NumMats = FMath::Max(1, C->GetNumMaterials());
				for (int32 s = 0; s < NumMats; ++s)
				{
					C->SetMaterial(s, OverrideMat);
				}
			}
		}
		return Actor;
	}

	APointLight* SpawnFillLight(UWorld* World, const FVector& Loc, const FLinearColor& Color, float Candelas, float Radius,
		bool bShadows, float VolScatter = 1.4f, FName Tag = GArenaDressTag)
	{
		if (!World) return nullptr;
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		APointLight* PL = World->SpawnActor<APointLight>(Loc, FRotator::ZeroRotator, Sp);
		if (PL)
		{
			PL->Tags.Add(GDressTag);
			if (Tag != NAME_None && Tag != GDressTag)
			{
				PL->Tags.Add(Tag);
			}
			if (UPointLightComponent* C = Cast<UPointLightComponent>(PL->GetLightComponent()))
			{
				C->SetMobility(EComponentMobility::Movable);
				C->SetIntensityUnits(ELightUnits::Candelas);
				C->SetLightColor(Color);
				C->SetIntensity(Candelas);
				C->SetAttenuationRadius(Radius);
				C->SetCastShadows(bShadows);
				C->SetSourceRadius(36.0f);
				C->SetVolumetricScatteringIntensity(VolScatter);
				C->SetSpecularScale(0.18f);
			}
		}
		return PL;
	}

	void AttachNiagaraToActor(AActor* Owner, const TCHAR* NiagaraPkg, const FVector& RelOffset, const FRotator& RelRot, const FVector& Scale)
	{
		if (!Owner || !Owner->GetRootComponent() || !NiagaraPkg) return;
		const FString Obj = FString::Printf(TEXT("%s.%s"), NiagaraPkg, *FPackageName::GetShortName(NiagaraPkg));
		UNiagaraSystem* Sys = LoadObject<UNiagaraSystem>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Sys) return;

		UNiagaraComponent* NC = NewObject<UNiagaraComponent>(Owner);
		if (!NC) return;
		NC->SetAsset(Sys);
		NC->SetupAttachment(Owner->GetRootComponent());
		NC->SetRelativeLocation(RelOffset);
		NC->SetRelativeRotation(RelRot);
		NC->SetRelativeScale3D(Scale);
		NC->bAutoActivate = true;
		NC->RegisterComponent();
		NC->Activate(true);
	}

	void SpawnBrazierFlame(UWorld* World, const FVector& GroundLoc, const FLinearColor& LightColor, float Candelas, float Radius,
		const TCHAR* FireNiagaraPkg, float FireScale, bool bCastShadow = false)
	{
		if (!World) return;
		UStaticMesh* TorchMesh = LoadSM(TEXT("/Game/MedievalDungeon/Meshes/Props/SM_Torch"));
		UStaticMesh* PillarBase = LoadSM(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Pillar"));
		if (TorchMesh)
		{
			SpawnMesh(World, TorchMesh, GroundLoc + FVector(0.0f, 0.0f, 70.0f), FRotator::ZeroRotator, FVector(1.55f), nullptr, GArenaDressTag, false);
		}
		else if (PillarBase)
		{
			SpawnMesh(World, PillarBase, GroundLoc, FRotator::ZeroRotator, FVector(0.32f, 0.32f, 0.25f), nullptr, GArenaDressTag, false);
		}

		if (APointLight* PL = SpawnFillLight(World, GroundLoc + FVector(0.0f, 0.0f, 155.0f), LightColor, Candelas, Radius, bCastShadow, 2.0f, GArenaDressTag))
		{
			AttachNiagaraToActor(PL, FireNiagaraPkg, FVector(0.0f, 0.0f, -15.0f), FRotator::ZeroRotator, FVector(FireScale));
		}
	}

	void SpawnPortalLandmark(UWorld* World, const FVector& Loc, const FRotator& Rot, const TCHAR* PortalNiagaraPkg,
		const FLinearColor& GlowColor, float Candelas, float Scale)
	{
		if (!World) return;
		if (APointLight* PL = SpawnFillLight(World, Loc + FVector(0.0f, 0.0f, 170.0f), GlowColor, Candelas, 1850.0f, false, 2.4f, GArenaDressTag))
		{
			AttachNiagaraToActor(PL, PortalNiagaraPkg, FVector(0.0f, 0.0f, -10.0f), Rot, FVector(Scale));
		}
	}

	bool FindRawGroundZ(UWorld* World, const FVector& RefPos, float& OutZ)
	{
		if (!World) return false;
		FCollisionQueryParams Q(SCENE_QUERY_STAT(AetheriaWorldGround), false);
		for (TActorIterator<APawn> It(World); It; ++It)
		{
			Q.AddIgnoredActor(*It);
		}
		FHitResult Hit;
		if (World->SweepSingleByChannel(
			Hit,
			RefPos + FVector(0.0f, 0.0f, 450.0f),
			RefPos - FVector(0.0f, 0.0f, 2600.0f),
			FQuat::Identity,
			ECC_Pawn,
			FCollisionShape::MakeSphere(24.0f),
			Q))
		{
			if (Hit.ImpactNormal.Z >= 0.7f && !Hit.bStartPenetrating)
			{
				OutZ = Hit.ImpactPoint.Z;
				return true;
			}
		}
		return false;
	}
}

const TCHAR* AetheriaWorldDirector::WorldDisplayName(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion: return TEXT("Бастион Аотера");
	case EAetheriaWorldId::RuinsTemple: return TEXT("Руины Монолита");
	case EAetheriaWorldId::VoidRealm: return TEXT("Пустотный Предел");
	case EAetheriaWorldId::BossSanctum: return TEXT("Святилище Архонта");
	default: return TEXT("Аэтерия");
	}
}

const TCHAR* AetheriaWorldDirector::WorldSubtitle(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion: return TEXT("Золотой час нагорья. Готический двор и камень Агоры.");
	case EAetheriaWorldId::RuinsTemple: return TEXT("Солнечные руины Монолита. Пыльные лучи сквозь древние колонны.");
	case EAetheriaWorldId::VoidRealm: return TEXT("Астральные обсидиановые крипты, сияние порталов и рун.");
	case EAetheriaWorldId::BossSanctum: return TEXT("Огненная сокровищница собора. Арена Архонта.");
	default: return TEXT("");
	}
}

static TArray<TWeakObjectPtr<ULevelStreamingDynamic>> GAetheriaStreamedLayouts;
static bool GAetheriaHasStreamedWorld = false;
static EAetheriaWorldId GAetheriaStreamedWorld = EAetheriaWorldId::Bastion;

// Four distinct verified artist-made demo maps under /Game/Maps/AetheriaLayouts/
static const TCHAR* LayoutGothic  = TEXT("/Game/Maps/AetheriaLayouts/Layout_GothicCourtyard");
static const TCHAR* LayoutAgora   = TEXT("/Game/Maps/AetheriaLayouts/Layout_AgoraPlaza");
static const TCHAR* LayoutDungeon = TEXT("/Game/Maps/AetheriaLayouts/Layout_MedievalDungeon");
static const TCHAR* LayoutSanctum = TEXT("/Game/Maps/AetheriaLayouts/Layout_TreasureSanctum");

const TCHAR* AetheriaWorldDirector::LayoutMapForWorld(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion:     return LayoutGothic;
	case EAetheriaWorldId::RuinsTemple: return LayoutAgora;
	case EAetheriaWorldId::VoidRealm:   return LayoutDungeon;
	case EAetheriaWorldId::BossSanctum: return LayoutSanctum;
	default:                            return LayoutGothic;
	}
}

static ULevelStreamingDynamic* StreamArtistLayout(UWorld* World, const TCHAR* Path, const FVector& Loc, const FRotator& Rot)
{
	if (!World || !Path || !Path[0]) return nullptr;
	bool bOk = false;
	ULevelStreamingDynamic* Stream = ULevelStreamingDynamic::LoadLevelInstance(World, FString(Path), Loc, Rot, bOk);
	if (Stream)
	{
		Stream->SetShouldBeLoaded(true);
		Stream->SetShouldBeVisible(true);
		Stream->bShouldBlockOnLoad = true;
		GAetheriaStreamedLayouts.Add(Stream);
		World->FlushLevelStreaming(EFlushLevelStreamingType::Full);

		// Convert streamed sublevel primitives/lights to Movable + visible (ForceNoPrecomputedLighting safe)
		if (ULevel* LoadedLvl = Stream->GetLoadedLevel())
		{
			for (AActor* Actor : LoadedLvl->Actors)
			{
				if (!Actor) continue;
				TArray<UPrimitiveComponent*> Prims;
				Actor->GetComponents<UPrimitiveComponent>(Prims);
				for (UPrimitiveComponent* P : Prims)
				{
					if (P)
					{
						P->SetMobility(EComponentMobility::Movable);
					}
				}
			}
		}

		UE_LOG(LogAetheriaWorld, Display, TEXT("[Aetheria] Streamed artist layout %s at %s ok=%d"), Path, *Loc.ToCompactString(), bOk ? 1 : 0);
	}
	else
	{
		UE_LOG(LogAetheriaWorld, Warning, TEXT("[Aetheria] Failed to stream artist layout %s"), Path);
	}
	return Stream;
}

static void UnloadArtistLayouts(UWorld* World)
{
	for (TWeakObjectPtr<ULevelStreamingDynamic>& Ptr : GAetheriaStreamedLayouts)
	{
		if (ULevelStreamingDynamic* S = Ptr.Get())
		{
			S->SetShouldBeVisible(false);
			S->SetShouldBeLoaded(false);
			S->bShouldBlockOnLoad = true;
		}
	}
	if (World)
	{
		World->FlushLevelStreaming(EFlushLevelStreamingType::Full);
		for (TWeakObjectPtr<ULevelStreamingDynamic>& Ptr : GAetheriaStreamedLayouts)
		{
			if (ULevelStreamingDynamic* S = Ptr.Get())
			{
				World->RemoveStreamingLevel(S);
			}
		}
	}
	GAetheriaStreamedLayouts.Reset();
	GAetheriaHasStreamedWorld = false;
}

static void StreamWorldLayoutsIfSparse(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return;
	// Count existing baked geometry meshes so baked World_* maps don't double-load a sublevel,
	// while empty shells or unbaked maps immediately stream their distinct artist layout!
	int32 ExistingStaticMeshes = 0;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(GDressTag)) continue;
		UStaticMeshComponent* SMC = It->GetStaticMeshComponent();
		if (SMC && SMC->GetStaticMesh())
		{
			const FString MName = SMC->GetStaticMesh()->GetName().ToLower();
			if (!MName.Contains(TEXT("sky")))
			{
				++ExistingStaticMeshes;
			}
		}
	}

	if (ExistingStaticMeshes >= 10)
	{
		return;
	}

	if (GAetheriaHasStreamedWorld && GAetheriaStreamedWorld == WorldId && GAetheriaStreamedLayouts.Num() > 0)
	{
		return;
	}

	UnloadArtistLayouts(World);
	const TCHAR* LayoutPath = AetheriaWorldDirector::LayoutMapForWorld(WorldId);
	if (StreamArtistLayout(World, LayoutPath, FVector::ZeroVector, FRotator::ZeroRotator))
	{
		GAetheriaHasStreamedWorld = true;
		GAetheriaStreamedWorld = WorldId;
	}
}

void AetheriaWorldDirector::ClearDressing(UWorld* World)
{
	if (!World) return;
	UnloadArtistLayouts(World);
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(GDressTag) || It->ActorHasTag(GArenaDressTag))
		{
			ToDestroy.Add(*It);
		}
	}
	for (AActor* A : ToDestroy)
	{
		A->Destroy();
	}
}

void AetheriaWorldDirector::ApplyLighting(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return;

	// Physically calibrated UE 5.8 Lumen + ASkyAtmosphere + bRealTimeCapture=true palettes
	// (Wukong / Crimson Desert moody contrast, zero white wash, zero pitch black).
	float DirI = 8.0f;
	float SkyI = 1.15f;
	float FogD = 0.0075f;
	float VolScat = 3.2f;
	float ScatDist = 0.74f;
	float Bias = -0.35f;
	float IndirectI = 1.10f;
	float Bloom = 0.42f;
	float BloomThresh = 1.05f;
	float Vignette = 0.20f;
	float SceneFringe = 0.14f;

	FLinearColor DirC(1.0f, 0.80f, 0.50f);       // Bastion: Golden-Hour Wukong Highlands
	FLinearColor SkyC(0.46f, 0.58f, 0.78f);      // crisp highland sky bounce
	FLinearColor LowerHemi(0.22f, 0.16f, 0.11f); // warm sandstone ground bounce (NEVER black)
	FLinearColor FogC(0.26f, 0.21f, 0.16f);      // golden-amber volumetric haze
	FColor ShaftTint(255, 212, 148);
	FVector4 Sat(1.12f, 1.10f, 1.08f, 1.0f);
	FVector4 SatSh(0.98f, 1.02f, 1.08f, 1.0f);
	FVector4 SatHi(1.16f, 1.08f, 0.94f, 1.0f);
	FVector4 GammaSh(1.10f, 1.09f, 1.11f, 1.0f);
	FVector4 GainHi(1.12f, 1.04f, 0.90f, 1.0f);
	FRotator DirRot(-28.0f, -38.0f, 0.0f);

	switch (WorldId)
	{
	case EAetheriaWorldId::RuinsTemple:
		// Sunlit ancient temple ruins with bright dusty god-ray shafts
		DirI = 9.0f;
		SkyI = 1.25f;
		FogD = 0.0088f;
		VolScat = 3.5f;
		ScatDist = 0.76f;
		Bias = -0.30f;
		IndirectI = 1.10f;
		Bloom = 0.42f;
		BloomThresh = 1.08f;
		Vignette = 0.18f;
		SceneFringe = 0.16f;
		DirC = FLinearColor(1.0f, 0.88f, 0.64f);
		SkyC = FLinearColor(0.58f, 0.68f, 0.56f);
		LowerHemi = FLinearColor(0.28f, 0.24f, 0.16f);
		FogC = FLinearColor(0.36f, 0.34f, 0.24f);
		ShaftTint = FColor(255, 232, 174);
		DirRot = FRotator(-34.0f, 132.0f, 0.0f);
		Sat = FVector4(1.10f, 1.10f, 1.04f, 1.0f);
		SatHi = FVector4(1.18f, 1.12f, 0.90f, 1.0f);
		GammaSh = FVector4(1.14f, 1.13f, 1.10f, 1.0f);
		GainHi = FVector4(1.14f, 1.08f, 0.92f, 1.0f);
		break;

	case EAetheriaWorldId::VoidRealm:
		// Dramatic Dark Obsidian-Cyan Sanctum with glowing portals & torches
		DirI = 2.2f;
		SkyI = 0.85f;
		FogD = 0.0110f;
		VolScat = 2.8f;
		ScatDist = 0.70f;
		Bias = -0.45f;
		IndirectI = 1.20f;
		Bloom = 0.50f;
		BloomThresh = 0.98f;
		Vignette = 0.24f;
		SceneFringe = 0.22f;
		DirC = FLinearColor(0.52f, 0.82f, 1.0f);
		SkyC = FLinearColor(0.36f, 0.52f, 0.88f);
		LowerHemi = FLinearColor(0.16f, 0.22f, 0.36f);
		FogC = FLinearColor(0.14f, 0.22f, 0.36f);
		ShaftTint = FColor(150, 214, 255);
		DirRot = FRotator(-46.0f, -25.0f, 0.0f);
		Sat = FVector4(1.14f, 1.12f, 1.24f, 1.0f);
		SatSh = FVector4(0.92f, 1.02f, 1.18f, 1.0f);
		SatHi = FVector4(0.98f, 1.14f, 1.24f, 1.0f);
		GammaSh = FVector4(1.10f, 1.14f, 1.20f, 1.0f);
		GainHi = FVector4(0.94f, 1.08f, 1.18f, 1.0f);
		break;

	case EAetheriaWorldId::BossSanctum:
		// Moody Fiery Golden-Amber Cathedral Arena
		DirI = 4.0f;
		SkyI = 1.05f;
		FogD = 0.0095f;
		VolScat = 3.1f;
		ScatDist = 0.72f;
		Bias = -0.20f;
		IndirectI = 1.20f;
		Bloom = 0.48f;
		BloomThresh = 1.00f;
		Vignette = 0.22f;
		SceneFringe = 0.20f;
		DirC = FLinearColor(1.0f, 0.68f, 0.34f);
		SkyC = FLinearColor(0.58f, 0.42f, 0.34f);
		LowerHemi = FLinearColor(0.28f, 0.16f, 0.10f);
		FogC = FLinearColor(0.30f, 0.18f, 0.12f);
		ShaftTint = FColor(255, 186, 110);
		DirRot = FRotator(-42.0f, 20.0f, 0.0f);
		Sat = FVector4(1.16f, 1.08f, 1.02f, 1.0f);
		SatHi = FVector4(1.24f, 1.10f, 0.86f, 1.0f);
		GammaSh = FVector4(1.14f, 1.08f, 1.06f, 1.0f);
		GainHi = FVector4(1.16f, 1.02f, 0.84f, 1.0f);
		break;

	default:
		break;
	}

	FActorSpawnParameters RigSp;
	RigSp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	// 1. DirectionalLight (guaranteed fallback spawn if map lacks one)
	int32 DirCount = 0;
	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		++DirCount;
		It->SetActorHiddenInGame(false);
		It->SetActorRotation(DirRot);
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			L->SetMobility(EComponentMobility::Movable);
			L->SetVisibility(true);
			L->bUseTemperature = false;
			L->SetIntensity(DirI);
			L->SetLightColor(DirC);
			L->SetAtmosphereSunLight(true);
			L->SetCastShadows(true);
			L->SetVolumetricScatteringIntensity(VolScat);
			L->SetEnableLightShaftOcclusion(true);
			L->SetOcclusionMaskDarkness(0.32f);
			L->SetOcclusionDepthRange(25000.0f);
			L->SetEnableLightShaftBloom(true);
			L->SetBloomScale(0.85f);
			L->SetBloomThreshold(0.15f);
			L->SetBloomTint(ShaftTint);
		}
	}
	if (DirCount == 0)
	{
		if (ADirectionalLight* DL = World->SpawnActor<ADirectionalLight>(FVector(0.0f, 0.0f, 1200.0f), DirRot, RigSp))
		{
			DL->Tags.Add(GFallbackLightTag);
			if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(DL->GetLightComponent()))
			{
				L->SetMobility(EComponentMobility::Movable);
				L->bUseTemperature = false;
				L->SetIntensity(DirI);
				L->SetLightColor(DirC);
				L->SetAtmosphereSunLight(true);
				L->SetCastShadows(true);
				L->SetVolumetricScatteringIntensity(VolScat);
				L->SetEnableLightShaftOcclusion(true);
				L->SetOcclusionMaskDarkness(0.32f);
				L->SetOcclusionDepthRange(25000.0f);
				L->SetEnableLightShaftBloom(true);
				L->SetBloomScale(0.85f);
				L->SetBloomThreshold(0.15f);
				L->SetBloomTint(ShaftTint);
			}
		}
	}

	// Clamp ALL pre-existing baked local lights (Point, Spot, Rect) in VoidRealm to kill white wall glare,
	// and hide any visible debug UShapeComponent wireframes left on level volumes/triggers.
	if (WorldId == EAetheriaWorldId::VoidRealm)
	{
		for (TActorIterator<AActor> ActIt(World); ActIt; ++ActIt)
		{
			AActor* Actor = *ActIt;
			if (!Actor || Actor->IsActorBeingDestroyed()) continue;
			if (Actor->ActorHasTag(GDressTag) || Actor->ActorHasTag(GArenaDressTag)) continue;

			TArray<ULocalLightComponent*> LocalLights;
			Actor->GetComponents<ULocalLightComponent>(LocalLights);
			for (ULocalLightComponent* LL : LocalLights)
			{
				if (!LL) continue;
				LL->SetIntensityUnits(ELightUnits::Candelas);
				LL->SetIntensity(FMath::Min(LL->Intensity, 10.0f));
				LL->SetSpecularScale(0.15f);
			}

			if (!Actor->IsA<APawn>())
			{
				TArray<UShapeComponent*> Shapes;
				Actor->GetComponents<UShapeComponent>(Shapes);
				for (UShapeComponent* Shape : Shapes)
				{
					if (Shape && !Shape->bHiddenInGame)
					{
						Shape->SetHiddenInGame(true);
					}
				}
			}
		}
	}

	// 2. SkyLight with bRealTimeCapture=true & non-black lower hemisphere (guaranteed fallback spawn)
	int32 SkyCount = 0;
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		++SkyCount;
		It->SetActorHiddenInGame(false);
		if (USkyLightComponent* L = It->GetLightComponent())
		{
			L->SetMobility(EComponentMobility::Movable);
			L->SetVisibility(true);
			L->SetIntensity(SkyI);
			L->SetLightColor(SkyC);
			L->bLowerHemisphereIsBlack = false;
			L->SetLowerHemisphereColor(LowerHemi);
			L->bRealTimeCapture = true;
			L->MarkRenderStateDirty();
			L->RecaptureSky();
		}
	}
	if (SkyCount == 0)
	{
		if (ASkyLight* SL = World->SpawnActor<ASkyLight>(FVector(0.0f, 0.0f, 600.0f), FRotator::ZeroRotator, RigSp))
		{
			SL->Tags.Add(GFallbackLightTag);
			if (USkyLightComponent* L = SL->GetLightComponent())
			{
				L->SetMobility(EComponentMobility::Movable);
				L->SetIntensity(SkyI);
				L->SetLightColor(SkyC);
				L->bLowerHemisphereIsBlack = false;
				L->SetLowerHemisphereColor(LowerHemi);
				L->bRealTimeCapture = true;
				L->MarkRenderStateDirty();
				L->RecaptureSky();
			}
		}
	}

	// 3. ExponentialHeightFog with VolumetricFog=true (guaranteed fallback spawn)
	int32 FogCount = 0;
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		++FogCount;
		It->SetActorHiddenInGame(false);
		if (UExponentialHeightFogComponent* F = It->GetComponent())
		{
			F->SetVisibility(true);
			F->SetFogDensity(FogD);
			F->SetFogHeightFalloff(0.14f);
			F->SetFogInscatteringColor(FogC);
			F->SetVolumetricFog(true);
			F->SetVolumetricFogScatteringDistribution(ScatDist);
			F->SetVolumetricFogExtinctionScale(1.1f);
		}
	}
	if (FogCount == 0)
	{
		if (AExponentialHeightFog* Fog = World->SpawnActor<AExponentialHeightFog>(FVector::ZeroVector, FRotator::ZeroRotator, RigSp))
		{
			Fog->Tags.Add(GFallbackLightTag);
			if (UExponentialHeightFogComponent* F = Fog->GetComponent())
			{
				F->SetFogDensity(FogD);
				F->SetFogHeightFalloff(0.14f);
				F->SetFogInscatteringColor(FogC);
				F->SetVolumetricFog(true);
				F->SetVolumetricFogScatteringDistribution(ScatDist);
				F->SetVolumetricFogExtinctionScale(1.1f);
			}
		}
	}

	// 4. SkyAtmosphere (guaranteed fallback spawn)
	ASkyAtmosphere* SkyAtmo = nullptr;
	for (TActorIterator<ASkyAtmosphere> It(World); It; ++It)
	{
		SkyAtmo = *It;
		SkyAtmo->SetActorHiddenInGame(false);
		if (USkyAtmosphereComponent* C = It->GetComponent())
		{
			C->SetVisibility(true);
		}
		break;
	}
	if (!SkyAtmo)
	{
		SkyAtmo = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, RigSp);
		if (SkyAtmo)
		{
			SkyAtmo->Tags.Add(GFallbackLightTag);
			SkyAtmo->SetActorHiddenInGame(false);
			if (USkyAtmosphereComponent* C = SkyAtmo->GetComponent())
			{
				C->SetVisibility(true);
			}
		}
	}

	// 5. Unbound PostProcessVolume with AutoExposureMin=0.25 & ACES calibration (guaranteed fallback spawn)
	auto ConfigurePostProcess = [&](APostProcessVolume* PP)
	{
		if (!PP) return;
		PP->bUnbound = true;
		PP->BlendWeight = 1.0f;
		PP->Priority = 20.0f;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = EAutoExposureMethod::AEM_Histogram;
		S.bOverride_AutoExposureMinBrightness = true;
		S.AutoExposureMinBrightness = 0.25f;
		S.bOverride_AutoExposureMaxBrightness = true;
		S.AutoExposureMaxBrightness = 3.5f;
		S.bOverride_AutoExposureSpeedUp = true;
		S.AutoExposureSpeedUp = 8.0f;
		S.bOverride_AutoExposureSpeedDown = true;
		S.AutoExposureSpeedDown = 8.0f;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = Bias;
		S.bOverride_IndirectLightingIntensity = true;
		S.IndirectLightingIntensity = IndirectI;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = Bloom;
		S.bOverride_BloomThreshold = true;
		S.BloomThreshold = BloomThresh;
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = Vignette;
		S.bOverride_MotionBlurAmount = true;
		S.MotionBlurAmount = 0.0f;
		S.bOverride_FilmSlope = true;
		S.FilmSlope = 0.88f;
		S.bOverride_FilmToe = true;
		S.FilmToe = 0.44f;
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(1.06f, 1.06f, 1.06f, 1.0f);
		S.bOverride_ColorGamma = true;
		S.ColorGamma = FVector4(1.0f, 1.0f, 1.0f, 1.0f);
		S.bOverride_ColorGammaShadows = true;
		S.ColorGammaShadows = GammaSh;
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = Sat;
		S.bOverride_ColorSaturationShadows = true;
		S.ColorSaturationShadows = SatSh;
		S.bOverride_ColorSaturationHighlights = true;
		S.ColorSaturationHighlights = SatHi;
		S.bOverride_ColorGainHighlights = true;
		S.ColorGainHighlights = GainHi;
		S.bOverride_AmbientOcclusionIntensity = true;
		S.AmbientOcclusionIntensity = 0.52f;
		S.bOverride_AmbientOcclusionRadius = true;
		S.AmbientOcclusionRadius = 42.0f;
		S.bOverride_SceneFringeIntensity = true;
		S.SceneFringeIntensity = SceneFringe;
	};

	int32 PPCount = 0;
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		++PPCount;
		ConfigurePostProcess(*It);
	}
	if (PPCount == 0)
	{
		if (APostProcessVolume* PP = World->SpawnActor<APostProcessVolume>(FVector(0.0f, 0.0f, 200.0f), FRotator::ZeroRotator, RigSp))
		{
			PP->Tags.Add(GFallbackLightTag);
			ConfigurePostProcess(PP);
		}
	}
}

void AetheriaWorldDirector::PatchBrokenMaterials(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return;

	UMaterialInterface* LargeFloorMat = LoadMat({
		TEXT("/Game/Materials/M_AetheriaSanctumFloor"),
		TEXT("/Game/MedievalDungeon/Materials/M_Stone_Altar_Inst"),
		TEXT("/Game/MedievalDungeon/Materials/M_BrickFloor_Clean_Inst")
	});
	UMaterialInterface* FloorMat = LoadMat({
		TEXT("/Game/MedievalDungeon/Materials/M_Stone_Altar_Inst"),
		TEXT("/Game/MedievalDungeon/Materials/M_BrickFloor_Clean_Inst"),
		TEXT("/Game/Materials/M_AetheriaSanctumFloor")
	});
	UMaterialInterface* WallMat = LoadMat({
		TEXT("/Game/Materials/M_AetheriaGothicStone"),
		TEXT("/Game/MedievalDungeon/Materials/M_StoneWall_Inst")
	});
	if (!LargeFloorMat) LargeFloorMat = FloorMat;
	if (!FloorMat) FloorMat = LargeFloorMat;
	if (!FloorMat && !WallMat)
	{
		return;
	}
	if (!FloorMat) { FloorMat = WallMat; LargeFloorMat = WallMat; }
	if (!WallMat) WallMat = FloorMat;

	int32 RestoredOriginalSlots = 0;
	int32 FallbackReplacedSlots = 0;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Actor = *It;
		if (!Actor || Actor->IsActorBeingDestroyed()) continue;
		TArray<UStaticMeshComponent*> Comps;
		Actor->GetComponents<UStaticMeshComponent>(Comps);
		for (UStaticMeshComponent* Comp : Comps)
		{
			if (!Comp) continue;
			const UStaticMesh* SM = Comp->GetStaticMesh();
			const FString MeshName = SM ? SM->GetName().ToLower() : FString();
			if (MeshName.Contains(TEXT("skysphere")) || MeshName.Contains(TEXT("skyfloor")))
			{
				continue;
			}
			// Hide overhead dungeon/crypt ceilings that block the top-down ARPG camera
			if (MeshName.Contains(TEXT("ceiling")) && (WorldId == EAetheriaWorldId::VoidRealm || WorldId == EAetheriaWorldId::BossSanctum))
			{
				Comp->SetVisibility(false, true);
				Comp->SetHiddenInGame(true, true);
				Comp->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				continue;
			}

			const FVector Scale = Comp->GetComponentScale();
			const bool bLargeScale = (FMath::Abs(Scale.X) > 3.0f || FMath::Abs(Scale.Y) > 3.0f);
			const int32 NumMats = Comp->GetNumMaterials();
			for (int32 Slot = 0; Slot < NumMats; ++Slot)
			{
				UMaterialInterface* MI = Comp->GetMaterial(Slot);
				const FString MatName = MI ? MI->GetName() : FString();
				const bool bVoidBlownPlaster = (WorldId == EAetheriaWorldId::VoidRealm)
					&& (MeshName.Contains(TEXT("plaster")) || MatName.Contains(TEXT("Plaster")) || MatName.Contains(TEXT("White")));
				const bool bNeedsFix = (!MI)
					|| bVoidBlownPlaster
					|| MatName.Contains(TEXT("M_Cine_pbr_"))
					|| MatName.Contains(TEXT("WorldGridMaterial"))
					|| MatName.Contains(TEXT("DefaultMaterial"));
				if (!bNeedsFix)
				{
					continue;
				}

				if (!bVoidBlownPlaster)
				{
					UMaterialInterface* MeshOrigMat = (SM && Slot < SM->GetStaticMaterials().Num()) ? SM->GetMaterial(Slot) : nullptr;
					if (MeshOrigMat)
					{
						const FString OrigName = MeshOrigMat->GetName();
						const bool bOrigBad = OrigName.Contains(TEXT("M_Cine_pbr_"))
							|| OrigName.Contains(TEXT("WorldGridMaterial"))
							|| OrigName.Contains(TEXT("DefaultMaterial"))
							|| OrigName.Contains(TEXT("M_Bug_Mesh"));
						if (!bOrigBad)
						{
							Comp->SetMaterial(Slot, nullptr);
							++RestoredOriginalSlots;
							continue;
						}
					}
				}

				const FString MatLower = MatName.ToLower();
				const bool bIsFloor = MatLower.Contains(TEXT("floor"))
					|| MeshName.Contains(TEXT("floor"))
					|| MeshName.Contains(TEXT("ground"))
					|| MeshName.Contains(TEXT("tile"))
					|| MeshName.Contains(TEXT("slab"))
					|| MeshName.Contains(TEXT("stair"))
					|| MeshName.Contains(TEXT("bridge"));
				UMaterialInterface* ChosenMat = bIsFloor ? (bLargeScale ? LargeFloorMat : FloorMat) : WallMat;
				Comp->SetMaterial(Slot, ChosenMat);
				++FallbackReplacedSlots;
			}
		}
	}

	if (RestoredOriginalSlots > 0 || FallbackReplacedSlots > 0)
	{
		UE_LOG(LogAetheriaWorld, Display, TEXT("[Aetheria] PatchBrokenMaterials: restored=%d fallback=%d in %s"),
			RestoredOriginalSlots, FallbackReplacedSlots, *World->GetMapName());
	}
}

static void EnsureWorldArenaDressing(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return;

	// Avoid duplicating dressing when ApplyWorldPresentation fires twice (BeginPlay + 0.35s retry)
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(GArenaDressTag))
		{
			return;
		}
	}

	FVector Anchor = FVector(0.0f, -150.0f, 0.0f);
	for (TActorIterator<APlayerStart> It(World); It; ++It)
	{
		Anchor = It->GetActorLocation();
		break;
	}
	float GroundZ = Anchor.Z - 110.0f;
	const bool bHasGroundAtStart = FindRawGroundZ(World, Anchor, GroundZ);
	if (!bHasGroundAtStart)
	{
		GroundZ = 0.0f;
	}
	const FVector GroundOrigin(Anchor.X, Anchor.Y, GroundZ);

	UStaticMesh* FloorSM = LoadSM(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Floor"));
	UStaticMesh* PillarSM = LoadSM(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Pillar"));
	UStaticMesh* CryptWallSM = LoadSM(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Crypt/SM_Crypt_Wall_Decorative_A"));
	UStaticMesh* ArchSM = LoadSM(TEXT("/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Decorative_Arches"));
	UStaticMesh* SovereignSM = LoadSM(TEXT("/Game/ForgottenSovereign/Meshes/SM_ForgottenSovereign"));

	UMaterialInterface* SanctumFloorMat = LoadMat({
		TEXT("/Game/Materials/M_AetheriaSanctumFloor"),
		TEXT("/Game/MedievalDungeon/Materials/M_BrickFloor_Clean_Inst")
	});
	UMaterialInterface* GothicStoneMat = LoadMat({
		TEXT("/Game/Materials/M_AetheriaGothicStone"),
		TEXT("/Game/MedievalDungeon/Materials/M_Crypt_Wall_Decorative_A_Inst")
	});

	// Guaranteed walkable fallback stone floor if no floor geometry exists under PlayerStart
	if (!bHasGroundAtStart && FloorSM)
	{
		for (int32 gx = -2; gx <= 2; ++gx)
		{
			for (int32 gy = -2; gy <= 2; ++gy)
			{
				SpawnMesh(
					World, FloorSM,
					FVector(GroundOrigin.X + gx * 600.0f, GroundOrigin.Y + gy * 600.0f, GroundZ),
					FRotator::ZeroRotator, FVector(1.05f, 1.05f, 1.0f),
					SanctumFloorMat, GArenaDressTag, true);
			}
		}
	}

	const TCHAR* FireWarm = TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_1");
	const TCHAR* FireArcane = TEXT("/Game/Stylish_Fire_VFX/Niagara/NS_Stylish_Fire_2");
	const TCHAR* PortalLightning = TEXT("/Game/Portals/Rounded/LightningPortal/NS_Lightning");
	const TCHAR* PortalWater = TEXT("/Game/Portals/Rounded/WaterPortal/NS_WaterPortal");
	const TCHAR* PortalFire = TEXT("/Game/Portals/Rectangle/FirePortal/NS_FireRec");

	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion:
	{
		// Golden-hour ceremonial braziers + warm bounce lights framing the Bastion courtyard approach
		const FLinearColor GoldTorch(1.0f, 0.62f, 0.24f);
		const FVector Offsets[] = {
			FVector(420.0f, -420.0f, 0.0f),
			FVector(-320.0f, -520.0f, 0.0f),
			FVector(900.0f, -880.0f, 0.0f),
			FVector(1420.0f, -960.0f, 0.0f),
			FVector(1080.0f, -1380.0f, 0.0f),
		};
		for (const FVector& Off : Offsets)
		{
			FVector Spot = GroundOrigin + Off;
			float Z = GroundZ;
			if (FindRawGroundZ(World, Spot + FVector(0.0f, 0.0f, 150.0f), Z)) { Spot.Z = Z; }
			SpawnBrazierFlame(World, Spot, GoldTorch, 18.0f, 1800.0f, FireWarm, 2.0f, false);
		}
		// Warm overhead sun-bounce key so hero silhouette & golden highlands read crisply
		SpawnFillLight(World, GroundOrigin + FVector(380.0f, -380.0f, 680.0f), FLinearColor(1.0f, 0.80f, 0.50f), 22.0f, 3800.0f, false, 1.1f);
		break;
	}

	case EAetheriaWorldId::RuinsTemple:
	{
		// Sunlit ancient temple ruins: eliminate shadow pockets around spawn + add golden sun shafts & temple braziers
		const FLinearColor SunAmber(1.0f, 0.76f, 0.36f);
		const FVector Offsets[] = {
			FVector(260.0f, -260.0f, 0.0f),
			FVector(-280.0f, -280.0f, 0.0f),
			FVector(580.0f, -640.0f, 0.0f),
			FVector(980.0f, -980.0f, 0.0f),
			FVector(200.0f, -880.0f, 0.0f),
			FVector(880.0f, -200.0f, 0.0f),
		};
		for (const FVector& Off : Offsets)
		{
			FVector Spot = GroundOrigin + Off;
			float Z = GroundZ;
			if (FindRawGroundZ(World, Spot + FVector(0.0f, 0.0f, 180.0f), Z)) { Spot.Z = Z; }
			SpawnBrazierFlame(World, Spot, SunAmber, 20.0f, 2200.0f, FireWarm, 2.2f, false);
		}
		// Bright dusty overhead sunlight shafts illuminating the monolith courtyard
		SpawnFillLight(World, GroundOrigin + FVector(0.0f, 0.0f, 760.0f), FLinearColor(1.0f, 0.88f, 0.62f), 26.0f, 4600.0f, false, 1.8f);
		SpawnFillLight(World, GroundOrigin + FVector(750.0f, -750.0f, 680.0f), FLinearColor(1.0f, 0.84f, 0.55f), 26.0f, 4400.0f, false, 1.6f);
		break;
	}

	case EAetheriaWorldId::VoidRealm:
	{
		// Luminous astral obsidian-cyan sanctum: seal the left void drop-off with walkable stone apron + portals & cyan torches!
		if (FloorSM)
		{
			for (int32 gx = -2; gx <= 4; ++gx)
			{
				for (int32 gy = -3; gy <= 3; ++gy)
				{
					const FVector TilePos(GroundOrigin.X + gx * 600.0f, GroundOrigin.Y + gy * 600.0f, GroundZ - 14.0f);
					SpawnMesh(World, FloorSM, TilePos, FRotator::ZeroRotator, FVector(1.04f, 1.04f, 1.0f), SanctumFloorMat, GArenaDressTag, true);
				}
			}
		}

		// Glowing astral portals flanking the obsidian nave
		SpawnPortalLandmark(World, GroundOrigin + FVector(650.0f, -520.0f, 0.0f), FRotator(0.0f, 45.0f, 0.0f),
			PortalLightning, FLinearColor(0.32f, 0.84f, 1.0f), 38.0f, 1.65f);
		SpawnPortalLandmark(World, GroundOrigin + FVector(1550.0f, 0.0f, 0.0f), FRotator(0.0f, 90.0f, 0.0f),
			PortalWater, FLinearColor(0.46f, 0.62f, 1.0f), 38.0f, 1.95f);

		// Cyan-violet arcane torches illuminating the entire corridor (NEVER pitch black)
		const FLinearColor AstralCyan(0.36f, 0.86f, 1.0f);
		const FLinearColor AstralViolet(0.62f, 0.42f, 1.0f);
		const FVector TorchSpots[] = {
			FVector(-320.0f, -340.0f, 0.0f),
			FVector(-320.0f,  340.0f, 0.0f),
			FVector( 380.0f, -380.0f, 0.0f),
			FVector( 380.0f,  380.0f, 0.0f),
			FVector(1050.0f, -420.0f, 0.0f),
			FVector(1050.0f,  420.0f, 0.0f),
		};
		for (int32 i = 0; i < UE_ARRAY_COUNT(TorchSpots); ++i)
		{
			const FLinearColor& Col = (i % 2 == 0) ? AstralCyan : AstralViolet;
			SpawnBrazierFlame(World, GroundOrigin + TorchSpots[i], Col, 24.0f, 2200.0f, FireArcane, 1.9f, false);
		}
		SpawnFillLight(World, GroundOrigin + FVector(220.0f, -120.0f, 720.0f), FLinearColor(0.50f, 0.78f, 1.0f), 28.0f, 4500.0f, false, 1.4f);
		break;
	}

	case EAetheriaWorldId::BossSanctum:
	{
		// Fiery golden-amber cathedral arena: wrap the central treasure hoard in a monumental walkable stone cathedral floor!
		if (FloorSM)
		{
			for (int32 gx = -3; gx <= 4; ++gx)
			{
				for (int32 gy = -4; gy <= 3; ++gy)
				{
					const FVector TilePos(GroundOrigin.X + gx * 600.0f, GroundOrigin.Y + gy * 600.0f, GroundZ - 14.0f);
					SpawnMesh(World, FloorSM, TilePos, FRotator::ZeroRotator, FVector(1.04f, 1.04f, 1.0f), SanctumFloorMat, GArenaDressTag, true);
				}
			}
		}

		// Ring of towering cathedral columns + decorative walls + blazing braziers around the treasure arena
		const int32 PillarCount = 10;
		const float RingRadius = 1480.0f;
		const FLinearColor EmberGold(1.0f, 0.56f, 0.16f);
		for (int32 i = 0; i < PillarCount; ++i)
		{
			const float Angle = (2.0f * PI * i) / static_cast<float>(PillarCount);
			const FVector Pos = GroundOrigin + FVector(FMath::Cos(Angle) * RingRadius, FMath::Sin(Angle) * RingRadius, -10.0f);
			const FRotator Facing(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f);
			if (PillarSM)
			{
				SpawnMesh(World, PillarSM, Pos, Facing, FVector(1.35f, 1.35f, 1.55f), GothicStoneMat, GArenaDressTag, true);
			}
			if (CryptWallSM && (i % 2 == 0))
			{
				const FVector OuterPos = GroundOrigin + FVector(FMath::Cos(Angle) * (RingRadius + 380.0f), FMath::Sin(Angle) * (RingRadius + 380.0f), -10.0f);
				SpawnMesh(World, CryptWallSM, OuterPos, Facing, FVector(1.3f, 1.3f, 1.35f), GothicStoneMat, GArenaDressTag, true);
			}
			if (i % 2 == 0)
			{
				const FVector BrazierPos = GroundOrigin + FVector(FMath::Cos(Angle) * 860.0f, FMath::Sin(Angle) * 860.0f, 0.0f);
				SpawnBrazierFlame(World, BrazierPos, EmberGold, 26.0f, 2500.0f, FireWarm, 2.5f, false);
			}
		}

		// Monumental Archon statue + blazing fiery boss portal framing the far cathedral altar
		const FVector AltarPos = GroundOrigin + FVector(1250.0f, -1250.0f, 0.0f);
		if (SovereignSM)
		{
			SpawnMesh(World, SovereignSM, AltarPos + FVector(280.0f, -280.0f, 0.0f), FRotator(0.0f, 135.0f, 0.0f), FVector(1.65f), nullptr, GArenaDressTag, true);
		}
		else if (ArchSM)
		{
			SpawnMesh(World, ArchSM, AltarPos + FVector(220.0f, -220.0f, 0.0f), FRotator(0.0f, 45.0f, 0.0f), FVector(1.35f), GothicStoneMat, GArenaDressTag, false);
		}
		SpawnPortalLandmark(World, AltarPos, FRotator(0.0f, 45.0f, 0.0f), PortalFire, FLinearColor(1.0f, 0.48f, 0.14f), 48.0f, 2.2f);
		SpawnFillLight(World, GroundOrigin + FVector(0.0f, 0.0f, 780.0f), FLinearColor(1.0f, 0.72f, 0.38f), 28.0f, 4800.0f, false, 1.7f);
		break;
	}

	default:
		break;
	}
}

// Build a readable Diablo/PoE-style route along the top-down look axis (+X,-Y):
// intro plaza -> obelisk seal -> sanctum gate -> elite vanguard -> boss arena.
void AetheriaWorldDirector::Dress(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return;
	StreamWorldLayoutsIfSparse(World, WorldId);
	PatchBrokenMaterials(World, WorldId);
	EnsureWorldArenaDressing(World, WorldId);
	ApplyLighting(World, WorldId);
	UE_LOG(LogAetheriaWorld, Display, TEXT("[Aetheria] Dressed world %s map=%s layout=%s"),
		WorldDisplayName(WorldId), *World->GetMapName(), LayoutMapForWorld(WorldId));
}

const TCHAR* AetheriaWorldDirector::PersistentMapForWorld(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::RuinsTemple: return TEXT("/Game/Maps/AetheriaWorlds/World_Ruins");
	case EAetheriaWorldId::VoidRealm: return TEXT("/Game/Maps/AetheriaWorlds/World_Void");
	case EAetheriaWorldId::BossSanctum: return TEXT("/Game/Maps/AetheriaWorlds/World_Sanctum");
	default: return TEXT("/Game/Maps/AetheriaWorlds/World_Bastion");
	}
}

FVector AetheriaWorldDirector::PlayerStartForWorld(UWorld* World, EAetheriaWorldId WorldId)
{
	FVector Base = FVector(0.f, -150.f, 120.f);
	bool bFoundStart = false;
	if (World)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			Base = It->GetActorLocation();
			bFoundStart = true;
			break;
		}
	}
	if (!bFoundStart)
	{
		switch (WorldId)
		{
		case EAetheriaWorldId::RuinsTemple:
			Base = FVector(4520.f, -4585.f, 200.f);
			break;
		case EAetheriaWorldId::BossSanctum:
			Base = FVector(0.f, 0.f, 110.f);
			break;
		default:
			Base = FVector(0.f, -150.f, 120.f);
			break;
		}
	}

	float GroundZ = 0.0f;
	if (World && FindRawGroundZ(World, Base, GroundZ))
	{
		Base.Z = GroundZ + 108.0f;
	}
	return Base;
}

bool AetheriaWorldDirector::MapBelongsToWorld(const FString& MapName, EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion: return MapName.Contains(TEXT("World_Bastion"));
	case EAetheriaWorldId::RuinsTemple: return MapName.Contains(TEXT("World_Ruins"));
	case EAetheriaWorldId::VoidRealm: return MapName.Contains(TEXT("World_Void"));
	case EAetheriaWorldId::BossSanctum: return MapName.Contains(TEXT("World_Sanctum"));
	default: return false;
	}
}

EAetheriaWorldId AetheriaWorldDirector::WorldIdFromMapName(const FString& MapName)
{
	if (MapName.Contains(TEXT("World_Ruins"))) return EAetheriaWorldId::RuinsTemple;
	if (MapName.Contains(TEXT("World_Void"))) return EAetheriaWorldId::VoidRealm;
	if (MapName.Contains(TEXT("World_Sanctum"))) return EAetheriaWorldId::BossSanctum;
	return EAetheriaWorldId::Bastion;
}

bool AetheriaWorldDirector::IsBakedWorldMap(const FString& MapName)
{
	return MapName.Contains(TEXT("World_Bastion"))
		|| MapName.Contains(TEXT("World_Ruins"))
		|| MapName.Contains(TEXT("World_Void"))
		|| MapName.Contains(TEXT("World_Sanctum"));
}

AAetheriaSanctumGate* AetheriaWorldDirector::SpawnGate(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return nullptr;
	for (TActorIterator<AAetheriaSanctumGate> It(World); It; ++It)
	{
		if (IsValid(*It) && !It->IsActorBeingDestroyed())
		{
			return *It;
		}
	}
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	FVector GateLoc = GateLocationForWorld(WorldId);
	float GroundZ = GateLoc.Z;
	if (FindRawGroundZ(World, GateLoc + FVector(0.0f, 0.0f, 200.0f), GroundZ))
	{
		GateLoc.Z = GroundZ;
	}
	AAetheriaSanctumGate* Gate = World->SpawnActor<AAetheriaSanctumGate>(GateLoc, FRotator(0.f, 45.f, 0.f), Sp);
	if (Gate)
	{
		Gate->Tags.Add(GDressTag);
	}
	return Gate;
}

const TCHAR* AetheriaWorldDirector::BossHeroForWorld(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion: return TEXT("Aurora");
	case EAetheriaWorldId::RuinsTemple: return TEXT("Khaimera");
	case EAetheriaWorldId::VoidRealm: return TEXT("Sevarog");
	case EAetheriaWorldId::BossSanctum: return TEXT("Grux");
	default: return TEXT("Sevarog");
	}
}

const TCHAR* AetheriaWorldDirector::BossPackageForWorld(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion: return TEXT("/Game/ParagonAurora");
	case EAetheriaWorldId::RuinsTemple: return TEXT("/Game/ParagonKhaimera");
	case EAetheriaWorldId::VoidRealm: return TEXT("/Game/ParagonSevarog");
	case EAetheriaWorldId::BossSanctum: return TEXT("/Game/ParagonGrux");
	default: return TEXT("/Game/ParagonSevarog");
	}
}

FVector AetheriaWorldDirector::GateLocationForWorld(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::RuinsTemple:
		return FVector(5380.f, -5380.f, 120.f);
	case EAetheriaWorldId::VoidRealm:
		return FVector(1550.f, -120.f, 0.f);
	case EAetheriaWorldId::BossSanctum:
		return FVector(780.f, -780.f, 0.f);
	case EAetheriaWorldId::Bastion:
	default:
		return FVector(1250.f, -1100.f, 0.f);
	}
}

FVector AetheriaWorldDirector::BossArenaLocationForWorld(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::RuinsTemple:
		return FVector(6200.f, -6150.f, 120.f);
	case EAetheriaWorldId::VoidRealm:
		return FVector(2850.f, -150.f, 0.f);
	case EAetheriaWorldId::BossSanctum:
		return FVector(1150.f, -1150.f, 20.f);
	case EAetheriaWorldId::Bastion:
	default:
		return FVector(2150.f, -1950.f, 0.f);
	}
}
