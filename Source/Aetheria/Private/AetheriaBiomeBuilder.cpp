#include "AetheriaBiomeBuilder.h"
#include "AetheriaBiomeDatabase.h"
#include "AetheriaMaterialLibrary.h"
#include "AetheriaGameState.h"
#include "AetheriaSpellActors.h"

#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/DirectionalLight.h"
#include "Engine/SkyLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Components/LightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "AetheriaWizardCharacter.h"

static FVector ScaleMeshToTargetSizeUU(const UStaticMesh* Mesh, const FVector& TargetSizeUU)
{
	if (!Mesh) return TargetSizeUU / 100.0f;
	const FVector Raw = Mesh->GetBoundingBox().GetSize();
	return FVector(
		TargetSizeUU.X / FMath::Max(10.0f, Raw.X),
		TargetSizeUU.Y / FMath::Max(10.0f, Raw.Y),
		TargetSizeUU.Z / FMath::Max(10.0f, Raw.Z));
}

static FVector ScaleMeshUniformHeightUU(const UStaticMesh* Mesh, float TargetHeightUU, float MaxHorizRadiusUU = 0.0f)
{
	if (!Mesh) return FVector(TargetHeightUU / 100.0f);
	const FVector Raw = Mesh->GetBoundingBox().GetSize();
	float S = TargetHeightUU / FMath::Max(10.0f, Raw.Z);
	if (MaxHorizRadiusUU > 0.0f)
	{
		const float MaxXY = FMath::Max(Raw.X, Raw.Y);
		if (MaxXY * S > MaxHorizRadiusUU * 2.0f)
		{
			S = (MaxHorizRadiusUU * 2.0f) / FMath::Max(10.0f, MaxXY);
		}
	}
	return FVector(S);
}

static float ComputeGroundedOffsetZ(const UStaticMesh* Mesh, float ScaleZ)
{
	if (!Mesh) return 0.0f;
	const FBox Box = Mesh->GetBoundingBox();
	return -Box.Min.Z * ScaleZ;
}

AAetheriaBiomeBuilder::AAetheriaBiomeBuilder()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("BiomeRoot"));
	SetRootComponent(Root);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));

	ArenaFloorMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ArenaFloorMesh"));
	ArenaFloorMesh->SetupAttachment(Root);
	if (CubeFinder.Succeeded())
	{
		ArenaFloorMesh->SetStaticMesh(CubeFinder.Object);
	}
	ArenaFloorMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -100.0f));
	ArenaFloorMesh->SetRelativeScale3D(FVector(360.0f, 360.0f, 2.0f));
	ArenaFloorMesh->SetCollisionProfileName(TEXT("BlockAll"));

	PillarsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PillarsISM"));
	PillarsISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) PillarsISM->SetStaticMesh(CylinderFinder.Object);
	PillarsISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CrystalsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CrystalsISM"));
	CrystalsISM->SetupAttachment(Root);
	if (ConeFinder.Succeeded()) CrystalsISM->SetStaticMesh(ConeFinder.Object);
	CrystalsISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	RingsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("RingsISM"));
	RingsISM->SetupAttachment(Root);
	if (PlaneFinder.Succeeded()) RingsISM->SetStaticMesh(PlaneFinder.Object);
	RingsISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	HazardSpireISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("HazardSpireISM"));
	HazardSpireISM->SetupAttachment(Root);
	if (ConeFinder.Succeeded()) HazardSpireISM->SetStaticMesh(ConeFinder.Object);
	HazardSpireISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BouldersISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BouldersISM"));
	BouldersISM->SetupAttachment(Root);
	if (SphereFinder.Succeeded()) BouldersISM->SetStaticMesh(SphereFinder.Object);
	BouldersISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	BraziersISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("BraziersISM"));
	BraziersISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) BraziersISM->SetStaticMesh(CylinderFinder.Object);
	BraziersISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StatuesISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("StatuesISM"));
	StatuesISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) StatuesISM->SetStaticMesh(CylinderFinder.Object);
	StatuesISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GatesISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("GatesISM"));
	GatesISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) GatesISM->SetStaticMesh(CylinderFinder.Object);
	GatesISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	ChestsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ChestsISM"));
	ChestsISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) ChestsISM->SetStaticMesh(CylinderFinder.Object);
	ChestsISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	LanternsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("LanternsISM"));
	LanternsISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) LanternsISM->SetStaticMesh(CylinderFinder.Object);
	LanternsISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	CitadelVistaISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CitadelVistaISM"));
	CitadelVistaISM->SetupAttachment(Root);
	if (ConeFinder.Succeeded()) CitadelVistaISM->SetStaticMesh(ConeFinder.Object);
	CitadelVistaISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TerrainPlatformsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TerrainPlatformsISM"));
	TerrainPlatformsISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) TerrainPlatformsISM->SetStaticMesh(CylinderFinder.Object);
	TerrainPlatformsISM->SetCollisionProfileName(TEXT("BlockAll"));

	CoverParapetsISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("CoverParapetsISM"));
	CoverParapetsISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) CoverParapetsISM->SetStaticMesh(CylinderFinder.Object);
	CoverParapetsISM->SetCollisionProfileName(TEXT("BlockAll"));

	TrenchFloorISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TrenchFloorISM"));
	TrenchFloorISM->SetupAttachment(Root);
	if (CylinderFinder.Succeeded()) TrenchFloorISM->SetStaticMesh(CylinderFinder.Object);
	TrenchFloorISM->SetCollisionProfileName(TEXT("NoCollision"));
	TrenchFloorISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AAetheriaBiomeBuilder::BeginPlay()
{
	Super::BeginPlay();
	EnsureEnvironmentActorsExist();
	RefreshBiomeMeshes();
	BuildLocation(CurrentLocationIndex);
}

