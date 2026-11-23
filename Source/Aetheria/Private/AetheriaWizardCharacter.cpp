#include "AetheriaWizardCharacter.h"
#include "AetheriaPackVFX.h"
#include "AetheriaTypes.h"
#include "AetheriaSpellDatabase.h"
#include "AetheriaGameState.h"
#include "AetheriaPlayerState.h"
#include "AetheriaMaterialLibrary.h"
#include "AetheriaAnimInstance.h"
#include "AetheriaSpellActors.h"
#include "AetheriaMobCharacter.h"
#include "AetheriaSaveGame.h"
#include "AetheriaBiomeDatabase.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "Engine/StaticMeshActor.h"
#include "EngineUtils.h"
#include "Engine/Engine.h"
#include "Engine/CollisionProfile.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"

#if __has_include("AetheriaGameMode.h")
#include "AetheriaGameMode.h"
#endif

#if __has_include("AetheriaBiomeBuilder.h")
#include "AetheriaBiomeBuilder.h"
#endif

AAetheriaWizardCharacter::AAetheriaWizardCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	// Setup capsule collision
	GetCapsuleComponent()->InitCapsuleSize(42.0f, 96.0f);
	GetCapsuleComponent()->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);

	// Movement settings for top-down isometric
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	GetCharacterMovement()->bOrientRotationToMovement = false;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 440.0f, 0.0f);
	GetCharacterMovement()->bConstrainToPlane = false;
	GetCharacterMovement()->bSnapToPlaneAtStart = false;
	GetCharacterMovement()->MaxStepHeight = 68.0f;
	GetCharacterMovement()->SetWalkableFloorAngle(52.0f);
	GetCharacterMovement()->MaxWalkSpeed = 660.0f;

	// Load built-in engine basic shapes guaranteed to be present in any packaged build
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMeshFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMeshFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeMeshFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));

	UStaticMesh* SphereMesh = SphereMeshFinder.Object;
	UStaticMesh* CylinderMesh = CylinderMeshFinder.Object;
	UStaticMesh* ConeMesh = ConeMeshFinder.Object;
	UStaticMesh* PlaneMesh = PlaneMeshFinder.Object;

	// 1. Cloak / Body Mesh (Obsidian Robe)
	CloakMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CloakMesh"));
	CloakMesh->SetupAttachment(RootComponent);
	CloakMesh->SetStaticMesh(ConeMesh);
	CloakMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -90.0f));
	CloakMesh->SetRelativeScale3D(FVector(0.72f, 0.72f, 1.75f));
	CloakMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CloakMesh->SetCastShadow(true);
	CloakMesh->bCastContactShadow = true;

	// 2. Mantle Mesh (Shoulder Pauldrons)
	MantleMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MantleMesh"));
	MantleMesh->SetupAttachment(RootComponent);
	MantleMesh->SetStaticMesh(CylinderMesh);
	MantleMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 32.0f));
	MantleMesh->SetRelativeScale3D(FVector(0.85f, 0.85f, 0.32f));
	MantleMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	MantleMesh->SetCastShadow(true);
	MantleMesh->bCastContactShadow = true;

	// 3. Head / Cowl Mesh
	HeadMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMesh"));
	HeadMesh->SetupAttachment(RootComponent);
	HeadMesh->SetStaticMesh(SphereMesh);
	HeadMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 74.0f));
	HeadMesh->SetRelativeScale3D(FVector(0.48f, 0.48f, 0.48f));
	HeadMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	HeadMesh->SetCastShadow(true);
	HeadMesh->bCastContactShadow = true;

	// 4. Staff Mesh
	StaffMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaffMesh"));
	StaffMesh->SetupAttachment(RootComponent);
	StaffMesh->SetStaticMesh(CylinderMesh);
	StaffMesh->SetRelativeLocation(FVector(42.0f, 32.0f, 6.0f));
	StaffMesh->SetRelativeScale3D(FVector(0.06f, 0.06f, 1.85f));
	StaffMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	StaffMesh->SetCastShadow(true);
	StaffMesh->bCastContactShadow = true;

	// 5. Staff Gem Mesh & Light (hidden muzzle anchor — never render giant sphere over head)
	StaffGemMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaffGemMesh"));
	StaffGemMesh->SetupAttachment(StaffMesh);
	StaffGemMesh->SetStaticMesh(SphereMesh);
	StaffGemMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 52.0f));
	StaffGemMesh->SetRelativeScale3D(FVector(0.08f, 0.08f, 0.08f));
	StaffGemMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	StaffGemMesh->SetVisibility(false);
	StaffGemMesh->SetHiddenInGame(true);
	StaffGemMesh->SetCastShadow(false);

	// 5b. Floor Arcane Cast Sigil (Hollow spinning arcane seal during spell cast choreography)
	CastSigilMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CastSigilMesh"));
	CastSigilMesh->SetupAttachment(RootComponent);
	CastSigilMesh->SetStaticMesh(PlaneMesh);
	CastSigilMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -86.0f));
	CastSigilMesh->SetRelativeScale3D(FVector(2.2f, 2.2f, 1.0f));
	CastSigilMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	CastSigilMesh->SetVisibility(false);
	CastSigilMesh->SetCastShadow(false);

	StaffLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("StaffLight"));
	StaffLight->SetupAttachment(StaffGemMesh);
	StaffLight->SetIntensity(750.0f);
	StaffLight->SetLightColor(AetheriaMindColors::MindCyan);
	StaffLight->SetAttenuationRadius(320.0f);
	StaffLight->SetCastShadows(false);

	// 6. Shield Shell Mesh (Hollow arcane floor ring when Shield > 0, never opaque sphere)
	ShieldMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShieldMesh"));
	ShieldMesh->SetupAttachment(RootComponent);
	ShieldMesh->SetStaticMesh(PlaneMesh);
	ShieldMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -85.0f));
	ShieldMesh->SetRelativeScale3D(FVector(2.35f, 2.35f, 1.0f));
	ShieldMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	ShieldMesh->SetVisibility(false);
	ShieldMesh->SetCastShadow(false);

	// 7. Cosmetic & Familiar Components
	HeadCosmeticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadCosmeticMesh"));
	HeadCosmeticMesh->SetupAttachment(RootComponent);
	HeadCosmeticMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 76.0f));
	HeadCosmeticMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	HeadCosmeticMesh->SetCastShadow(true);
	HeadCosmeticMesh->bCastContactShadow = true;
	HeadCosmeticMesh->SetVisibility(false);

	BackCosmeticMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BackCosmeticMesh"));
	BackCosmeticMesh->SetupAttachment(CloakMesh);
	BackCosmeticMesh->SetRelativeLocation(FVector(-20.0f, 0.0f, 35.0f));
	BackCosmeticMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BackCosmeticMesh->SetCastShadow(true);
	BackCosmeticMesh->bCastContactShadow = true;
	BackCosmeticMesh->SetVisibility(false);

	FloatingGrimoireMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FloatingGrimoireMesh"));
	FloatingGrimoireMesh->SetupAttachment(RootComponent);
	FloatingGrimoireMesh->SetRelativeLocation(FVector(40.0f, -48.0f, 30.0f));
	FloatingGrimoireMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	FloatingGrimoireMesh->SetCastShadow(true);
	FloatingGrimoireMesh->bCastContactShadow = true;
	FloatingGrimoireMesh->SetVisibility(false);

	FamiliarSkullMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FamiliarSkullMesh"));
	FamiliarSkullMesh->SetupAttachment(RootComponent);
	FamiliarSkullMesh->SetRelativeLocation(FVector(-35.0f, -40.0f, 55.0f));
	FamiliarSkullMesh->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	FamiliarSkullMesh->SetCastShadow(true);
	FamiliarSkullMesh->bCastContactShadow = true;
	FamiliarSkullMesh->SetVisibility(false);

	FamiliarLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("FamiliarLight"));
	FamiliarLight->SetupAttachment(FloatingGrimoireMesh);
	FamiliarLight->SetCastShadows(false);
	FamiliarLight->SetIntensity(600.0f);
	FamiliarLight->SetAttenuationRadius(250.0f);
	FamiliarLight->SetLightColor(AetheriaMindColors::MindCyan);
	FamiliarLight->SetVisibility(false);

	HeroRimLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("HeroRimLight"));
	HeroRimLight->SetupAttachment(RootComponent);
	HeroRimLight->SetCastShadows(false);
	HeroRimLight->SetIntensity(800.0f);
	HeroRimLight->SetAttenuationRadius(350.0f);
	HeroRimLight->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
	HeroRimLight->SetLightColor(FLinearColor(0.72f, 0.76f, 0.84f, 1.0f));

	// 8. Orbiting Elemental Orbs (Q, W, E) as crisp arcane motes beside shoulders (no giant bloom spheres)
	OrbPivot = CreateDefaultSubobject<USceneComponent>(TEXT("OrbPivot"));
	OrbPivot->SetupAttachment(RootComponent);
	OrbPivot->SetRelativeLocation(FVector(0.0f, 0.0f, 36.0f));

	OrbMesh0 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh0"));
	OrbMesh0->SetupAttachment(OrbPivot);
	OrbMesh0->SetStaticMesh(SphereMesh);
	OrbMesh0->SetRelativeScale3D(FVector(0.11f, 0.11f, 0.11f));
	OrbMesh0->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	OrbMesh0->SetCastShadow(false);

	OrbLight0 = CreateDefaultSubobject<UPointLightComponent>(TEXT("OrbLight0"));
	OrbLight0->SetupAttachment(OrbMesh0);
	OrbLight0->SetIntensity(240.0f);
	OrbLight0->SetAttenuationRadius(220.0f);
	OrbLight0->SetLightColor(AetheriaMindColors::IgnisPlasma);
	OrbLight0->SetCastShadows(false);

	OrbMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh1"));
	OrbMesh1->SetupAttachment(OrbPivot);
	OrbMesh1->SetStaticMesh(SphereMesh);
	OrbMesh1->SetRelativeScale3D(FVector(0.11f, 0.11f, 0.11f));
	OrbMesh1->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	OrbMesh1->SetCastShadow(false);

	OrbLight1 = CreateDefaultSubobject<UPointLightComponent>(TEXT("OrbLight1"));
	OrbLight1->SetupAttachment(OrbMesh1);
	OrbLight1->SetIntensity(240.0f);
	OrbLight1->SetAttenuationRadius(220.0f);
	OrbLight1->SetLightColor(AetheriaMindColors::MindCyan);
	OrbLight1->SetCastShadows(false);

	OrbMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh2"));
	OrbMesh2->SetupAttachment(OrbPivot);
	OrbMesh2->SetStaticMesh(SphereMesh);
	OrbMesh2->SetRelativeScale3D(FVector(0.11f, 0.11f, 0.11f));
	OrbMesh2->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	OrbMesh2->SetCastShadow(false);

	OrbLight2 = CreateDefaultSubobject<UPointLightComponent>(TEXT("OrbLight2"));
	OrbLight2->SetupAttachment(OrbMesh2);
	OrbLight2->SetIntensity(240.0f);
	OrbLight2->SetAttenuationRadius(220.0f);
	OrbLight2->SetLightColor(AetheriaMindColors::MindViolet);
	OrbLight2->SetCastShadows(false);

	// 9. Top-down Diablo IV Isometric Camera Boom & Camera Component
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	// Godot SpringArm3D: 21.5 m, rotation (-52, -45, 0), collision_mask 0.
	// Diablo IV-style framing: ~55 deg down, 45 deg diagonal, ~16.5 m arm.
	// No collision test (no jitter); walls between camera and hero are
	// hidden from the main pass instead (see UpdateCameraOccluders).
	CameraBoom->TargetArmLength = 1650.0f;
	CameraBoom->SetRelativeRotation(FRotator(-55.0f, -45.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	CameraBoom->bInheritYaw = false;
	CameraBoom->bInheritPitch = false;
	CameraBoom->bInheritRoll = false;
	CameraBoom->bEnableCameraLag = true;
	CameraBoom->CameraLagSpeed = 9.0f;
	CameraBoom->CameraLagMaxDistance = 350.0f;
	CameraBoom->bEnableCameraRotationLag = false;
	// Camera-space lift: puts the hero slightly below screen centre so more
	// of the arena ahead is visible.
	CameraBoom->SocketOffset = FVector(0.0f, 0.0f, 130.0f);

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCameraComponent"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;
	TopDownCameraComponent->FieldOfView = 52.0f;

	// Default active orbs & per-spell cooldown table (index = EAetheriaSpellId)
	ActiveOrbs.Add(EAetheriaOrb::Ignis);
	ActiveOrbs.Add(EAetheriaOrb::Tempest);
	ActiveOrbs.Add(EAetheriaOrb::Graviton);
	SpellCooldowns.Init(0.0f, 17);

	SlotD = EAetheriaSpellId::Fireball;
	SlotF = EAetheriaSpellId::Chronosphere;
}

void AAetheriaWizardCharacter::BeginPlay()
{
	if (!FindComponentByClass<UPointLightComponent>())
	{
		if (UPointLightComponent* Fill = NewObject<UPointLightComponent>(this, TEXT("PlayerFillLight")))
		{
			Fill->SetupAttachment(GetRootComponent());
			Fill->SetRelativeLocation(FVector(-80.f, 90.f, 160.f));
			Fill->SetIntensityUnits(ELightUnits::Candelas);
			Fill->SetIntensity(1200.f);
			Fill->SetLightColor(FLinearColor(1.0f, 0.92f, 0.82f));
			Fill->SetAttenuationRadius(420.f);
			Fill->SetCastShadows(false);
			Fill->SetMobility(EComponentMobility::Movable);
			Fill->RegisterComponent();
		}
		if (UPointLightComponent* Rim = NewObject<UPointLightComponent>(this, TEXT("PlayerRimLight")))
		{
			Rim->SetupAttachment(GetRootComponent());
			Rim->SetRelativeLocation(FVector(140.f, -120.f, 180.f));
			Rim->SetIntensityUnits(ELightUnits::Candelas);
			Rim->SetIntensity(900.f);
			Rim->SetLightColor(FLinearColor(0.55f, 0.75f, 1.0f));
			Rim->SetAttenuationRadius(480.f);
			Rim->SetCastShadows(false);
			Rim->SetMobility(EComponentMobility::Movable);
			Rim->RegisterComponent();
		}
	}

	Super::BeginPlay();

	if (SpellCooldowns.Num() < 17)
	{
		SpellCooldowns.Init(0.0f, 17);
	}
	PrevActorYaw = GetActorRotation().Yaw;
	if (GetWorld() && GetWorld()->GetNetMode() != NM_Standalone)
	{
		bIsInLobbyMode = false;
	}

	SetupSculptedMeshes();
	SetupDynamicMaterials();
	ApplyCustomizationVisuals();
	UpdateOrbVisuals();
	UpdateShieldVisuals();
}

void AAetheriaWizardCharacter::SetupSculptedMeshes()
{
	bUsingSkeletalPresentation = UAetheriaAnimInstance::BindBestPresentation(
		this,
		GetCapsuleComponent() ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 96.0f,
		{ TEXT("/Game/ParagonGideon") },
		TEXT("Gideon"),
		TEXT("/Game/Characters/Archmage"),
		TEXT("SKM_Archmage"));

	if (bUsingSkeletalPresentation)
	{
		if (CloakMesh) { CloakMesh->SetVisibility(false); CloakMesh->SetHiddenInGame(true); }
		if (MantleMesh) { MantleMesh->SetVisibility(false); MantleMesh->SetHiddenInGame(true); }
		if (HeadMesh) { HeadMesh->SetVisibility(false); HeadMesh->SetHiddenInGame(true); }
		if (StaffGemMesh) { StaffGemMesh->SetVisibility(false); StaffGemMesh->SetHiddenInGame(true); }
		if (HeadCosmeticMesh) { HeadCosmeticMesh->SetVisibility(false); HeadCosmeticMesh->SetHiddenInGame(true); }
		AttachHeroStaff();
		return;
	}

	if (UStaticMesh* RobedMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_ArchmageRobed"), TEXT("")))
	{
		if (CloakMesh)
		{
			CloakMesh->SetStaticMesh(RobedMesh);
			CloakMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
			CloakMesh->SetRelativeRotation(FRotator::ZeroRotator);
			CloakMesh->SetRelativeScale3D(FVector(1.08f, 1.08f, 1.08f));
		}
		if (MantleMesh) MantleMesh->SetVisibility(false);
		if (HeadMesh) HeadMesh->SetVisibility(false);
	}

	if (UStaticMesh* GnarledStaff = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_GnarledStaff"), TEXT("")))
	{
		if (StaffMesh)
		{
			StaffMesh->SetStaticMesh(GnarledStaff);
			StaffMesh->SetRelativeLocation(FVector(36.0f, 36.0f, -68.0f));
			StaffMesh->SetRelativeRotation(FRotator::ZeroRotator);
			StaffMesh->SetRelativeScale3D(FVector(0.95f, 0.95f, 0.95f));
		}
	}
	if (StaffGemMesh)
	{
		StaffGemMesh->SetVisibility(false);
		StaffGemMesh->SetHiddenInGame(true);
	}
}

void AAetheriaWizardCharacter::SetupDynamicMaterials()
{
	UMaterialInterface* ObsidianBase = UAetheriaMaterialLibrary::GetDarkIronMaterial(this);
	if (!ObsidianBase)
	{
		ObsidianBase = UAetheriaMaterialLibrary::GetObsidianMaterial(this);
	}
	UMaterialInterface* EmissiveBase = UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);
	UMaterialInterface* RingBase = UAetheriaMaterialLibrary::GetArcaneRingMaterial(this);

	if (ObsidianBase && !bUsingSkeletalPresentation)
	{
		FLinearColor TintColor = AetheriaMindColors::GetPartyColor(PartyColorIndex);
		DynamicCloakMaterial = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, ObsidianBase, AetheriaMindColors::ObsidianVoid, TintColor, 0.35f, 1.0f);

		if (DynamicCloakMaterial)
		{
			CloakMesh->SetMaterial(0, DynamicCloakMaterial);
			MantleMesh->SetMaterial(0, DynamicCloakMaterial);
			HeadMesh->SetMaterial(0, DynamicCloakMaterial);
			StaffMesh->SetMaterial(0, DynamicCloakMaterial);
		}
	}

	if (EmissiveBase)
	{
		DynamicStaffGemMaterial = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, EmissiveBase, AetheriaMindColors::MindCyan, AetheriaMindColors::MindCyan, 1.5f, 1.0f);
		if (DynamicStaffGemMaterial)
		{
			StaffGemMesh->SetMaterial(0, DynamicStaffGemMaterial);
		}
	}

	UMaterialInterface* RingMatBase = RingBase ? RingBase : EmissiveBase;
	if (RingMatBase)
	{
		DynamicShieldMaterial = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, RingMatBase, AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 3.2f, 0.86f);
		if (DynamicShieldMaterial && ShieldMesh)
		{
			ShieldMesh->SetMaterial(0, DynamicShieldMaterial);
		}
		if (CastSigilMesh)
		{
			if (UMaterialInstanceDynamic* SigilMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, RingMatBase, AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 3.8f, 0.90f))
			{
				CastSigilMesh->SetMaterial(0, SigilMat);
			}
		}
	}

	if (EmissiveBase)
	{

		for (int32 i = 0; i < 3; ++i)
		{
			EAetheriaOrb Orb = (ActiveOrbs.IsValidIndex(i)) ? ActiveOrbs[i] : EAetheriaOrb::Ignis;
			FLinearColor OrbCol = AetheriaMindColors::GetOrbColor(Orb);
			DynamicOrbMaterials[i] = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, EmissiveBase, OrbCol, OrbCol, 4.0f, 1.0f);

			if (UStaticMeshComponent* M = GetOrbMesh(i))
			{
				M->SetMaterial(0, DynamicOrbMaterials[i]);
			}
		}
	}
}

