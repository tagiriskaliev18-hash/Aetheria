#include "AetheriaWorldDirector.h"
#include "Engine/LevelStreamingDynamic.h"
#include "AetheriaSanctumGate.h"
#include "AetheriaPackVFX.h"
#include "Engine/World.h"
#include "GameFramework/PlayerStart.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Components/StaticMeshComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/PostProcessComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/SpotLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/PointLight.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Misc/PackageName.h"
#include "Materials/MaterialInterface.h"
#include "Engine/TextureLightProfile.h"
#include "Engine/DecalActor.h"
#include "Components/DecalComponent.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogAetheriaWorld, Log, All);

static const FName GDressTag(TEXT("AetheriaBastionDressing"));

namespace
{
	UStaticMesh* LoadSM(const TCHAR* Pkg)
	{
		if (!Pkg) return nullptr;
		const FString Obj = FString::Printf(TEXT("%s.%s"), Pkg, *FPackageName::GetShortName(Pkg));
		return LoadObject<UStaticMesh>(nullptr, *Obj, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	AStaticMeshActor* SpawnMesh(UWorld* World, UStaticMesh* Mesh, const FVector& Loc, const FRotator& Rot, const FVector& Scale)
	{
		if (!World || !Mesh) return nullptr;
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AStaticMeshActor* Actor = World->SpawnActor<AStaticMeshActor>(Loc, Rot, Sp);
		if (!Actor) return nullptr;
		Actor->Tags.Add(GDressTag);
		if (UStaticMeshComponent* C = Actor->GetStaticMeshComponent())
		{
			C->SetMobility(EComponentMobility::Movable);
			C->SetStaticMesh(Mesh);
			C->SetWorldScale3D(Scale);
			C->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
		}
		return Actor;
	}

	void SpawnFillLight(UWorld* World, const FVector& Loc, const FLinearColor& Color, float Candelas, float Radius, bool bShadows)
	{
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (APointLight* PL = World->SpawnActor<APointLight>(Loc, FRotator::ZeroRotator, Sp))
		{
			PL->Tags.Add(GDressTag);
			if (UPointLightComponent* C = Cast<UPointLightComponent>(PL->GetLightComponent()))
			{
				C->SetMobility(EComponentMobility::Movable);
				C->SetIntensityUnits(ELightUnits::Candelas);
				C->SetLightColor(Color);
				C->SetIntensity(Candelas);
				C->SetAttenuationRadius(Radius);
				C->SetCastShadows(bShadows);
				C->SetSourceRadius(40.0f);
				C->SetVolumetricScatteringIntensity(bShadows ? 1.6f : 0.5f);
				if (UTextureLightProfile* IES = LoadObject<UTextureLightProfile>(nullptr,
					TEXT("/Game/LightProfilePack/Lighting/Profiles/Beam.Beam"), nullptr, LOAD_NoWarn | LOAD_Quiet))
				{
					C->SetIESTexture(IES);
					C->SetUseIESBrightness(true);
				}
			}
		}
	}

	void SpawnDecal(UWorld* World, const TCHAR* MatPath, const FVector& Loc, const FRotator& Rot, const FVector& Size)
	{
		if (!World || !MatPath) return;
		UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, MatPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Mat) return;
		FActorSpawnParameters Sp;
		Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		if (ADecalActor* Decal = World->SpawnActor<ADecalActor>(Loc, Rot, Sp))
		{
			Decal->Tags.Add(GDressTag);
			if (UDecalComponent* C = Decal->GetDecal())
			{
				C->SetDecalMaterial(Mat);
				C->DecalSize = Size;
				C->SetFadeScreenSize(0.0f);
			}
		}
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
	case EAetheriaWorldId::Bastion: return TEXT("Готический двор и камень Агоры.");
	case EAetheriaWorldId::RuinsTemple: return TEXT("Площадь Агоры. Колонны и руины.");
	case EAetheriaWorldId::VoidRealm: return TEXT("Средневековые крипты, пустотный свет.");
	case EAetheriaWorldId::BossSanctum: return TEXT("Сокровищница Древних. Арена босса.");
	default: return TEXT("");
	}
}


static TArray<TWeakObjectPtr<ULevelStreamingDynamic>> GAetheriaStreamedLayouts;
static bool GAetheriaHasStreamedWorld = false;
static EAetheriaWorldId GAetheriaStreamedWorld = EAetheriaWorldId::Bastion;

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
		UE_LOG(LogAetheriaWorld, Display, TEXT("[Aetheria] Streamed artist layout %s at %s ok=%d"), Path, *Loc.ToCompactString(), bOk ? 1 : 0);
	}
	else
	{
		UE_LOG(LogAetheriaWorld, Warning, TEXT("[Aetheria] Failed to stream artist layout %s — duplicate pack demo maps into /Game/Maps/AetheriaLayouts first"), Path);
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

// One complete artist-made demo map per world. Do not guess a second-instance
// offset (that overlaps or mis-scales). After inventory, Bastion will stitch
// Gothic start -> gate -> Medieval rooms -> Treasures boss using measured bounds.
static const TCHAR* LayoutGothic  = TEXT("/Game/Maps/AetheriaLayouts/Layout_GothicCourtyard");
static const TCHAR* LayoutDungeon = TEXT("/Game/Maps/AetheriaLayouts/Layout_MedievalDungeon");
static const TCHAR* LayoutSanctum = TEXT("/Game/Maps/AetheriaLayouts/Layout_TreasureSanctum");

static void StreamWorldLayouts(UWorld* World, EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::Bastion:
		// Gothic Review is a single catalog building at the origin and would
		// overlap the dungeon PlayerStart. Use the composed Medieval demo.
		StreamArtistLayout(World, LayoutDungeon, FVector::ZeroVector, FRotator::ZeroRotator);
		break;
	case EAetheriaWorldId::RuinsTemple:
		StreamArtistLayout(World, LayoutDungeon, FVector::ZeroVector, FRotator::ZeroRotator);
		break;
	case EAetheriaWorldId::VoidRealm:
		StreamArtistLayout(World, LayoutDungeon, FVector::ZeroVector, FRotator::ZeroRotator);
		break;
	case EAetheriaWorldId::BossSanctum:
		StreamArtistLayout(World, LayoutSanctum, FVector::ZeroVector, FRotator::ZeroRotator);
		break;
	default:
		StreamArtistLayout(World, LayoutGothic, FVector::ZeroVector, FRotator::ZeroRotator);
		break;
	}
}