void AAetheriaBiomeBuilder::RefreshBiomeMeshes()
{
	if (UStaticMesh* PillarMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_GothicPillar"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		PillarsISM->SetStaticMesh(PillarMesh);
	}
	if (UStaticMesh* ArchMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_RuinedArch"), TEXT("/Engine/BasicShapes/Plane.Plane")))
	{
		RingsISM->SetStaticMesh(ArchMesh);
	}
	if (UStaticMesh* BoulderMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CliffBoulder"), TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		BouldersISM->SetStaticMesh(BoulderMesh);
	}
	if (UStaticMesh* BrazierMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_IronBrazier"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		BraziersISM->SetStaticMesh(BrazierMesh);
	}
	if (UStaticMesh* TreeOrSpireMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(
		(CurrentLocationIndex == 0 || CurrentLocationIndex == 3 || CurrentLocationIndex == 4) ? TEXT("SM_DeadTree") : TEXT("SM_RunicAltar"),
		TEXT("/Engine/BasicShapes/Cone.Cone")))
	{
		CrystalsISM->SetStaticMesh(TreeOrSpireMesh);
	}

	// 5 Drop-in Gothic Architecture & Prop Meshes
	UStaticMesh* StatueMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_GothicStatue"), TEXT("/Game/Meshes/SM_GothicPillar.SM_GothicPillar"));
	if (!StatueMesh) StatueMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_GothicPillar"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (StatueMesh) StatuesISM->SetStaticMesh(StatueMesh);

	const FString GateMeshName = (CurrentLocationIndex % 2 == 0) ? TEXT("SM_LargeIronGate") : TEXT("SM_CastleDoor");
	UStaticMesh* GateMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(GateMeshName, TEXT("/Game/Meshes/SM_RuinedArch.SM_RuinedArch"));
	if (!GateMesh) GateMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_RuinedArch"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (GateMesh) GatesISM->SetStaticMesh(GateMesh);

	if (UStaticMesh* ChestMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_TreasureChest"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder")))
	{
		ChestsISM->SetStaticMesh(ChestMesh);
	}

	FString LanternName = TEXT("SM_GothicLantern");
	if (CurrentLocationIndex == 1 || CurrentLocationIndex == 7) // Caldera / Titan Forge
	{
		LanternName = TEXT("SM_StoneFirePit");
	}
	else if (CurrentLocationIndex == 0 || CurrentLocationIndex == 5) // Bastion / Mirror Labyrinth
	{
		LanternName = TEXT("SM_LanternChandelier");
	}
	else if (CurrentLocationIndex == 3 || CurrentLocationIndex == 4) // Gravity / Storm Necropolis
	{
		LanternName = TEXT("SM_LionRelief");
	}
	UStaticMesh* LanternMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(LanternName, TEXT("/Game/Meshes/SM_IronBrazier.SM_IronBrazier"));
	if (!LanternMesh) LanternMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_IronBrazier"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (LanternMesh) LanternsISM->SetStaticMesh(LanternMesh);

	UStaticMesh* CitadelMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CitadelFortress"), TEXT("/Game/Meshes/SM_RuinedArch.SM_RuinedArch"));
	if (!CitadelMesh) CitadelMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_RuinedArch"), TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (CitadelMesh) CitadelVistaISM->SetStaticMesh(CitadelMesh);

	UStaticMesh* ParapetMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_RunicAltar"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (!ParapetMesh) ParapetMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CliffBoulder"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (ParapetMesh)
	{
		CoverParapetsISM->SetStaticMesh(ParapetMesh);
	}
}

void AAetheriaBiomeBuilder::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UpdateAstrolabeRings(DeltaTime);
	if (UWorld* World = GetWorld())
	{
		UpdateBrazierFlicker(World->GetTimeSeconds());

		if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
		{
			if (GS->CurrentLocationIndex != CurrentLocationIndex)
			{
				BuildLocation(GS->CurrentLocationIndex);
			}
			SyncObjectives(GS->Objectives, DeltaTime);
		}
	}
}

void AAetheriaBiomeBuilder::EnsureEnvironmentActorsExist()
{
	UWorld* World = GetWorld();
	if (!World) return;

	if (!IsValid(PostProcessVol))
	{
		for (TActorIterator<APostProcessVolume> It(World); It; ++It)
		{
			PostProcessVol = *It;
			break;
		}
		if (!IsValid(PostProcessVol))
		{
			PostProcessVol = World->SpawnActor<APostProcessVolume>(FVector::ZeroVector, FRotator::ZeroRotator);
		}
		if (IsValid(PostProcessVol))
		{
			PostProcessVol->bUnbound = true;
			FPostProcessSettings& PP = PostProcessVol->Settings;
			PP.bOverride_BloomIntensity = true;
			PP.BloomIntensity = 0.36f;
			PP.bOverride_BloomThreshold = true;
			PP.BloomThreshold = 1.45f;
			PP.bOverride_FilmSlope = true;
			PP.FilmSlope = 0.90f;
			PP.bOverride_FilmToe = true;
			PP.FilmToe = 0.38f;
			PP.bOverride_VignetteIntensity = true;
			PP.VignetteIntensity = 0.42f;
			PP.bOverride_AutoExposureMinBrightness = true;
			// VisionQA: lift the shadow floor — crushed blacks made 40-80% of frames unreadable.
			PP.AutoExposureMinBrightness = 1.6f;
			PP.bOverride_AutoExposureMaxBrightness = true;
			PP.AutoExposureMaxBrightness = 1.8f;
		}
	}

	if (!IsValid(KeyLight))
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It)
		{
			KeyLight = *It;
			break;
		}
		if (!IsValid(KeyLight))
		{
			KeyLight = World->SpawnActor<ADirectionalLight>(FVector(0.0f, 0.0f, 2400.0f), FRotator(-46.0f, -35.0f, 0.0f));
		}
		if (IsValid(KeyLight))
		{
			if (UDirectionalLightComponent* DirComp = Cast<UDirectionalLightComponent>(KeyLight->GetLightComponent()))
			{
				DirComp->SetMobility(EComponentMobility::Movable);
				DirComp->SetIntensity(3.15f);
				DirComp->ContactShadowLength = 0.08f;
			}
			else if (KeyLight->GetLightComponent())
			{
				KeyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
				KeyLight->GetLightComponent()->SetIntensity(3.15f);
			}
		}
	}

	if (!IsValid(SkyLight))
	{
		for (TActorIterator<ASkyLight> It(World); It; ++It)
		{
			SkyLight = *It;
			break;
		}
		if (!IsValid(SkyLight))
		{
			SkyLight = World->SpawnActor<ASkyLight>(FVector(0.0f, 0.0f, 1200.0f), FRotator::ZeroRotator);
		}
		if (IsValid(SkyLight) && SkyLight->GetLightComponent())
		{
			SkyLight->GetLightComponent()->SetMobility(EComponentMobility::Movable);
			// VisionQA: 1.05 gave zero readable fill; RecaptureSky() (in BuildLocation) feeds captured scene.
			SkyLight->GetLightComponent()->SetIntensity(3.0f);
		}
	}

	if (!IsValid(HeightFog))
	{
		for (TActorIterator<AExponentialHeightFog> It(World); It; ++It)
		{
			HeightFog = *It;
			break;
		}
		if (!IsValid(HeightFog))
		{
			HeightFog = World->SpawnActor<AExponentialHeightFog>(FVector(0.0f, 0.0f, 180.0f), FRotator::ZeroRotator);
		}
		if (IsValid(HeightFog) && HeightFog->GetComponent())
		{
			HeightFog->GetComponent()->SetVolumetricFog(true);
			HeightFog->GetComponent()->SetVolumetricFogScatteringDistribution(0.45f);
		}
	}
}