void AAetheriaWizardCharacter::ApplyCustomizationVisuals()
{
	if (bUsingSkeletalPresentation)
	{
		if (CloakMesh) { CloakMesh->SetVisibility(false); CloakMesh->SetHiddenInGame(true); }
		if (MantleMesh) { MantleMesh->SetVisibility(false); MantleMesh->SetHiddenInGame(true); }
		if (HeadMesh) { HeadMesh->SetVisibility(false); HeadMesh->SetHiddenInGame(true); }
		if (StaffMesh && bStaffAttachedToHand)
		{
			StaffMesh->SetVisibility(true);
			StaffMesh->SetHiddenInGame(false);
		}
		return;
	}
	const int32 ArchIdx = FMath::Clamp(Customization.BodyArchetypeIndex, 0, 9);
	const FString HeroMeshName = FString::Printf(TEXT("SM_Hero_%02d"), ArchIdx);

	FString FallbackHero = TEXT("SM_ArchmageRobed");
	if (ArchIdx >= 3 && ArchIdx <= 5)
	{
		FallbackHero = TEXT("SM_Hero_Necromancer");
	}
	else if (ArchIdx >= 6 && ArchIdx <= 7)
	{
		FallbackHero = TEXT("SM_Hero_LichKing");
	}
	else if (ArchIdx >= 8)
	{
		FallbackHero = TEXT("SM_Hero_Chronomancer");
	}

	UStaticMesh* LoadedHeroMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(HeroMeshName, TEXT(""));
	bool bLoadedCustomHero = (LoadedHeroMesh != nullptr);
	if (!LoadedHeroMesh)
	{
		LoadedHeroMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(FallbackHero, TEXT(""));
		if (LoadedHeroMesh) bLoadedCustomHero = true;
	}
	if (!LoadedHeroMesh)
	{
		LoadedHeroMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_ArchmageRobed"), TEXT(""));
		if (LoadedHeroMesh) bLoadedCustomHero = true;
	}
	bUsingCustomSculptedHero = bLoadedCustomHero;

	const float ClampedScale = FMath::Clamp(Customization.BodyScale, 0.85f, 1.25f);
	if (CloakMesh && LoadedHeroMesh)
	{
		CloakMesh->SetStaticMesh(LoadedHeroMesh);
		CloakMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
		CloakMesh->SetRelativeRotation(FRotator::ZeroRotator);
		CloakMesh->SetRelativeScale3D(FVector(ClampedScale * 1.08f, ClampedScale * 1.08f, ClampedScale * 1.08f));
	}
	if (MantleMesh) MantleMesh->SetVisibility(false);
	if (HeadMesh) HeadMesh->SetVisibility(false);

	const FLinearColor DyeColor = AetheriaMindColors::GetRobeDyeColor(Customization.RobeDyeIndex);
	const FLinearColor RuneColor = AetheriaMindColors::GetRuneGlowColor(Customization.RuneGlowIndex);
	const FLinearColor MetalColor = AetheriaMindColors::GetTrimMetalColor(Customization.TrimMetalIndex);

	const FString TexStem = FString::Printf(TEXT("Hero_%02d"), ArchIdx);
	DynamicCloakMaterial = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TexStem, DyeColor, RuneColor, 0.35f);

	if (!DynamicCloakMaterial)
	{
		UMaterialInterface* BaseMat = UAetheriaMaterialLibrary::GetMasterCharacterPBRMaterial(this);
		if (!BaseMat) BaseMat = UAetheriaMaterialLibrary::GetObsidianMaterial(this);
		DynamicCloakMaterial = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, BaseMat, DyeColor, RuneColor, 0.35f, 1.0f);
	}

	if (DynamicCloakMaterial && CloakMesh)
	{
		DynamicCloakMaterial->SetScalarParameterValue(FName("ClothWind"), 1.0f);
		for (int32 s = 0; s < CloakMesh->GetNumMaterials(); ++s)
		{
			CloakMesh->SetMaterial(s, DynamicCloakMaterial);
		}
	}

	if (LastPreviewArchetype >= 0 && LastPreviewArchetype != Customization.BodyArchetypeIndex &&
		(bIsInLobbyMode || bIsInEditorMode))
	{
		CastAnimTimer = 0.46f;
		InvokePulseTimer = 0.32f;
	}
	LastPreviewArchetype = Customization.BodyArchetypeIndex;

	// Headgear Cosmetic
	if (HeadCosmeticMesh)
	{
		if (Customization.HeadgearIndex <= 0)
		{
			HeadCosmeticMesh->SetVisibility(false);
		}
		else
		{
			static const TCHAR* HeadMeshes[] = {
				TEXT("SM_Head_BoneCrown"),
				TEXT("SM_Head_HorrorHorns"),
				TEXT("SM_Head_ChronoHalo"),
				TEXT("SM_Head_HopeMask"),
				TEXT("SM_Head_AbyssHood")
			};
			const int32 HeadIdx = FMath::Clamp(Customization.HeadgearIndex - 1, 0, 4);
			if (UStaticMesh* HM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(HeadMeshes[HeadIdx], TEXT("")))
			{
				HeadCosmeticMesh->SetStaticMesh(HM);
				HeadCosmeticMesh->SetVisibility(true);
				const float BaseScale = ClampedScale * 1.08f;
				HeadCosmeticMesh->SetRelativeScale3D(FVector(0.74f * BaseScale));
				HeadCosmeticMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 134.0f * BaseScale));

				UMaterialInstanceDynamic* HeadMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
					this, HeadMeshes[HeadIdx], MetalColor, RuneColor, 0.45f);
				if (HeadMat)
				{
					for (int32 s = 0; s < HeadCosmeticMesh->GetNumMaterials(); ++s)
					{
						HeadCosmeticMesh->SetMaterial(s, HeadMat);
					}
				}
			}
			else
			{
				HeadCosmeticMesh->SetVisibility(false);
			}
		}
	}

	// Staff Cosmetic
	if (StaffMesh)
	{
		if (Customization.StaffIndex <= 0)
		{
			if (bLoadedCustomHero || bUsingCustomSculptedHero)
			{
				StaffMesh->SetVisibility(false);
				if (StaffGemMesh) StaffGemMesh->SetVisibility(false);
			}
			else
			{
				StaffMesh->SetVisibility(true);
				if (StaffGemMesh) StaffGemMesh->SetVisibility(false);
			}
		}
		else
		{
			static const TCHAR* StaffMeshes[] = {
				TEXT("SM_GnarledStaff"),
				TEXT("SM_Staff_Arcane01"),
				TEXT("SM_Staff_Arcane02"),
				TEXT("SM_OrnateMace")
			};
			const int32 StaffIdx = FMath::Clamp(Customization.StaffIndex - 1, 0, 3);
			UStaticMesh* SM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(StaffMeshes[StaffIdx], TEXT(""));
			if (!SM)
			{
				SM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_GnarledStaff"), TEXT(""));
			}
			if (!SM)
			{
				SM = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
			}
			if (SM)
			{
				StaffMesh->SetStaticMesh(SM);
				StaffMesh->SetVisibility(true);
				if (StaffGemMesh) StaffGemMesh->SetVisibility(false);

				UMaterialInstanceDynamic* StaffMat = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
					this, StaffMeshes[StaffIdx], MetalColor, RuneColor, 0.40f);
				if (!StaffMat)
				{
					StaffMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
						this, UAetheriaMaterialLibrary::GetObsidianMaterial(this), MetalColor, RuneColor, 0.40f, 1.0f);
				}
				if (StaffMat)
				{
					for (int32 s = 0; s < StaffMesh->GetNumMaterials(); ++s)
					{
						StaffMesh->SetMaterial(s, StaffMat);
					}
				}
			}
		}
	}

	// Familiar & Grimoire Cosmetic
	// Custom sculpted SM_Hero_00..09 already hold weapons & tomes in-hand; hide extra floating props at default FamiliarIndex == 0 so nothing clips!
	if (FloatingGrimoireMesh && FamiliarSkullMesh)
	{
		if (bLoadedCustomHero && Customization.FamiliarIndex == 0)
		{
			FamiliarSkullMesh->SetVisibility(false);
			FloatingGrimoireMesh->SetVisibility(false);
			if (FamiliarLight) FamiliarLight->SetVisibility(bIsInLobbyMode || bIsInEditorMode);
		}
		else if (Customization.FamiliarIndex == 0)
		{
			if (UStaticMesh* SkullM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_Familiar_SpectralSkull"), TEXT("")))
			{
				FamiliarSkullMesh->SetStaticMesh(SkullM);
				FamiliarSkullMesh->SetVisibility(true);
				if (DynamicCloakMaterial) FamiliarSkullMesh->SetMaterial(0, DynamicCloakMaterial);
			}
			if (UStaticMesh* GrimM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_Familiar_OpenGrimoire"), TEXT("")))
			{
				FloatingGrimoireMesh->SetStaticMesh(GrimM);
				FloatingGrimoireMesh->SetVisibility(true);
				if (DynamicCloakMaterial) FloatingGrimoireMesh->SetMaterial(0, DynamicCloakMaterial);
			}
			if (FamiliarLight) FamiliarLight->SetVisibility(true);
		}
		else if (Customization.FamiliarIndex == 1)
		{
			FamiliarSkullMesh->SetVisibility(false);
			if (UStaticMesh* GrimM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_Familiar_OpenGrimoire"), TEXT("")))
			{
				FloatingGrimoireMesh->SetStaticMesh(GrimM);
				FloatingGrimoireMesh->SetVisibility(true);
				if (DynamicCloakMaterial) FloatingGrimoireMesh->SetMaterial(0, DynamicCloakMaterial);
			}
			if (FamiliarLight) FamiliarLight->SetVisibility(true);
		}
		else if (Customization.FamiliarIndex == 2)
		{
			FamiliarSkullMesh->SetVisibility(false);
			if (UStaticMesh* AstroM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_Familiar_Astrolabe"), TEXT("")))
			{
				FloatingGrimoireMesh->SetStaticMesh(AstroM);
				FloatingGrimoireMesh->SetVisibility(true);
				if (DynamicCloakMaterial) FloatingGrimoireMesh->SetMaterial(0, DynamicCloakMaterial);
			}
			if (FamiliarLight) FamiliarLight->SetVisibility(true);
		}
		else
		{
			FamiliarSkullMesh->SetVisibility(false);
			FloatingGrimoireMesh->SetVisibility(false);
			if (FamiliarLight) FamiliarLight->SetVisibility(bIsInLobbyMode || bIsInEditorMode);
		}
	}

	// Back Cosmetic
	if (BackCosmeticMesh)
	{
		if (Customization.BackIndex <= 0)
		{
			BackCosmeticMesh->SetVisibility(false);
		}
		else
		{
			static const TCHAR* BackMeshes[] = {
				TEXT("SM_Back_DemonWings"),
				TEXT("SM_Back_ArchonHalo"),
				TEXT("SM_Back_TatteredCape")
			};
			const int32 BackIdx = FMath::Clamp(Customization.BackIndex - 1, 0, 2);
			if (UStaticMesh* BM = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(BackMeshes[BackIdx], TEXT("")))
			{
				BackCosmeticMesh->SetStaticMesh(BM);
				BackCosmeticMesh->SetVisibility(true);
				if (DynamicCloakMaterial)
				{
					for (int32 s = 0; s < BackCosmeticMesh->GetNumMaterials(); ++s)
					{
						BackCosmeticMesh->SetMaterial(s, DynamicCloakMaterial);
					}
				}
			}
			else
			{
				BackCosmeticMesh->SetVisibility(false);
			}
		}
	}

	// Lights & Emissive accents
	if (StaffLight) StaffLight->SetLightColor(RuneColor);
	if (HeroRimLight)
	{
		FLinearColor NeutralRim = MetalColor;
		if (NeutralRim.G > NeutralRim.R * 1.25f && NeutralRim.G > NeutralRim.B * 1.25f)
		{
			NeutralRim = FLinearColor(0.72f, 0.76f, 0.84f, 1.0f);
		}
		HeroRimLight->SetLightColor(NeutralRim);
	}
	if (FamiliarLight) FamiliarLight->SetLightColor(RuneColor);

	if (DynamicStaffGemMaterial)
	{
		DynamicStaffGemMaterial->SetVectorParameterValue(FName("BaseColor"), RuneColor);
		DynamicStaffGemMaterial->SetVectorParameterValue(FName("EmissiveColor"), RuneColor);
	}
}

void AAetheriaWizardCharacter::SetEditorCameraMode(bool bOpen, float TurntableYaw)
{
	bIsInEditorMode = bOpen;
	EditorTurntableYaw = TurntableYaw;
}

void AAetheriaWizardCharacter::OnRep_Customization()
{
	ApplyCustomizationVisuals();
}

void AAetheriaWizardCharacter::ServerSetCustomization_Implementation(FAetheriaCharacterCustomization NewCustomization)
{
	Customization = NewCustomization;
	ApplyCustomizationVisuals();
}

void AAetheriaWizardCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 1. Orbiting 3D Elemental Spheres around wizard shoulders (plus Invoke pulse & downed beacon)
	OrbitAngle += DeltaSeconds * (bIsAlive ? 135.0f : 42.0f);
	if (OrbitAngle >= 360.0f)
	{
		OrbitAngle -= 360.0f;
	}

	if (LastSeenWizardHP < 0.0f)
	{
		LastSeenWizardHP = MaxHP;
	}
	if (HP < LastSeenWizardHP && bIsAlive)
	{
		HitFlashTimer = 0.18f;
		HitRecoilTimer = 0.16f;
		PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::HitReact));
	}
	LastSeenWizardHP = HP;

	DownedAnimAlpha = FMath::FInterpTo(DownedAnimAlpha, bIsAlive ? 0.0f : 1.0f, DeltaSeconds, 7.5f);
	HitRecoilTimer = FMath::Max(0.0f, HitRecoilTimer - DeltaSeconds);
	InvokePulseTimer = FMath::Max(0.0f, InvokePulseTimer - DeltaSeconds);

	const bool bHideOrbsInMenu = (bIsInLobbyMode || bIsInEditorMode);
	float OrbRadius = FMath::Lerp(74.0f, 98.0f, DownedAnimAlpha);
	float OrbScale = 0.075f;
	float OrbHeightZ = FMath::Lerp(30.0f, -48.0f, DownedAnimAlpha);
	if (InvokePulseTimer > 0.0f && bIsAlive)
	{
		const float P = 1.0f - FMath::Clamp(InvokePulseTimer / 0.32f, 0.0f, 1.0f);
		if (P < 0.35f)
		{
			const float T = P / 0.35f;
			OrbRadius = FMath::Lerp(74.0f, 44.0f, T);
			OrbScale = FMath::Lerp(0.075f, 0.11f, T);
		}
		else
		{
			const float T = 1.0f - FMath::Pow(1.0f - ((P - 0.35f) / 0.65f), 3.0f);
			OrbRadius = FMath::Lerp(96.0f, 74.0f, T);
			OrbScale = FMath::Lerp(0.13f, 0.075f, T);
		}
	}

	for (int32 i = 0; i < 3; ++i)
	{
		UStaticMeshComponent* OrbM = GetOrbMesh(i);
		if (OrbM)
		{
			OrbM->SetVisibility(!bHideOrbsInMenu && bIsAlive);
			const float AngleDeg = OrbitAngle + i * 120.0f;
			const float AngleRad = FMath::DegreesToRadians(AngleDeg);
			const float MotionAmp = FMath::Clamp(GetVelocity().Size2D() / 660.0f, 0.0f, 1.0f);
			const float Bobbing = FMath::Sin(FMath::DegreesToRadians(OrbitAngle * 2.2f + i * 90.0f)) * (4.0f * MotionAmp);
			const FVector LocalPos(FMath::Cos(AngleRad) * OrbRadius, FMath::Sin(AngleRad) * OrbRadius, OrbHeightZ + Bobbing);
			OrbM->SetRelativeLocation(LocalPos);
			OrbM->SetRelativeScale3D(FVector(OrbScale));
		}
	}

	// 2. Per-spell cooldown countdowns & slot sync ([P0-1])
	for (float& CD : SpellCooldowns)
	{
		CD = FMath::Max(0.0f, CD - DeltaSeconds);
	}
	SlotDCooldownRemaining = GetSpellCooldownRemaining(SlotD);
	SlotFCooldownRemaining = GetSpellCooldownRemaining(SlotF);
	DashCooldownRemaining = FMath::Max(0.0f, DashCooldownRemaining - DeltaSeconds);
	DashInvulnRemaining = FMath::Max(0.0f, DashInvulnRemaining - DeltaSeconds);
	FlaskCooldownRemaining = FMath::Max(0.0f, FlaskCooldownRemaining - DeltaSeconds);
	SelfCastCooldownRemaining = FMath::Max(0.0f, SelfCastCooldownRemaining - DeltaSeconds);
	HasteBuffTimer = FMath::Max(0.0f, HasteBuffTimer - DeltaSeconds);

	CastAnimTimer = FMath::Max(0.0f, CastAnimTimer - DeltaSeconds);
	DashAnimTimer = FMath::Max(0.0f, DashAnimTimer - DeltaSeconds);
	HitFlashTimer = FMath::Max(0.0f, HitFlashTimer - DeltaSeconds);
	CameraShakeTrauma = FMath::Max(0.0f, CameraShakeTrauma - DeltaSeconds * 2.6f);

	// 2b. Kinematic Glide Dash (0.22s smooth swept traversal + 3 spectral afterimages)
	if (bGlideDashing)
	{
		const float PrevElapsed = GlideDashElapsed;
		const float StepDt = FMath::Min(DeltaSeconds, FMath::Max(0.0f, GlideDashDuration - PrevElapsed));
		GlideDashElapsed += DeltaSeconds;

		if (StepDt > 0.0f && (HasAuthority() || IsLocallyControlled()))
		{
			FHitResult SweepHit;
			AddActorWorldOffset(GlideDashVelocity * StepDt, true, &SweepHit);
		}

		const float Progress = FMath::Clamp(GlideDashElapsed / FMath::Max(0.01f, GlideDashDuration), 0.0f, 1.0f);
		if ((DashGhostStep == 0 && Progress >= 0.25f) ||
			(DashGhostStep == 1 && Progress >= 0.58f) ||
			(DashGhostStep == 2 && Progress >= 0.90f))
		{
			SpawnAfterimageGhost(GetActorLocation(), GetActorRotation(), 0.72f - DashGhostStep * 0.16f);
			++DashGhostStep;
		}

		if (GlideDashElapsed >= GlideDashDuration)
		{
			bGlideDashing = false;
			if (UWorld* World = GetWorld())
			{
				AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation() - FVector(0.0f, 0.0f, 82.0f), 320.0f, AetheriaMindColors::MindViolet, 0.38f);
			}
		}
	}

	// Hit Flash scalar parameter update
	if (DynamicCloakMaterial)
	{
		const float FlashStrength = FMath::Clamp(HitFlashTimer / 0.18f, 0.0f, 1.0f);
		DynamicCloakMaterial->SetScalarParameterValue(FName("HitFlash"), FlashStrength);
	}

	// Restore local host wizard progression once possessed
	if (!bProgressionLoaded && HasAuthority())
	{
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			bProgressionLoaded = true;
			if (PC->IsLocalPlayerController())
			{
				if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
				{
					WizardLevel = FMath::Max(1, Save->WizardLevel);
					CurrentXP = FMath::Max(0, Save->CurrentXP);
					ArcaneEssence = FMath::Max(0, Save->ArcaneEssence);
					EssenceEmpowerRank = FMath::Max(0, Save->EssenceEmpowerRank);
					Customization = Save->SavedCustomization;
					ApplyCustomizationVisuals();

					MaxHP = 450.0f + 22.0f * static_cast<float>(WizardLevel - 1) + 20.0f * static_cast<float>(EssenceEmpowerRank);
					MaxMana = 250.0f + 12.0f * static_cast<float>(WizardLevel - 1) + 10.0f * static_cast<float>(EssenceEmpowerRank);

					for (const FString& RelicId : Save->EquippedRelicNames)
					{
						GrantRelicBonus(RelicId, RelicId, 1.0f);
					}
					HP = MaxHP;
					Mana = MaxMana;
				}
			}
		}
	}

	// 3. Smooth Camera Boom transition (Editor vs Lobby Character Select vs Diablo IV Gameplay + Trauma)
	if (CameraBoom)
	{
		// Godot: spring_length = lerp(..., desired - trauma*0.45 m, delta*10)
		const float TraumaPunchUU = CameraShakeTrauma * 45.0f;
		float TargetArmLength = DesiredCameraArmLength - TraumaPunchUU;
		FRotator TargetRot(-55.0f, -45.0f, 0.0f);
		const FVector BaseSocketOffset(0.0f, 0.0f, 130.0f);
		FVector TargetSocketOffset = BaseSocketOffset;

		if (bIsInEditorMode)
		{
			TargetArmLength = 450.0f;
			TargetRot = FRotator(-11.0f, -45.0f + EditorTurntableYaw, 0.0f);
			TargetSocketOffset = FVector(0.0f, 72.0f, 18.0f);
		}
		else if (BossFocusTimer > 0.0f && BossFocusTarget.IsValid())
		{
			BossFocusTimer -= DeltaSeconds;
			const FVector ToBoss = BossFocusTarget->GetActorLocation() - GetActorLocation();
			const float Yaw = ToBoss.Rotation().Yaw;
			TargetArmLength = 980.0f;
			TargetRot = FRotator(-28.0f, Yaw, 0.0f);
			TargetSocketOffset = FVector(0.0f, 0.0f, 60.0f);
			if (BossFocusTimer <= 0.0f)
			{
				BossFocusTarget.Reset();
			}
		}
		else if (bIsInLobbyMode)
		{
			const float T = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			TargetArmLength = 720.0f + FMath::Cos(T * 0.13f) * 48.0f;
			TargetRot = FRotator(-18.0f + FMath::Sin(T * 0.11f) * 2.2f, -45.0f + FMath::Sin(T * 0.15f) * 9.0f, 0.0f);
			// Menu panel covers the left third: shift the camera left so the
			// hero sits right of centre in the courtyard shot.
			TargetSocketOffset = FVector(0.0f, -150.0f + FMath::Sin(T * 0.17f) * 16.0f, 36.0f);
		}
		else if (CameraShakeTrauma > 0.001f)
		{
			// Godot Camera3D h/v_offset: trauma^2 * 0.38 m
			const float TNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const float Mag = CameraShakeTrauma * CameraShakeTrauma * 38.0f;
			TargetSocketOffset = BaseSocketOffset + FVector(
				0.0f,
				FMath::Sin(TNow * 52.0f) * Mag,
				FMath::Cos(TNow * 47.0f) * Mag);
			CameraShakeTrauma = FMath::Max(0.0f, CameraShakeTrauma - DeltaSeconds * 2.8f);
		}

		const float InterpSpeed = 10.0f; // Godot lerpf rate delta*10
		const float CamInterp = bIsInLobbyMode ? 4.0f : InterpSpeed;
		CameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, CamInterp);
		CameraBoom->SetRelativeRotation(FMath::RInterpTo(CameraBoom->GetRelativeRotation(), TargetRot, DeltaSeconds, CamInterp));
		CameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocketOffset, DeltaSeconds, CamInterp);
		if (TopDownCameraComponent)
		{
			const float TargetFOV = bIsInLobbyMode ? 50.0f : (bIsInEditorMode ? 48.0f : 52.0f);
			TopDownCameraComponent->SetFieldOfView(FMath::FInterpTo(TopDownCameraComponent->FieldOfView, TargetFOV, DeltaSeconds, 4.0f));
		}
		// Lag only in gameplay; lobby/editor/boss shots are authored framings.
		CameraBoom->bEnableCameraLag = !bIsInLobbyMode && !bIsInEditorMode;
		UpdateCameraOccluders(DeltaSeconds);
	}

	// 4. Procedural animations: Turn-banking, Living Sanctuary Breathing + Cursor Tracking, 3-phase Cast Choreography, Glide Stretch
	SmoothedLocomotionVel = FMath::VInterpTo(SmoothedLocomotionVel, GetVelocity(), DeltaSeconds, 8.5f);
	const float Speed = SmoothedLocomotionVel.Size2D();
	const float MaxSpeed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 660.0f;
	const float SpeedRatio = FMath::Clamp(Speed / FMath::Max(1.0f, MaxSpeed), 0.0f, 1.0f);

	// Weight transfer & deceleration lean-back recovery
	const FVector RawLocalVel = GetActorQuat().UnrotateVector(GetVelocity());
	const FVector SmoothLocalVel = GetActorQuat().UnrotateVector(SmoothedLocomotionVel);
	const float AccelDeltaX = (RawLocalVel.X - SmoothLocalVel.X) / FMath::Max(DeltaSeconds, 0.001f);
	const float InertiaPitchLean = FMath::Clamp(AccelDeltaX * -0.012f, -8.0f, 6.0f);

	const float CurActorYaw = GetActorRotation().Yaw;
	const float YawVelDegPerSec = FMath::FindDeltaAngleDegrees(PrevActorYaw, CurActorYaw) / FMath::Max(DeltaSeconds, 0.001f);
	PrevActorYaw = CurActorYaw;
	const float TargetBankRoll = FMath::Clamp(YawVelDegPerSec * 0.014f, -4.0f, 4.0f) * SpeedRatio;
	CurrentBankRoll = FMath::FInterpTo(CurrentBankRoll, TargetBankRoll, DeltaSeconds, 10.0f);

	if (bHasDesiredAimYaw && bIsAlive && !bIsInLobbyMode && !bIsInEditorMode)
	{
		const FRotator CurRot = GetActorRotation();
		const FRotator TargetRot(0.0f, DesiredAimYaw, 0.0f);
		const FRotator InterpedRot = FMath::RInterpTo(CurRot, TargetRot, DeltaSeconds, 14.0f);
		SetActorRotation(FRotator(0.0f, InterpedRot.Yaw, 0.0f));
		if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurRot.Yaw, DesiredAimYaw)) < 0.4f)
		{
			// Keep tracking the cursor; PlayerController refreshes DesiredAimYaw each tick.
		}
	}

	WalkCyclePhase += DeltaSeconds * SpeedRatio * 11.0f;
	if (WalkCyclePhase > 2.0f * PI * 100.0f)
	{
		WalkCyclePhase = FMath::Fmod(WalkCyclePhase, 2.0f * PI);
	}

	// Detect stride trough zero-crossings for organic footstep sound & subtle runner dust puff
	const float CurStrideSin = FMath::Sin(WalkCyclePhase);
	if (PrevStrideSin < 0.0f && CurStrideSin >= 0.0f && SpeedRatio > 0.12f && bIsAlive)
	{
		UAetheriaMaterialLibrary::PlayGameSoundAtLocation(this, *FString::Printf(TEXT("SFX_Step_Stone_%02d"), FMath::RandRange(1, 4)), GetActorLocation(), 0.18f, FMath::FRandRange(0.95f, 1.12f));
		if (SpeedRatio > 0.55f)
		{
			if (UWorld* World = GetWorld())
			{
				AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation() - FVector(0.0f, 0.0f, 85.0f), 45.0f, FLinearColor(0.85f, 0.80f, 0.70f, 0.25f), 0.18f);
			}
		}
	}
	PrevStrideSin = CurStrideSin;

	const float TimeSec = GetWorld()->GetTimeSeconds();
	const float IdleWeight = (1.0f - SpeedRatio) * (bIsAlive ? 1.0f : 0.0f);

	// Calm solemn guardian poise & gentle breathing (GPU ClothWind handles cloak billow; avoid stiff chess-piece rocking)
	float DesiredLookYaw = FMath::Sin(TimeSec * 0.85f) * 1.2f * IdleWeight;
	float DesiredLookPitch = FMath::Sin(TimeSec * 1.65f) * 0.65f * IdleWeight;
	const float BreathRoll = FMath::Cos(TimeSec * 1.15f) * 0.35f * IdleWeight;
	const float BreathChestScale = 1.0f + FMath::Sin(TimeSec * 1.65f) * 0.008f * IdleWeight;

	if ((bIsInLobbyMode || bIsInEditorMode) && IsLocallyControlled())
	{
		if (const APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			float MouseX = 0.0f, MouseY = 0.0f;
			int32 ViewX = 1920, ViewY = 1080;
			PC->GetViewportSize(ViewX, ViewY);
			if (ViewX > 0 && ViewY > 0 && PC->GetMousePosition(MouseX, MouseY))
			{
				const float NormX = FMath::Clamp((MouseX / static_cast<float>(ViewX)) * 2.0f - 1.0f, -1.0f, 1.0f);
				const float NormY = FMath::Clamp((MouseY / static_cast<float>(ViewY)) * 2.0f - 1.0f, -1.0f, 1.0f);
				DesiredLookYaw += NormX * 5.5f;
				DesiredLookPitch += -NormY * 1.2f;
			}
		}
	}
	CurrentLookYawOffset = FMath::FInterpTo(CurrentLookYawOffset, DesiredLookYaw, DeltaSeconds, 6.0f);
	CurrentLookPitchOffset = FMath::FInterpTo(CurrentLookPitchOffset, DesiredLookPitch, DeltaSeconds, 6.0f);

	// 3-Point Diablo IV Sanctuary Hero Lighting (Warm Brazier Key + Cool Rim + Soft Fill) in Lobby/Editor
	if (bIsInLobbyMode || bIsInEditorMode)
	{
		if (StaffLight)
		{
			StaffLight->SetVisibility(true);
			StaffLight->SetRelativeLocation(FVector(155.0f, -105.0f, 95.0f));
			StaffLight->SetLightColor(FLinearColor(1.0f, 0.66f, 0.34f));
			StaffLight->SetAttenuationRadius(650.0f);
			StaffLight->SetIntensity(5200.0f);
		}
		if (HeroRimLight)
		{
			HeroRimLight->SetVisibility(true);
			HeroRimLight->SetRelativeLocation(FVector(-135.0f, 120.0f, 125.0f));
			HeroRimLight->SetAttenuationRadius(520.0f);
			HeroRimLight->SetIntensity(3800.0f);
		}
		if (FamiliarLight)
		{
			FamiliarLight->SetVisibility(true);
			FamiliarLight->SetRelativeLocation(FVector(95.0f, 115.0f, 52.0f));
			FamiliarLight->SetLightColor(FLinearColor(0.58f, 0.78f, 1.0f));
			FamiliarLight->SetAttenuationRadius(480.0f);
			FamiliarLight->SetIntensity(1900.0f);
		}
	}
	else
	{
		if (StaffLight)
		{
			StaffLight->SetRelativeLocation(FVector::ZeroVector);
			StaffLight->SetLightColor(AetheriaMindColors::GetRuneGlowColor(Customization.RuneGlowIndex));
			StaffLight->SetAttenuationRadius(320.0f);
			StaffLight->SetIntensity(750.0f);
		}
		if (HeroRimLight)
		{
			HeroRimLight->SetRelativeLocation(FVector(0.0f, 0.0f, 30.0f));
			HeroRimLight->SetAttenuationRadius(350.0f);
			HeroRimLight->SetIntensity(800.0f);
		}
		if (FamiliarLight)
		{
			FamiliarLight->SetRelativeLocation(FVector::ZeroVector);
			FamiliarLight->SetAttenuationRadius(250.0f);
			FamiliarLight->SetIntensity(600.0f);
		}
	}

	const float StepBob = FMath::Abs(FMath::Sin(WalkCyclePhase)) * 4.5f * SpeedRatio;
	const float ForwardTilt = -2.4f * SpeedRatio;
	const float HipSway = FMath::Sin(WalkCyclePhase) * 0.65f * SpeedRatio;

	// Critically damped secondary follow-through for robes, cosmetics and familiars
	ClothFollowPitch = FMath::FInterpTo(ClothFollowPitch, ForwardTilt * 1.35f + InertiaPitchLean, DeltaSeconds, 7.5f);
	ClothFollowRoll = FMath::FInterpTo(ClothFollowRoll, CurrentBankRoll * 1.4f + HipSway * 1.1f, DeltaSeconds, 7.5f);
	FamiliarBobVel = FMath::FInterpTo(FamiliarBobVel, (SpeedRatio * 5.0f) * FMath::Cos(TimeSec * 3.2f), DeltaSeconds, 6.0f);

	float HitRecoilPitch = 0.0f;
	float HitRecoilBack = 0.0f;
	if (HitRecoilTimer > 0.0f && bIsAlive)
	{
		const float RP = FMath::Sin(FMath::Clamp(HitRecoilTimer / 0.16f, 0.0f, 1.0f) * PI);
		HitRecoilPitch = 11.0f * RP;
		HitRecoilBack = -9.0f * RP;
	}

	// 3-Phase Cast Choreography with Spell-Specific Branching (Overhead Cataclysm, Braced Beam, Levitating Surge, Snappy Bolt)
	float CastPitch = 0.0f;
	float CastLunge = 0.0f;
	float StaffThrust = 0.0f;
	float StaffLift = 0.0f;
	if (CastAnimTimer > 0.0f && bIsAlive)
	{
		const float PhaseT = 1.0f - FMath::Clamp(CastAnimTimer / 0.46f, 0.0f, 1.0f);

		const bool bCataclysm = (ActiveCastAnimSpell == EAetheriaSpellId::ChaosMeteor ||
			ActiveCastAnimSpell == EAetheriaSpellId::GravityCataclysm ||
			ActiveCastAnimSpell == EAetheriaSpellId::BlackHole);
		const bool bBracedBeam = (ActiveCastAnimSpell == EAetheriaSpellId::SupernovaBeam);
		const bool bSurge = (ActiveCastAnimSpell == EAetheriaSpellId::PrismWall ||
			ActiveCastAnimSpell == EAetheriaSpellId::Chronosphere ||
			ActiveCastAnimSpell == EAetheriaSpellId::PrimalOverload);

		if (bCataclysm)
		{
			// Overhead Cataclysm: immense rear-back, staff held high overhead, violent downward slam
			if (PhaseT < 0.35f)
			{
				const float W = PhaseT / 0.35f;
				CastPitch = FMath::Lerp(0.0f, 18.0f, W);
				CastLunge = FMath::Lerp(0.0f, -12.0f, W);
				StaffThrust = FMath::Lerp(0.0f, -16.0f, W);
				StaffLift = FMath::Lerp(0.0f, 44.0f, W);
			}
			else if (PhaseT < 0.65f)
			{
				const float S = (PhaseT - 0.35f) / 0.30f;
				const float Snap = FMath::Sin(S * PI * 0.5f);
				CastPitch = FMath::Lerp(18.0f, -22.0f, Snap);
				CastLunge = FMath::Lerp(-12.0f, 26.0f, Snap);
				StaffThrust = FMath::Lerp(-16.0f, 42.0f, Snap);
				StaffLift = FMath::Lerp(44.0f, -12.0f, Snap);
			}
			else
			{
				const float R = (PhaseT - 0.65f) / 0.35f;
				const float Ease = 1.0f - FMath::Pow(1.0f - R, 2.0f);
				CastPitch = FMath::Lerp(-22.0f, 0.0f, Ease);
				CastLunge = FMath::Lerp(26.0f, 0.0f, Ease);
				StaffThrust = FMath::Lerp(42.0f, 0.0f, Ease);
				StaffLift = FMath::Lerp(-12.0f, 0.0f, Ease);
			}
		}
		else if (bBracedBeam)
		{
			// Braced Recoil Tremor: deep braced low stance, high frequency channel vibration
			const float Jitter = FMath::Sin(TimeSec * 48.0f) * 2.8f;
			CastPitch = -8.0f + Jitter * 0.4f;
			CastLunge = -6.0f + Jitter * 0.5f;
			StaffThrust = 34.0f + Jitter;
			StaffLift = 6.0f;
		}
		else if (bSurge)
		{
			// Sweeping/Levitating Surge: floating upward elevation, sweeping outward gesture
			const float SurgeCurve = FMath::Sin(PhaseT * PI);
			CastPitch = -12.0f * SurgeCurve;
			CastLunge = 12.0f * SurgeCurve;
			StaffThrust = 26.0f * SurgeCurve;
			StaffLift = 22.0f * SurgeCurve;
		}
		else
		{
			// Standard 3-Phase Windup / Snap / Recover
			if (PhaseT < 0.24f)
			{
				const float W = PhaseT / 0.24f;
				CastPitch = FMath::Lerp(0.0f, 10.0f, W);
				CastLunge = FMath::Lerp(0.0f, -8.0f, W);
				StaffThrust = FMath::Lerp(0.0f, -14.0f, W);
				StaffLift = FMath::Lerp(0.0f, 16.0f, W);
			}
			else if (PhaseT < 0.56f)
			{
				const float S = (PhaseT - 0.24f) / 0.32f;
				const float SnapCurve = FMath::Sin(S * PI * 0.5f);
				CastPitch = FMath::Lerp(10.0f, -16.0f, SnapCurve);
				CastLunge = FMath::Lerp(-8.0f, 24.0f, SnapCurve);
				StaffThrust = FMath::Lerp(-14.0f, 38.0f, SnapCurve);
				StaffLift = FMath::Lerp(16.0f, 4.0f, SnapCurve);
			}
			else
			{
				const float R = (PhaseT - 0.56f) / 0.44f;
				const float EaseOut = 1.0f - FMath::Pow(1.0f - R, 2.0f);
				CastPitch = FMath::Lerp(-16.0f, 0.0f, EaseOut);
				CastLunge = FMath::Lerp(24.0f, 0.0f, EaseOut);
				StaffThrust = FMath::Lerp(38.0f, 0.0f, EaseOut);
				StaffLift = FMath::Lerp(4.0f, 0.0f, EaseOut);
			}
		}

		if (CastSigilMesh)
		{
			CastSigilMesh->SetVisibility(true);
			const float SigilScale = FMath::Lerp(1.35f, 2.45f, FMath::Sin(PhaseT * PI));
			CastSigilMesh->SetRelativeScale3D(FVector(SigilScale, SigilScale, 1.0f));
			CastSigilMesh->SetRelativeRotation(FRotator(0.0f, TimeSec * 260.0f, 0.0f));
		}
	}
	else if (CastSigilMesh)
	{
		CastSigilMesh->SetVisibility(false);
	}

	float DashStretch = 1.0f;
	float DashTilt = 0.0f;
	if (DashAnimTimer > 0.0f && bIsAlive)
	{
		const float DashNorm = FMath::Clamp(DashAnimTimer / 0.24f, 0.0f, 1.0f);
		const float DashCurve = FMath::Sin(DashNorm * PI);
		DashStretch = 1.0f + 0.22f * DashCurve;
		DashTilt = -16.0f * DashCurve;
	}

	// Downed / Fallen Co-op Pose
	const float DownedPitch = -56.0f * DownedAnimAlpha;
	const float DownedRoll = 14.0f * DownedAnimAlpha;
	const float DownedZ = -34.0f * DownedAnimAlpha;

	const float BaseScale = FMath::Clamp(Customization.BodyScale, 0.85f, 1.25f) * 1.08f;
	const FVector CloakLoc(CastLunge + HitRecoilBack, 0.0f, -88.0f + StepBob + DownedZ);
	const FRotator CloakRot(ForwardTilt + CurrentLookPitchOffset + CastPitch + DashTilt + HitRecoilPitch + DownedPitch, CurrentLookYawOffset, HipSway + BreathRoll + CurrentBankRoll + DownedRoll);

	if (CloakMesh)
	{
		CloakMesh->SetRelativeLocation(CloakLoc);
		CloakMesh->SetRelativeRotation(CloakRot);
		CloakMesh->SetRelativeScale3D(FVector(BaseScale * DashStretch * BreathChestScale, BaseScale * BreathChestScale, BaseScale / FMath::Sqrt(DashStretch)));
	}

	if (MantleMesh && MantleMesh->IsVisible())
	{
		MantleMesh->SetRelativeRotation(FRotator(ClothFollowPitch * 0.8f, 0.0f, ClothFollowRoll * 0.8f));
	}

	if (HeadCosmeticMesh && HeadCosmeticMesh->IsVisible())
	{
		const FVector CrownOffset = CloakRot.Quaternion().RotateVector(FVector(0.0f, 0.0f, 134.0f * BaseScale));
		HeadCosmeticMesh->SetRelativeLocation(CloakLoc + CrownOffset);
		HeadCosmeticMesh->SetRelativeRotation(CloakRot);
		HeadCosmeticMesh->SetRelativeScale3D(FVector(0.74f * BaseScale));
	}

	if (BackCosmeticMesh && BackCosmeticMesh->IsVisible())
	{
		BackCosmeticMesh->SetRelativeRotation(FRotator(ClothFollowPitch * 1.25f, 0.0f, ClothFollowRoll * 1.15f));
	}

	if (StaffMesh && StaffMesh->IsVisible() && !bStaffAttachedToHand)
	{
		if (bUsingCustomSculptedHero)
		{
			if (Customization.StaffIndex <= 0)
			{
				StaffMesh->SetVisibility(false);
			}
			else
			{
				// Holster StaffMesh strapped snugly across the upper back instead of levitating upright next to character's boots
				const float LevitateBob = FMath::Sin(TimeSec * 2.4f) * 1.4f;
				const float CastSurgeX = StaffThrust * 0.25f;
				const float CastSurgeZ = StaffLift * 0.35f;
				const FVector BackHolsterLoc(-18.0f + CastSurgeX, 8.0f, 24.0f + StepBob * 0.35f + LevitateBob + CastSurgeZ);
				const FRotator BackHolsterRot(18.0f + CastPitch * 0.4f, 12.0f, 38.0f + CurrentBankRoll * 0.35f);
				StaffMesh->SetRelativeLocation(BackHolsterLoc);
				StaffMesh->SetRelativeRotation(BackHolsterRot);
			}
		}
		else
		{
			const float StaffDownZ = -26.0f * DownedAnimAlpha;
			const float StaffDownPitch = -64.0f * DownedAnimAlpha;
			StaffMesh->SetRelativeLocation(FVector(36.0f + StaffThrust + HitRecoilBack, 36.0f, -68.0f + StepBob + StaffLift + StaffDownZ));
			StaffMesh->SetRelativeRotation(FRotator(CastPitch * 1.6f + HitRecoilPitch + StaffDownPitch, 0.0f, CurrentBankRoll * 0.5f + DownedRoll));
		}
	}

	// 5. Familiar & Grimoire orbital follow (driven smoothly by locomotion, secondary cloth & cast recoil)
	if (FloatingGrimoireMesh && FloatingGrimoireMesh->IsVisible())
	{
		const FVector BaseOffset(40.0f + CastLunge * 0.5f, -48.0f, 30.0f + DownedZ * 0.8f + FamiliarBobVel);
		const float StrideFollow = FMath::Sin(WalkCyclePhase) * 3.5f * SpeedRatio;
		FloatingGrimoireMesh->SetRelativeLocation(BaseOffset + FVector(0.0f, 0.0f, StepBob * 0.7f + StrideFollow));
		FloatingGrimoireMesh->SetRelativeRotation(FRotator(ClothFollowPitch * 0.7f + CastPitch * 0.5f, 0.0f, 10.0f * (1.0f - DownedAnimAlpha) + ClothFollowRoll * 0.75f));
	}

	if (FamiliarSkullMesh && FamiliarSkullMesh->IsVisible())
	{
		const float SkullOrbit = TimeSec * (0.9f + SpeedRatio * 1.8f);
		const FVector BaseOffset(-35.0f + FMath::Cos(SkullOrbit) * 8.0f, -40.0f + FMath::Sin(SkullOrbit) * 8.0f, 55.0f + DownedZ + FamiliarBobVel * 0.6f);
		FamiliarSkullMesh->SetRelativeLocation(BaseOffset + FVector(0.0f, 0.0f, StepBob * 0.6f));
		FamiliarSkullMesh->SetRelativeRotation(FRotator(ClothFollowPitch * 0.6f + DownedPitch * 0.4f, FMath::Sin(SkullOrbit * 0.7f) * 18.0f, ClothFollowRoll * 0.7f));
	}

	// 6. Server-side Mana regeneration and passive state
	if (HasAuthority() && bIsAlive)
	{
		Mana = FMath::Clamp(Mana + 3.2f * DeltaSeconds, 0.0f, MaxMana);

		int32 LocIdx = 0;
		AAetheriaGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAetheriaGameState>() : nullptr;
		if (GS)
		{
			LocIdx = GS->CurrentLocationIndex;
		}

		const FVector WizardLoc = GetActorLocation();
		bOnHighGround = UAetheriaBiomeDatabase::IsOnHighGround(LocIdx, WizardLoc);
		bInSunkenTrench = UAetheriaBiomeDatabase::IsInSunkenTrench(LocIdx, WizardLoc);

		// Magicka physical elemental states on the wizard (Wet extinguishes Fire; Water trench wets)
		if (bInSunkenTrench)
		{
			bIsWet = true;
			WetTimer = 2.5f;
			WizardBurnTimer = 0.0f;
			WizardBurnTickAccum = 0.0f;
			bIsBurning = false;
		}
		else if (WetTimer > 0.0f)
		{
			WetTimer = FMath::Max(0.0f, WetTimer - DeltaSeconds);
			bIsWet = (WetTimer > 0.0f);
		}
		else
		{
			bIsWet = false;
		}

		if (WizardBurnTimer > 0.0f && !bIsWet)
		{
			WizardBurnTimer = FMath::Max(0.0f, WizardBurnTimer - DeltaSeconds);
			bIsBurning = (WizardBurnTimer > 0.0f);
			WizardBurnTickAccum += DeltaSeconds;
			if (WizardBurnTickAccum >= 0.5f)
			{
				WizardBurnTickAccum -= 0.5f;
				ApplyDamageToWizard(WizardBurnDPS * 0.5f);
			}
		}
		else
		{
			bIsBurning = false;
			WizardBurnTickAccum = 0.0f;
		}

		const float HasteMult = (HasteBuffTimer > 0.0f) ? 1.25f : 1.0f;
		const float NewSpeed = 660.0f * (bInChronosphereBuff ? 1.4f : 1.0f) * HasteMult * FMath::Max(0.45f, 1.0f - 0.08f * ColdStacks) * (bInSunkenTrench && !bInChronosphereBuff ? 0.82f : 1.0f);
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->MaxWalkSpeed = NewSpeed;
		}

		const FVector2D Cur2D(WizardLoc.X, WizardLoc.Y);
		const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 42.0f;
		const FVector2D Resolved2D = UAetheriaBiomeDatabase::ResolveCoverAvoidance2D(LocIdx, Cur2D, CapsuleRadius);
		if (FVector2D::DistSquared(Cur2D, Resolved2D) > 1.0f)
		{
			SetActorLocation(FVector(Resolved2D.X, Resolved2D.Y, WizardLoc.Z), true);
		}

		if (LocIdx == 2 && bOnHighGround)
		{
			ColdStacks = 0;
		}

		if (UAetheriaBiomeDatabase::IsInSanctuary(LocIdx, WizardLoc) && (!GS || GS->CurrentAct < 5))
		{
			HP = FMath::Clamp(HP + 4.0f * DeltaSeconds, 0.0f, MaxHP);
		}
	}

	// 7. Update hollow arcane floor shield ring visual state
	if (ShieldMesh)
	{
		ShieldMesh->SetVisibility(Shield > 0.0f);
		if (Shield > 0.0f)
		{
			const float TNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
			const float Pulse = 1.0f + 0.05f * FMath::Sin(TNow * 4.0f);
			ShieldMesh->SetRelativeScale3D(FVector(2.35f * Pulse, 2.35f * Pulse, 1.0f));
			ShieldMesh->SetRelativeRotation(FRotator(0.0f, TNow * 48.0f, 0.0f));
		}
	}
}

void AAetheriaWizardCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAetheriaWizardCharacter, Customization);
	DOREPLIFETIME(AAetheriaWizardCharacter, ActiveOrbs);
	DOREPLIFETIME(AAetheriaWizardCharacter, SlotD);
	DOREPLIFETIME(AAetheriaWizardCharacter, SlotF);
	DOREPLIFETIME(AAetheriaWizardCharacter, SlotDCooldownRemaining);
	DOREPLIFETIME(AAetheriaWizardCharacter, SlotFCooldownRemaining);
	DOREPLIFETIME(AAetheriaWizardCharacter, SpellCooldowns);
	DOREPLIFETIME(AAetheriaWizardCharacter, HP);
	DOREPLIFETIME(AAetheriaWizardCharacter, MaxHP);
	DOREPLIFETIME(AAetheriaWizardCharacter, Mana);
	DOREPLIFETIME(AAetheriaWizardCharacter, MaxMana);
	DOREPLIFETIME(AAetheriaWizardCharacter, Shield);
	DOREPLIFETIME(AAetheriaWizardCharacter, DashCooldownRemaining);
	DOREPLIFETIME(AAetheriaWizardCharacter, PartyColorIndex);
	DOREPLIFETIME(AAetheriaWizardCharacter, bIsAlive);
	DOREPLIFETIME(AAetheriaWizardCharacter, ColdStacks);
	DOREPLIFETIME(AAetheriaWizardCharacter, bInChronosphereBuff);
	DOREPLIFETIME(AAetheriaWizardCharacter, bOnHighGround);
	DOREPLIFETIME(AAetheriaWizardCharacter, Kills);
	DOREPLIFETIME(AAetheriaWizardCharacter, DamageDealt);
	DOREPLIFETIME(AAetheriaWizardCharacter, WizardName);
	DOREPLIFETIME(AAetheriaWizardCharacter, WizardLevel);
	DOREPLIFETIME(AAetheriaWizardCharacter, CurrentXP);
	DOREPLIFETIME(AAetheriaWizardCharacter, ArcaneEssence);
	DOREPLIFETIME(AAetheriaWizardCharacter, RelicIgnisBonus);
	DOREPLIFETIME(AAetheriaWizardCharacter, RelicChronoCDR);
	DOREPLIFETIME(AAetheriaWizardCharacter, RelicBlackHoleRadiusBonus);
	DOREPLIFETIME(AAetheriaWizardCharacter, RelicPrismSplitBonus);
	DOREPLIFETIME(AAetheriaWizardCharacter, RelicAllElementBonus);
	DOREPLIFETIME(AAetheriaWizardCharacter, EssenceEmpowerRank);
	DOREPLIFETIME(AAetheriaWizardCharacter, HealingFlaskCharges);
	DOREPLIFETIME(AAetheriaWizardCharacter, MaxHealingFlaskCharges);
	DOREPLIFETIME(AAetheriaWizardCharacter, FlaskCooldownRemaining);
	DOREPLIFETIME(AAetheriaWizardCharacter, bIsWet);
	DOREPLIFETIME(AAetheriaWizardCharacter, bIsBurning);
}