void AetheriaWorldDirector::ClearDressing(UWorld* World)
{
	if (!World) return;
	UnloadArtistLayouts(World);
	TArray<AActor*> ToDestroy;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		if (It->ActorHasTag(GDressTag))
		{
			ToDestroy.Add(*It);
		}
	}
	for (AActor* A : ToDestroy)
	{
		A->Destroy();
	}
}

// QA 2026-11-23: the baked Void layout ships one boundary wall hanging below
// the floor plane (AABB top z=0 -> renders as a black pit; one slot still on
// the WorldGridMaterial placeholder) and a missing SM_Floor tile row behind
// it (x~1650, y 800..1100). Runtime patch mirrors the GUI-editor fix; every
// clause is guarded so it becomes a no-op once the map is corrected at source.
static void PatchVoidField(UWorld* World)
{
	if (!World) return;

	UMaterialInterface* StoneMI = nullptr;
	AStaticMeshActor* SunkenWall = nullptr;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* A = *It;
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
		if (!M || !M->GetName().StartsWith(TEXT("SM_Dungeon_Wall")) ||
		    M->GetName().Contains(TEXT("Decorative")))
		{
			continue;
		}
		const FVector O = A->GetActorLocation();
		const FBox B = A->GetComponentsBoundingBox();
		if (B.Max.Z < 40.0f && O.X > 1400.0f && O.X < 1800.0f &&
		    O.Y > 900.0f && O.Y < 1300.0f)
		{
			SunkenWall = A;
			continue;
		}
		for (int32 i = 0; i < C->GetNumMaterials() && !StoneMI; ++i)
		{
			UMaterialInterface* MI = C->GetMaterial(i);
			if (MI && MI->GetName() == TEXT("M_StoneWall_Inst"))
			{
				StoneMI = MI;
			}
		}
	}

	if (SunkenWall && StoneMI)
	{
		const FVector O = SunkenWall->GetActorLocation();
		SunkenWall->SetActorLocation(FVector(O.X, O.Y, O.Z + 300.0f));
		int32 Fixed = 0;
		TArray<UStaticMeshComponent*> WallComps;
		SunkenWall->GetComponents(WallComps);
		for (UStaticMeshComponent* C : WallComps)
		{
			for (int32 i = 0; i < C->GetNumMaterials(); ++i)
			{
				UMaterialInterface* MI = C->GetMaterial(i);
				if (MI && MI->GetName() == TEXT("WorldGridMaterial"))
				{
					C->SetMaterial(i, StoneMI);
					++Fixed;
				}
			}
		}
		UE_LOG(LogTemp, Display,
		       TEXT("[Aetheria] VoidField patch: wall raised +300, %d placeholder slot(s) -> M_StoneWall_Inst"),
		       Fixed);
	}

	// missing floor row: clone the nearest healthy SM_Floor tile
	AStaticMeshActor* BaseTile = nullptr;
	bool Covered = false;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		AStaticMeshActor* A = *It;
		UStaticMeshComponent* C = A->GetStaticMeshComponent();
		UStaticMesh* M = C ? C->GetStaticMesh() : nullptr;
		if (!M)
			continue;
		const FVector O = A->GetActorLocation();
		const FBox B = A->GetComponentsBoundingBox();
		if (M->GetName().StartsWith(TEXT("SM_Floor")) && O.Z > -100.0f &&
		    O.Z < 100.0f && !BaseTile)
		{
			BaseTile = A;
		}
		if (B.Min.X <= 1650.0f && B.Max.X >= 1650.0f &&
		    B.Min.Y <= 1050.0f && B.Max.Y >= 1050.0f &&
		    B.Max.Z > -50.0f && B.Max.Z < 400.0f)
		{
			Covered = true;
		}
	}
	if (BaseTile && !Covered)
	{
		UStaticMeshComponent* BC = BaseTile->GetStaticMeshComponent();
		UStaticMesh* TileMesh = BC->GetStaticMesh();
		const FRotator TileRot = BaseTile->GetActorRotation();
		UMaterialInterface* TileMI = BC->GetMaterial(0);

		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride =
		    ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		const FVector Spots[2] = {FVector(1650.0, 800.0, 0.0),
		                          FVector(1650.0, 1100.0, 0.0)};
		for (const FVector& Pos : Spots)
		{
			AStaticMeshActor* T = World->SpawnActor<AStaticMeshActor>(
			    AStaticMeshActor::StaticClass(), Pos, TileRot, P);
			if (!T)
				continue;
			UStaticMeshComponent* C = T->GetStaticMeshComponent();
			C->SetStaticMesh(TileMesh);
			C->SetMobility(EComponentMobility::Stationary);
			C->SetMaterial(0, TileMI);
			T->SetActorRotation(TileRot);
			UE_LOG(LogTemp, Display,
			       TEXT("[Aetheria] VoidField patch: SM_Floor tile placed at (%.0f, %.0f)"),
			       Pos.X, Pos.Y);
		}
	}
}