void AAetheriaBiomeBuilder::BuildLocation(int32 LocationIndex)
{
	CurrentLocationIndex = FMath::Clamp(LocationIndex, 0, 9);
	EnsureEnvironmentActorsExist();
	RefreshBiomeMeshes();

	PillarsISM->ClearInstances();
	CrystalsISM->ClearInstances();
	RingsISM->ClearInstances();
	HazardSpireISM->ClearInstances();
	BouldersISM->ClearInstances();
	BraziersISM->ClearInstances();
	StatuesISM->ClearInstances();
	GatesISM->ClearInstances();
	ChestsISM->ClearInstances();
	LanternsISM->ClearInstances();
	CitadelVistaISM->ClearInstances();
	TerrainPlatformsISM->ClearInstances();
	CoverParapetsISM->ClearInstances();
	TrenchFloorISM->ClearInstances();

	const FAetheriaLocationDef Loc = UAetheriaBiomeDatabase::GetLocation(CurrentLocationIndex);

	UMaterialInterface* FloorBase = UAetheriaMaterialLibrary::GetBiomeFloorMaterial(CurrentLocationIndex, this);
	UMaterialInterface* EmissiveBase = UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);

	FloorDynamicMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this,
		FloorBase,
		FLinearColor(0.88f, 0.90f, 0.96f, 1.0f),
		Loc.RimColor * 0.12f,
		1.4f,
		1.0f);
	if (FloorDynamicMat)
	{
		ArenaFloorMesh->SetMaterial(0, FloorDynamicMat);
	}

	// Dynamic PBR Materials with dark-iron / photogrammetry texture mapping
	auto ApplyMatAllSlots = [](UInstancedStaticMeshComponent* ISM, UMaterialInterface* Mat)
	{
		if (!ISM || !Mat) return;
		const int32 NumMats = FMath::Max(1, ISM->GetNumMaterials());
		for (int32 SlotIdx = 0; SlotIdx < NumMats; ++SlotIdx)
		{
			ISM->SetMaterial(SlotIdx, Mat);
		}
	};

	UMaterialInstanceDynamic* StatueMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TEXT("GothicStatue"), FLinearColor(0.85f, 0.85f, 0.90f, 1.0f), Loc.RimColor * 0.1f, 0.3f);
	ApplyMatAllSlots(StatuesISM, StatueMat);

	const FString GateTexStem = (CurrentLocationIndex % 2 == 0) ? TEXT("LargeIronGate") : TEXT("CastleDoor");
	UMaterialInstanceDynamic* GateMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, GateTexStem, FLinearColor(0.78f, 0.75f, 0.72f, 1.0f), FLinearColor::Black, 0.0f);
	ApplyMatAllSlots(GatesISM, GateMat);

	UMaterialInstanceDynamic* ChestMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TEXT("TreasureChest"), FLinearColor(0.92f, 0.84f, 0.68f, 1.0f), Loc.AccentColor * 0.2f, 0.5f);
	ApplyMatAllSlots(ChestsISM, ChestMat);

	FString LanternTexStem = TEXT("GothicLantern");
	if (CurrentLocationIndex == 1 || CurrentLocationIndex == 7)
	{
		LanternTexStem = TEXT("StoneFirePit");
	}
	else if (CurrentLocationIndex == 0 || CurrentLocationIndex == 5)
	{
		LanternTexStem = TEXT("LanternChandelier");
	}
	else if (CurrentLocationIndex == 3 || CurrentLocationIndex == 4)
	{
		LanternTexStem = TEXT("LionRelief");
	}
	UMaterialInstanceDynamic* LanternMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, LanternTexStem, FLinearColor(0.80f, 0.75f, 0.70f, 1.0f), Loc.AccentColor, 3.5f);
	ApplyMatAllSlots(LanternsISM, LanternMat);

	UMaterialInstanceDynamic* CitadelMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TEXT("CitadelFortress"), FLinearColor(0.70f, 0.72f, 0.80f, 1.0f), Loc.RimColor * 0.15f, 0.2f);
	ApplyMatAllSlots(CitadelVistaISM, CitadelMat);

	UMaterialInstanceDynamic* PillarMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TEXT("GothicPillar"), FLinearColor(0.82f, 0.84f, 0.90f, 1.0f), Loc.RimColor * 0.08f, 0.3f);
	ApplyMatAllSlots(PillarsISM, PillarMat);

	UMaterialInstanceDynamic* ArchMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TEXT("RuinedArch"), FLinearColor(0.82f, 0.84f, 0.90f, 1.0f), Loc.RimColor * 0.08f, 0.3f);
	ApplyMatAllSlots(RingsISM, ArchMat);

	UMaterialInstanceDynamic* BoulderMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TEXT("CliffBoulder"), FLinearColor(0.82f, 0.84f, 0.90f, 1.0f), Loc.RimColor * 0.08f, 0.3f);
	ApplyMatAllSlots(BouldersISM, BoulderMat);

	const FString FoliageStem = (CurrentLocationIndex == 0 || CurrentLocationIndex == 3 || CurrentLocationIndex == 4) ? TEXT("DeadTree") : TEXT("RunicAltar");
	UMaterialInstanceDynamic* FoliageMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, FoliageStem, FLinearColor(0.82f, 0.84f, 0.90f, 1.0f), Loc.RimColor * 0.08f, 0.3f);
	ApplyMatAllSlots(CrystalsISM, FoliageMat);

	UMaterialInstanceDynamic* BrazierMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TEXT("IronBrazier"), FLinearColor(0.75f, 0.72f, 0.70f, 1.0f), FLinearColor::Black, 0.0f);
	ApplyMatAllSlots(BraziersISM, BrazierMat);

	const FLinearColor BrazierFireColor = FLinearColor::LerpUsingHSV(FLinearColor(1.0f, 0.34f, 0.05f, 1.0f), Loc.AccentColor, 0.28f);
	if (UMaterialInstanceDynamic* FlameMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, EmissiveBase, BrazierFireColor, BrazierFireColor, 18.0f, 1.0f))
	{
		HazardSpireISM->SetMaterial(0, FlameMat);
	}

	UMaterialInterface* StoneBase = UAetheriaMaterialLibrary::GetGothicStoneMaterial(this);
	UMaterialInstanceDynamic* PlatformMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, FloorBase, FLinearColor(0.86f, 0.88f, 0.94f, 1.0f), Loc.RimColor * 0.15f, 1.2f, 1.0f);
	ApplyMatAllSlots(TerrainPlatformsISM, PlatformMat);

	UMaterialInstanceDynamic* ParapetMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, StoneBase, FLinearColor(0.78f, 0.80f, 0.86f, 1.0f), Loc.RimColor * 0.08f, 0.4f, 1.0f);
	ApplyMatAllSlots(CoverParapetsISM, ParapetMat);

	UMaterialInstanceDynamic* TrenchMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, FloorBase, FLinearColor(0.28f, 0.34f, 0.44f, 1.0f), Loc.AccentColor * 0.42f, 1.2f, 1.0f);
	ApplyMatAllSlots(TrenchFloorISM, TrenchMat);

	const UStaticMesh* ParapetSM = CoverParapetsISM ? CoverParapetsISM->GetStaticMesh() : nullptr;

	for (const FAetheriaTerrainZone& Zone : Loc.TerrainZones)
	{
		switch (Zone.Kind)
		{
		case EAetheriaTerrainZoneKind::ElevatedPlateau:
		{
			// 3 gentle concentric steps (max ~15 UU rise per tier) so players/bots walk smoothly up & down
			const float HeightUU = FMath::Clamp(Zone.ElevationUU, 24.0f, 54.0f);
			const float OuterR = Zone.RadiusUU + FMath::Max(0.0f, Zone.RampWidthUU * 0.55f);
			for (int32 Tier = 0; Tier < 3; ++Tier)
			{
				const float Alpha = (Tier + 1) / 3.0f;
				const float TierR = FMath::Lerp(OuterR, Zone.RadiusUU, static_cast<float>(Tier) / 2.0f);
				const float TierH = HeightUU * Alpha;
				const float ScaleXY = (TierR * 2.0f) / 100.0f;
				const float ScaleZ = TierH / 100.0f;
				const FVector TierLoc(Zone.Center.X, Zone.Center.Y, TierH * 0.5f);
				TerrainPlatformsISM->AddInstance(FTransform(FRotator(0.0f, Zone.YawDeg, 0.0f), TierLoc, FVector(ScaleXY, ScaleXY, ScaleZ)));
			}
			break;
		}

		case EAetheriaTerrainZoneKind::ChokepointBridge:
		{
			const float BridgeLength = Zone.LengthUU;
			const float BridgeWidth = Zone.WidthUU;
			const float BridgeHeight = 14.0f;
			const FVector BridgeLoc(Zone.Center.X, Zone.Center.Y, BridgeHeight * 0.5f);
			const FRotator BridgeRot(0.0f, Zone.YawDeg, 0.0f);
			const FVector BridgeScale(BridgeLength / 100.0f, BridgeWidth / 100.0f, BridgeHeight / 100.0f);
			TerrainPlatformsISM->AddInstance(FTransform(BridgeRot, BridgeLoc, BridgeScale));
			break;
		}

		case EAetheriaTerrainZoneKind::CoverParapet:
		{
			if (Zone.Center.Size() < 1950.0f) break;
			const FVector TargetSize(Zone.LengthUU, Zone.WidthUU, Zone.ElevationUU);
			const FVector ParapetScale = ScaleMeshToTargetSizeUU(ParapetSM, TargetSize);
			const float GroundZ = ComputeGroundedOffsetZ(ParapetSM, ParapetScale.Z);
			const FVector ParapetLoc(Zone.Center.X, Zone.Center.Y, GroundZ);
			const FRotator ParapetRot(0.0f, Zone.YawDeg, 0.0f);
			CoverParapetsISM->AddInstance(FTransform(ParapetRot, ParapetLoc, ParapetScale));
			break;
		}

		case EAetheriaTerrainZoneKind::SunkenTrench:
		{
			// Sit flush just above ArenaFloorMesh (Z=0 top) so frost/water channels are clearly visible
			const float TrenchLength = Zone.LengthUU;
			const float TrenchWidth = Zone.WidthUU;
			const FVector TrenchLoc(Zone.Center.X, Zone.Center.Y, 1.5f);
			const FRotator TrenchRot(0.0f, Zone.YawDeg, 0.0f);
			const FVector TrenchScale(TrenchLength / 100.0f, TrenchWidth / 100.0f, 0.025f);
			TrenchFloorISM->AddInstance(FTransform(TrenchRot, TrenchLoc, TrenchScale));
			break;
		}

		case EAetheriaTerrainZoneKind::SanctuaryRing:
		default:
			break;
		}
	}

	// Chiaroscuro Diablo key/fill separation
	if (IsValid(KeyLight) && KeyLight->GetLightComponent())
	{
		const FLinearColor WarmKey = FMath::Lerp(FLinearColor(1.0f, 0.94f, 0.86f, 1.0f), Loc.AccentColor, 0.20f);
		KeyLight->GetLightComponent()->SetLightColor(WarmKey);
		KeyLight->GetLightComponent()->SetIntensity(3.15f);
	}
	if (IsValid(SkyLight) && SkyLight->GetLightComponent())
	{
		const FLinearColor CoolFill = FMath::Lerp(FLinearColor(0.68f, 0.75f, 0.88f, 1.0f), Loc.RimColor, 0.30f);
		SkyLight->GetLightComponent()->SetLightColor(CoolFill);
		SkyLight->GetLightComponent()->SetIntensity(3.0f);
		SkyLight->GetLightComponent()->RecaptureSky();
	}
	if (IsValid(HeightFog) && HeightFog->GetComponent())
	{
		HeightFog->GetComponent()->SetFogInscatteringColor(Loc.FogColor);
		// VisionQA: 0.015 was invisible; raise for aerial-perspective depth layering.
		HeightFog->GetComponent()->SetFogDensity(0.032f);
	}

	const float R = Loc.ArenaRadiusUU;
	SpawnSanctuaryBraziers(R, BrazierFireColor);

	switch (Loc.Theme)
	{
	case EAetheriaBiomeTheme::Bastion: BuildBastionArchitecture(R); break;
	case EAetheriaBiomeTheme::Caldera: BuildCalderaArchitecture(R); break;
	case EAetheriaBiomeTheme::Glacier: BuildGlacierArchitecture(R); break;
	case EAetheriaBiomeTheme::GravityGardens: BuildGravityGardensArchitecture(R); break;
	case EAetheriaBiomeTheme::StormNecropolis: BuildStormNecropolisArchitecture(R); break;
	case EAetheriaBiomeTheme::MirrorLabyrinth: BuildMirrorLabyrinthArchitecture(R); break;
	case EAetheriaBiomeTheme::SunkenAstrolabe: BuildSunkenAstrolabeArchitecture(R); break;
	case EAetheriaBiomeTheme::TitanForge: BuildTitanForgeArchitecture(R); break;
	case EAetheriaBiomeTheme::TimeTemple: BuildTimeTempleArchitecture(R); break;
	case EAetheriaBiomeTheme::ChronoSealHeart: BuildChronoSealArchitecture(R); break;
	default: BuildBastionArchitecture(R); break;
	}
}