UStaticMeshComponent* AAetheriaWizardCharacter::GetOrbMesh(int32 Index) const
{
	switch (Index)
	{
	case 0: return OrbMesh0;
	case 1: return OrbMesh1;
	case 2: return OrbMesh2;
	default: return nullptr;
	}
}

UPointLightComponent* AAetheriaWizardCharacter::GetOrbLight(int32 Index) const
{
	switch (Index)
	{
	case 0: return OrbLight0;
	case 1: return OrbLight1;
	case 2: return OrbLight2;
	default: return nullptr;
	}
}

void AAetheriaWizardCharacter::OnRep_ActiveOrbs()
{
	UpdateOrbVisuals();
	if (IsLocallyControlled() && ActiveOrbs.Num() > 0)
	{
		const EAetheriaOrb Newest = ActiveOrbs.Last();
		const FString SfxName = (Newest == EAetheriaOrb::Ignis) ? TEXT("SFX_OrbQ")
			: (Newest == EAetheriaOrb::Tempest) ? TEXT("SFX_OrbW") : TEXT("SFX_OrbE");
		UAetheriaMaterialLibrary::PlayGameSound2D(this, SfxName, 0.52f);
	}
}

void AAetheriaWizardCharacter::OnRep_PartyColorIndex()
{
	SetupDynamicMaterials();
}