void AetheriaWorldDirector::ApplyLighting(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return;

	// Readability pass: the first bake read as near-black on half the screen.
	// Roughly double key+sky and lift manual exposure ~1 stop per world; the
	// per-world spread (Void stays darkest) keeps the dark-fantasy mood.
	// QA 2026-11-22 retest rounds 1-2: Bastion got a real warm key with a
	// proper key:fill ratio (95/22, upper-left) after the flat-wash P0; Void
	// exposure/bloom pulled back + spots clamped for the Sevaug wall blowout.
	float DirI = 85.0f, SkyI = 22.0f, FogD = 0.012f, Bias = 5.2f, Bloom = 1.25f, Vignette = 0.18f;
	FLinearColor DirC(0.66f, 0.74f, 0.92f);
	FLinearColor SkyC(0.40f, 0.48f, 0.62f);
	FLinearColor FogC(0.26f, 0.27f, 0.31f);
	FVector4 Sat(1.16f, 1.16f, 1.16f, 1.0f);
	FVector4 SatSh(0.92f, 1.04f, 1.16f, 1.0f);
	FVector4 SatHi(1.18f, 1.08f, 0.92f, 1.0f);
	FRotator DirRot(-56.0f, -30.0f, 0.0f);

	switch (WorldId)
	{
	case EAetheriaWorldId::RuinsTemple:
		DirI = 42.0f; SkyI = 18.0f; FogD = 0.011f; Bias = 4.2f;
		DirC = FLinearColor(0.85f, 0.78f, 0.55f);
		SkyC = FLinearColor(0.45f, 0.55f, 0.42f);
		FogC = FLinearColor(0.28f, 0.32f, 0.22f);
		DirRot = FRotator(-48.0f, 40.0f, 0.0f);
		SatHi = FVector4(1.20f, 1.12f, 0.85f, 1.0f);
		break;
	case EAetheriaWorldId::VoidRealm:
		DirI = 22.0f; SkyI = 13.0f; FogD = 0.016f; Bias = 3.6f; Vignette = 0.32f; Bloom = 1.20f;
		DirC = FLinearColor(0.45f, 0.35f, 0.95f);
		SkyC = FLinearColor(0.22f, 0.18f, 0.40f);
		FogC = FLinearColor(0.14f, 0.10f, 0.24f);
		Sat = FVector4(1.22f, 1.05f, 1.28f, 1.0f);
		SatSh = FVector4(0.80f, 0.85f, 1.25f, 1.0f);
		SatHi = FVector4(0.95f, 1.15f, 1.30f, 1.0f);
		DirRot = FRotator(-62.0f, -10.0f, 0.0f);
		break;
	case EAetheriaWorldId::BossSanctum:
		DirI = 32.0f; SkyI = 12.0f; FogD = 0.013f; Bias = 4.3f; Bloom = 1.40f; Vignette = 0.26f;
		DirC = FLinearColor(0.95f, 0.62f, 0.35f);
		SkyC = FLinearColor(0.35f, 0.28f, 0.32f);
		FogC = FLinearColor(0.20f, 0.12f, 0.10f);
		SatHi = FVector4(1.28f, 1.10f, 0.82f, 1.0f);
		DirRot = FRotator(-70.0f, 15.0f, 0.0f);
		break;
	default:
		break;
	}

	for (TActorIterator<ADirectionalLight> It(World); It; ++It)
	{
		if (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))
		{
			It->SetActorRotation(DirRot);
			L->SetIntensity(DirI);
			L->SetLightColor(DirC);
			L->SetAtmosphereSunLight(true);
			L->SetVolumetricScatteringIntensity(4.5f);
			L->SetEnableLightShaftOcclusion(true);
			L->SetOcclusionMaskDarkness(0.35f);
			L->SetOcclusionDepthRange(25000.0f);
			L->SetEnableLightShaftBloom(true);
			L->SetBloomScale(1.25f);
			L->SetBloomThreshold(0.0f);
			L->SetBloomTint(FColor(200, 170, 255));
		}
	}
	// QA 2026-11-22: the Sevaug wall reads as an unshadowed blown spot; clamp
	// runaway spot intensities so level torches stay torch-sized (Void only —
	// the only world where the blowout was flagged).
	if (WorldId == EAetheriaWorldId::VoidRealm)
	{
		for (TActorIterator<ASpotLight> SpotIt(World); SpotIt; ++SpotIt)
		{
			if (USpotLightComponent* S = Cast<USpotLightComponent>(SpotIt->GetLightComponent()))
			{
				if (S->Intensity > 12.0f)
				{
					S->SetIntensity(12.0f);
				}
			}
		}
		PatchVoidField(World);
	}
	for (TActorIterator<ASkyLight> It(World); It; ++It)
	{
		if (USkyLightComponent* L = It->GetLightComponent())
		{
			L->SetIntensity(SkyI);
			L->SetLightColor(SkyC);
			L->bRealTimeCapture = true;
			L->MarkRenderStateDirty();
		}
	}
	for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
	{
		if (UExponentialHeightFogComponent* F = It->GetComponent())
		{
			F->SetFogDensity(FogD);
			F->SetFogHeightFalloff(0.12f);
			F->SetFogInscatteringColor(FogC);
			F->SetVolumetricFog(true);
			F->SetVolumetricFogScatteringDistribution(0.72f);
		}
	}
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
		FActorSpawnParameters SkySp;
		SkySp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SkyAtmo = World->SpawnActor<ASkyAtmosphere>(FVector::ZeroVector, FRotator::ZeroRotator, SkySp);
		if (SkyAtmo)
		{
			SkyAtmo->SetActorHiddenInGame(false);
			if (USkyAtmosphereComponent* C = SkyAtmo->GetComponent())
			{
				C->SetVisibility(true);
			}
		}
	}
	for (TActorIterator<APostProcessVolume> It(World); It; ++It)
	{
		APostProcessVolume* PP = *It;
		PP->bUnbound = true;
		PP->BlendWeight = 1.0f;
		PP->Priority = 10.0f;
		FPostProcessSettings& S = PP->Settings;
		S.bOverride_AutoExposureMethod = true;
		S.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;
		S.bOverride_AutoExposureBias = true;
		S.AutoExposureBias = Bias;
		S.bOverride_BloomIntensity = true;
		S.BloomIntensity = Bloom;
		S.bOverride_BloomThreshold = true;
		S.BloomThreshold = 0.45f;
		S.bOverride_VignetteIntensity = true;
		S.VignetteIntensity = Vignette;
		S.bOverride_MotionBlurAmount = true;
		S.MotionBlurAmount = 0.0f;
		S.bOverride_ColorContrast = true;
		S.ColorContrast = FVector4(1.22f, 1.22f, 1.22f, 1.0f);
		S.bOverride_ColorSaturation = true;
		S.ColorSaturation = Sat;
		S.bOverride_ColorSaturationShadows = true;
		S.ColorSaturationShadows = SatSh;
		S.bOverride_ColorSaturationHighlights = true;
		S.ColorSaturationHighlights = SatHi;
		S.bOverride_ColorGainHighlights = true;
		S.ColorGainHighlights = FVector4(1.10f, 1.03f, 0.90f, 1.0f);
		S.bOverride_AmbientOcclusionIntensity = true;
		S.AmbientOcclusionIntensity = 1.0f;
		S.bOverride_AmbientOcclusionRadius = true;
		S.AmbientOcclusionRadius = 45.0f;
		S.bOverride_SceneFringeIntensity = true;
		S.SceneFringeIntensity = 0.4f;
	}
}