void AAetheriaBiomeBuilder::SpawnSanctuaryBraziers(float ArenaRadius, FLinearColor FireColor)
{
	BrazierFlameBaseTransforms.Empty();

	while (BrazierLights.Num() < 12)
	{
		const int32 Idx = BrazierLights.Num();
		UPointLightComponent* PL = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("BrazierLight_%d"), Idx));
		PL->SetupAttachment(GetRootComponent());
		PL->SetAttenuationRadius(1500.0f);
		PL->RegisterComponent();
		BrazierLights.Add(PL);
	}

	for (int32 i = 0; i < BrazierLights.Num(); ++i)
	{
		if (BrazierLights[i])
		{
			BrazierLights[i]->SetCastShadows(i % 2 == 0);
			BrazierLights[i]->ContactShadowLength = 0.06f;
		}
	}

	const UStaticMesh* BrazierSM = BraziersISM ? BraziersISM->GetStaticMesh() : nullptr;
	const FVector BrazierScale = ScaleMeshUniformHeightUU(BrazierSM, 155.0f, 88.0f);
	const float BrazierOffsetZ = ComputeGroundedOffsetZ(BrazierSM, BrazierScale.Z);
	const float FlameTipZ = BrazierSM ? (BrazierOffsetZ + BrazierSM->GetBoundingBox().Max.Z * BrazierScale.Z + 12.0f) : 148.0f;

	for (int32 i = 0; i < 12; ++i)
	{
		const float Angle = (2.0f * PI * i) / 12.0f;
		const float Dist = (i % 2 == 0) ? 1750.0f : 3150.0f;
		const FVector Pos(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, BrazierOffsetZ);

		BraziersISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f), Pos, BrazierScale));

		const FVector BaseFlamePos(Pos.X, Pos.Y, FlameTipZ);
		const float BaseYawDeg = FMath::RadiansToDegrees(Angle);

		const FTransform FlameCore(FRotator(0.0f, BaseYawDeg, 0.0f), BaseFlamePos, FVector(0.16f, 0.16f, 0.58f));
		const FTransform FlameTongueA(FRotator(14.0f, BaseYawDeg + 45.0f, 0.0f), BaseFlamePos + FVector(6.0f, 4.0f, -4.0f), FVector(0.12f, 0.12f, 0.44f));
		const FTransform FlameTongueB(FRotator(-14.0f, BaseYawDeg - 45.0f, 0.0f), BaseFlamePos + FVector(-6.0f, -4.0f, -4.0f), FVector(0.12f, 0.12f, 0.44f));

		HazardSpireISM->AddInstance(FlameCore);
		BrazierFlameBaseTransforms.Add(FlameCore);

		HazardSpireISM->AddInstance(FlameTongueA);
		BrazierFlameBaseTransforms.Add(FlameTongueA);

		HazardSpireISM->AddInstance(FlameTongueB);
		BrazierFlameBaseTransforms.Add(FlameTongueB);

		if (BrazierLights.IsValidIndex(i) && BrazierLights[i])
		{
			BrazierLights[i]->SetWorldLocation(FVector(Pos.X, Pos.Y, FlameTipZ + 55.0f));
			BrazierLights[i]->SetLightColor(FireColor);
			BrazierLights[i]->SetIntensity(9200.0f);
		}
	}
}