void AAetheriaWizardCharacter::UpdateOrbVisuals()
{
	for (int32 i = 0; i < 3; ++i)
	{
		if (ActiveOrbs.IsValidIndex(i))
		{
			const EAetheriaOrb Orb = ActiveOrbs[i];
			const FLinearColor Color = AetheriaMindColors::GetOrbColor(Orb);

			if (UPointLightComponent* Light = GetOrbLight(i))
			{
				Light->SetLightColor(Color);
			}

			if (DynamicOrbMaterials[i])
			{
				DynamicOrbMaterials[i]->SetVectorParameterValue(FName("BaseColor"), Color);
				DynamicOrbMaterials[i]->SetVectorParameterValue(FName("EmissiveColor"), Color);
			}
		}
	}
}

void AAetheriaWizardCharacter::UpdateShieldVisuals()
{
	ShieldMesh->SetVisibility(Shield > 0.0f);
}

void AAetheriaWizardCharacter::ServerPushOrb_Implementation(EAetheriaOrb Orb)
{
	if (!bIsAlive) return;

	if (ActiveOrbs.Num() >= 3)
	{
		ActiveOrbs.RemoveAt(0);
	}
	ActiveOrbs.Add(Orb);

	UpdateOrbVisuals();

	if (IsLocallyControlled())
	{
		const FString SfxName = (Orb == EAetheriaOrb::Ignis) ? TEXT("SFX_OrbQ")
			: (Orb == EAetheriaOrb::Tempest) ? TEXT("SFX_OrbW") : TEXT("SFX_OrbE");
		UAetheriaMaterialLibrary::PlayGameSound2D(this, SfxName, 0.52f);
	}
}

void AAetheriaWizardCharacter::ServerInvoke_Implementation(bool bAutoCast, FVector AimPoint)
{
	if (!bIsAlive) return;

	const FAetheriaSpellDef Resolved = UAetheriaSpellDatabase::ResolveSpellFromOrbs(ActiveOrbs);
	if (Resolved.SpellId == EAetheriaSpellId::None) return;

	if (SlotD != Resolved.SpellId)
	{
		SlotF = SlotD;
		SlotD = Resolved.SpellId;
		SlotFCooldownRemaining = GetSpellCooldownRemaining(SlotF);
		SlotDCooldownRemaining = GetSpellCooldownRemaining(SlotD);
	}

	MulticastPlayInvokeFX();

	if (bAutoCast)
	{
		ServerCastSlot_Implementation(0, AimPoint);
	}
}

void AAetheriaWizardCharacter::ServerCastSlot_Implementation(int32 SlotIndex, FVector AimPoint)
{
	if (!bIsAlive) return;

	const EAetheriaSpellId SpellToCast = (SlotIndex == 0) ? SlotD : SlotF;
	if (SpellToCast == EAetheriaSpellId::None || !IsSpellOffCooldown(SpellToCast)) return;

	const FAetheriaSpellDef Def = UAetheriaSpellDatabase::GetSpellDef(SpellToCast);
	if (Mana < Def.ManaCost) return;

	Mana -= Def.ManaCost;
	const int32 SpellIdx = static_cast<int32>(SpellToCast);
	if (SpellCooldowns.IsValidIndex(SpellIdx))
	{
		SpellCooldowns[SpellIdx] = GetEffectiveSpellCooldown(SpellToCast, Def.Cooldown);
	}
	SlotDCooldownRemaining = GetSpellCooldownRemaining(SlotD);
	SlotFCooldownRemaining = GetSpellCooldownRemaining(SlotF);

	FaceWorldPoint(AimPoint);
	CastAnimTimer = 0.46f;
	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::CastPrimary));
	ExecuteSpellServer(SpellToCast, AimPoint);
}

void AAetheriaWizardCharacter::ServerInstantCastCombo_Implementation(FVector AimPoint)
{
	if (!bIsAlive) return;

	const FAetheriaSpellDef Resolved = UAetheriaSpellDatabase::ResolveSpellFromOrbs(ActiveOrbs);
	if (Resolved.SpellId == EAetheriaSpellId::None || !IsSpellOffCooldown(Resolved.SpellId)) return;

	if (Mana < Resolved.ManaCost) return;

	Mana -= Resolved.ManaCost;
	if (SlotD != Resolved.SpellId)
	{
		SlotF = SlotD;
		SlotD = Resolved.SpellId;
	}
	const int32 SpellIdx = static_cast<int32>(Resolved.SpellId);
	if (SpellCooldowns.IsValidIndex(SpellIdx))
	{
		SpellCooldowns[SpellIdx] = GetEffectiveSpellCooldown(Resolved.SpellId, Resolved.Cooldown);
	}
	SlotDCooldownRemaining = GetSpellCooldownRemaining(SlotD);
	SlotFCooldownRemaining = GetSpellCooldownRemaining(SlotF);

	FaceWorldPoint(AimPoint);
	CastAnimTimer = 0.46f;
	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::CastPrimary));
	ExecuteSpellServer(Resolved.SpellId, AimPoint);
}

void AAetheriaWizardCharacter::ServerBasicAttack_Implementation(FVector AimPoint)
{
	if (!bIsAlive) return;

	FVector ClampedAimPoint = AimPoint;
	ClampedAimPoint.Z = FMath::Clamp(ClampedAimPoint.Z, -140.0f, 360.0f);

	FaceWorldPoint(ClampedAimPoint);
	CastAnimTimer = 0.30f;
	const FVector MuzzleLoc = GetSpellMuzzleLocation();
	FVector Dir = (ClampedAimPoint - MuzzleLoc);
	Dir.Z = 0.0f;
	Dir.Normalize();
	if (Dir.IsNearlyZero())
	{
		Dir = GetActorForwardVector();
	}

	const float HighGroundMult = bOnHighGround ? 1.15f : 1.0f;
	const float BasicDamage = 48.0f * HighGroundMult;

	if (UWorld* World = GetWorld())
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;

		if (AAetheriaSpellProjectile* Bolt = World->SpawnActor<AAetheriaSpellProjectile>(MuzzleLoc, Dir.Rotation(), SpawnParams))
		{
			Bolt->InitializeProjectile(
				EAetheriaSpellId::None,
				this,
				Dir,
				BasicDamage,
				2800.0f,
				65.0f,
				AetheriaMindColors::MindCyan,
				AetheriaMindColors::MindViolet);
		}
	}

	MulticastPlaySpellFX(EAetheriaSpellId::None, MuzzleLoc, MuzzleLoc + Dir * 2500.0f, {});
}

void AAetheriaWizardCharacter::ServerDash_Implementation(FVector AimPoint)
{
	if (!bIsAlive || DashCooldownRemaining > 0.0f) return;

	DashCooldownRemaining = 2.4f;
	DashInvulnRemaining = 0.28f;
	DashAnimTimer = 0.24f;
	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::Dash));

	FVector DashDir = (AimPoint - GetActorLocation());
	DashDir.Z = 0.0f;
	if (!DashDir.Normalize())
	{
		DashDir = GetActorForwardVector();
	}

	SetActorRotation(FRotator(0.0f, DashDir.Rotation().Yaw, 0.0f));

	const FVector StartLoc = GetActorLocation();
	const FVector TargetLoc = StartLoc + DashDir * 750.0f;

	bGlideDashing = true;
	GlideDashElapsed = 0.0f;
	GlideDashDuration = 0.22f;
	GlideDashVelocity = DashDir * (750.0f / GlideDashDuration);
	DashGhostStep = 0;

	AddCameraTrauma(0.16f);
	MulticastPlayDashFX(StartLoc, TargetLoc, DashDir);
}

void AAetheriaWizardCharacter::MulticastPlayDashFX_Implementation(FVector StartPos, FVector EndPos, FVector DashDir)
{
	DashAnimTimer = 0.24f;
	if (!DashDir.IsNearlyZero())
	{
		SetActorRotation(FRotator(0.0f, DashDir.Rotation().Yaw, 0.0f));
	}

	if (!HasAuthority())
	{
		bGlideDashing = true;
		GlideDashElapsed = 0.0f;
		GlideDashDuration = 0.22f;
		GlideDashVelocity = DashDir * (750.0f / GlideDashDuration);
		DashGhostStep = 0;
	}

	UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_Dash"), 0.85f);
	SpawnAfterimageGhost(StartPos, GetActorRotation(), 0.85f);

	if (UWorld* World = GetWorld())
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, StartPos - FVector(0.0f, 0.0f, 82.0f), 340.0f, AetheriaMindColors::MindCyan, 0.36f);
	}
}

FVector AAetheriaWizardCharacter::GetSpellMuzzleLocation() const
{
	if (StaffTip && bStaffAttachedToHand)
	{
		return StaffTip->GetComponentLocation();
	}
	if (StaffMesh && StaffMesh->IsVisible())
	{
		return StaffMesh->GetComponentTransform().TransformPosition(FVector(0.0f, 0.0f, 110.0f));
	}
	return GetActorLocation() + GetActorForwardVector() * 58.0f + FVector(0.0f, 0.0f, 46.0f);
}