static UStaticMesh* LoadDungeonSM(const TCHAR* Path)
{
	return LoadObject<UStaticMesh>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
}

// Build a readable Diablo/PoE-style route along the top-down look axis (+X,-Y):
// intro plaza (no combat) -> corridor -> gate -> arena -> chokepoint -> far arena / boss pit.
// No ceilings over the playable camera. No inhibitor rings, no random crate scatter.
void AetheriaWorldDirector::Dress(UWorld* World, EAetheriaWorldId WorldId)
{
	if (!World) return;
	// Geometry is baked into /Game/Maps/AetheriaWorlds/World_* in the editor
	// (Movable mobility, ForceNoPrecomputedLighting). Do not runtime
	// LoadLevelInstance — that path rendered as an empty void in -game.
	ApplyLighting(World, WorldId);
	UE_LOG(LogAetheriaWorld, Display, TEXT("[Aetheria] Dressed baked world %s map=%s"), WorldDisplayName(WorldId), *World->GetMapName());
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
	if (World)
	{
		for (TActorIterator<APlayerStart> It(World); It; ++It)
		{
			return It->GetActorLocation();
		}
	}
	switch (WorldId)
	{
	case EAetheriaWorldId::BossSanctum:
		return FVector(0.f, 0.f, 90.f);
	default:
		return FVector(0.f, -150.f, 120.f);
	}
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
	FActorSpawnParameters Sp;
	Sp.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	const FVector GateLoc = GateLocationForWorld(WorldId);
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
	// Measured from demo-map inventory. Dungeon: Doorway_Large2 mid-route.
	switch (WorldId)
	{
	case EAetheriaWorldId::BossSanctum:
		return FVector(200.f, -200.f, 0.f);
	default:
		return FVector(3150.f, -100.f, 0.f);
	}
}

FVector AetheriaWorldDirector::BossArenaLocationForWorld(EAetheriaWorldId WorldId)
{
	switch (WorldId)
	{
	case EAetheriaWorldId::BossSanctum:
		return FVector(-400.f, -200.f, 20.f);
	default:
		// Medieval crypt doorway at the far +X end of the demo.
		return FVector(7950.f, -350.f, -500.f);
	}
}