void AAetheriaBiomeBuilder::UpdateBrazierFlicker(float TimeSeconds)
{
	for (int32 i = 0; i < BrazierLights.Num(); ++i)
	{
		if (BrazierLights[i])
		{
			const float Flicker = 1.0f + 0.16f * FMath::Sin(TimeSeconds * 7.5f + i * 2.1f) + 0.08f * FMath::Cos(TimeSeconds * 13.0f + i);
			BrazierLights[i]->SetIntensity(9200.0f * Flicker);
		}
	}

	if (HazardSpireISM && BrazierFlameBaseTransforms.Num() > 0 && HazardSpireISM->GetInstanceCount() >= BrazierFlameBaseTransforms.Num())
	{
		for (int32 Idx = 0; Idx < BrazierFlameBaseTransforms.Num(); ++Idx)
		{
			const FTransform& BaseXform = BrazierFlameBaseTransforms[Idx];
			const int32 TongueType = Idx % 3;
			const int32 BrazierIdx = Idx / 3;

			const float Phase = TimeSeconds * 6.5f + BrazierIdx * 1.7f + TongueType * 2.4f;
			const float HeightFlicker = 1.0f + 0.22f * FMath::Sin(Phase) + 0.12f * FMath::Cos(Phase * 1.7f);
			const float WindLeanPitch = FMath::Sin(TimeSeconds * 3.8f + BrazierIdx * 0.9f) * 6.5f;
			const float WindLeanRoll = FMath::Cos(TimeSeconds * 3.2f + BrazierIdx * 1.3f) * 5.0f;
			const float TwistYaw = FMath::Sin(Phase * 0.8f) * 12.0f;

			FTransform AnimXform = BaseXform;
			FVector Scale = BaseXform.GetScale3D();
			Scale.Z *= HeightFlicker;
			Scale.X *= (2.0f - HeightFlicker * 0.5f);
			Scale.Y *= (2.0f - HeightFlicker * 0.5f);
			AnimXform.SetScale3D(Scale);

			FRotator Rot = BaseXform.GetRotation().Rotator();
			Rot.Pitch += WindLeanPitch;
			Rot.Roll += WindLeanRoll;
			Rot.Yaw += TwistYaw;
			AnimXform.SetRotation(Rot.Quaternion());

			HazardSpireISM->UpdateInstanceTransform(Idx, AnimXform, false, false, false);
		}
		HazardSpireISM->MarkRenderStateDirty();
	}
}

void AAetheriaBiomeBuilder::PopulatePerimeterVistasAndGates(float ArenaRadius)
{
	const UStaticMesh* GateSM = GatesISM ? GatesISM->GetStaticMesh() : nullptr;
	const UStaticMesh* LanternSM = LanternsISM ? LanternsISM->GetStaticMesh() : nullptr;
	const UStaticMesh* StatueSM = StatuesISM ? StatuesISM->GetStaticMesh() : nullptr;
	const UStaticMesh* ChestSM = ChestsISM ? ChestsISM->GetStaticMesh() : nullptr;
	const UStaticMesh* CitadelSM = CitadelVistaISM ? CitadelVistaISM->GetStaticMesh() : nullptr;

	const FVector GateScale = ScaleMeshUniformHeightUU(GateSM, 520.0f, 300.0f);
	const FVector LanternScale = ScaleMeshUniformHeightUU(LanternSM, 145.0f, 80.0f);
	const FVector StatueScale = ScaleMeshUniformHeightUU(StatueSM, 480.0f, 175.0f);
	const FVector ChestScale = ScaleMeshUniformHeightUU(ChestSM, 115.0f, 95.0f);
	const FVector CitadelScale = ScaleMeshUniformHeightUU(CitadelSM, 2600.0f, 1800.0f);

	// 1. Wrought-Iron Gates at 4 Cardinal Directions (~5200 UU)
	static const float CardinalAngles[4] = { 0.0f, PI * 0.5f, PI, PI * 1.5f };
	for (int32 i = 0; i < 4; ++i)
	{
		const float Angle = CardinalAngles[i];
		const FVector GatePos(FMath::Cos(Angle) * 5200.0f, FMath::Sin(Angle) * 5200.0f, ComputeGroundedOffsetZ(GateSM, GateScale.Z));
		const FRotator GateRot(0.0f, FMath::RadiansToDegrees(Angle) + 90.0f, 0.0f);
		GatesISM->AddInstance(FTransform(GateRot, GatePos, GateScale));

		const FVector RightDir(-FMath::Sin(Angle), FMath::Cos(Angle), 0.0f);
		const float LZ = ComputeGroundedOffsetZ(LanternSM, LanternScale.Z);
		const FVector LanternPosA = FVector(GatePos.X, GatePos.Y, LZ) + RightDir * 290.0f;
		const FVector LanternPosB = FVector(GatePos.X, GatePos.Y, LZ) - RightDir * 290.0f;
		LanternsISM->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f), LanternPosA, LanternScale));
		LanternsISM->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f), LanternPosB, LanternScale));
	}

	// 2. Gothic Statues on Intermediate Diagonals (~4700 UU)
	static const float DiagAngles[4] = { PI * 0.25f, PI * 0.75f, PI * 1.25f, PI * 1.75f };
	for (int32 i = 0; i < 4; ++i)
	{
		const float Angle = DiagAngles[i];
		const FVector StatuePos(FMath::Cos(Angle) * 4700.0f, FMath::Sin(Angle) * 4700.0f, ComputeGroundedOffsetZ(StatueSM, StatueScale.Z));
		const FRotator StatueRot(0.0f, FMath::RadiansToDegrees(Angle) + 180.0f, 0.0f);
		StatuesISM->AddInstance(FTransform(StatueRot, StatuePos, StatueScale));

		LanternsISM->AddInstance(FTransform(FRotator(0.0f, FMath::RadiansToDegrees(Angle), 0.0f), FVector(StatuePos.X, StatuePos.Y, ComputeGroundedOffsetZ(LanternSM, LanternScale.Z)) - FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f) * 210.0f, LanternScale * 0.85f));
	}

	// 3. Relic Treasure Chests near Shrines/Arches (~4500 UU)
	static const float ChestAngles[4] = { PI * 0.125f, PI * 0.625f, PI * 1.125f, PI * 1.625f };
	for (int32 i = 0; i < 4; ++i)
	{
		const float Angle = ChestAngles[i];
		const FVector ChestPos(FMath::Cos(Angle) * 4500.0f, FMath::Sin(Angle) * 4500.0f, ComputeGroundedOffsetZ(ChestSM, ChestScale.Z));
		const FRotator ChestRot(0.0f, FMath::RadiansToDegrees(Angle) + 180.0f, 0.0f);
		ChestsISM->AddInstance(FTransform(ChestRot, ChestPos, ChestScale));
	}

	// 4. Distant Gothic Citadel Fortress Vistas on Far Perimeter (13500..15500 UU)
	for (int32 i = 0; i < 8; ++i)
	{
		const float Angle = (2.0f * PI * i) / 8.0f + 0.15f;
		const float VistaDist = 13800.0f + (i % 3) * 750.0f;
		const float VistaZ = ComputeGroundedOffsetZ(CitadelSM, CitadelScale.Z) - 180.0f;
		const FVector VistaPos(FMath::Cos(Angle) * VistaDist, FMath::Sin(Angle) * VistaDist, VistaZ);
		const FRotator VistaRot(0.0f, FMath::RadiansToDegrees(Angle) + 180.0f, 0.0f);
		CitadelVistaISM->AddInstance(FTransform(VistaRot, VistaPos, CitadelScale));
	}
}