void AAetheriaWizardCharacter::AttachHeroStaff()
{
	if (!StaffMesh)
	{
		return;
	}
	static const TCHAR* Candidates[] = {
		TEXT("/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Lunar_Wizard_Staff.SM_Staff_Lunar_Wizard_Staff"),
		TEXT("/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Crystalline_Conduit_Staff.SM_Staff_Crystalline_Conduit_Staff"),
		TEXT("/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Winged_Golden_Staff.SM_Staff_Winged_Golden_Staff"),
		TEXT("/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Forgotten_Relic_Staff.SM_Staff_Forgotten_Relic_Staff")
	};
	UStaticMesh* StaffSM = nullptr;
	for (const TCHAR* P : Candidates)
	{
		StaffSM = LoadObject<UStaticMesh>(nullptr, P, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (StaffSM) break;
	}
	if (!StaffSM)
	{
		return;
	}
	StaffMesh->SetStaticMesh(StaffSM);
	StaffMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StaffMesh->SetCastShadow(true);
	StaffMesh->SetVisibility(true);
	StaffMesh->SetHiddenInGame(false);
	StaffMesh->SetRelativeScale3D(FVector(0.55f));

	USkeletalMeshComponent* Skel = GetMesh();
	FName Socket = NAME_None;
	if (Skel)
	{
		// Grip on the right-hand BONE. Paragon Gideon has no weapon_r, and
		// Muzzle_01 is rotated 180 deg about the knuckle axis and sits past the
		// fingertips, which left the staff sticking out of the palm.
		static const FName Names[] = {
			FName(TEXT("weapon_r")), FName(TEXT("hand_r")), FName(TEXT("Hand_R")),
			FName(TEXT("ik_hand_gun")), FName(TEXT("VB_weapon_r"))
		};
		for (const FName& N : Names)
		{
			if (Skel->DoesSocketExist(N)) { Socket = N; break; }
		}
	}
	if (Skel && Socket != NAME_None)
	{
		// hand_r local frame (UE / Paragon right-side convention, measured from
		// Gideon's bind pose): -X = wrist -> knuckles, +Z = knuckle line
		// (pinky -> index/thumb side), Y = palm normal. A closed fist wraps the
		// shaft along the knuckle line, so the staff's +Z (gem end) maps onto
		// hand +Z and the grip point sits in the centre of the fist.
		float StaffScale = 0.80f;
		FVector GripInHand(-9.0f, 2.5f, 0.0f);
		// Tilt -65 deg about the palm normal: shaft stands mostly upright
		// in idle/run (hand_r +X is forearm-up there) with the head above the fist.
		FRotator RelRot(-65.0f, 0.0f, 0.0f);
		float GripFromTop = 0.45f;
		// Dev overrides for tuning without recompiling:
		//   -StaffGrip=x,y,z  -StaffRot=pitch,yaw,roll  -StaffScale=s  -StaffGripFromTop=f
		{
			FString V;
			TArray<FString> P;
			if (FParse::Value(FCommandLine::Get(), TEXT("StaffGrip="), V, false))
			{
				V.ParseIntoArray(P, TEXT(","));
				if (P.Num() == 3) { GripInHand = FVector(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])); }
			}
			if (FParse::Value(FCommandLine::Get(), TEXT("StaffRot="), V, false))
			{
				P.Reset();
				V.ParseIntoArray(P, TEXT(","));
				if (P.Num() == 3) { RelRot = FRotator(FCString::Atof(*P[0]), FCString::Atof(*P[1]), FCString::Atof(*P[2])); }
			}
			FParse::Value(FCommandLine::Get(), TEXT("StaffScale="), StaffScale);
			FParse::Value(FCommandLine::Get(), TEXT("StaffGripFromTop="), GripFromTop);
		}
		FVector RelLoc = GripInHand;
		if (UStaticMesh* BoundMesh = StaffMesh->GetStaticMesh())
		{
			const FBox Box = BoundMesh->GetBoundingBox();
			const float Len = FMath::Max(1.0f, Box.Max.Z - Box.Min.Z);
			const float GripZ = Box.Max.Z - Len * GripFromTop;
			RelLoc = GripInHand - RelRot.RotateVector(FVector(0.0f, 0.0f, GripZ * StaffScale));
		}
		StaffMesh->AttachToComponent(Skel, FAttachmentTransformRules::SnapToTargetNotIncludingScale, Socket);
		StaffMesh->SetRelativeRotation(RelRot);
		StaffMesh->SetRelativeLocation(RelLoc);
		StaffMesh->SetRelativeScale3D(FVector(StaffScale));
		bStaffAttachedToHand = true;
		UE_LOG(LogTemp, Display, TEXT("[Aetheria] Staff gripped on %s grip=%s rot=%s scale=%.2f"), *Socket.ToString(), *GripInHand.ToCompactString(), *RelRot.ToCompactString(), StaffScale);
	}
	else
	{
		StaffMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
		StaffMesh->SetRelativeLocation(FVector(36.0f, 36.0f, -20.0f));
		StaffMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 0.0f));
		bStaffAttachedToHand = false;
	}

	if (!StaffTip)
	{
		StaffTip = NewObject<USceneComponent>(this, TEXT("StaffTip"));
		StaffTip->SetupAttachment(StaffMesh);
		StaffTip->RegisterComponent();
	}
	else if (StaffTip->GetAttachParent() != StaffMesh)
	{
		StaffTip->AttachToComponent(StaffMesh, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	}
	StaffTip->SetRelativeLocation(FVector(0.0f, 0.0f, 118.0f));
}

void AAetheriaWizardCharacter::PlaySpellCastAnimation(EAetheriaSpellId SpellId)
{
	USkeletalMeshComponent* Skel = GetMesh();
	if (!Skel) return;
	if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Skel->GetAnimInstance()))
	{
		Anim->PlayCastForSpell(SpellId);
	}
}

void AAetheriaWizardCharacter::PlayBossIntroFocus(AActor* Target, float Duration)
{
	if (!Target) return;
	BossFocusTarget = Target;
	BossFocusTimer = FMath::Max(0.8f, Duration);
}

void AAetheriaWizardCharacter::FaceWorldPoint(const FVector& TargetPoint)
{
	FVector Dir = TargetPoint - GetActorLocation();
	Dir.Z = 0.0f;
	if (Dir.Normalize())
	{
		DesiredAimYaw = Dir.Rotation().Yaw;
		bHasDesiredAimYaw = true;
		if (GetVelocity().Size2D() < 15.0f && !bGlideDashing)
		{
			SetActorRotation(FRotator(0.0f, DesiredAimYaw, 0.0f));
		}
	}
}

void AAetheriaWizardCharacter::SpawnAfterimageGhost(const FVector& WorldPos, const FRotator& WorldRot, float AlphaScale)
{
	UWorld* World = GetWorld();
	if (!World || !CloakMesh || !CloakMesh->GetStaticMesh()) return;

	FActorSpawnParameters P;
	P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AActor* Ghost = World->SpawnActor<AActor>(AActor::StaticClass(), WorldPos, WorldRot, P);
	if (!Ghost) return;

	Ghost->SetReplicates(false);
	Ghost->SetLifeSpan(0.26f);

	UStaticMeshComponent* GhostMesh = NewObject<UStaticMeshComponent>(Ghost, TEXT("DashGhostMesh"));
	GhostMesh->SetStaticMesh(CloakMesh->GetStaticMesh());
	GhostMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GhostMesh->SetCastShadow(false);
	const float BaseScale = FMath::Clamp(Customization.BodyScale, 0.85f, 1.25f) * 1.06f;
	GhostMesh->SetRelativeLocation(FVector(0.0f, 0.0f, -88.0f));
	GhostMesh->SetRelativeScale3D(FVector(BaseScale * 1.08f, BaseScale, BaseScale));
	Ghost->SetRootComponent(GhostMesh);
	GhostMesh->RegisterComponent();

	UMaterialInterface* HoloMatBase = UAetheriaMaterialLibrary::GetChronosphereDomeMaterial(this);
	if (!HoloMatBase) HoloMatBase = UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);
	if (UMaterialInstanceDynamic* GhostMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		Ghost, HoloMatBase, AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 2.4f * AlphaScale, 0.42f * AlphaScale))
	{
		for (int32 s = 0; s < GhostMesh->GetNumMaterials(); ++s)
		{
			GhostMesh->SetMaterial(s, GhostMat);
		}
	}
}

void AAetheriaWizardCharacter::SetLobbyCameraMode(bool bLobbyActive)
{
	bIsInLobbyMode = bLobbyActive;
	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		if (bLobbyActive)
		{
			// Lobby has no guaranteed walkable floor under the pawn (and the
			// hero was falling out of the courtyard into a blue sky).
			MoveComp->StopMovementImmediately();
			MoveComp->GravityScale = 0.0f;
			MoveComp->SetMovementMode(MOVE_Flying);
			MoveComp->Velocity = FVector::ZeroVector;
		}
		else
		{
			MoveComp->GravityScale = 1.0f;
			MoveComp->SetMovementMode(MOVE_Walking);
		}
	}
	if (bLobbyActive)
	{
		bHasDesiredAimYaw = false;
		SetActorRotation(FRotator(0.0f, 135.0f, 0.0f));
	}
}

void AAetheriaWizardCharacter::UpdateCameraOccluders(float DeltaSeconds)
{
	UWorld* World = GetWorld();
	const bool bActive = World && TopDownCameraComponent && IsLocallyControlled() && !bIsInLobbyMode && !bIsInEditorMode;
	if (!bActive)
	{
		for (TWeakObjectPtr<UPrimitiveComponent>& Old : CameraOccluders)
		{
			if (Old.IsValid()) { Old->SetRenderInMainPass(true); }
		}
		CameraOccluders.Reset();
		return;
	}
	OccluderScanTimer -= DeltaSeconds;
	if (OccluderScanTimer > 0.0f) return;
	OccluderScanTimer = 0.05f;

	const FVector CamLoc = TopDownCameraComponent->GetComponentLocation();
	const FVector CamRight = TopDownCameraComponent->GetRightVector();
	const FVector CamUp = TopDownCameraComponent->GetUpVector();
	const FVector Feet = GetActorLocation();
	const FVector Hero = Feet + FVector(0.0f, 0.0f, 40.0f);
	const FVector HeroHigh = Feet + FVector(0.0f, 0.0f, 80.0f);
	FCollisionQueryParams Q(SCENE_QUERY_STAT(AetheriaCamOcclusion), false, this);

	TArray<TWeakObjectPtr<UPrimitiveComponent>> NowHit;
	auto Consider = [&](UPrimitiveComponent* C, const FVector* ImpactPointOpt)
	{
		if (!C || !C->IsA<UStaticMeshComponent>()) return;
		AActor* Owner = C->GetOwner();
		if (!Owner || Owner == this || Owner->IsA<APawn>()) return;

		const FBoxSphereBounds& B = C->Bounds;
		const float MaxZ = B.Origin.Z + B.BoxExtent.Z;
		const float MinZ = B.Origin.Z - B.BoxExtent.Z;
		// Omit walk floors and low props/curbs.
		if (MaxZ <= Feet.Z + 65.0f || B.BoxExtent.Z < 45.0f) return;
		// Skip arena-wide ISM floors; keep columns, arches, and wall ISMs.
		if (C->IsA<UInstancedStaticMeshComponent>() && B.BoxExtent.X > 1800.0f && B.BoxExtent.Y > 1800.0f && MinZ < Feet.Z + 50.0f) return;
		if (B.BoxExtent.X > 1800.0f && B.BoxExtent.Y > 1800.0f && MinZ < Feet.Z + 50.0f && ImpactPointOpt && ImpactPointOpt->Z <= Feet.Z + 55.0f) return;

		NowHit.AddUnique(C);
	};

	const FCollisionShape SweepSphere = FCollisionShape::MakeSphere(115.0f);
	const ECollisionChannel Channels[] = { ECC_Visibility, ECC_Camera, ECC_WorldStatic };
	struct FSweepRay { FVector Start; FVector Target; };
	const FSweepRay Rays[] = {
		{ CamLoc, Hero },
		{ CamLoc + CamRight * 120.0f, Hero },
		{ CamLoc - CamRight * 120.0f, Hero },
		{ CamLoc + CamUp * 80.0f, Hero },
		{ CamLoc, HeroHigh }
	};

	for (const FSweepRay& Ray : Rays)
	{
		const FVector ToTarget = Ray.Target - Ray.Start;
		const float Dist = ToTarget.Size();
		const FVector SweepEnd = (Dist > 200.0f) ? (Ray.Target - (ToTarget / Dist) * 80.0f) : Ray.Target;
		for (ECollisionChannel Chan : Channels)
		{
			TArray<FHitResult> Hits;
			World->SweepMultiByChannel(Hits, Ray.Start, SweepEnd, FQuat::Identity, Chan, SweepSphere, Q);
			for (const FHitResult& H : Hits)
			{
				Consider(H.GetComponent(), &H.ImpactPoint);
			}
		}
	}

	// Catch wide pillars/arches overlapping directly around the camera lens.
	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		CamLoc,
		FQuat::Identity,
		FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllStaticObjects),
		FCollisionShape::MakeSphere(280.0f),
		Q);
	for (const FOverlapResult& O : Overlaps)
	{
		Consider(O.GetComponent(), nullptr);
	}

	// Fallback for decorative NoCollision overhead arches/columns in baked levels.
	const FVector CenterDir = (Hero - CamLoc).GetSafeNormal();
	const FVector CenterEnd = (FVector::Dist(CamLoc, Hero) > 200.0f) ? (Hero - CenterDir * 80.0f) : Hero;
	const FVector CenterSeg = CenterEnd - CamLoc;
	for (TActorIterator<AStaticMeshActor> It(World); It; ++It)
	{
		UStaticMeshComponent* SM = It->GetStaticMeshComponent();
		if (!SM || !SM->IsVisible() || SM->bHiddenInGame || SM->GetCollisionEnabled() != ECollisionEnabled::NoCollision) continue;
		const FBoxSphereBounds& B = SM->Bounds;
		if (B.BoxExtent.GetMax() >= 1200.0f) continue;
		if (B.Origin.Z + B.BoxExtent.Z <= Feet.Z + 65.0f || B.BoxExtent.Z < 45.0f) continue;
		if (FMath::LineBoxIntersection(B.GetBox().ExpandBy(115.0f), CamLoc, CenterEnd, CenterSeg))
		{
			NowHit.AddUnique(SM);
		}
	}

	for (TWeakObjectPtr<UPrimitiveComponent>& C : NowHit)
	{
		if (C.IsValid())
		{
			if (!CameraOccluders.Contains(C))
			{
				UE_LOG(LogTemp, Display, TEXT("[CamOcc] Hiding %s (%s)"), *GetNameSafe(C->GetOwner()), *GetNameSafe(C.Get()));
			}
			C->SetRenderInMainPass(false);
		}
	}
	for (TWeakObjectPtr<UPrimitiveComponent>& Old : CameraOccluders)
	{
		if (Old.IsValid() && !NowHit.Contains(Old)) { Old->SetRenderInMainPass(true); }
	}
	CameraOccluders = MoveTemp(NowHit);
}

void AAetheriaWizardCharacter::AdjustCameraZoom(float WheelDelta)
{
	if (!bIsInEditorMode && !bIsInLobbyMode)
	{
		// Godot: wheel step 1.6 m, clamp 13.5-30.0 m
		DesiredCameraArmLength = FMath::Clamp(DesiredCameraArmLength - WheelDelta * 160.0f, 1200.0f, 2600.0f);
	}
}

void AAetheriaWizardCharacter::AddCameraTrauma(float Amount)
{
	if (IsLocallyControlled())
	{
		CameraShakeTrauma = FMath::Clamp(CameraShakeTrauma + Amount, 0.0f, 1.0f);
	}
}

void AAetheriaWizardCharacter::MulticastPlaySpellFX_Implementation(
	EAetheriaSpellId SpellId, FVector Origin, FVector Target, const TArray<FVector>& ChainPoints)
{
	ActiveCastAnimSpell = SpellId;
	CastAnimTimer = (SpellId == EAetheriaSpellId::None) ? 0.30f : 0.46f;
	FaceWorldPoint(Target);
	PlaySpellCastAnimation(SpellId);

	const bool bHeavyUltimate = (SpellId == EAetheriaSpellId::BlackHole ||
		SpellId == EAetheriaSpellId::Chronosphere ||
		SpellId == EAetheriaSpellId::ChaosMeteor ||
		SpellId == EAetheriaSpellId::SupernovaBeam ||
		SpellId == EAetheriaSpellId::GravityCataclysm ||
		SpellId == EAetheriaSpellId::PrimalOverload);
	AddCameraTrauma(bHeavyUltimate ? 0.36f : (SpellId == EAetheriaSpellId::None ? 0.05f : 0.14f));

	if (UWorld* World = GetWorld())
	{
		FString CastSfx = TEXT("SFX_BasicBolt");
		switch (SpellId)
		{
		case EAetheriaSpellId::None:
			if (ActiveOrbs.Num() > 0)
			{
				const EAetheriaOrb LeadOrb = ActiveOrbs.Last();
				CastSfx = (LeadOrb == EAetheriaOrb::Ignis) ? TEXT("SFX_BasicBolt_Ignis")
					: (LeadOrb == EAetheriaOrb::Tempest) ? TEXT("SFX_BasicBolt_Tempest") : TEXT("SFX_BasicBolt_Graviton");
			}
			break;
		case EAetheriaSpellId::Fireball:         CastSfx = TEXT("SFX_Fireball_Cast"); break;
		case EAetheriaSpellId::BlackHole:        CastSfx = TEXT("SFX_BlackHole"); break;
		case EAetheriaSpellId::Chronosphere:     CastSfx = TEXT("SFX_Chronosphere"); break;
		case EAetheriaSpellId::ChaosMeteor:      CastSfx = TEXT("SFX_ChaosMeteor"); break;
		case EAetheriaSpellId::ChainLightning:   CastSfx = TEXT("SFX_ChainLightning"); break;
		case EAetheriaSpellId::SupernovaBeam:    CastSfx = TEXT("SFX_SupernovaBeam"); break;
		case EAetheriaSpellId::ChronoBlink:      CastSfx = TEXT("SFX_ChronoBlink"); break;
		case EAetheriaSpellId::GravityCataclysm: CastSfx = TEXT("SFX_GravityCataclysm"); break;
		case EAetheriaSpellId::PrismWall:        CastSfx = TEXT("SFX_PrismWall"); break;
		case EAetheriaSpellId::PrimalOverload:   CastSfx = TEXT("SFX_PrimalOverload"); break;
		case EAetheriaSpellId::Macropyre:        CastSfx = TEXT("SFX_Macropyre"); break;
		case EAetheriaSpellId::PlasmaField:      CastSfx = TEXT("SFX_PlasmaField"); break;
		case EAetheriaSpellId::CataclysmNova:    CastSfx = TEXT("SFX_CataclysmNova"); break;
		case EAetheriaSpellId::SunStrike:        CastSfx = TEXT("SFX_SunStrike"); break;
		case EAetheriaSpellId::EyeOfTheStorm:    CastSfx = TEXT("SFX_EyeOfTheStorm"); break;
		case EAetheriaSpellId::RequiemBlast:     CastSfx = TEXT("SFX_RequiemBlast"); break;
		default: break;
		}
		UAetheriaMaterialLibrary::PlayGameSoundAtLocation(this, CastSfx, Origin, (SpellId == EAetheriaSpellId::None) ? 0.65f : 0.85f);
		UAetheriaPackVFX::PlaySpell(this, SpellId, Origin, Target, ChainPoints);
	}
}

void AAetheriaWizardCharacter::ExecuteSpellServer(EAetheriaSpellId SpellId, const FVector& AimPoint)
{
	if (!HasAuthority() || !bIsAlive) return;

	UWorld* World = GetWorld();
	if (!World) return;

	FVector ClampedAimPoint = AimPoint;
	ClampedAimPoint.Z = FMath::Clamp(ClampedAimPoint.Z, -140.0f, 360.0f);

	const FAetheriaSpellDef Def = UAetheriaSpellDatabase::GetSpellDef(SpellId);
	const FVector Origin = GetSpellMuzzleLocation();

	FVector AimDir = (ClampedAimPoint - Origin);
	AimDir.Z = 0.0f;
	AimDir.Normalize();
	if (AimDir.IsNearlyZero())
	{
		AimDir = GetActorForwardVector();
	}

	float TriadMultiplier = 1.0f;
#if __has_include("AetheriaGameMode.h")
	if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(World->GetAuthGameMode()))
	{
		if (AAetheriaBiomeBuilder* BB = GM->GetBiomeBuilder())
		{
			TriadMultiplier = BB->GetCurrentTriadMultiplier(SpellId);
		}
	}
#endif

	const float HighGroundMult = bOnHighGround ? 1.15f : 1.0f;
	const float PowerMult = GetSpellPowerMultiplier(SpellId);
	const float FinalDamage = Def.BaseDamage * TriadMultiplier * HighGroundMult * PowerMult;

	// Ground zone height conformance so targeting tall bosses never leaves floor seals floating
	int32 AimLocIdx = 0;
	if (AAetheriaGameState* AimGS = World->GetGameState<AAetheriaGameState>())
	{
		AimLocIdx = AimGS->CurrentLocationIndex;
	}
	const float GroundAimZ = UAetheriaBiomeDatabase::GroundZAt(this, AimLocIdx, FVector2D(ClampedAimPoint.X, ClampedAimPoint.Y));
	const FVector GroundAimPoint(ClampedAimPoint.X, ClampedAimPoint.Y, GroundAimZ);

	switch (SpellId)
	{
	// 1. QQQ - Solar Cataclysm Fireball
	case EAetheriaSpellId::Fireball:
	{
		bIsWet = false;
		WetTimer = 0.0f;

		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellProjectile* Proj = World->SpawnActor<AAetheriaSpellProjectile>(Origin, AimDir.Rotation(), P))
		{
			Proj->InitializeProjectile(
				SpellId,
				this,
				AimDir,
				FinalDamage,
				1750.0f,
				Def.RadiusUU,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, Origin + AimDir * Def.RangeUU, {});
		break;
	}

	// 2. EEE - Event Horizon Black Hole
	case EAetheriaSpellId::BlackHole:
	{
		const float EffectiveRadius = Def.RadiusUU * (1.0f + RelicBlackHoleRadiusBonus);
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* Zone = World->SpawnActor<AAetheriaSpellZone>(GroundAimPoint, FRotator::ZeroRotator, P))
		{
			Zone->InitializeZone(
				SpellId,
				this,
				GroundAimPoint,
				AimDir,
				EffectiveRadius,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, GroundAimPoint, {});
		break;
	}

	// 3. WWW - Chronosphere
	case EAetheriaSpellId::Chronosphere:
	{
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* Zone = World->SpawnActor<AAetheriaSpellZone>(GroundAimPoint, FRotator::ZeroRotator, P))
		{
			Zone->InitializeZone(
				SpellId,
				this,
				GroundAimPoint,
				AimDir,
				Def.RadiusUU,
				Def.Duration,
				0.0f,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, GroundAimPoint, {});
		break;
	}

	// 4. QQE - Chaos Meteor
	case EAetheriaSpellId::ChaosMeteor:
	{
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* MeteorZone = World->SpawnActor<AAetheriaSpellZone>(GroundAimPoint, FRotator::ZeroRotator, P))
		{
			MeteorZone->InitializeZone(
				SpellId,
				this,
				GroundAimPoint,
				AimDir,
				Def.RadiusUU,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, GroundAimPoint, {});
		break;
	}

	// 5. QWW - Arcane Chain Lightning
	case EAetheriaSpellId::ChainLightning:
	{
		int32 LocIdx = 0;
		AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>();
		if (GS)
		{
			LocIdx = GS->CurrentLocationIndex;
		}

		// Wet Overcharge: vaporizes wetness on the caster for +25% lightning power without self-harm
		float WetOverchargeMult = 1.0f;
		if (bIsWet)
		{
			bIsWet = false;
			WetTimer = 0.0f;
			WetOverchargeMult = 1.25f;
			if (GS)
			{
				GS->SetSynergyBanner(TEXT("Мокрый Разряд! +25% мощи молнии без урона по себе!"));
			}
		}

		const bool bIsBotCaster = (Cast<APlayerController>(GetController()) == nullptr);
		const float FFScale = (GS && !bIsBotCaster) ? GS->GetEffectiveFriendlyFireScale() : 0.0f;

		TArray<FVector> LightningPoints;
		LightningPoints.Add(Origin);

		TArray<AActor*> HitActors;
		AActor* CurrentSource = this;

		for (int32 Hop = 0; Hop < 5; ++Hop)
		{
			AActor* BestTarget = nullptr;
			float BestDistSq = (Hop == 0) ? (Def.RangeUU * Def.RangeUU) : (900.0f * 900.0f);

			for (TActorIterator<AActor> It(World); It; ++It)
			{
				AActor* Potential = *It;
				if (!Potential || Potential == this || HitActors.Contains(Potential)) continue;

				if (!Potential->IsA<AAetheriaMobCharacter>()) continue;

				float DistSq = FVector::DistSquared(CurrentSource->GetActorLocation(), Potential->GetActorLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					BestTarget = Potential;
				}
			}

			// Optional Friendly Fire 30% arc jump if no enemy left on hop > 0
			if (!BestTarget && Hop > 0 && FFScale > 0.0f)
			{
				float AllyBestDistSq = 750.0f * 750.0f;
				for (TActorIterator<AAetheriaWizardCharacter> AllyIt(World); AllyIt; ++AllyIt)
				{
					AAetheriaWizardCharacter* Ally = *AllyIt;
					if (!Ally || Ally == this || !Ally->bIsAlive || HitActors.Contains(Ally)) continue;
					const float DistSq = FVector::DistSquared(CurrentSource->GetActorLocation(), Ally->GetActorLocation());
					if (DistSq < AllyBestDistSq)
					{
						AllyBestDistSq = DistSq;
						BestTarget = Ally;
					}
				}
			}

			if (BestTarget)
			{
				HitActors.Add(BestTarget);
				LightningPoints.Add(BestTarget->GetActorLocation() + FVector(0, 0, 45));

				if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(BestTarget))
				{
					float ResonanceMult = Mob->IsChronoFrozen() ? 1.4f : 1.0f;
					float TrenchMult = UAetheriaBiomeDatabase::IsInSunkenTrench(LocIdx, Mob->GetActorLocation()) ? 1.20f : 1.0f;
					Mob->ApplyElectroShock(2.5f);
					Mob->ApplySpellDamage(FinalDamage * WetOverchargeMult * ResonanceMult * TrenchMult, this, false, 0.4f);
				}
				else if (AAetheriaWizardCharacter* AllyWiz = Cast<AAetheriaWizardCharacter>(BestTarget))
				{
					AllyWiz->ApplyDamageToWizard(FinalDamage * WetOverchargeMult * FFScale);
				}
				CurrentSource = BestTarget;
			}
			else
			{
				break;
			}
		}

		if (LightningPoints.Num() == 1)
		{
			LightningPoints.Add(Origin + AimDir * Def.RangeUU);
		}

		MulticastPlaySpellFX(SpellId, Origin, ClampedAimPoint, LightningPoints);
		break;
	}

	// 6. QQW - Solar Supernova Beam
	case EAetheriaSpellId::SupernovaBeam:
	{
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* BeamZone = World->SpawnActor<AAetheriaSpellZone>(Origin, FRotator::ZeroRotator, P))
		{
			BeamZone->InitializeZone(
				SpellId,
				this,
				Origin,
				AimDir,
				Def.RadiusUU,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, Origin + AimDir * Def.RangeUU, {});
		break;
	}

	// 7. WWE - Chrono-Blink Hologram
	case EAetheriaSpellId::ChronoBlink:
	{
		const FVector OldPos = GetActorLocation();
		const float BlinkDistance = FMath::Clamp(FVector::Dist2D(OldPos, ClampedAimPoint), 0.0f, Def.RangeUU);
		const float DestGroundZ = UAetheriaBiomeDatabase::GroundZAt(this, AimLocIdx, FVector2D(OldPos.X + AimDir.X * BlinkDistance, OldPos.Y + AimDir.Y * BlinkDistance));
		const FVector NewPos(OldPos.X + AimDir.X * BlinkDistance, OldPos.Y + AimDir.Y * BlinkDistance, DestGroundZ + 92.0f);

		FHitResult Hit;
		SetActorLocation(NewPos, true, &Hit, ETeleportType::TeleportPhysics);

		const FVector DecoyGroundPos(OldPos.X, OldPos.Y, UAetheriaBiomeDatabase::GroundZAt(this, AimLocIdx, FVector2D(OldPos.X, OldPos.Y)));
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* HoloZone = World->SpawnActor<AAetheriaSpellZone>(DecoyGroundPos, FRotator::ZeroRotator, P))
		{
			HoloZone->InitializeZone(
				SpellId,
				this,
				DecoyGroundPos,
				AimDir,
				Def.RadiusUU,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, OldPos, NewPos, {});
		break;
	}

	// 8. QEE - Gravity Cataclysm
	case EAetheriaSpellId::GravityCataclysm:
	{
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* CataclysmZone = World->SpawnActor<AAetheriaSpellZone>(GroundAimPoint, FRotator::ZeroRotator, P))
		{
			CataclysmZone->InitializeZone(
				SpellId,
				this,
				GroundAimPoint,
				AimDir,
				Def.RadiusUU,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, GroundAimPoint, {});
		break;
	}

	// 9. WEE - Cryo-Void Prism Wall
	case EAetheriaSpellId::PrismWall:
	{
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* WallZone = World->SpawnActor<AAetheriaSpellZone>(GroundAimPoint, FRotator::ZeroRotator, P))
		{
			WallZone->InitializeZone(
				SpellId,
				this,
				GroundAimPoint,
				AimDir,
				Def.RadiusUU,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, GroundAimPoint, {});
		break;
	}

	// 10. QWE - Primal Triad Overload
	case EAetheriaSpellId::PrimalOverload:
	{
		AddShield(120.0f);

		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* NovaZone = World->SpawnActor<AAetheriaSpellZone>(GetActorLocation(), FRotator::ZeroRotator, P))
		{
			NovaZone->InitializeZone(
				SpellId,
				this,
				GetActorLocation(),
				GetActorForwardVector(),
				Def.RadiusUU,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}

		TArray<AActor*> NearbyActors;
		UGameplayStatics::GetAllActorsOfClass(World, AAetheriaWizardCharacter::StaticClass(), NearbyActors);
		bool bRevivedAnyAlly = false;
		for (AActor* Act : NearbyActors)
		{
			if (!Act) continue;
			if (FVector::Dist(GetActorLocation(), Act->GetActorLocation()) <= Def.RadiusUU)
			{
				if (AAetheriaWizardCharacter* Ally = Cast<AAetheriaWizardCharacter>(Act))
				{
					if (!Ally->bIsAlive)
					{
						Ally->ReviveWizard(0.60f);
						bRevivedAnyAlly = true;
					}
					Ally->AddShield(120.0f);
				}
			}
		}
		if (bRevivedAnyAlly)
		{
			if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
			{
				GS->SetSynergyBanner(TEXT("Триада Воскрешения: Маг поднят в строй!"));
			}
		}

		MulticastPlaySpellFX(SpellId, GetActorLocation(), GetActorLocation(), {});
		break;
	}

	// 11..16 - Extended Signature Arsenal (Macropyre, PlasmaField, CataclysmNova, SunStrike, EyeOfTheStorm, RequiemBlast)
	case EAetheriaSpellId::Macropyre:
	case EAetheriaSpellId::PlasmaField:
	case EAetheriaSpellId::CataclysmNova:
	case EAetheriaSpellId::SunStrike:
	case EAetheriaSpellId::EyeOfTheStorm:
	case EAetheriaSpellId::RequiemBlast:
	{
		const FVector SpawnLoc = (Def.TargetShape == EAetheriaTargetShape::PointAoE) ? GroundAimPoint : GetActorLocation();
		FActorSpawnParameters P;
		P.Owner = this;
		P.Instigator = this;
		if (AAetheriaSpellZone* ExtZone = World->SpawnActor<AAetheriaSpellZone>(SpawnLoc, FRotator::ZeroRotator, P))
		{
			ExtZone->InitializeZone(
				SpellId,
				this,
				SpawnLoc,
				AimDir,
				Def.RadiusUU,
				Def.Duration,
				FinalDamage,
				Def.PrimaryColor,
				Def.AccentColor);
		}
		MulticastPlaySpellFX(SpellId, Origin, SpawnLoc, {});
		break;
	}

	default:
		break;
	}
}