void AAetheriaBiomeBuilder::BuildBastionArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float OuterRingR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector PillarS = ScaleMeshUniformHeightUU(PillarsISM->GetStaticMesh(), 580.0f, 120.0f);
	const FVector ArchS   = ScaleMeshUniformHeightUU(RingsISM->GetStaticMesh(), 520.0f, 360.0f);
	const FVector BouldS  = ScaleMeshUniformHeightUU(BouldersISM->GetStaticMesh(), 165.0f, 130.0f);
	const FVector CrystS  = ScaleMeshUniformHeightUU(CrystalsISM->GetStaticMesh(), 460.0f, 200.0f);

	for (int32 i = 0; i < 16; ++i)
	{
		const float Angle = (2.0f * PI * i) / 16.0f;
		const FVector Pos(FMath::Cos(Angle) * OuterRingR, FMath::Sin(Angle) * OuterRingR, ComputeGroundedOffsetZ(PillarsISM->GetStaticMesh(), PillarS.Z));
		PillarsISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f), Pos, PillarS));
		if (i % 4 == 0)
		{
			const float ArchR = OuterRingR * 1.16f;
			const FVector APos(FMath::Cos(Angle) * ArchR, FMath::Sin(Angle) * ArchR, ComputeGroundedOffsetZ(RingsISM->GetStaticMesh(), ArchS.Z));
			RingsISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f), APos, ArchS));
		}
	}
	for (int32 b = 0; b < 18; ++b)
	{
		const float Angle = (2.0f * PI * b) / 18.0f + 0.14f;
		const float BR = OuterRingR * 1.10f;
		const FVector BPos(FMath::Cos(Angle) * BR, FMath::Sin(Angle) * BR, ComputeGroundedOffsetZ(BouldersISM->GetStaticMesh(), BouldS.Z));
		BouldersISM->AddInstance(FTransform(FRotator(0.f, b * 42.f, 0.f), BPos, BouldS));
		if (b % 3 == 0)
		{
			const FVector CPos(BPos.X * 1.04f, BPos.Y * 1.04f, ComputeGroundedOffsetZ(CrystalsISM->GetStaticMesh(), CrystS.Z));
			CrystalsISM->AddInstance(FTransform(FRotator(0.f, b * 27.f, 0.f), CPos, CrystS));
		}
	}
}

void AAetheriaBiomeBuilder::BuildCalderaArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector BouldS = ScaleMeshUniformHeightUU(BouldersISM->GetStaticMesh(), 185.0f, 145.0f);
	const FVector ArchS  = ScaleMeshUniformHeightUU(RingsISM->GetStaticMesh(), 520.0f, 360.0f);
	for (int32 i = 0; i < 24; ++i)
	{
		const float Angle = (2.0f * PI * i) / 24.0f;
		const float Dist = BaseR + (i % 3) * 320.0f;
		BouldersISM->AddInstance(FTransform(FRotator(8.f, i * 33.f, 0.f), FVector(FMath::Cos(Angle) * Dist, FMath::Sin(Angle) * Dist, ComputeGroundedOffsetZ(BouldersISM->GetStaticMesh(), BouldS.Z)), BouldS * 1.15f));
	}
	for (int32 i = 0; i < 8; ++i)
	{
		const float Angle = (2.0f * PI * i) / 8.0f;
		const float AD = BaseR + 420.0f;
		RingsISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f), FVector(FMath::Cos(Angle) * AD, FMath::Sin(Angle) * AD, ComputeGroundedOffsetZ(RingsISM->GetStaticMesh(), ArchS.Z)), ArchS));
	}
}

void AAetheriaBiomeBuilder::BuildGlacierArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector BouldS  = ScaleMeshUniformHeightUU(BouldersISM->GetStaticMesh(), 195.0f, 135.0f);
	const FVector PillarS = ScaleMeshUniformHeightUU(PillarsISM->GetStaticMesh(), 620.0f, 125.0f);
	for (int32 i = 0; i < 22; ++i)
	{
		const float Angle = (2.0f * PI * i) / 22.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, ComputeGroundedOffsetZ(BouldersISM->GetStaticMesh(), BouldS.Z));
		BouldersISM->AddInstance(FTransform(FRotator(0.f, i * 28.f, 0.f), Pos, FVector(BouldS.X, BouldS.Y, BouldS.Z * 1.35f)));
		if (i % 2 == 0)
		{
			PillarsISM->AddInstance(FTransform(FRotator(4.f, i * 19.f, 0.f), FVector(Pos.X * 1.06f, Pos.Y * 1.06f, ComputeGroundedOffsetZ(PillarsISM->GetStaticMesh(), PillarS.Z)), PillarS));
		}
	}
}

void AAetheriaBiomeBuilder::BuildGravityGardensArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector PillarS = ScaleMeshUniformHeightUU(PillarsISM->GetStaticMesh(), 540.0f, 120.0f);
	const FVector BouldS  = ScaleMeshUniformHeightUU(BouldersISM->GetStaticMesh(), 150.0f, 120.0f);
	for (int32 i = 0; i < 18; ++i)
	{
		const float Angle = (2.0f * PI * i) / 18.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, 420.0f + (i % 4) * 140.0f);
		PillarsISM->AddInstance(FTransform(FRotator(180.f, i * 18.f, 0.f), Pos, PillarS));
		BouldersISM->AddInstance(FTransform(FRotator(0.f, i * 45.f, 0.f), FVector(Pos.X, Pos.Y, ComputeGroundedOffsetZ(BouldersISM->GetStaticMesh(), BouldS.Z)), BouldS));
	}
}

void AAetheriaBiomeBuilder::BuildStormNecropolisArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector PillarS = ScaleMeshUniformHeightUU(PillarsISM->GetStaticMesh(), 640.0f, 125.0f);
	const FVector CrystS  = ScaleMeshUniformHeightUU(CrystalsISM->GetStaticMesh(), 460.0f, 195.0f);
	for (int32 i = 0; i < 16; ++i)
	{
		const float Angle = (2.0f * PI * i) / 16.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, ComputeGroundedOffsetZ(PillarsISM->GetStaticMesh(), PillarS.Z));
		PillarsISM->AddInstance(FTransform(FRotator::ZeroRotator, Pos, PillarS));
		CrystalsISM->AddInstance(FTransform(FRotator(0.f, i * 40.f, 0.f), FVector(Pos.X * 1.12f, Pos.Y * 1.12f, ComputeGroundedOffsetZ(CrystalsISM->GetStaticMesh(), CrystS.Z)), CrystS));
	}
}

void AAetheriaBiomeBuilder::BuildMirrorLabyrinthArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector ArchS = ScaleMeshUniformHeightUU(RingsISM->GetStaticMesh(), 520.0f, 360.0f);
	for (int32 i = 0; i < 16; ++i)
	{
		const float Angle = (2.0f * PI * i) / 16.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, ComputeGroundedOffsetZ(RingsISM->GetStaticMesh(), ArchS.Z));
		RingsISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle) + 90.f, 0.f), Pos, ArchS));
	}
}

void AAetheriaBiomeBuilder::BuildSunkenAstrolabeArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.58f);
	const FVector ArchS  = ScaleMeshUniformHeightUU(RingsISM->GetStaticMesh(), 540.0f, 360.0f);
	const FVector BouldS = ScaleMeshUniformHeightUU(BouldersISM->GetStaticMesh(), 160.0f, 125.0f);
	for (int32 i = 0; i < 20; ++i)
	{
		const float Angle = (2.0f * PI * i) / 20.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, ComputeGroundedOffsetZ(RingsISM->GetStaticMesh(), ArchS.Z));
		RingsISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f), Pos, ArchS));
		BouldersISM->AddInstance(FTransform(FRotator(0.f, i * 29.f, 0.f), FVector(Pos.X * 1.08f, Pos.Y * 1.08f, ComputeGroundedOffsetZ(BouldersISM->GetStaticMesh(), BouldS.Z)), BouldS));
	}
}

void AAetheriaBiomeBuilder::BuildTitanForgeArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector PillarS = ScaleMeshUniformHeightUU(PillarsISM->GetStaticMesh(), 560.0f, 130.0f);
	const FVector BouldS  = ScaleMeshUniformHeightUU(BouldersISM->GetStaticMesh(), 175.0f, 140.0f);
	for (int32 i = 0; i < 14; ++i)
	{
		const float Angle = (2.0f * PI * i) / 14.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, ComputeGroundedOffsetZ(PillarsISM->GetStaticMesh(), PillarS.Z));
		PillarsISM->AddInstance(FTransform(FRotator::ZeroRotator, Pos, PillarS));
		BouldersISM->AddInstance(FTransform(FRotator(0.f, i * 50.f, 0.f), FVector(Pos.X * 1.10f, Pos.Y * 1.10f, ComputeGroundedOffsetZ(BouldersISM->GetStaticMesh(), BouldS.Z)), BouldS));
	}
}

void AAetheriaBiomeBuilder::BuildTimeTempleArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.56f);
	const FVector PillarS = ScaleMeshUniformHeightUU(PillarsISM->GetStaticMesh(), 600.0f, 125.0f);
	const FVector ArchS   = ScaleMeshUniformHeightUU(RingsISM->GetStaticMesh(), 520.0f, 360.0f);
	for (int32 i = 0; i < 18; ++i)
	{
		const float Angle = (2.0f * PI * i) / 18.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, ComputeGroundedOffsetZ(PillarsISM->GetStaticMesh(), PillarS.Z));
		PillarsISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 8.f), Pos, PillarS));
		if (i % 2 == 0)
		{
			RingsISM->AddInstance(FTransform(FRotator(0.f, i * 20.f, 0.f), FVector(Pos.X * 1.12f, Pos.Y * 1.12f, ComputeGroundedOffsetZ(RingsISM->GetStaticMesh(), ArchS.Z)), ArchS));
		}
	}
}

void AAetheriaBiomeBuilder::BuildChronoSealArchitecture(float ArenaRadius)
{
	PopulatePerimeterVistasAndGates(ArenaRadius);
	const float BaseR = FMath::Max(3200.0f, ArenaRadius * 0.58f);
	const FVector ArchS   = ScaleMeshUniformHeightUU(RingsISM->GetStaticMesh(), 540.0f, 360.0f);
	const FVector PillarS = ScaleMeshUniformHeightUU(PillarsISM->GetStaticMesh(), 600.0f, 125.0f);
	for (int32 i = 0; i < 20; ++i)
	{
		const float Angle = (2.0f * PI * i) / 20.0f;
		const FVector Pos(FMath::Cos(Angle) * BaseR, FMath::Sin(Angle) * BaseR, ComputeGroundedOffsetZ(RingsISM->GetStaticMesh(), ArchS.Z));
		RingsISM->AddInstance(FTransform(FRotator(0.f, FMath::RadiansToDegrees(Angle), 0.f), Pos, ArchS));
		PillarsISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector(Pos.X * 1.12f, Pos.Y * 1.12f, ComputeGroundedOffsetZ(PillarsISM->GetStaticMesh(), PillarS.Z)), PillarS));
	}
}

void AAetheriaBiomeBuilder::UpdateAstrolabeRings(float DeltaTime)
{
	if (CurrentLocationIndex == 6 || CurrentLocationIndex == 8 || CurrentLocationIndex == 9)
	{
		AstrolabeRotationAngle += DeltaTime * 5.0f;
		RingsISM->SetRelativeRotation(FRotator(0.0f, AstrolabeRotationAngle, 0.0f));
	}
	else
	{
		RingsISM->SetRelativeRotation(FRotator::ZeroRotator);
	}
}