void AAetheriaWizardCharacter::ApplyDamageToWizard(float Amount)
{
	if (!HasAuthority() || !bIsAlive || Amount <= 0.0f || DashInvulnRemaining > 0.0f) return;

	HitFlashTimer = 0.18f;
	HitRecoilTimer = 0.16f;
	const bool bHadShield = (Shield > 0.0f);

	if (Shield > 0.0f)
	{
		if (Shield >= Amount)
		{
			Shield -= Amount;
			Amount = 0.0f;
		}
		else
		{
			Amount -= Shield;
			Shield = 0.0f;
		}
		UpdateShieldVisuals();
	}

	if (Amount <= 0.0f && bHadShield)
	{
		UAetheriaMaterialLibrary::PlayGameSoundAtLocation(this, TEXT("SFX_Hit_Shield_Absorb"), GetActorLocation(), 0.75f);
	}
	else if (Amount > 0.0f)
	{
		UAetheriaMaterialLibrary::PlayGameSoundAtLocation(this, TEXT("SFX_Hit_Flesh"), GetActorLocation(), 0.80f);
		HP = FMath::Clamp(HP - Amount, 0.0f, MaxHP);
		if (HP <= 0.0f)
		{
			bIsAlive = false;
			PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::Death));
			if (UCharacterMovementComponent* Mov = GetCharacterMovement())
			{
				Mov->StopMovementImmediately();
				Mov->DisableMovement();
			}
			bGlideDashing = false;
			HitFlashTimer = 0.25f;
		}
	}
}

void AAetheriaWizardCharacter::AddShield(float Amount)
{
	if (Amount <= 0.0f || !bIsAlive) return;

	Shield = FMath::Clamp(Shield + Amount, 0.0f, 600.0f);
	UpdateShieldVisuals();
}

bool AAetheriaWizardCharacter::IsSpellOffCooldown(EAetheriaSpellId SpellId) const
{
	return GetSpellCooldownRemaining(SpellId) <= 0.0f;
}

float AAetheriaWizardCharacter::GetSpellCooldownRemaining(EAetheriaSpellId SpellId) const
{
	const int32 Idx = static_cast<int32>(SpellId);
	if (SpellCooldowns.IsValidIndex(Idx))
	{
		return SpellCooldowns[Idx];
	}
	if (SlotD == SpellId) return SlotDCooldownRemaining;
	if (SlotF == SpellId) return SlotFCooldownRemaining;
	return 0.0f;
}

float AAetheriaWizardCharacter::GetSlotCooldown(int32 SlotIndex) const
{
	return (SlotIndex == 0) ? SlotDCooldownRemaining : SlotFCooldownRemaining;
}

void AAetheriaWizardCharacter::AwardProgression(int32 XPDelta, int32 EssenceDelta)
{
	if (!HasAuthority()) return;

	CurrentXP += FMath::Max(0, XPDelta);
	ArcaneEssence += FMath::Max(0, EssenceDelta);

	const int32 OldLevel = WizardLevel;
	while (CurrentXP >= UAetheriaSaveGame::GetXPForNextLevel(WizardLevel))
	{
		CurrentXP -= UAetheriaSaveGame::GetXPForNextLevel(WizardLevel);
		WizardLevel++;
		MaxHP += 22.0f;
		MaxMana += 12.0f;
		HP = FMath::Min(MaxHP, HP + 110.0f);
		Mana = MaxMana;
	}

	if (WizardLevel > OldLevel)
	{
		UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_LevelUp"), 0.90f);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalPlayerController())
		{
			if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
			{
				Save->WizardLevel = WizardLevel;
				Save->CurrentXP = CurrentXP;
				Save->ArcaneEssence = ArcaneEssence;
				Save->EssenceEmpowerRank = EssenceEmpowerRank;
				Save->TotalKills = FMath::Max(Save->TotalKills, Kills);
				for (const FString& RelicId : OwnedRelicIds)
				{
					Save->EquippedRelicNames.AddUnique(RelicId);
				}
				UAetheriaSaveGame::PersistSave(Save);
			}
		}
	}
}

void AAetheriaWizardCharacter::MulticastPlayInvokeFX_Implementation()
{
	InvokePulseTimer = 0.32f;
	const float Vol = IsLocallyControlled() ? 0.65f : 0.35f;
	UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_Invoke"), Vol);

	if (UWorld* World = GetWorld())
	{
		const FLinearColor InvokeColor = ActiveOrbs.Num() > 0
			? AetheriaMindColors::GetOrbColor(ActiveOrbs.Last())
			: AetheriaMindColors::MindCyan;
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation() - FVector(0.0f, 0.0f, 80.0f), 210.0f, InvokeColor, 0.28f);
	}
}

void AAetheriaWizardCharacter::ServerUseHealingFlask_Implementation()
{
	if (!bIsAlive || HealingFlaskCharges <= 0 || FlaskCooldownRemaining > 0.0f || HP >= MaxHP)
	{
		return;
	}

	HealingFlaskCharges--;
	FlaskCooldownRemaining = 4.0f;
	HP = FMath::Clamp(HP + MaxHP * 0.38f, 0.0f, MaxHP);
	ColdStacks = 0;
	WizardBurnTimer = 0.0f;
	WizardBurnTickAccum = 0.0f;
	bIsBurning = false;

	UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_Flask_Drink"), 0.85f);
	if (UWorld* World = GetWorld())
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 340.0f, FLinearColor(0.10f, 0.95f, 0.45f, 1.0f), 0.45f);
	}
}

void AAetheriaWizardCharacter::ServerSelfCast_Implementation()
{
	if (!bIsAlive || SelfCastCooldownRemaining > 0.0f || Mana < 20.0f)
	{
		return;
	}

	Mana -= 20.0f;
	SelfCastCooldownRemaining = 5.0f;
	CastAnimTimer = 0.32f;

	int32 QCount = 0;
	int32 WCount = 0;
	int32 ECount = 0;
	for (EAetheriaOrb Orb : ActiveOrbs)
	{
		if (Orb == EAetheriaOrb::Ignis) QCount++;
		else if (Orb == EAetheriaOrb::Tempest) WCount++;
		else if (Orb == EAetheriaOrb::Graviton) ECount++;
	}

	UWorld* World = GetWorld();
	const FVector MyLoc = GetActorLocation();

	if (QCount >= WCount && QCount >= ECount)
	{
		// Ignis Self-Cast: Dry, cleanse chill, +25% Haste (4s), local 320 UU flame burst
		bIsWet = false;
		WetTimer = 0.0f;
		ColdStacks = 0;
		HasteBuffTimer = 4.0f;
		if (World)
		{
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, MyLoc, 340.0f, AetheriaMindColors::IgnisPlasma, 0.45f);
			TArray<FOverlapResult> Overlaps;
			World->OverlapMultiByChannel(Overlaps, MyLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(320.0f));
			const float BurstDmg = 95.0f * GetSpellPowerMultiplier(EAetheriaSpellId::Fireball);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
				{
					Mob->ApplySpellDamage(BurstDmg, this, true, 0.25f);
				}
			}
		}
	}
	else if (WCount >= QCount && WCount >= ECount)
	{
		// Tempest Self-Cast: Extinguish burn, +75 HP heal, +90 shield
		WizardBurnTimer = 0.0f;
		WizardBurnTickAccum = 0.0f;
		bIsBurning = false;
		HP = FMath::Clamp(HP + 75.0f, 0.0f, MaxHP);
		AddShield(90.0f);
		if (World)
		{
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, MyLoc, 320.0f, AetheriaMindColors::MindCyan, 0.45f);
		}
	}
	else
	{
		// Graviton Self-Cast: +115 shield + 460 UU gravity knockback pulse
		AddShield(115.0f);
		if (World)
		{
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, MyLoc, 460.0f, AetheriaMindColors::MindViolet, 0.45f);
			TArray<FOverlapResult> Overlaps;
			World->OverlapMultiByChannel(Overlaps, MyLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(460.0f));
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
				{
					const FVector PushDir = (Mob->GetActorLocation() - MyLoc).GetSafeNormal2D();
					Mob->ApplyStatusImpulse(PushDir, 680.0f, this, true);
				}
			}
		}
	}

	const FString SelfCastSfx = (QCount >= WCount && QCount >= ECount) ? TEXT("SFX_SelfCast_Ignis")
		: (WCount >= QCount && WCount >= ECount) ? TEXT("SFX_SelfCast_Tempest") : TEXT("SFX_SelfCast_Graviton");
	UAetheriaMaterialLibrary::PlayGameSoundAtLocation(this, SelfCastSfx, MyLoc, 0.80f);
}

void AAetheriaWizardCharacter::ServerEmpowerWithEssence_Implementation()
{
	if (!bIsAlive) return;

	const int32 Cost = 35 + EssenceEmpowerRank * 15;
	if (ArcaneEssence < Cost)
	{
		UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_Mana_Empty_Deny"), 0.70f);
		if (UWorld* World = GetWorld())
		{
			if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
			{
				GS->SetSynergyBanner(FString::Printf(TEXT("Для усиления [G] нужно %d Эссенции (есть %d)"), Cost, ArcaneEssence));
			}
		}
		return;
	}

	ArcaneEssence -= Cost;
	EssenceEmpowerRank++;
	MaxHP += 20.0f;
	HP = FMath::Min(MaxHP, HP + 60.0f);
	MaxMana += 10.0f;
	Mana = MaxMana;

	UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_UI_Forge_Anvil"), 0.90f);
	if (UWorld* World = GetWorld())
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 400.0f, FLinearColor(0.95f, 0.78f, 0.25f, 1.0f), 0.55f);
		if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
		{
			GS->SetSynergyBanner(FString::Printf(TEXT("КУЗНЯ ЭССЕНЦИИ [Ранг %d]: +6%% урона стихий, +20 MaxHP, +10 MaxMana!"), EssenceEmpowerRank));
		}
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalPlayerController())
		{
			if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
			{
				Save->ArcaneEssence = ArcaneEssence;
				Save->EssenceEmpowerRank = EssenceEmpowerRank;
				UAetheriaSaveGame::PersistSave(Save);
			}
		}
	}
}

void AAetheriaWizardCharacter::GrantRelicBonus(const FString& RelicId, const FString& RelicName, float RarityPowerMult)
{
	if (!HasAuthority()) return;

	const bool bFirstTime = !OwnedRelicIds.Contains(RelicId);
	if (bFirstTime)
	{
		OwnedRelicIds.Add(RelicId);
	}

	const float StackScale = bFirstTime ? FMath::Clamp(RarityPowerMult, 0.8f, 2.0f) : 0.25f;

	if (RelicId == TEXT("ember_heart"))
	{
		RelicIgnisBonus = FMath::Min(0.85f, RelicIgnisBonus + 0.12f * StackScale);
	}
	else if (RelicId == TEXT("chrono_astrolabe"))
	{
		RelicChronoCDR = FMath::Min(0.45f, RelicChronoCDR + 0.10f * StackScale);
	}
	else if (RelicId == TEXT("singularity_seal"))
	{
		RelicBlackHoleRadiusBonus = FMath::Min(0.60f, RelicBlackHoleRadiusBonus + 0.15f * StackScale);
	}
	else if (RelicId == TEXT("prism_monocle"))
	{
		RelicPrismSplitBonus = FMath::Min(0.75f, RelicPrismSplitBonus + 0.18f * StackScale);
	}
	else if (RelicId == TEXT("archon_crown"))
	{
		RelicAllElementBonus = FMath::Min(0.65f, RelicAllElementBonus + 0.12f * StackScale);
		const float ManaGain = bFirstTime ? 25.0f : 6.0f;
		MaxMana += ManaGain;
		Mana = FMath::Min(MaxMana, Mana + ManaGain);
	}

	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->IsLocalPlayerController())
		{
			if (UAetheriaSaveGame* Save = UAetheriaSaveGame::LoadOrCreateSave())
			{
				Save->EquippedRelicNames.AddUnique(RelicId);
				UAetheriaSaveGame::PersistSave(Save);
			}
		}
	}
}

void AAetheriaWizardCharacter::ApplyWizardBurn(float Duration, float DPS)
{
	if (!HasAuthority() || !bIsAlive || bIsWet) return;
	WizardBurnTimer = FMath::Max(WizardBurnTimer, Duration);
	WizardBurnDPS = FMath::Max(WizardBurnDPS, DPS);
	bIsBurning = true;
}

void AAetheriaWizardCharacter::ApplyWizardWet(float Duration)
{
	if (!HasAuthority() || !bIsAlive) return;
	bIsWet = true;
	WetTimer = FMath::Max(WetTimer, Duration);
	WizardBurnTimer = 0.0f;
	WizardBurnTickAccum = 0.0f;
	bIsBurning = false;
}

void AAetheriaWizardCharacter::RefillFlaskFromKill(bool bIsEliteOrBoss)
{
	if (!HasAuthority()) return;
	FlaskKillProgress += bIsEliteOrBoss ? 3 : 1;
	if (FlaskKillProgress >= 4)
	{
		FlaskKillProgress -= 4;
		HealingFlaskCharges = FMath::Clamp(HealingFlaskCharges + 1, 0, MaxHealingFlaskCharges);
	}
}

float AAetheriaWizardCharacter::GetSpellPowerMultiplier(EAetheriaSpellId SpellId) const
{
	const float LevelMult = 1.0f + 0.04f * static_cast<float>(FMath::Max(0, WizardLevel - 1));
	const float EssenceMult = 1.0f + 0.06f * static_cast<float>(FMath::Max(0, EssenceEmpowerRank));
	float ElementMult = 1.0f + RelicAllElementBonus;

	if (SpellId == EAetheriaSpellId::Fireball ||
		SpellId == EAetheriaSpellId::ChaosMeteor ||
		SpellId == EAetheriaSpellId::SupernovaBeam)
	{
		ElementMult += RelicIgnisBonus;
	}

	return LevelMult * EssenceMult * ElementMult;
}

float AAetheriaWizardCharacter::GetEffectiveSpellCooldown(EAetheriaSpellId SpellId, float BaseCooldown) const
{
	float Mult = 1.0f;
	if (SpellId == EAetheriaSpellId::Chronosphere || SpellId == EAetheriaSpellId::ChronoBlink)
	{
		Mult = FMath::Max(0.45f, 1.0f - RelicChronoCDR);
	}
	return BaseCooldown * Mult;
}

void AAetheriaWizardCharacter::ReviveWizard(float HealthFraction)
{
	if (!HasAuthority()) return;

	bIsAlive = true;
	RespawnTimer = 0.0f;
	HP = FMath::Clamp(MaxHP * FMath::Clamp(HealthFraction, 0.1f, 1.0f), 1.0f, MaxHP);
	Mana = FMath::Max(Mana, MaxMana * 0.5f);
	ColdStacks = 0;
	WizardBurnTimer = 0.0f;
	WizardBurnTickAccum = 0.0f;
	bIsBurning = false;
	ResetPresentationDeath();
	DownedAnimAlpha = 0.0f;
	bGlideDashing = false;

	if (UCharacterMovementComponent* MoveComp = GetCharacterMovement())
	{
		MoveComp->SetMovementMode(MOVE_Walking);
	}

	if (UWorld* World = GetWorld())
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 380.0f, AetheriaMindColors::MindCyan, 0.55f);
	}
}


void AAetheriaWizardCharacter::SetCursorAimPoint(const FVector& WorldPoint)
{
	FaceWorldPoint(WorldPoint);
}

void AAetheriaWizardCharacter::PlayPresentationAction(uint8 Action)
{
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Skel->GetAnimInstance()))
		{
			Anim->PlayAction(static_cast<EAetheriaAnimAction>(Action));
		}
	}
}

void AAetheriaWizardCharacter::ResetPresentationDeath()
{
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Skel->GetAnimInstance()))
		{
			Anim->ReviveFromDeath();
		}
	}
}