void AAetheriaBiomeBuilder::SyncObjectives(const TArray<FAetheriaObjectiveInfo>& Objectives, float DeltaTime)
{
	UStaticMesh* BeaconMesh  = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_RunicAltar"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* ObeliskMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_GothicStatue"), TEXT("/Game/Meshes/SM_RunicAltar.SM_RunicAltar"));
	if (!ObeliskMesh) ObeliskMesh = BeaconMesh;
	UStaticMesh* PylonMesh   = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_GothicPillar"), TEXT("/Game/Meshes/SM_RunicAltar.SM_RunicAltar"));
	if (!PylonMesh) PylonMesh = BeaconMesh;
	UStaticMesh* PortalMesh  = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_LargeIronGate"), TEXT("/Game/Meshes/SM_RuinedArch.SM_RuinedArch"));
	if (!PortalMesh) PortalMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_RuinedArch"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* ConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
	UMaterialInterface* StoneBase = UAetheriaMaterialLibrary::GetGothicStoneMaterial(this);
	UMaterialInterface* EmissiveBase = UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);
	UMaterialInterface* RingBase = UAetheriaMaterialLibrary::GetArcaneRingMaterial(this);

	while (ObjectiveSlots.Num() < Objectives.Num())
	{
		const int32 Idx = ObjectiveSlots.Num();
		FAetheriaObjectiveVisualSlot Slot;
		Slot.BaseMesh = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ObjBase_%d"), Idx));
		Slot.BaseMesh->SetupAttachment(GetRootComponent());
		Slot.BaseMesh->SetStaticMesh(BeaconMesh);
		Slot.BaseMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slot.BaseMesh->RegisterComponent();

		Slot.CoreMesh = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ObjCore_%d"), Idx));
		Slot.CoreMesh->SetupAttachment(Slot.BaseMesh);
		Slot.CoreMesh->SetStaticMesh(ConeMesh);
		Slot.CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slot.CoreMesh->RegisterComponent();

		Slot.RingMesh = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("ObjRing_%d"), Idx));
		Slot.RingMesh->SetupAttachment(Slot.BaseMesh);
		Slot.RingMesh->SetStaticMesh(PlaneMesh);
		Slot.RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Slot.RingMesh->SetCastShadow(false);
		Slot.RingMesh->RegisterComponent();

		Slot.ObjectiveLight = NewObject<UPointLightComponent>(this, *FString::Printf(TEXT("ObjLight_%d"), Idx));
		Slot.ObjectiveLight->SetupAttachment(Slot.BaseMesh);
		Slot.ObjectiveLight->SetCastShadows(false);
		Slot.ObjectiveLight->SetAttenuationRadius(1400.0f);
		Slot.ObjectiveLight->RegisterComponent();

		ObjectiveSlots.Add(Slot);
	}

	const float SpinDeg = GetWorld() ? GetWorld()->GetTimeSeconds() * 55.0f : 0.0f;
	bool bHideNearHeroForLobby = false;
	if (GetWorld())
	{
		for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
		{
			if (const AAetheriaWizardCharacter* W = *It)
			{
				if (W->IsLocallyControlled() && (W->IsInLobbyCameraMode() || W->IsInEditorCameraMode()))
				{
					bHideNearHeroForLobby = true;
					break;
				}
			}
		}
	}

	for (int32 i = 0; i < ObjectiveSlots.Num(); ++i)
	{
		FAetheriaObjectiveVisualSlot& Slot = ObjectiveSlots[i];
		if (i >= Objectives.Num() || !Objectives[i].bActive ||
			(bHideNearHeroForLobby && FVector2D(Objectives[i].Location.X, Objectives[i].Location.Y).Size() < 2400.0f))
		{
			Slot.BaseMesh->SetVisibility(false, true);
			Slot.ObjectiveLight->SetIntensity(0.0f);
			continue;
		}

		const FAetheriaObjectiveInfo& Info = Objectives[i];
		const float TerrainZ = UAetheriaBiomeDatabase::SampleTerrainHeightAt(CurrentLocationIndex, FVector2D(Info.Location.X, Info.Location.Y));
		Slot.BaseMesh->SetVisibility(true, true);
		Slot.BaseMesh->SetWorldLocation(FVector(Info.Location.X, Info.Location.Y, TerrainZ));

		UStaticMesh* ChosenMesh = BeaconMesh;
		float TargetObjH = 165.0f;
		if (Info.Kind == EAetheriaObjectiveKind::Obelisk)
		{
			ChosenMesh = ObeliskMesh;
			TargetObjH = 260.0f;
		}
		else if (Info.Kind == EAetheriaObjectiveKind::Pylon)
		{
			ChosenMesh = PylonMesh;
			TargetObjH = 290.0f;
		}
		else if (Info.Kind == EAetheriaObjectiveKind::Portal)
		{
			ChosenMesh = PortalMesh;
			TargetObjH = 420.0f;
		}
		const FVector BaseScale = ScaleMeshUniformHeightUU(ChosenMesh, TargetObjH, 165.0f);
		Slot.BaseMesh->SetStaticMesh(ChosenMesh);
		Slot.BaseMesh->SetWorldScale3D(BaseScale);
		Slot.BaseMesh->SetWorldLocation(FVector(Info.Location.X, Info.Location.Y, TerrainZ + ComputeGroundedOffsetZ(ChosenMesh, BaseScale.Z)));

		if (UMaterialInstanceDynamic* AltarMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(this, StoneBase, FLinearColor(0.8f, 0.8f, 0.85f), FLinearColor::Black, 0.0f, 1.0f))
		{
			Slot.BaseMesh->SetMaterial(0, AltarMat);
		}

		FLinearColor Color = AetheriaMindColors::MindCyan;
		if (Info.bCompleted) Color = FLinearColor(0.06f, 0.90f, 0.38f, 1.0f);
		else if (Info.Kind == EAetheriaObjectiveKind::Pylon) Color = AetheriaMindColors::MindViolet;

		Slot.CoreMesh->SetWorldLocation(FVector(Info.Location.X, Info.Location.Y, TerrainZ + 210.0f));
		Slot.CoreMesh->SetWorldScale3D(Info.Kind == EAetheriaObjectiveKind::Portal ? FVector(2.8f, 0.35f, 4.2f) : FVector(0.9f, 0.9f, 2.2f));
		Slot.CoreMesh->SetWorldRotation(FRotator(0.0f, SpinDeg, 0.0f));

		const float RingScale = FMath::Max(2.0f, Info.RadiusUU / 50.0f);
		Slot.RingMesh->SetWorldLocation(FVector(Info.Location.X, Info.Location.Y, TerrainZ + 4.0f));
		Slot.RingMesh->SetWorldRotation(FRotator(0.0f, -SpinDeg * 0.4f, 0.0f));
		Slot.RingMesh->SetWorldScale3D(FVector(RingScale, RingScale, 1.0f));

		if (UMaterialInstanceDynamic* Mat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(this, EmissiveBase, Color, Color, 4.5f, 0.85f))
		{
			Slot.CoreMesh->SetMaterial(0, Mat);
		}
		if (UMaterialInstanceDynamic* RingMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(this, RingBase ? RingBase : EmissiveBase, Color, Color, 4.2f, 0.88f))
		{
			Slot.RingMesh->SetMaterial(0, RingMat);
		}

		Slot.ObjectiveLight->SetWorldLocation(FVector(Info.Location.X, Info.Location.Y, TerrainZ + 240.0f));
		Slot.ObjectiveLight->SetLightColor(Color);
		Slot.ObjectiveLight->SetIntensity(Info.bCompleted ? 3000.0f : 6500.0f);
	}
}

void AAetheriaBiomeBuilder::TriggerBossSlamVisual(const FVector& Location, float RadiusUU, FLinearColor Color)
{
	if (UWorld* World = GetWorld())
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Location, RadiusUU, Color, 0.65f);
	}
}

void AAetheriaBiomeBuilder::TriggerAmbushTelegraphVisual(const FVector& Location, float RadiusUU, FLinearColor Color)
{
	if (UWorld* World = GetWorld())
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, Location, RadiusUU, Color, 0.85f);
	}
}

int32 AAetheriaBiomeBuilder::RipNearbyMatterForSingularity(const FVector& WorldCenter, float PullRadiusUU, int32 MaxChunks, TArray<FTransform>& OutRippedTransforms)
{
	// Architectural safety guard: Black Hole must NEVER rip, shrink, or displace world environment ISMs or buildings.
	(void)WorldCenter;
	(void)PullRadiusUU;
	(void)MaxChunks;
	OutRippedTransforms.Reset();
	return 0;
}

float AAetheriaBiomeBuilder::GetCurrentTriadMultiplier(EAetheriaSpellId SpellId) const
{
	if (CurrentLocationIndex == 5) return 1.35f;
	if (CurrentLocationIndex == 6) return 1.25f;
	if (CurrentLocationIndex == 9)
	{
		if (SpellId == EAetheriaSpellId::PrimalOverload) return 1.35f;
		if (const UWorld* World = GetWorld())
		{
			if (const AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
			{
				const EAetheriaOrb Phase = GS->ActiveTriadPhaseOrb;
				if (Phase == EAetheriaOrb::Ignis &&
					(SpellId == EAetheriaSpellId::Fireball || SpellId == EAetheriaSpellId::ChaosMeteor || SpellId == EAetheriaSpellId::SupernovaBeam))
					return 1.35f;
				if (Phase == EAetheriaOrb::Tempest &&
					(SpellId == EAetheriaSpellId::Chronosphere || SpellId == EAetheriaSpellId::ChainLightning || SpellId == EAetheriaSpellId::ChronoBlink))
					return 1.35f;
				if (Phase == EAetheriaOrb::Graviton &&
					(SpellId == EAetheriaSpellId::BlackHole || SpellId == EAetheriaSpellId::GravityCataclysm || SpellId == EAetheriaSpellId::PrismWall))
					return 1.35f;
			}
		}
	}
	return 1.0f;
}
