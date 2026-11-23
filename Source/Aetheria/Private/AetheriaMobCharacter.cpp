#include "AetheriaMobCharacter.h"
#include "AetheriaPackVFX.h"
#include "AetheriaAnimInstance.h"
#include "AetheriaWizardCharacter.h"
#include "AIController.h"
#include "AetheriaGameMode.h"
#include "AetheriaWorldDirector.h"
#include "AetheriaWizardCharacter.h"
#include "AetheriaMaterialLibrary.h"
#include "AetheriaAnimInstance.h"
#include "AetheriaSpellActors.h"
#include "AetheriaBiomeBuilder.h"
#include "AetheriaBiomeDatabase.h"
#include "AetheriaGameState.h"
#include "Net/UnrealNetwork.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Engine/OverlapResult.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

AAetheriaMobCharacter::AAetheriaMobCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);
	SetNetUpdateFrequency(20.0f);

	GetCapsuleComponent()->InitCapsuleSize(90.0f, 110.0f);
	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	GetCapsuleComponent()->SetCollisionObjectType(ECC_Pawn);
	GetCapsuleComponent()->SetCollisionResponseToAllChannels(ECR_Block);
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);

	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 540.0f, 0.0f);
	GetCharacterMovement()->MaxWalkSpeed = 500.0f;
	GetCharacterMovement()->GravityScale = 1.0f;
	GetCharacterMovement()->bRunPhysicsWithNoController = true;

	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	AIControllerClass = AAIController::StaticClass();

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	UStaticMesh* CubeMesh = CubeFinder.Succeeded() ? CubeFinder.Object : nullptr;
	UStaticMesh* SphereMesh = SphereFinder.Succeeded() ? SphereFinder.Object : nullptr;
	UStaticMesh* ConeMesh = ConeFinder.Succeeded() ? ConeFinder.Object : nullptr;
	UStaticMesh* CylinderMesh = CylinderFinder.Succeeded() ? CylinderFinder.Object : nullptr;

	BodyMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BodyMeshComponent"));
	BodyMeshComponent->SetupAttachment(RootComponent);
	BodyMeshComponent->SetCollisionProfileName(TEXT("NoCollision"));
	BodyMeshComponent->SetCastShadow(true);
	BodyMeshComponent->bCastContactShadow = true;
	if (CylinderMesh)
	{
		BodyMeshComponent->SetStaticMesh(CylinderMesh);
	}

	HeadMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("HeadMeshComponent"));
	HeadMeshComponent->SetupAttachment(BodyMeshComponent);
	HeadMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	HeadMeshComponent->SetCastShadow(true);
	HeadMeshComponent->bCastContactShadow = true;
	if (CubeMesh)
	{
		HeadMeshComponent->SetStaticMesh(CubeMesh);
	}

	ShoulderLeftMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShoulderLeftMesh"));
	ShoulderLeftMesh->SetupAttachment(BodyMeshComponent);
	ShoulderLeftMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShoulderLeftMesh->SetCastShadow(true);
	ShoulderLeftMesh->bCastContactShadow = true;
	if (CubeMesh)
	{
		ShoulderLeftMesh->SetStaticMesh(CubeMesh);
	}

	ShoulderRightMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("ShoulderRightMesh"));
	ShoulderRightMesh->SetupAttachment(BodyMeshComponent);
	ShoulderRightMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	ShoulderRightMesh->SetCastShadow(true);
	ShoulderRightMesh->bCastContactShadow = true;
	if (CubeMesh)
	{
		ShoulderRightMesh->SetStaticMesh(CubeMesh);
	}

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(BodyMeshComponent);
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh)
	{
		CoreMesh->SetStaticMesh(SphereMesh);
	}

	FinMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinMesh1"));
	FinMesh1->SetupAttachment(BodyMeshComponent);
	FinMesh1->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (ConeMesh)
	{
		FinMesh1->SetStaticMesh(ConeMesh);
	}

	FinMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinMesh2"));
	FinMesh2->SetupAttachment(BodyMeshComponent);
	FinMesh2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (ConeMesh)
	{
		FinMesh2->SetStaticMesh(ConeMesh);
	}

	FinMesh3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinMesh3"));
	FinMesh3->SetupAttachment(BodyMeshComponent);
	FinMesh3->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (ConeMesh)
	{
		FinMesh3->SetStaticMesh(ConeMesh);
	}

	FinMesh4 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("FinMesh4"));
	FinMesh4->SetupAttachment(BodyMeshComponent);
	FinMesh4->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (ConeMesh)
	{
		FinMesh4->SetStaticMesh(ConeMesh);
	}

	OrbMesh1 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh1"));
	OrbMesh1->SetupAttachment(RootComponent);
	OrbMesh1->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh)
	{
		OrbMesh1->SetStaticMesh(SphereMesh);
	}

	OrbMesh2 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh2"));
	OrbMesh2->SetupAttachment(RootComponent);
	OrbMesh2->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh)
	{
		OrbMesh2->SetStaticMesh(SphereMesh);
	}

	OrbMesh3 = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OrbMesh3"));
	OrbMesh3->SetupAttachment(RootComponent);
	OrbMesh3->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (SphereMesh)
	{
		OrbMesh3->SetStaticMesh(SphereMesh);
	}
}

void AAetheriaMobCharacter::BeginPlay()
{
	Super::BeginPlay();

	UpdateVisualSilhouette();
	UpdateMaterials();
}

void AAetheriaMobCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAetheriaMobCharacter, MobDisplayName);
	DOREPLIFETIME(AAetheriaMobCharacter, MobKind);
	DOREPLIFETIME(AAetheriaMobCharacter, HP);
	DOREPLIFETIME(AAetheriaMobCharacter, MaxHP);
	DOREPLIFETIME(AAetheriaMobCharacter, bIsFrozen);
	DOREPLIFETIME(AAetheriaMobCharacter, BossPhase);
	DOREPLIFETIME(AAetheriaMobCharacter, BossOrbs);
	DOREPLIFETIME(AAetheriaMobCharacter, bIsTelegraphingSlam);
	DOREPLIFETIME(AAetheriaMobCharacter, BurnStacks);
	DOREPLIFETIME(AAetheriaMobCharacter, bIsStaggered);
	DOREPLIFETIME(AAetheriaMobCharacter, StaggerGauge);
	DOREPLIFETIME(AAetheriaMobCharacter, bIsTaunted);
	DOREPLIFETIME(AAetheriaMobCharacter, SpawnAmbushKind);
	DOREPLIFETIME(AAetheriaMobCharacter, bIsEliteLeader);
}

void AAetheriaMobCharacter::InitFromArchetype(const FAetheriaMobArchetype& Archetype, float HPScale, float DamageScale, int32 InLocationIndex)
{
	MobDisplayName = Archetype.DisplayName;
	MobKind = Archetype.Kind;
	MaxHP = FMath::Max(10.0f, Archetype.MaxHP * HPScale);
	HP = MaxHP;
	BaseDamage = Archetype.Damage * DamageScale;
	AttackRangeUU = Archetype.AttackRangeUU;
	BodyColor = Archetype.BodyColor;
	AccentColor = Archetype.AccentColor;
	bExplodeOnDeath = Archetype.bExplodeOnDeath;
	bCastsOrbs = Archetype.bCastsOrbs;
	LocationIndex = InLocationIndex;

	if (GetCharacterMovement())
	{
		GetCharacterMovement()->MaxWalkSpeed = Archetype.MoveSpeedUU;
	}

	if (MobKind == EAetheriaMobKind::Boss)
	{
		GetCapsuleComponent()->SetCapsuleRadius(220.0f);
		GetCapsuleComponent()->SetCapsuleHalfHeight(300.0f);

		BossOrbs.Empty();
		BossOrbs.Add(EAetheriaOrb::Ignis);
		BossOrbs.Add(EAetheriaOrb::Tempest);
		BossOrbs.Add(EAetheriaOrb::Graviton);
	}
	else
	{
		GetCapsuleComponent()->SetCapsuleRadius(Archetype.CollisionRadiusUU);
		GetCapsuleComponent()->SetCapsuleHalfHeight(Archetype.CollisionRadiusUU * (MobKind == EAetheriaMobKind::Bruiser ? 2.0f : 1.4f));
	}

	UpdateVisualSilhouette();
	UpdateMaterials();
}

void AAetheriaMobCharacter::InitAmbushEmergence(EAetheriaAmbushKind InKind, bool bInElite)
{
	SpawnAmbushKind = InKind;
	bIsEliteLeader = bInElite;
	if (bInElite && MobKind != EAetheriaMobKind::Boss)
	{
		MaxHP *= 1.35f;
		HP = MaxHP;
		BaseDamage *= 1.15f;
		SetActorScale3D(GetActorScale3D() * 1.22f);
	}
	BeginTypedEntrance();
}

void AAetheriaMobCharacter::BeginTypedEntrance()
{
	bEntranceLocked = true;
	if (UCharacterMovementComponent* Mov = GetCharacterMovement())
	{
		Mov->StopMovementImmediately();
		Mov->DisableMovement();
	}

	const FVector Loc = GetActorLocation();
	EntranceStartZ = Loc.Z;
	const bool bElite = bIsEliteLeader && MobKind != EAetheriaMobKind::Boss;
	const bool bBoss = (MobKind == EAetheriaMobKind::Boss);

	if (bBoss)
	{
		EmergenceTimer = 2.2f;
		UAetheriaPackVFX::PlaySpawnEntrance(this, MobKind, Loc, false, true);
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
		{
			Anim->PlayAction(EAetheriaAnimAction::CastHeavy);
		}
		UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_BossRoar"), 0.95f);
		for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
		{
			if (It->IsLocallyControlled())
			{
				It->PlayBossIntroFocus(this, 2.4f);
				break;
			}
		}
	}
	else if (bElite)
	{
		EmergenceTimer = 1.05f;
		SetActorLocation(FVector(Loc.X, Loc.Y, Loc.Z - 160.0f));
		UAetheriaPackVFX::PlaySpawnEntrance(this, MobKind, Loc, true, false);
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
		{
			Anim->PlayAction(EAetheriaAnimAction::CastPrimary);
		}
	}
	else if (MobKind == EAetheriaMobKind::Swarm)
	{
		EmergenceTimer = 0.85f;
		SetActorLocation(FVector(Loc.X, Loc.Y, Loc.Z - 200.0f));
		UAetheriaPackVFX::PlaySpawnEntrance(this, MobKind, Loc, false, false);
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
		{
			Anim->PlayAction(EAetheriaAnimAction::AttackMelee);
		}
	}
	else if (MobKind == EAetheriaMobKind::Ranged)
	{
		EmergenceTimer = 0.90f;
		if (GetMesh())
		{
			GetMesh()->SetVisibility(false, true);
		}
		UAetheriaPackVFX::PlaySpawnEntrance(this, MobKind, Loc, false, false);
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
		{
			Anim->PlayAction(EAetheriaAnimAction::CastPrimary);
		}
	}
	else if (MobKind == EAetheriaMobKind::Bruiser)
	{
		EmergenceTimer = 0.95f;
		SetActorLocation(FVector(Loc.X, Loc.Y, Loc.Z + 900.0f));
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(GetMesh() ? GetMesh()->GetAnimInstance() : nullptr))
		{
			Anim->PlayAction(EAetheriaAnimAction::AttackSlam);
		}
	}
	else
	{
		EmergenceTimer = 0.70f;
		UAetheriaPackVFX::PlaySpawnEntrance(this, MobKind, Loc, false, false);
	}
}

void AAetheriaMobCharacter::TriggerImpactHitstop(float Duration)
{
	HitstopTimer = FMath::Max(HitstopTimer, Duration);
	if (!bIsFrozen)
	{
		CustomTimeDilation = 0.08f;
	}
}

void AAetheriaMobCharacter::ApplyBurnStack(float BaseSpellDamage, AActor* InstigatorWizard, int32 StacksToAdd)
{
	if (HP <= 0.0f) return;

	LastInstigator = InstigatorWizard;
	BurnStacks = FMath::Clamp(BurnStacks + StacksToAdd, 1, 5);
	BurnDamagePerSec = FMath::Max(BurnDamagePerSec, BaseSpellDamage * 0.15f);

	const float BiomeDurMult = (LocationIndex == 1) ? 0.65f : 1.0f;
	BurnTimeRemaining = 3.5f * BiomeDurMult;

	// Cross-status synergy: Freeze + Ignite -> Thermal Shock!
	if (bIsFrozen && ThermalShockCooldown <= 0.0f)
	{
		ThermalShockCooldown = 1.2f;
		TriggerImpactHitstop(0.048f);
		const float ThermalShockDmg = (MobKind == EAetheriaMobKind::Boss)
			? FMath::Min(MaxHP * 0.06f, 260.0f)
			: FMath::Min(MaxHP * 0.35f, FMath::Max(120.0f, BaseSpellDamage) * 0.65f);

		if (UWorld* World = GetWorld())
		{
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(World, TEXT("SFX_Synergy_ThermalShock"), GetActorLocation(), 0.90f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 320.0f, AetheriaMindColors::IgnisPlasma, 0.5f);

			if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
			{
				GS->SetSynergyBanner(TEXT("Термошок: Крио-Плазменный Взрыв!"));
			}

			TArray<FOverlapResult> Overlaps;
			FCollisionShape SphereShape = FCollisionShape::MakeSphere(320.0f);
			World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AAetheriaMobCharacter* OtherMob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
				{
					const float TargetCapDmg = (OtherMob->MobKind == EAetheriaMobKind::Boss)
						? FMath::Min(OtherMob->MaxHP * 0.06f, 260.0f)
						: ThermalShockDmg;
					OtherMob->ApplySpellDamage(TargetCapDmg, InstigatorWizard, false, 0.0f);
				}
			}
		}

		if (MobKind == EAetheriaMobKind::Boss && UnstoppableTimer <= 0.0f)
		{
			StaggerGauge = FMath::Clamp(StaggerGauge + 28.0f, 0.0f, 100.0f);
			if (StaggerGauge >= 100.0f)
			{
				bIsStaggered = true;
				StaggerTimer = 2.2f;
				bIsTelegraphingSlam = false;
				StaggerGauge = 0.0f;
				TriggerImpactHitstop(0.065f);
			}
		}
	}
}

void AAetheriaMobCharacter::ApplyAirborneLift(float BaseLaunchZ, float StunDuration, AActor* InstigatorWizard)
{
	if (HP <= 0.0f) return;
	LastInstigator = InstigatorWizard;

	// Cross-status synergy: Freeze + Knockup/Gravity -> Cryo-Shatter!
	if (bIsFrozen)
	{
		TriggerImpactHitstop(0.048f);
		const float ShatterDmg = MaxHP * 0.40f;
		ApplySpellDamage(ShatterDmg, InstigatorWizard, false, StunDuration);

		if (UWorld* World = GetWorld())
		{
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(World, TEXT("SFX_Synergy_CryoShatter"), GetActorLocation(), 0.90f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 350.0f, AetheriaMindColors::MindCyan, 0.5f);

			if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
			{
				GS->SetSynergyBanner(TEXT("Крио-Раскол: Гравитационный Взрыв Осколков!"));
			}

			TArray<FOverlapResult> Overlaps;
			FCollisionShape SphereShape = FCollisionShape::MakeSphere(350.0f);
			World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AAetheriaMobCharacter* OtherMob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
				{
					if (OtherMob != this)
					{
						OtherMob->ApplySpellDamage(ShatterDmg * 0.40f, InstigatorWizard, false, 0.5f);
					}
				}
			}
		}

		if (MobKind == EAetheriaMobKind::Swarm && HP < MaxHP * 0.18f)
		{
			HP = 0.0f;
			Die();
			return;
		}

		if (MobKind == EAetheriaMobKind::Boss && UnstoppableTimer <= 0.0f)
		{
			StaggerGauge = FMath::Clamp(StaggerGauge + 28.0f, 0.0f, 100.0f);
			if (StaggerGauge >= 100.0f)
			{
				bIsStaggered = true;
				StaggerTimer = 2.2f;
				bIsTelegraphingSlam = false;
				StaggerGauge = 0.0f;
				TriggerImpactHitstop(0.065f);
			}
		}
	}
	else
	{
		if (StunDuration > 0.0f)
		{
			ApplySpellDamage(0.0f, InstigatorWizard, false, StunDuration);
		}
	}

	LaunchCharacter(FVector(0.0f, 0.0f, BaseLaunchZ), true, true);
	AirborneTrackingTimer = 1.0f;
}

void AAetheriaMobCharacter::ApplyStatusImpulse(const FVector& PushDir2D, float BaseImpulseUU, AActor* InstigatorWizard, bool bFromGravityOrBlast)
{
	if (HP <= 0.0f) return;
	LastInstigator = InstigatorWizard;

	// Cross-status synergy: Freeze + Impulse/Blast -> Cryo-Shatter!
	if (bIsFrozen && bFromGravityOrBlast)
	{
		const float ShatterDmg = MaxHP * 0.40f;
		ApplySpellDamage(ShatterDmg, InstigatorWizard, false, 0.6f);

		if (UWorld* World = GetWorld())
		{
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(World, TEXT("SFX_Synergy_CryoShatter"), GetActorLocation(), 0.90f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 350.0f, AetheriaMindColors::MindCyan, 0.5f);

			if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
			{
				GS->SetSynergyBanner(TEXT("Крио-Раскол: Гравитационный Взрыв Осколков!"));
			}

			TArray<FOverlapResult> Overlaps;
			FCollisionShape SphereShape = FCollisionShape::MakeSphere(350.0f);
			World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AAetheriaMobCharacter* OtherMob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
				{
					if (OtherMob != this)
					{
						OtherMob->ApplySpellDamage(ShatterDmg * 0.40f, InstigatorWizard, false, 0.4f);
					}
				}
			}
		}

		if (MobKind == EAetheriaMobKind::Swarm && HP < MaxHP * 0.18f)
		{
			HP = 0.0f;
			Die();
			return;
		}

		if (MobKind == EAetheriaMobKind::Boss && UnstoppableTimer <= 0.0f)
		{
			StaggerGauge = FMath::Clamp(StaggerGauge + 28.0f, 0.0f, 100.0f);
			if (StaggerGauge >= 100.0f)
			{
				bIsStaggered = true;
				StaggerTimer = 2.2f;
				bIsTelegraphingSlam = false;
				StaggerGauge = 0.0f;
			}
		}
	}

	float MassScale = 1.0f;
	switch (MobKind)
	{
	case EAetheriaMobKind::Swarm:   MassScale = 1.35f; break;
	case EAetheriaMobKind::Ranged:  MassScale = 1.10f; break;
	case EAetheriaMobKind::Bruiser: MassScale = 0.55f; break;
	case EAetheriaMobKind::Boss:    MassScale = 0.15f; break;
	}

	const float EffectiveImpulse = BaseImpulseUU * MassScale;
	FVector Dir = PushDir2D;
	Dir.Z = 0.0f;
	Dir.Normalize();
	if (Dir.IsNearlyZero())
	{
		Dir = -GetActorForwardVector();
	}

	const FVector StartLoc = GetActorLocation();
	const FVector2D Start2D(StartLoc.X, StartLoc.Y);
	const FVector2D Dir2D(Dir.X, Dir.Y);
	const FVector2D ProjectedEnd2D = Start2D + Dir2D * EffectiveImpulse;

	FVector2D Hit2D;
	bool bHitWall = UAetheriaBiomeDatabase::SegmentHitsCoverParapet(LocationIndex, Start2D, ProjectedEnd2D, Hit2D);

	float ArenaRadius = 12000.0f;
	FAetheriaLocationDef LocDef = UAetheriaBiomeDatabase::GetLocation(LocationIndex);
	if (LocDef.ArenaRadiusUU > 1000.0f)
	{
		ArenaRadius = LocDef.ArenaRadiusUU;
	}
	bool bHitEdge = ProjectedEnd2D.Size() > (ArenaRadius - 250.0f);

	if (bHitWall || bHitEdge)
	{
		// Wall Slam!
		TriggerImpactHitstop(0.048f);
		ApplySpellDamage(95.0f, InstigatorWizard, false, 0.75f);

		if (MobKind == EAetheriaMobKind::Boss && UnstoppableTimer <= 0.0f)
		{
			StaggerGauge = FMath::Clamp(StaggerGauge + 28.0f, 0.0f, 100.0f);
			if (StaggerGauge >= 100.0f)
			{
				bIsStaggered = true;
				StaggerTimer = 2.2f;
				bIsTelegraphingSlam = false;
				StaggerGauge = 0.0f;
				TriggerImpactHitstop(0.065f);
			}
		}

		FVector WallImpactPoint = bHitWall ? FVector(Hit2D.X, Hit2D.Y, StartLoc.Z) : FVector(ProjectedEnd2D.X, ProjectedEnd2D.Y, StartLoc.Z);
		if (UWorld* World = GetWorld())
		{
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(World, TEXT("SFX_Wall_Slam"), WallImpactPoint, 0.85f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, WallImpactPoint, 280.0f, AetheriaMindColors::CrispAlabaster, 0.4f);
		}
	}

	LaunchCharacter(Dir * EffectiveImpulse + FVector(0.0f, 0.0f, 120.0f), true, false);
}

void AAetheriaMobCharacter::ApplyTaunt(const FVector& DecoyLocation, float Duration)
{
	bIsTaunted = true;
	TauntTimer = Duration;
	TauntTargetLoc = DecoyLocation;
}

void AAetheriaMobCharacter::ApplyElectroShock(float Duration)
{
	ElectroShockTimer = FMath::Max(ElectroShockTimer, Duration);
	if (MobKind == EAetheriaMobKind::Ranged)
	{
		AttackTimer = AttackCooldown;
	}
}

void AAetheriaMobCharacter::ApplyGravitationalTidalPull(const FVector& ToSingularityDir, float NormalizedProximity)
{
	if (bDying) return;
	const float ClampProx = FMath::Clamp(NormalizedProximity, 0.0f, 1.0f);
	const float WeightResistance = (MobKind == EAetheriaMobKind::Boss) ? 0.25f : ((MobKind == EAetheriaMobKind::Bruiser) ? 0.62f : 1.0f);
	const float TargetStretch = ClampProx * WeightResistance;
	if (TargetStretch >= GravitationalStretchAlpha)
	{
		GravitationalStretchAlpha = TargetStretch;
		GravitationalPullDir = ToSingularityDir.GetSafeNormal();
	}
}

bool AAetheriaMobCharacter::IsAirborneOrJuggled() const
{
	return (GetCharacterMovement() && GetCharacterMovement()->IsFalling()) || AirborneTrackingTimer > 0.0f;
}

void AAetheriaMobCharacter::ApplySpellDamage(float Amount, AActor* InstigatorWizard, bool bIgnite, float StunDuration)
{
	if (HP <= 0.0f)
	{
		return;
	}

	LastInstigator = InstigatorWizard;

	// Cross-Status Synergy: Freeze + Ignite -> Thermal Shock!
	if (bIsFrozen && bIgnite && ThermalShockCooldown <= 0.0f)
	{
		ThermalShockCooldown = 1.2f;
		TriggerImpactHitstop(0.048f);
		const float ThermalShockDmg = (MobKind == EAetheriaMobKind::Boss)
			? FMath::Min(MaxHP * 0.06f, 260.0f)
			: FMath::Min(MaxHP * 0.35f, FMath::Max(120.0f, Amount) * 0.65f);

		if (UWorld* World = GetWorld())
		{
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(World, TEXT("SFX_Synergy_ThermalShock"), GetActorLocation(), 0.90f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 320.0f, AetheriaMindColors::IgnisPlasma, 0.5f);

			if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
			{
				GS->SetSynergyBanner(TEXT("Термошок: Крио-Плазменный Взрыв!"));
			}

			TArray<FOverlapResult> Overlaps;
			FCollisionShape SphereShape = FCollisionShape::MakeSphere(320.0f);
			World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AAetheriaMobCharacter* OtherMob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
				{
					const float TargetCapDmg = (OtherMob->MobKind == EAetheriaMobKind::Boss)
						? FMath::Min(OtherMob->MaxHP * 0.06f, 260.0f)
						: ThermalShockDmg;
					OtherMob->ApplySpellDamage(TargetCapDmg, InstigatorWizard, false, 0.0f);
				}
			}
		}

		if (MobKind == EAetheriaMobKind::Boss && UnstoppableTimer <= 0.0f)
		{
			StaggerGauge = FMath::Clamp(StaggerGauge + 28.0f, 0.0f, 100.0f);
			if (StaggerGauge >= 100.0f)
			{
				bIsStaggered = true;
				StaggerTimer = 2.2f;
				bIsTelegraphingSlam = false;
				StaggerGauge = 0.0f;
				TriggerImpactHitstop(0.065f);
			}
		}
	}

	// 1. Armor Melt: +4% per burn stack (Boss capped at +15%)
	float ArmorMeltPercent = 0.04f * BurnStacks;
	if (MobKind == EAetheriaMobKind::Boss)
	{
		ArmorMeltPercent = FMath::Min(ArmorMeltPercent, 0.15f);
	}

	// 2. ElectroShock: +12%
	const float ElectroMult = (ElectroShockTimer > 0.0f) ? 1.12f : 1.0f;

	// 3. Aerial Juggle: +25%
	const float JuggleMult = IsAirborneOrJuggled() ? 1.25f : 1.0f;

	// 4. Stunned Ranged Exposed: +20%
	const float ExposedMult = (StunTimeRemaining > 0.0f && MobKind == EAetheriaMobKind::Ranged) ? 1.20f : 1.0f;

	// 5. Boss Stagger: +30%
	const float StaggerMult = bIsStaggered ? 1.30f : 1.0f;

	const float FinalDamage = Amount * (1.0f + ArmorMeltPercent) * ElectroMult * JuggleMult * ExposedMult * StaggerMult;

	HitFlashTimer = 0.22f;
	if (FinalDamage > 0.5f)
	{
		HitRecoilTimer = 0.16f;
	}
	if (Amount >= BaseDamage * 2.5f || FinalDamage >= BaseDamage * 2.5f)
	{
		TriggerImpactHitstop(0.048f);
	}
	HP = FMath::Clamp(HP - FinalDamage, 0.0f, MaxHP);

	if (bIgnite)
	{
		ApplyBurnStack(Amount, InstigatorWizard, 1);
	}

	if (StunDuration > 0.0f)
	{
		if (MobKind == EAetheriaMobKind::Boss)
		{
			if (UnstoppableTimer <= 0.0f)
			{
				StaggerGauge = FMath::Clamp(StaggerGauge + StunDuration * 25.0f, 0.0f, 100.0f);
				if (StaggerGauge >= 100.0f)
				{
					bIsStaggered = true;
					StaggerTimer = 2.2f;
					bIsTelegraphingSlam = false;
					StaggerGauge = 0.0f;
					TriggerImpactHitstop(0.065f);
				}
			}
		}
		else if (MobKind == EAetheriaMobKind::Bruiser || bIsEliteLeader)
		{
			float DR = (StunCountInWindow == 0) ? 1.0f : (StunCountInWindow == 1 ? 0.5f : 0.25f);
			StunCountInWindow++;
			if (StunWindowTimer <= 0.0f)
			{
				StunWindowTimer = 5.0f;
			}
			StunTimeRemaining = FMath::Max(StunTimeRemaining, StunDuration * 0.70f * DR);
			AttackTimer = AttackCooldown;
		}
		else if (MobKind == EAetheriaMobKind::Ranged)
		{
			StunTimeRemaining = FMath::Max(StunTimeRemaining, StunDuration * 1.25f);
			AttackTimer = AttackCooldown; // interrupts cast
		}
		else // Swarm
		{
			StunTimeRemaining = FMath::Max(StunTimeRemaining, StunDuration);
			AttackTimer = AttackCooldown;
		}
	}

	if (MobKind == EAetheriaMobKind::Boss)
	{
		if (HP < MaxHP * 0.33f)
		{
			BossPhase = 3;
		}
		else if (HP < MaxHP * 0.66f)
		{
			BossPhase = 2;
		}
	}

	if (HP <= 0.0f)
	{
		Die();
	}
}

void AAetheriaMobCharacter::SetChronoFrozen(bool bFrozen)
{
	bIsFrozen = bFrozen;
	if (bFrozen)
	{
		FrozenTimeRemaining = 5.0f;
		if (GetCharacterMovement())
		{
			GetCharacterMovement()->StopMovementImmediately();
		}

		if (BurnStacks > 0 && ThermalShockCooldown <= 0.0f)
		{
			ThermalShockCooldown = 1.2f;
			const float ThermalShockDmg = (MobKind == EAetheriaMobKind::Boss)
				? FMath::Min(MaxHP * 0.06f, 260.0f)
				: (MaxHP * 0.35f);

			if (UWorld* World = GetWorld())
			{
				AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation(), 320.0f, AetheriaMindColors::IgnisPlasma, 0.5f);

				if (AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
				{
					GS->SetSynergyBanner(TEXT("Термошок: Крио-Плазменный Взрыв!"));
				}

				TArray<FOverlapResult> Overlaps;
				FCollisionShape SphereShape = FCollisionShape::MakeSphere(320.0f);
				World->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);
				for (const FOverlapResult& Overlap : Overlaps)
				{
					if (AAetheriaMobCharacter* OtherMob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
					{
						const float TargetCapDmg = (OtherMob->MobKind == EAetheriaMobKind::Boss)
							? FMath::Min(OtherMob->MaxHP * 0.06f, 260.0f)
							: ThermalShockDmg;
						OtherMob->ApplySpellDamage(TargetCapDmg, LastInstigator.Get(), false, 0.0f);
					}
				}
			}

			if (MobKind == EAetheriaMobKind::Boss && UnstoppableTimer <= 0.0f)
			{
				StaggerGauge = FMath::Clamp(StaggerGauge + 28.0f, 0.0f, 100.0f);
				if (StaggerGauge >= 100.0f)
				{
					bIsStaggered = true;
					StaggerTimer = 2.2f;
					bIsTelegraphingSlam = false;
					StaggerGauge = 0.0f;
				}
			}
		}
	}
	CustomTimeDilation = bFrozen ? 0.0f : 1.0f;
}

void AAetheriaMobCharacter::OnRep_MobKind()
{
	UpdateVisualSilhouette();
	UpdateMaterials();
}

void AAetheriaMobCharacter::OnRep_MobDisplayName()
{
}

void AAetheriaMobCharacter::OnRep_HP()
{
	if (LastSeenHP < 0.0f)
	{
		LastSeenHP = MaxHP;
	}
	if (HP < LastSeenHP && HP > 0.0f)
	{
		HitFlashTimer = 0.22f;
		HitRecoilTimer = 0.16f;
	}
	LastSeenHP = HP;

	if (HP <= 0.0f)
	{
		// Visual death effect on client if destroyed
	}
}

void AAetheriaMobCharacter::OnRep_IsFrozen()
{
	CustomTimeDilation = bIsFrozen ? 0.0f : 1.0f;
}

void AAetheriaMobCharacter::OnRep_BossOrbs()
{
}

void AAetheriaMobCharacter::UpdateVisualSilhouette()
{
	const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 96.0f;
	switch (MobKind)
	{
	case EAetheriaMobKind::Boss:
	{
		// 04:07: every Paragon hero except Gideon is a boss, one per world.
		EAetheriaWorldId Wid = EAetheriaWorldId::Bastion;
		if (UWorld* World = GetWorld())
		{
			if (const AAetheriaGameMode* GM = World->GetAuthGameMode<AAetheriaGameMode>())
			{
				Wid = GM->GetActiveWorld();
			}
		}
		const TCHAR* Pkg = AetheriaWorldDirector::BossPackageForWorld(Wid);
		const TCHAR* Hero = AetheriaWorldDirector::BossHeroForWorld(Wid);
		bUsingSkeletalPresentation = UAetheriaAnimInstance::BindBestPresentation(
			this, HalfH, { FString(Pkg) }, FString(Hero),
			TEXT("/Game/Characters/BossArchon"), TEXT("SKM_BossArchon"));
		break;
	}
	case EAetheriaMobKind::Swarm:
		// SwarmReaper Anims/Montages were quarantined after a broken Interchange
		// reimport. Paragon Minions ship a complete melee mesh + anim set and
		// read as a pack, which fits a swarm better than a frozen Godot mesh.
		bUsingSkeletalPresentation = UAetheriaAnimInstance::BindBestPresentation(
			this, HalfH, { TEXT("/Game/ParagonMinions") }, TEXT("Minion"),
			TEXT("/Game/Characters/SwarmReaper"), TEXT("SKM_SwarmReaper"));
		break;
	case EAetheriaMobKind::Ranged:
		bUsingSkeletalPresentation = UAetheriaAnimInstance::BindBestPresentation(
			this, HalfH, { TEXT("/Game/Characters/RangedLich") }, TEXT("RangedLich"),
			TEXT("/Game/Characters/RangedLich"), TEXT("SKM_RangedLich"));
		break;
	case EAetheriaMobKind::Bruiser:
		bUsingSkeletalPresentation = UAetheriaAnimInstance::BindBestPresentation(
			this, HalfH, { TEXT("/Game/Characters/BruiserJuggernaut") }, TEXT("BruiserJuggernaut"),
			TEXT("/Game/Characters/BruiserJuggernaut"), TEXT("SKM_BruiserJuggernaut"));
		break;
	default:
		break;
	}

	UE_LOG(LogTemp, Display, TEXT("[Aetheria] Mob bind kind=%d skeletal=%d"), static_cast<int32>(MobKind), bUsingSkeletalPresentation ? 1 : 0);
	if (bUsingSkeletalPresentation)
	{
			if (BodyMeshComponent) { BodyMeshComponent->SetVisibility(false); BodyMeshComponent->SetHiddenInGame(true); }
			if (HeadMeshComponent) { HeadMeshComponent->SetVisibility(false); HeadMeshComponent->SetHiddenInGame(true); }
			if (ShoulderLeftMesh) { ShoulderLeftMesh->SetVisibility(false); ShoulderLeftMesh->SetHiddenInGame(true); }
			if (ShoulderRightMesh) { ShoulderRightMesh->SetVisibility(false); ShoulderRightMesh->SetHiddenInGame(true); }
			if (CoreMesh) { CoreMesh->SetVisibility(false); CoreMesh->SetHiddenInGame(true); }
			if (FinMesh1) { FinMesh1->SetVisibility(false); FinMesh1->SetHiddenInGame(true); }
			if (FinMesh2) { FinMesh2->SetVisibility(false); FinMesh2->SetHiddenInGame(true); }
			if (FinMesh3) { FinMesh3->SetVisibility(false); FinMesh3->SetHiddenInGame(true); }
			if (FinMesh4) { FinMesh4->SetVisibility(false); FinMesh4->SetHiddenInGame(true); }
			return;
	}


	if (!BodyMeshComponent)
	{
		return;
	}

	if (HeadMeshComponent) HeadMeshComponent->SetVisibility(false);
	if (ShoulderLeftMesh) ShoulderLeftMesh->SetVisibility(false);
	if (ShoulderRightMesh) ShoulderRightMesh->SetVisibility(false);
	if (CoreMesh) CoreMesh->SetVisibility(false);
	if (FinMesh1) FinMesh1->SetVisibility(false);
	if (FinMesh2) FinMesh2->SetVisibility(false);
	if (FinMesh3) FinMesh3->SetVisibility(false);
	if (FinMesh4) FinMesh4->SetVisibility(false);
	if (OrbMesh1) OrbMesh1->SetVisibility(false);
	if (OrbMesh2) OrbMesh2->SetVisibility(false);
	if (OrbMesh3) OrbMesh3->SetVisibility(false);

	FString SculptedMeshName;
	FString FallbackMesh;
	float SculptedScale = 1.15f;
	float SculptedZOffset = -72.0f;

	if (MobKind == EAetheriaMobKind::Boss)
	{
		const int32 LocClamped = FMath::Clamp(LocationIndex, 0, 9);
		SculptedMeshName = FString::Printf(TEXT("SM_Boss_Loc%02d"), LocClamped);
		FallbackMesh = TEXT("SM_ArchonBoss");
		SculptedScale = 1.90f;
		SculptedZOffset = -125.0f;
	}
	else if (MobKind == EAetheriaMobKind::Swarm)
	{
		SculptedMeshName = FString::Printf(TEXT("SM_Mob_Swarm_%02d"), LocationIndex % 4);
		FallbackMesh = TEXT("SM_VoidFiend");
		SculptedScale = 1.15f;
		SculptedZOffset = -72.0f;
	}
	else if (MobKind == EAetheriaMobKind::Ranged)
	{
		SculptedMeshName = FString::Printf(TEXT("SM_Mob_Ranged_%02d"), LocationIndex % 4);
		FallbackMesh = TEXT("SM_VoidFiend");
		SculptedScale = 1.25f;
		SculptedZOffset = -72.0f;
	}
	else if (MobKind == EAetheriaMobKind::Bruiser)
	{
		SculptedMeshName = FString::Printf(TEXT("SM_Mob_Bruiser_%02d"), LocationIndex % 4);
		FallbackMesh = TEXT("SM_StoneColossus");
		SculptedScale = 1.25f;
		SculptedZOffset = -92.0f;
	}

	UStaticMesh* CustomMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(SculptedMeshName, TEXT(""));
	if (!CustomMesh && !FallbackMesh.IsEmpty())
	{
		CustomMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(FallbackMesh, TEXT(""));
	}

	if (CustomMesh)
	{
		BaseMeshZOffset = SculptedZOffset;
		BaseMeshScale = FVector(SculptedScale);
		BaseMeshRotation = FRotator::ZeroRotator;
		BodyMeshComponent->SetStaticMesh(CustomMesh);
		BodyMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, SculptedZOffset));
		BodyMeshComponent->SetRelativeRotation(BaseMeshRotation);
		BodyMeshComponent->SetRelativeScale3D(BaseMeshScale);
		if (MobKind == EAetheriaMobKind::Boss)
		{
			if (OrbMesh1) { OrbMesh1->SetVisibility(true); OrbMesh1->SetRelativeScale3D(FVector(0.65f)); }
			if (OrbMesh2) { OrbMesh2->SetVisibility(true); OrbMesh2->SetRelativeScale3D(FVector(0.65f)); }
			if (OrbMesh3) { OrbMesh3->SetVisibility(true); OrbMesh3->SetRelativeScale3D(FVector(0.65f)); }
		}
		return;
	}

	// Basic shape fallback
	switch (MobKind)
	{
	case EAetheriaMobKind::Swarm:
	{
		BaseMeshZOffset = -20.0f;
		BaseMeshRotation = FRotator(90.0f, 0.0f, 0.0f);
		BaseMeshScale = FVector(0.55f, 0.55f, 0.85f);
		BodyMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -20.0f));
		BodyMeshComponent->SetRelativeRotation(BaseMeshRotation);
		BodyMeshComponent->SetRelativeScale3D(BaseMeshScale);

		if (FinMesh1)
		{
			FinMesh1->SetVisibility(true);
			FinMesh1->SetRelativeLocation(FVector(35.0f, 35.0f, 0.0f));
			FinMesh1->SetRelativeRotation(FRotator(0.0f, 45.0f, -40.0f));
			FinMesh1->SetRelativeScale3D(FVector(0.25f, 0.15f, 0.55f));
		}
		if (FinMesh2)
		{
			FinMesh2->SetVisibility(true);
			FinMesh2->SetRelativeLocation(FVector(-35.0f, 35.0f, 0.0f));
			FinMesh2->SetRelativeRotation(FRotator(0.0f, 135.0f, -40.0f));
			FinMesh2->SetRelativeScale3D(FVector(0.25f, 0.15f, 0.55f));
		}
		if (FinMesh3)
		{
			FinMesh3->SetVisibility(true);
			FinMesh3->SetRelativeLocation(FVector(-35.0f, -35.0f, 0.0f));
			FinMesh3->SetRelativeRotation(FRotator(0.0f, 225.0f, -40.0f));
			FinMesh3->SetRelativeScale3D(FVector(0.25f, 0.15f, 0.55f));
		}
		if (FinMesh4)
		{
			FinMesh4->SetVisibility(true);
			FinMesh4->SetRelativeLocation(FVector(35.0f, -35.0f, 0.0f));
			FinMesh4->SetRelativeRotation(FRotator(0.0f, 315.0f, -40.0f));
			FinMesh4->SetRelativeScale3D(FVector(0.25f, 0.15f, 0.55f));
		}
		break;
	}
	case EAetheriaMobKind::Bruiser:
	{
		BaseMeshZOffset = -50.0f;
		BaseMeshRotation = FRotator::ZeroRotator;
		BaseMeshScale = FVector(1.6f, 1.6f, 1.9f);
		BodyMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -50.0f));
		BodyMeshComponent->SetRelativeRotation(BaseMeshRotation);
		BodyMeshComponent->SetRelativeScale3D(BaseMeshScale);

		if (HeadMeshComponent)
		{
			HeadMeshComponent->SetVisibility(true);
			HeadMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
			HeadMeshComponent->SetRelativeRotation(FRotator::ZeroRotator);
			HeadMeshComponent->SetRelativeScale3D(FVector(0.75f, 0.75f, 0.75f));
		}
		if (ShoulderLeftMesh)
		{
			ShoulderLeftMesh->SetVisibility(true);
			ShoulderLeftMesh->SetRelativeLocation(FVector(0.0f, -120.0f, 90.0f));
			ShoulderLeftMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 25.0f));
			ShoulderLeftMesh->SetRelativeScale3D(FVector(0.85f, 0.85f, 1.1f));
		}
		if (ShoulderRightMesh)
		{
			ShoulderRightMesh->SetVisibility(true);
			ShoulderRightMesh->SetRelativeLocation(FVector(0.0f, 120.0f, 90.0f));
			ShoulderRightMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -25.0f));
			ShoulderRightMesh->SetRelativeScale3D(FVector(0.85f, 0.85f, 1.1f));
		}
		break;
	}
	case EAetheriaMobKind::Ranged:
	{
		BaseMeshZOffset = -40.0f;
		BaseMeshRotation = FRotator::ZeroRotator;
		BaseMeshScale = FVector(0.65f, 0.65f, 1.6f);
		BodyMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -40.0f));
		BodyMeshComponent->SetRelativeRotation(BaseMeshRotation);
		BodyMeshComponent->SetRelativeScale3D(BaseMeshScale);

		if (HeadMeshComponent)
		{
			HeadMeshComponent->SetVisibility(true);
			HeadMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 130.0f));
			HeadMeshComponent->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
			HeadMeshComponent->SetRelativeScale3D(FVector(0.65f, 0.65f, 1.2f));
		}
		if (CoreMesh)
		{
			CoreMesh->SetVisibility(true);
			CoreMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f));
			CoreMesh->SetRelativeRotation(FRotator::ZeroRotator);
			CoreMesh->SetRelativeScale3D(FVector(0.5f, 0.5f, 0.5f));
		}
		break;
	}
	case EAetheriaMobKind::Boss:
	{
		BaseMeshZOffset = -70.0f;
		BaseMeshRotation = FRotator::ZeroRotator;
		BaseMeshScale = FVector(2.6f, 2.6f, 3.2f);
		BodyMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, -70.0f));
		BodyMeshComponent->SetRelativeRotation(BaseMeshRotation);
		BodyMeshComponent->SetRelativeScale3D(BaseMeshScale);

		if (HeadMeshComponent)
		{
			HeadMeshComponent->SetVisibility(true);
			HeadMeshComponent->SetRelativeLocation(FVector(0.0f, 0.0f, 200.0f));
			HeadMeshComponent->SetRelativeRotation(FRotator(180.0f, 0.0f, 0.0f));
			HeadMeshComponent->SetRelativeScale3D(FVector(1.4f, 1.4f, 1.1f));
		}
		if (ShoulderLeftMesh)
		{
			ShoulderLeftMesh->SetVisibility(true);
			ShoulderLeftMesh->SetRelativeLocation(FVector(0.0f, -190.0f, 140.0f));
			ShoulderLeftMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, 30.0f));
			ShoulderLeftMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.6f));
		}
		if (ShoulderRightMesh)
		{
			ShoulderRightMesh->SetVisibility(true);
			ShoulderRightMesh->SetRelativeLocation(FVector(0.0f, 190.0f, 140.0f));
			ShoulderRightMesh->SetRelativeRotation(FRotator(0.0f, 0.0f, -30.0f));
			ShoulderRightMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.6f));
		}
		if (CoreMesh)
		{
			CoreMesh->SetVisibility(true);
			CoreMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));
			CoreMesh->SetRelativeScale3D(FVector(1.2f, 1.2f, 1.2f));
		}
		if (OrbMesh1)
		{
			OrbMesh1->SetVisibility(true);
			OrbMesh1->SetRelativeScale3D(FVector(0.65f, 0.65f, 0.65f));
		}
		if (OrbMesh2)
		{
			OrbMesh2->SetVisibility(true);
			OrbMesh2->SetRelativeScale3D(FVector(0.65f, 0.65f, 0.65f));
		}
		if (OrbMesh3)
		{
			OrbMesh3->SetVisibility(true);
			OrbMesh3->SetRelativeScale3D(FVector(0.65f, 0.65f, 0.65f));
		}
		break;
	}
	default:
		break;
	}
}

void AAetheriaMobCharacter::UpdateMaterials()
{
	FString TexStem;
	float EmissiveBoost = 0.45f;
	FLinearColor BaseTint = BodyColor;

	if (MobKind == EAetheriaMobKind::Boss)
	{
		const int32 LocClamped = FMath::Clamp(LocationIndex, 0, 9);
		TexStem = FString::Printf(TEXT("Boss_Loc%02d"), LocClamped);
		EmissiveBoost = 0.55f;
		BaseTint = FLinearColor::White;
	}
	else if (MobKind == EAetheriaMobKind::Swarm)
	{
		TexStem = FString::Printf(TEXT("Mob_Swarm_%02d"), LocationIndex % 4);
	}
	else if (MobKind == EAetheriaMobKind::Ranged)
	{
		TexStem = FString::Printf(TEXT("Mob_Ranged_%02d"), LocationIndex % 4);
	}
	else if (MobKind == EAetheriaMobKind::Bruiser)
	{
		TexStem = FString::Printf(TEXT("Mob_Bruiser_%02d"), LocationIndex % 4);
	}

	DynamicBodyMaterial = UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
		this, TexStem, BaseTint, AccentColor, EmissiveBoost);

	if (!DynamicBodyMaterial)
	{
		UMaterialInterface* Obsidian = (MobKind == EAetheriaMobKind::Bruiser)
			? UAetheriaMaterialLibrary::GetGothicStoneMaterial(this)
			: UAetheriaMaterialLibrary::GetDarkIronMaterial(this);
		if (!Obsidian)
		{
			Obsidian = UAetheriaMaterialLibrary::GetObsidianMaterial(this);
		}
		DynamicBodyMaterial = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, Obsidian, BaseTint, AccentColor, (MobKind == EAetheriaMobKind::Boss ? 6.0f : 2.5f), 1.0f);
	}

	if (BodyMeshComponent && DynamicBodyMaterial)
	{
		for (int32 s = 0; s < BodyMeshComponent->GetNumMaterials(); ++s)
		{
			BodyMeshComponent->SetMaterial(s, DynamicBodyMaterial);
		}
	}

	UMaterialInterface* EmissiveBase = UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);
	UMaterialInstanceDynamic* AccentDyn = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, EmissiveBase, AccentColor, AccentColor, (MobKind == EAetheriaMobKind::Boss ? 8.0f : 4.0f), 1.0f);

	if (HeadMeshComponent && DynamicBodyMaterial) HeadMeshComponent->SetMaterial(0, DynamicBodyMaterial);
	if (ShoulderLeftMesh && DynamicBodyMaterial) ShoulderLeftMesh->SetMaterial(0, DynamicBodyMaterial);
	if (ShoulderRightMesh && DynamicBodyMaterial) ShoulderRightMesh->SetMaterial(0, DynamicBodyMaterial);
	if (FinMesh1 && AccentDyn) FinMesh1->SetMaterial(0, AccentDyn);
	if (FinMesh2 && AccentDyn) FinMesh2->SetMaterial(0, AccentDyn);
	if (FinMesh3 && AccentDyn) FinMesh3->SetMaterial(0, AccentDyn);
	if (FinMesh4 && AccentDyn) FinMesh4->SetMaterial(0, AccentDyn);
	if (CoreMesh && AccentDyn) CoreMesh->SetMaterial(0, AccentDyn);

	if (MobKind == EAetheriaMobKind::Boss)
	{
		UMaterialInstanceDynamic* IgnisDyn = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, EmissiveBase, AetheriaMindColors::IgnisPlasma, AetheriaMindColors::IgnisPlasma, 10.0f, 1.0f);
		UMaterialInstanceDynamic* TempestDyn = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, EmissiveBase, AetheriaMindColors::MindCyan, AetheriaMindColors::MindCyan, 10.0f, 1.0f);
		UMaterialInstanceDynamic* GravitonDyn = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, EmissiveBase, AetheriaMindColors::MindViolet, AetheriaMindColors::MindViolet, 10.0f, 1.0f);

		if (OrbMesh1 && IgnisDyn) OrbMesh1->SetMaterial(0, IgnisDyn);
		if (OrbMesh2 && TempestDyn) OrbMesh2->SetMaterial(0, TempestDyn);
		if (OrbMesh3 && GravitonDyn) OrbMesh3->SetMaterial(0, GravitonDyn);
	}
}

void AAetheriaMobCharacter::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	// Hitstop execution: freeze/slow custom time dilation with wall-clock time
	if (HitstopTimer > 0.0f)
	{
		HitstopTimer = FMath::Max(0.0f, HitstopTimer - FApp::GetDeltaTime());
		if (!bIsFrozen)
		{
			CustomTimeDilation = 0.08f;
		}
		if (HitstopTimer <= 0.0f && !bIsFrozen)
		{
			CustomTimeDilation = (SlowRecoveryTimer > 0.0f) ? 0.65f : 1.0f;
		}
	}

	// Timers
	AttackAnimTimer = FMath::Max(0.0f, AttackAnimTimer - DeltaTime);
	HitFlashTimer = FMath::Max(0.0f, HitFlashTimer - DeltaTime);
	HitRecoilTimer = FMath::Max(0.0f, HitRecoilTimer - DeltaTime);
	LandingSquashTimer = FMath::Max(0.0f, LandingSquashTimer - DeltaTime);
	SlamSnapTimer = FMath::Max(0.0f, SlamSnapTimer - DeltaTime);
	ThermalShockCooldown = FMath::Max(0.0f, ThermalShockCooldown - DeltaTime);
	ElectroShockTimer = FMath::Max(0.0f, ElectroShockTimer - DeltaTime);
	AirborneTrackingTimer = FMath::Max(0.0f, AirborneTrackingTimer - DeltaTime);
	RiftSpeedBurstTimer = FMath::Max(0.0f, RiftSpeedBurstTimer - DeltaTime);
	SlowRecoveryTimer = FMath::Max(0.0f, SlowRecoveryTimer - DeltaTime);
	GravitationalStretchAlpha = FMath::Max(0.0f, GravitationalStretchAlpha - DeltaTime * 4.5f);

	StunWindowTimer = FMath::Max(0.0f, StunWindowTimer - DeltaTime);
	if (StunWindowTimer <= 0.0f)
	{
		StunCountInWindow = 0;
	}

	if (UnstoppableTimer > 0.0f)
	{
		UnstoppableTimer = FMath::Max(0.0f, UnstoppableTimer - DeltaTime);
	}

	if (bIsStaggered)
	{
		StaggerTimer -= DeltaTime;
		if (StaggerTimer <= 0.0f)
		{
			bIsStaggered = false;
			UnstoppableTimer = 6.0f;
		}
	}
	StaggerPoseAlpha = FMath::FInterpTo(StaggerPoseAlpha, bIsStaggered ? 1.0f : 0.0f, DeltaTime, 10.0f);

	if (bPrevTelegraphingSlam && !bIsTelegraphingSlam && !bIsStaggered && MobKind == EAetheriaMobKind::Boss)
	{
		SlamSnapTimer = 0.38f;
	}
	bPrevTelegraphingSlam = bIsTelegraphingSlam;

	if (bIsTaunted)
	{
		TauntTimer -= DeltaTime;
		if (TauntTimer <= 0.0f)
		{
			bIsTaunted = false;
		}
	}

	// Aerial Juggle & Touchdown Squash / Dust Shockwave
	const bool bNowFalling = GetCharacterMovement() ? GetCharacterMovement()->IsFalling() : false;
	if (bWasAirborneLastFrame && !bNowFalling)
	{
		LandingSquashTimer = 0.24f;
		if (UWorld* World = GetWorld())
		{
			const float RingRad = (MobKind == EAetheriaMobKind::Boss) ? 340.0f : ((MobKind == EAetheriaMobKind::Bruiser) ? 230.0f : 130.0f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(World, GetActorLocation() - FVector(0.0f, 0.0f, 55.0f), RingRad, AccentColor, 0.34f);
		}
		if (HasAuthority() && MobKind == EAetheriaMobKind::Bruiser)
		{
			ApplySpellDamage(45.0f, LastInstigator.Get());
		}
	}
	bWasAirborneLastFrame = bNowFalling;

	// Typed spawn entrance: burrow / materialize / crash / circle / boss.
	float EmergenceZ = 0.0f;
	if (EmergenceTimer > 0.0f)
	{
		const float Duration = FMath::Max(0.05f, EmergenceTimer + DeltaTime);
		EmergenceTimer -= DeltaTime;
		const float Alpha = FMath::Clamp(1.0f - (EmergenceTimer / FMath::Max(0.05f, Duration + (EmergenceTimer))), 0.0f, 1.0f);
		// Recompute alpha from remaining vs kind duration stored in the current timer start is awkward;
		// use a smooth step based on remaining time relative to a generous max.
		const float KindDur = (MobKind == EAetheriaMobKind::Boss) ? 2.2f : (MobKind == EAetheriaMobKind::Bruiser ? 0.95f : 0.90f);
		const float A = FMath::Clamp(1.0f - (EmergenceTimer / KindDur), 0.0f, 1.0f);
		const float Ease = A * A * (3.0f - 2.0f * A);
		if (MobKind == EAetheriaMobKind::Bruiser && !bIsEliteLeader)
		{
			const FVector P = GetActorLocation();
			SetActorLocation(FVector(P.X, P.Y, FMath::Lerp(EntranceStartZ + 900.0f, EntranceStartZ, Ease)));
			if (EmergenceTimer <= 0.0f)
			{
				UAetheriaPackVFX::PlaySpawnEntrance(this, MobKind, FVector(P.X, P.Y, EntranceStartZ), false, false);
			}
		}
		else if (MobKind == EAetheriaMobKind::Ranged && !bIsEliteLeader)
		{
			if (GetMesh() && A > 0.35f)
			{
				GetMesh()->SetVisibility(true, true);
			}
		}
		else
		{
			EmergenceZ = FMath::Lerp(-200.0f, 0.0f, Ease);
		}
		if (EmergenceTimer <= 0.0f && bEntranceLocked)
		{
			bEntranceLocked = false;
			if (GetMesh()) GetMesh()->SetVisibility(true, true);
			if (UCharacterMovementComponent* Mov = GetCharacterMovement())
			{
				Mov->SetDefaultMovementMode();
			}
		}
	}

	if (DynamicBodyMaterial)
	{
		const float FlashStrength = FMath::Clamp(HitFlashTimer / 0.22f, 0.0f, 1.0f);
		DynamicBodyMaterial->SetScalarParameterValue(FName("HitFlash"), FlashStrength);
	}

	// Procedural Animation & Kinematics
	const float TimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	const float HorizSpeed = GetVelocity().Size2D();
	const float MaxSpeed = GetCharacterMovement() ? GetCharacterMovement()->MaxWalkSpeed : 500.0f;
	const float SpeedRatio = bNowFalling ? 0.0f : FMath::Clamp(HorizSpeed / FMath::Max(1.0f, MaxSpeed), 0.0f, 1.0f);

	if (WindupAnimTimer > 0.0f) WindupAnimTimer = FMath::Max(0.0f, WindupAnimTimer - DeltaTime);
	if (LungeAnimTimer > 0.0f) LungeAnimTimer = FMath::Max(0.0f, LungeAnimTimer - DeltaTime);

	const float CurMobYaw = GetActorRotation().Yaw;
	const float MobYawDelta = FMath::FindDeltaAngleDegrees(PrevMobYaw, CurMobYaw) / FMath::Max(DeltaTime, 0.001f);
	PrevMobYaw = CurMobYaw;
	const float TargetMobBank = FMath::Clamp(MobYawDelta * 0.045f, -15.0f, 15.0f) * FMath::Max(0.25f, SpeedRatio);
	CurrentBankRoll = FMath::FInterpTo(CurrentBankRoll, TargetMobBank, DeltaTime, 9.0f);

	float DeathAlpha = 0.0f;
	float DissolveSinkZ = 0.0f;
	float DissolveScale = 1.0f;
	float DeathPitch = 0.0f;
	float DeathRoll = 0.0f;
	if (bDying)
	{
		DeathDissolveTimer = FMath::Max(0.0f, DeathDissolveTimer - DeltaTime);
		DeathAlpha = 1.0f - FMath::Clamp(DeathDissolveTimer / 0.95f, 0.0f, 1.0f);

		// Directional knockback topple away from LastInstigator
		const float FallPhase = FMath::Clamp(DeathAlpha / 0.55f, 0.0f, 1.0f);
		const float BaseFallAngle = FMath::Lerp(0.0f, 82.0f, FallPhase);
		DeathPitch = BaseFallAngle * DeathFallAxis.Y;
		DeathRoll = BaseFallAngle * DeathFallAxis.X * 0.45f;

		// Subtle ground bounce around 0.55
		float GroundBounceZ = 0.0f;
		if (DeathAlpha >= 0.50f && DeathAlpha <= 0.65f)
		{
			const float BounceT = (DeathAlpha - 0.50f) / 0.15f;
			GroundBounceZ = FMath::Sin(BounceT * PI) * 12.0f;
		}

		// Molten/arcane HitFlash char surge
		if (DeathAlpha < 0.65f)
		{
			HitFlashTimer = FMath::Max(HitFlashTimer, (1.0f - DeathAlpha / 0.65f) * 0.45f);
		}

		// Ash dissolve shrink ONLY in final 0.72..1.0 phase
		if (DeathAlpha > 0.72f)
		{
			const float DissolvePhase = (DeathAlpha - 0.72f) / 0.28f;
			DissolveScale = FMath::Lerp(1.0f, 0.08f, DissolvePhase);
			DissolveSinkZ = -75.0f * DissolvePhase + GroundBounceZ;
		}
		else
		{
			DissolveScale = 1.0f;
			DissolveSinkZ = GroundBounceZ;
		}

		if (DeathDissolveTimer <= 0.0f)
		{
			Destroy();
			return;
		}
	}

	// Shared secondary modifiers (Hit Recoil, Landing Squash, Airborne Tumble, Gravitational Spaghettification)
	float RecoilPitch = 0.0f;
	float RecoilX = 0.0f;
	if (HitRecoilTimer > 0.0f && !bDying)
	{
		const float RP = FMath::Sin(FMath::Clamp(HitRecoilTimer / 0.16f, 0.0f, 1.0f) * PI);
		RecoilPitch = 9.5f * RP;
		RecoilX = -10.0f * RP;
	}

	FVector AnimScaleMult(1.0f, 1.0f, 1.0f);
	if (LandingSquashTimer > 0.0f)
	{
		const float LS = FMath::Clamp(LandingSquashTimer / 0.24f, 0.0f, 1.0f);
		const float SquashCurve = FMath::Sin(LS * PI);
		AnimScaleMult.X *= (1.0f + 0.18f * SquashCurve);
		AnimScaleMult.Y *= (1.0f + 0.18f * SquashCurve);
		AnimScaleMult.Z *= (1.0f - 0.22f * SquashCurve);
	}

	float AirPitch = 0.0f;
	float AirRoll = 0.0f;
	if (bNowFalling)
	{
		const float VelZ = GetVelocity().Z;
		AirPitch = FMath::Clamp(VelZ * 0.025f, -26.0f, 24.0f);
		AirRoll = FMath::Sin(TimeSec * 12.0f) * 14.0f;
		const float StretchZ = 1.0f + FMath::Clamp(FMath::Abs(VelZ) / 1100.0f, 0.0f, 0.22f);
		AnimScaleMult.Z *= StretchZ;
		AnimScaleMult.X /= FMath::Sqrt(StretchZ);
		AnimScaleMult.Y /= FMath::Sqrt(StretchZ);
	}

	float GravPitch = 0.0f;
	float GravRoll = 0.0f;
	float GravLiftZ = 0.0f;
	if (GravitationalStretchAlpha > 0.01f)
	{
		const FVector LocalPull = GetActorQuat().UnrotateVector(GravitationalPullDir);
		GravPitch = (-LocalPull.Z * 30.0f + LocalPull.X * 24.0f) * GravitationalStretchAlpha;
		GravRoll = LocalPull.Y * 24.0f * GravitationalStretchAlpha + FMath::Sin(TimeSec * 28.0f) * 4.0f * GravitationalStretchAlpha;
		GravLiftZ = ((MobKind == EAetheriaMobKind::Boss) ? 14.0f : 38.0f) * GravitationalStretchAlpha;
		AnimScaleMult.X *= (1.0f + 0.55f * GravitationalStretchAlpha);
		AnimScaleMult.Y *= (1.0f - 0.28f * GravitationalStretchAlpha);
		AnimScaleMult.Z *= (1.0f + 0.30f * GravitationalStretchAlpha);
	}

	// Limb animation helper for fallback sculpted meshes
	auto AnimateVisibleLimbs = [&](float Phase, float AtkNorm, bool bAtkActive, float StaggerA)
	{
		const float Swing = FMath::Sin(Phase) * 14.0f * SpeedRatio;
		const float AtkSwing = bAtkActive ? ((AtkNorm < 0.35f) ? FMath::Lerp(0.0f, 28.0f, AtkNorm / 0.35f) : FMath::Lerp(28.0f, -34.0f, (AtkNorm - 0.35f) / 0.65f)) : 0.0f;
		const float StaggerDroop = -18.0f * StaggerA;

		if (ShoulderLeftMesh && ShoulderLeftMesh->IsVisible())
		{
			ShoulderLeftMesh->SetRelativeRotation(FRotator(Swing + AtkSwing + StaggerDroop, 0.0f, 25.0f));
		}
		if (ShoulderRightMesh && ShoulderRightMesh->IsVisible())
		{
			ShoulderRightMesh->SetRelativeRotation(FRotator(-Swing + AtkSwing + StaggerDroop, 0.0f, -25.0f));
		}
		if (HeadMeshComponent && HeadMeshComponent->IsVisible())
		{
			const float BaseHeadPitch = (MobKind == EAetheriaMobKind::Ranged || MobKind == EAetheriaMobKind::Boss) ? 180.0f : 0.0f;
			HeadMeshComponent->SetRelativeRotation(FRotator(BaseHeadPitch + StaggerDroop * 0.8f + RecoilPitch, FMath::Sin(TimeSec * 2.4f) * 5.0f * (1.0f - StaggerA), 0.0f));
		}
	};

	if (MobKind == EAetheriaMobKind::Bruiser)
	{
		WalkPhase += DeltaTime * SpeedRatio * 5.2f;
		const float StompSin = FMath::Sin(WalkPhase);
		const float HeavyBob = FMath::Pow(FMath::Abs(StompSin), 1.7f) * 10.0f * SpeedRatio;
		const float HeavyRoll = StompSin * 5.5f * SpeedRatio + CurrentBankRoll;
		const float PitchTilt = -6.5f * SpeedRatio;

		// Footstep dust ring on stomp footfall
		if (SpeedRatio > 0.35f && PrevStrideSin > 0.0f && StompSin <= 0.0f)
		{
			AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation() + FVector(0.0f, 0.0f, -70.0f), 65.0f, FLinearColor(0.24f, 0.20f, 0.16f, 0.35f), 0.22f);
		}
		PrevStrideSin = StompSin;

		float AttackLunge = 0.0f;
		float AttackPitch = 0.0f;
		float AtkNorm = 0.0f;
		const bool bAtkActive = (AttackAnimTimer > 0.0f);
		if (bAtkActive)
		{
			AtkNorm = 1.0f - FMath::Clamp(AttackAnimTimer / 0.35f, 0.0f, 1.0f);
			AttackLunge = FMath::Sin(AtkNorm * PI) * 32.0f;
			AttackPitch = (AtkNorm < 0.35f) ? FMath::Lerp(0.0f, 14.0f, AtkNorm / 0.35f) : FMath::Lerp(14.0f, -18.0f, (AtkNorm - 0.35f) / 0.65f);
		}

		const float StaggerPitch = -20.0f * StaggerPoseAlpha;
		const float StaggerZ = -18.0f * StaggerPoseAlpha;

		if (BodyMeshComponent)
		{
			BodyMeshComponent->SetRelativeLocation(FVector(AttackLunge + RecoilX, 0.0f, BaseMeshZOffset + HeavyBob + EmergenceZ + DissolveSinkZ + GravLiftZ + StaggerZ));
			BodyMeshComponent->SetRelativeRotation(BaseMeshRotation + FRotator(PitchTilt + AttackPitch + RecoilPitch + AirPitch + GravPitch + StaggerPitch + DeathPitch, 0.0f, HeavyRoll + AirRoll + GravRoll + DeathRoll));
			BodyMeshComponent->SetRelativeScale3D(BaseMeshScale * AnimScaleMult * DissolveScale);
		}
		AnimateVisibleLimbs(WalkPhase, AtkNorm, bAtkActive, StaggerPoseAlpha);
	}
	else if (MobKind == EAetheriaMobKind::Boss)
	{
		WalkPhase += DeltaTime * SpeedRatio * 4.8f;
		const float BossBob = FMath::Abs(FMath::Sin(WalkPhase)) * 6.0f * SpeedRatio;
		const float BossRoll = FMath::Sin(WalkPhase) * 2.8f * SpeedRatio + CurrentBankRoll * 0.6f;

		// Causal Slam Choreography: +22 deg rear-back anticipation -> -28 deg violent smash snap -> power3.out settle
		float TelegraphPitch = 0.0f;
		float TelegraphZ = 0.0f;
		float SlamTremorZ = 0.0f;
		if (bIsTelegraphingSlam)
		{
			TelegraphPitch = 22.0f + FMath::Sin(TimeSec * 28.0f) * 2.5f;
			TelegraphZ = 28.0f;
		}
		else if (SlamSnapTimer > 0.0f)
		{
			const float SnapT = 1.0f - FMath::Clamp(SlamSnapTimer / 0.38f, 0.0f, 1.0f);
			if (SnapT < 0.25f)
			{
				const float Strike = SnapT / 0.25f;
				TelegraphPitch = FMath::Lerp(22.0f, -28.0f, Strike);
				TelegraphZ = FMath::Lerp(28.0f, -22.0f, Strike);
			}
			else
			{
				const float Rec = (SnapT - 0.25f) / 0.75f;
				const float EaseOut = 1.0f - FMath::Pow(1.0f - Rec, 3.0f);
				TelegraphPitch = FMath::Lerp(-28.0f, 0.0f, EaseOut);
				TelegraphZ = FMath::Lerp(-22.0f, 0.0f, EaseOut);
				const float SmashImpact = 1.0f - EaseOut;
				SlamTremorZ = FMath::Sin(TimeSec * 50.0f) * 5.0f * SmashImpact;
				AnimScaleMult.X *= (1.0f + 0.14f * SmashImpact);
				AnimScaleMult.Y *= (1.0f + 0.14f * SmashImpact);
				AnimScaleMult.Z *= (1.0f - 0.14f * SmashImpact);
			}
		}

		// Boss Stagger slumped knee collapse
		const float StaggerPitch = -28.0f * StaggerPoseAlpha;
		const float StaggerZ = -44.0f * StaggerPoseAlpha;
		const float StaggerRoll = FMath::Sin(TimeSec * 14.0f) * 5.5f * StaggerPoseAlpha;

		float AttackLunge = 0.0f;
		float AttackPitch = 0.0f;
		float AtkNorm = 0.0f;
		const bool bAtkActive = (AttackAnimTimer > 0.0f);
		if (bAtkActive)
		{
			AtkNorm = 1.0f - FMath::Clamp(AttackAnimTimer / 0.42f, 0.0f, 1.0f);
			AttackLunge = FMath::Sin(AtkNorm * PI) * 38.0f;
			AttackPitch = (AtkNorm < 0.35f) ? FMath::Lerp(0.0f, 14.0f, AtkNorm / 0.35f) : FMath::Lerp(14.0f, -18.0f, (AtkNorm - 0.35f) / 0.65f);
		}

		if (BodyMeshComponent)
		{
			BodyMeshComponent->SetRelativeLocation(FVector(AttackLunge + RecoilX, 0.0f, BaseMeshZOffset + BossBob + TelegraphZ + SlamTremorZ + StaggerZ + EmergenceZ + DissolveSinkZ + GravLiftZ));
			BodyMeshComponent->SetRelativeRotation(BaseMeshRotation + FRotator(TelegraphPitch + AttackPitch + StaggerPitch + RecoilPitch * 0.4f + GravPitch + DeathPitch, 0.0f, BossRoll + StaggerRoll + GravRoll + DeathRoll));
			BodyMeshComponent->SetRelativeScale3D(BaseMeshScale * AnimScaleMult * DissolveScale);
		}
		AnimateVisibleLimbs(WalkPhase, AtkNorm, bAtkActive, StaggerPoseAlpha);

		// Orbiting orbs (collapse inward and hide during death dissolve)
		OrbitAngle += DeltaTime * (bDying ? 240.0f : 80.0f);
		const float Rad = FMath::DegreesToRadians(OrbitAngle);
		const float OrbitDist = bDying ? FMath::Lerp(260.0f, 15.0f, DeathAlpha) : 260.0f;
		const float OrbScale = bDying ? FMath::Lerp(0.65f, 0.02f, DeathAlpha) : 0.65f;
		const bool bShowOrbs = (!bDying || DeathAlpha < 0.88f);
		const FVector Center = GetActorLocation() + FVector(0.0f, 0.0f, 120.0f + StaggerZ + DissolveSinkZ);

		UStaticMeshComponent* Orbs[3] = { OrbMesh1, OrbMesh2, OrbMesh3 };
		const float PhaseOffsets[3] = { 0.0f, 2.0944f, 4.1888f };
		for (int32 oi = 0; oi < 3; ++oi)
		{
			if (Orbs[oi])
			{
				Orbs[oi]->SetVisibility(bShowOrbs);
				if (bShowOrbs)
				{
					Orbs[oi]->SetWorldLocation(Center + FVector(FMath::Cos(Rad + PhaseOffsets[oi]) * OrbitDist, FMath::Sin(Rad + PhaseOffsets[oi]) * OrbitDist, 0.0f));
					Orbs[oi]->SetRelativeScale3D(FVector(OrbScale));
				}
			}
		}
	}
	else if (MobKind == EAetheriaMobKind::Swarm)
	{
		WalkPhase += DeltaTime * SpeedRatio * 15.0f;
		const float Bob = FMath::Abs(FMath::Sin(WalkPhase)) * 4.5f * SpeedRatio;
		const float Roll = FMath::Sin(WalkPhase * 1.4f) * 6.0f * SpeedRatio + CurrentBankRoll;
		const float PredatoryPitch = -12.0f - 8.0f * SpeedRatio;

		float AttackLunge = 0.0f;
		float AttackPitch = 0.0f;
		float AtkNorm = 0.0f;
		const bool bAtkActive = (AttackAnimTimer > 0.0f);
		if (bAtkActive)
		{
			AtkNorm = 1.0f - FMath::Clamp(AttackAnimTimer / 0.30f, 0.0f, 1.0f);
			AttackLunge = FMath::Sin(AtkNorm * PI) * 26.0f;
			AttackPitch = (AtkNorm < 0.32f) ? FMath::Lerp(0.0f, 12.0f, AtkNorm / 0.32f) : FMath::Lerp(12.0f, -16.0f, (AtkNorm - 0.32f) / 0.68f);
		}

		if (BodyMeshComponent)
		{
			BodyMeshComponent->SetRelativeLocation(FVector(AttackLunge + RecoilX, 0.0f, BaseMeshZOffset + Bob + EmergenceZ + DissolveSinkZ + GravLiftZ));
			BodyMeshComponent->SetRelativeRotation(BaseMeshRotation + FRotator(PredatoryPitch + AttackPitch + RecoilPitch + AirPitch + GravPitch + DeathPitch, 0.0f, Roll + AirRoll + GravRoll + DeathRoll));
			BodyMeshComponent->SetRelativeScale3D(BaseMeshScale * AnimScaleMult * DissolveScale);
		}
		AnimateVisibleLimbs(WalkPhase, AtkNorm, bAtkActive, StaggerPoseAlpha);

		// Animate Swarm fins if fallback fins are visible
		if (FinMesh1 && FinMesh1->IsVisible())
		{
			const float FinBeat = FMath::Sin(TimeSec * (10.0f + SpeedRatio * 16.0f)) * 18.0f;
			FinMesh1->SetRelativeRotation(FRotator(FinBeat, 45.0f, -40.0f));
			if (FinMesh2) FinMesh2->SetRelativeRotation(FRotator(-FinBeat, 135.0f, -40.0f));
			if (FinMesh3) FinMesh3->SetRelativeRotation(FRotator(FinBeat, 225.0f, -40.0f));
			if (FinMesh4) FinMesh4->SetRelativeRotation(FRotator(-FinBeat, 315.0f, -40.0f));
		}
	}
	else // Ranged
	{
		WalkPhase += DeltaTime * SpeedRatio * 7.0f;
		const float HoverBob = FMath::Sin(TimeSec * 2.8f) * 8.0f + FMath::Abs(FMath::Sin(WalkPhase)) * 4.0f * SpeedRatio;
		const float Fig8Yaw = FMath::Sin(TimeSec * 2.6f) * 7.5f;
		const float Fig8Roll = FMath::Sin(TimeSec * 5.2f) * 4.5f + CurrentBankRoll;
		const float HoverTilt = -3.5f * SpeedRatio;

		float AttackLunge = 0.0f;
		float TorsoRecoilPitch = 0.0f;
		float AtkNorm = 0.0f;
		const bool bAtkActive = (AttackAnimTimer > 0.0f);
		if (bAtkActive)
		{
			AtkNorm = 1.0f - FMath::Clamp(AttackAnimTimer / 0.35f, 0.0f, 1.0f);
			TorsoRecoilPitch = -16.0f * FMath::Sin(AtkNorm * PI);
			AttackLunge = -12.0f * FMath::Sin(AtkNorm * PI);
		}

		if (BodyMeshComponent)
		{
			BodyMeshComponent->SetRelativeLocation(FVector(AttackLunge + RecoilX, 0.0f, BaseMeshZOffset + HoverBob + EmergenceZ + DissolveSinkZ + GravLiftZ));
			BodyMeshComponent->SetRelativeRotation(BaseMeshRotation + FRotator(HoverTilt + TorsoRecoilPitch + RecoilPitch + AirPitch + GravPitch + DeathPitch, Fig8Yaw, Fig8Roll + AirRoll + GravRoll + DeathRoll));
			BodyMeshComponent->SetRelativeScale3D(BaseMeshScale * AnimScaleMult * DissolveScale);
		}
		AnimateVisibleLimbs(WalkPhase, AtkNorm, bAtkActive, StaggerPoseAlpha);

		if (CoreMesh && CoreMesh->IsVisible())
		{
			const float CorePulse = (HorizSpeed > 25.0f) ? FMath::Sin(WalkPhase * 2.0f) * 10.0f : FMath::Sin(TimeSec * 4.0f) * 5.0f;
			CoreMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 50.0f + CorePulse + DissolveSinkZ));
			CoreMesh->AddLocalRotation(FRotator(0.0f, 95.0f * DeltaTime, 0.0f));
		}
	}

	if (bDying)
	{
		return;
	}

	if (HasAuthority())
	{
		ServerTickAI(DeltaTime);
	}
}

void AAetheriaMobCharacter::ServerTickAI(float DeltaTime)
{
	if (HP <= 0.0f || bIsStaggered)
	{
		return;
	}

	if (bIsFrozen)
	{
		FrozenTimeRemaining -= DeltaTime;
		if (FrozenTimeRemaining <= 0.0f)
		{
			SetChronoFrozen(false);
		}
		return;
	}

	if (StunTimeRemaining > 0.0f)
	{
		StunTimeRemaining -= DeltaTime;
		return;
	}

	if (BurnTimeRemaining > 0.0f)
	{
		BurnTimeRemaining -= DeltaTime;
		const float BiomeDamageMult = (LocationIndex == 2) ? 1.35f : 1.0f;
		const float StackMult = 1.0f + 0.30f * FMath::Max(0, BurnStacks - 1);
		const float Dps = BurnDamagePerSec * StackMult * BiomeDamageMult;
		HP = FMath::Clamp(HP - Dps * DeltaTime, 0.0f, MaxHP);
		if (HP <= 0.0f)
		{
			Die();
			return;
		}
		if (BurnTimeRemaining <= 0.0f)
		{
			BurnStacks = 0;
		}
	}

	const FVector MyLoc = GetActorLocation();

	// Swarm with BurnStacks >= 3: PanicWeave & Wildfire Spread
	if (MobKind == EAetheriaMobKind::Swarm && BurnStacks >= 3)
	{
		PanicWeavePhase += DeltaTime * 9.0f;
		WildfireSpreadTimer -= DeltaTime;
		if (WildfireSpreadTimer <= 0.0f)
		{
			WildfireSpreadTimer = 0.5f;
			if (UWorld* World = GetWorld())
			{
				TArray<FOverlapResult> Overlaps;
				FCollisionShape SphereShape = FCollisionShape::MakeSphere(240.0f);
				World->OverlapMultiByChannel(Overlaps, MyLoc, FQuat::Identity, ECC_Pawn, SphereShape);
				for (const FOverlapResult& Overlap : Overlaps)
				{
					if (AAetheriaMobCharacter* OtherMob = Cast<AAetheriaMobCharacter>(Overlap.GetActor()))
					{
						if (OtherMob != this && OtherMob->MobKind == EAetheriaMobKind::Swarm && OtherMob->BurnStacks == 0)
						{
							OtherMob->ApplyBurnStack(BurnDamagePerSec / 0.15f, LastInstigator.Get(), 1);
						}
					}
				}
			}
		}
	}

	// Cover Avoidance: gently push out of CoverParapets
	const FVector2D Cur2D(MyLoc.X, MyLoc.Y);
	const float CapsuleRadius = GetCapsuleComponent() ? GetCapsuleComponent()->GetScaledCapsuleRadius() : 90.0f;
	const FVector2D Resolved2D = UAetheriaBiomeDatabase::ResolveCoverAvoidance2D(LocationIndex, Cur2D, CapsuleRadius);
	if (FVector2D::DistSquared(Cur2D, Resolved2D) > 1.0f)
	{
		SetActorLocation(FVector(Resolved2D.X, Resolved2D.Y, MyLoc.Z), true);
	}

	AAetheriaWizardCharacter* NearestWizard = nullptr;
	float MinDistSq = MAX_FLT;

	for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
	{
		AAetheriaWizardCharacter* Wiz = *It;
		if (Wiz && Wiz->bIsAlive && Wiz->HP > 0.0f)
		{
			float DistSq = FVector::DistSquared(MyLoc, Wiz->GetActorLocation());
			if (DistSq < MinDistSq)
			{
				MinDistSq = DistSq;
				NearestWizard = Wiz;
			}
		}
	}

	if (!NearestWizard && !bIsTaunted)
	{
		return;
	}

	const FVector TargetLoc = bIsTaunted ? TauntTargetLoc : NearestWizard->GetActorLocation();
	FVector ToTarget = TargetLoc - MyLoc;
	ToTarget.Z = 0.0f;
	const float Dist = ToTarget.Size();
	FVector MoveDir = ToTarget.GetSafeNormal();

	// Swarm in PanicWeave: zigzag movement
	if (MobKind == EAetheriaMobKind::Swarm && BurnStacks >= 3)
	{
		MoveDir = MoveDir.RotateAngleAxis(FMath::Sin(PanicWeavePhase) * 45.0f, FVector::UpVector);
	}

	// Ranged slowly favors high ground / plateau when player is distant
	if (MobKind == EAetheriaMobKind::Ranged && Dist > 900.0f && !bIsTaunted)
	{
		const FVector PlateauLoc = UAetheriaBiomeDatabase::FindNearestPlateauLocation(LocationIndex, MyLoc);
		FVector ToPlateau = (PlateauLoc - MyLoc);
		ToPlateau.Z = 0.0f;
		if (ToPlateau.Size() > 150.0f)
		{
			MoveDir = (MoveDir * 0.45f + ToPlateau.GetSafeNormal() * 0.55f).GetSafeNormal();
		}
	}

	if (!MoveDir.IsNearlyZero())
	{
		FRotator DesiredRot = MoveDir.Rotation();
		DesiredRot.Pitch = 0.0f;
		DesiredRot.Roll = 0.0f;
		SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRot, DeltaTime, 8.0f));
	}

	if (MobKind == EAetheriaMobKind::Boss)
	{
		SlamCooldownTimer -= DeltaTime;
		if (bIsTelegraphingSlam)
		{
			SlamTelegraphTimer -= DeltaTime;
			if (SlamTelegraphTimer <= 0.0f)
			{
				ExecuteBossSlam();
				bIsTelegraphingSlam = false;
			}
			return;
		}
		else if (SlamCooldownTimer <= 0.0f && Dist <= 1400.0f && !bIsTaunted)
		{
			bIsTelegraphingSlam = true;
			SlamTelegraphTimer = 1.2f;
			SlamCooldownTimer = 11.0f;
			SlamTargetLocation = TargetLoc;

			UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_BossRoar"), 0.95f);

			for (TActorIterator<AAetheriaBiomeBuilder> BuilderIt(GetWorld()); BuilderIt; ++BuilderIt)
			{
				BuilderIt->TriggerBossSlamVisual(SlamTargetLocation, 750.0f, AccentColor);
				break;
			}
			return;
		}

		if (NearestWizard)
		{
			OrbCastTimer -= DeltaTime;
			if (OrbCastTimer <= 0.0f)
			{
				OrbCastTimer = bCastsOrbs ? 4.2f : 6.2f;
				ExecuteBossOrbSpell(NearestWizard);
			}
		}
	}

	// Movement speed scaling (Rift breach burst, Prism wall slow)
	if (GetCharacterMovement())
	{
		float SpeedScale = 1.0f;
		if (RiftSpeedBurstTimer > 0.0f && !bIsFrozen)
		{
			SpeedScale *= 1.20f;
		}
		if (SlowRecoveryTimer > 0.0f)
		{
			SpeedScale *= 0.25f;
		}
		GetCharacterMovement()->MaxWalkSpeed = 500.0f * SpeedScale;
	}

	if (Dist > AttackRangeUU * 0.85f)
	{
		AddMovementInput(MoveDir, 1.0f);
	}

	// Emergence blocks melee/ranged attacks until <= 0
	if (EmergenceTimer > 0.0f)
	{
		return;
	}

	// Ranged burning has -25% attack speed (longer cooldown)
	const float EffectiveCooldown = (MobKind == EAetheriaMobKind::Ranged && BurnStacks > 0) ? AttackCooldown * 1.333f : AttackCooldown;

	AttackTimer -= DeltaTime;
	if (AttackTimer <= 0.0f && Dist <= AttackRangeUU && NearestWizard)
	{
		AttackTimer = EffectiveCooldown;
		if (MobKind == EAetheriaMobKind::Ranged)
		{
			const float KiteDistance = Dist;
			if (KiteDistance < 850.0f)
			{
				AddMovementInput((GetActorLocation() - NearestWizard->GetActorLocation()).GetSafeNormal(), 1.0f);
			}
			ExecuteRangedAttack(NearestWizard);
		}
		else if (MobKind == EAetheriaMobKind::Bruiser && Dist > 450.0f && Dist < 1400.0f)
		{
			LaunchCharacter((NearestWizard->GetActorLocation() - GetActorLocation()).GetSafeNormal() * 1400.0f, true, false);
			PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::AttackSlam));
			ExecuteMeleeAttack(NearestWizard);
		}
		else
		{
			ExecuteMeleeAttack(NearestWizard);
		}
	}
}

void AAetheriaMobCharacter::ExecuteMeleeAttack(AAetheriaWizardCharacter* TargetWizard)
{
	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::AttackMelee));
	if (!TargetWizard || !TargetWizard->bIsAlive)
	{
		return;
	}

	AttackAnimTimer = 0.35f;
	TargetWizard->ApplyDamageToWizard(BaseDamage);
}

void AAetheriaMobCharacter::ExecuteRangedAttack(AAetheriaWizardCharacter* TargetWizard)
{
	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::CastPrimary));
	if (!TargetWizard || !TargetWizard->bIsAlive)
	{
		return;
	}

	AttackAnimTimer = 0.35f;
	FVector MuzzleLoc = GetActorLocation() + GetActorForwardVector() * 80.0f + FVector(0.0f, 0.0f, 50.0f);
	FVector ShootDir = (TargetWizard->GetActorLocation() - MuzzleLoc).GetSafeNormal();

	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = this;
	SpawnParams.Instigator = this;

	AAetheriaSpellProjectile* Bolt = GetWorld()->SpawnActor<AAetheriaSpellProjectile>(MuzzleLoc, ShootDir.Rotation(), SpawnParams);
	if (Bolt)
	{
		Bolt->InitializeProjectile(
			EAetheriaSpellId::None,
			this,
			ShootDir,
			BaseDamage,
			1800.0f,
			35.0f,
			AccentColor,
			AccentColor,
			true,
			false);
	}
	else
	{
		TargetWizard->ApplyDamageToWizard(BaseDamage);
	}
}

void AAetheriaMobCharacter::ExecuteBossSlam()
{
	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::AttackSlam));
	SlamSnapTimer = 0.38f;
	AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), SlamTargetLocation, 800.0f, AccentColor, 0.8f);

	const float SlamRadiusSq = 800.0f * 800.0f;
	const float SlamDamage = BaseDamage * 2.2f;

	for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
	{
		AAetheriaWizardCharacter* Wiz = *It;
		if (Wiz && Wiz->bIsAlive && Wiz->HP > 0.0f)
		{
			if (FVector::DistSquared(SlamTargetLocation, Wiz->GetActorLocation()) <= SlamRadiusSq)
			{
				Wiz->ApplyDamageToWizard(SlamDamage);
			}
		}
	}
}

void AAetheriaMobCharacter::ExecuteBossOrbSpell(AAetheriaWizardCharacter* TargetWizard)
{
	if (!TargetWizard || !TargetWizard->bIsAlive || !GetWorld())
	{
		return;
	}

	if (BossOrbs.Num() > 0)
	{
		EAetheriaOrb First = BossOrbs[0];
		BossOrbs.RemoveAt(0);
		BossOrbs.Add(First);
	}

	BossSpellCycle = (BossSpellCycle + 1) % 3;
	const int32 SpellCycle = BossSpellCycle;
	AttackAnimTimer = 0.42f;
	WindupAnimTimer = 0.25f;
	LungeAnimTimer = 0.22f;

	const FVector MyLoc = GetActorLocation();
	const FVector TargetLoc = TargetWizard->GetActorLocation();
	const FVector AimDir = (TargetLoc - MyLoc).GetSafeNormal2D();

	auto SpawnBossProjectile = [this](const FVector& Origin, const FVector& Dir, float Dmg, float Speed, float Radius, const FLinearColor& Col, EAetheriaSpellId SpId = EAetheriaSpellId::None)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		SpawnParams.Instigator = this;
		if (AAetheriaSpellProjectile* Bolt = GetWorld()->SpawnActor<AAetheriaSpellProjectile>(Origin, Dir.Rotation(), SpawnParams))
		{
			Bolt->InitializeProjectile(SpId, this, Dir, Dmg, Speed, Radius, Col, AccentColor, true, false);
		}
	};

	const FVector Muzzle = MyLoc + FVector(0.0f, 0.0f, 85.0f);

	// Unique signature mechanics per Biome Location (0..9)
	switch (LocationIndex)
	{
	case 0: // Shattered Aether Bastion — Cross-salvo of 4 ether lances
	{
		for (int32 i = 0; i < 4; ++i)
		{
			const float YawOff = -24.0f + i * 16.0f;
			SpawnBossProjectile(Muzzle, AimDir.RotateAngleAxis(YawOff, FVector::UpVector), BaseDamage * 1.15f, 1650.0f, 48.0f, AetheriaMindColors::MindCyan);
		}
		return;
	}
	case 1: // Ash Caldera of Ignis — 8-way magma nova burst
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), MyLoc, 560.0f, AetheriaMindColors::IgnisPlasma, 0.55f);
		for (int32 i = 0; i < 8; ++i)
		{
			const FVector Dir = FVector::ForwardVector.RotateAngleAxis(i * 45.0f, FVector::UpVector);
			SpawnBossProjectile(Muzzle + Dir * 110.0f, Dir, BaseDamage * 1.25f, 1450.0f, 65.0f, AetheriaMindColors::IgnisPlasma, EAetheriaSpellId::Fireball);
		}
		return;
	}
	case 2: // Crystal Glacier — Cryo Pulse adding ColdStacks
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), TargetLoc, 650.0f, AetheriaMindColors::MindCyan, 0.65f);
		for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
		{
			if (AAetheriaWizardCharacter* Wiz = *It)
			{
				if (Wiz->bIsAlive && FVector::Dist2D(TargetLoc, Wiz->GetActorLocation()) <= 650.0f)
				{
					Wiz->ColdStacks = FMath::Min(5, Wiz->ColdStacks + 2);
					Wiz->ApplyDamageToWizard(BaseDamage * 1.15f);
				}
			}
		}
		return;
	}
	case 3: // Gravity Gardens — Twin Black Hole wells flanking the target
	{
		const FVector RightDir = FVector::CrossProduct(FVector::UpVector, AimDir).GetSafeNormal();
		for (int32 Side = -1; Side <= 1; Side += 2)
		{
			const FVector WellLoc = TargetLoc + RightDir * (Side * 420.0f);
			FActorSpawnParameters P;
			P.Owner = this;
			if (AAetheriaSpellZone* Hole = GetWorld()->SpawnActor<AAetheriaSpellZone>(WellLoc, FRotator::ZeroRotator, P))
			{
				Hole->InitializeZone(EAetheriaSpellId::BlackHole, this, WellLoc, FVector::DownVector, 460.0f, 3.2f, BaseDamage * 0.35f, AetheriaMindColors::MindViolet, AetheriaMindColors::MindViolet, true);
			}
		}
		return;
	}
	case 4: // Storm Necropolis — Arcing lightning strike
	{
		TArray<FVector> Pts;
		Pts.Add(Muzzle + FVector(0.0f, 0.0f, 60.0f));
		Pts.Add(FMath::Lerp(Muzzle, TargetLoc, 0.5f) + FVector(0.0f, 0.0f, 220.0f));
		Pts.Add(TargetLoc + FVector(0.0f, 0.0f, 40.0f));
		AAetheriaLightningArcVFX::SpawnLightningArc(GetWorld(), Pts, AetheriaMindColors::MindCyan, 0.55f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), TargetLoc, 380.0f, AetheriaMindColors::MindCyan, 0.45f);
		if (FVector::Dist2D(TargetLoc, TargetWizard->GetActorLocation()) <= 420.0f)
		{
			TargetWizard->ApplyDamageToWizard(BaseDamage * (TargetWizard->bIsWet ? 1.85f : 1.35f));
		}
		return;
	}
	case 5: // Mirror Labyrinth — 5-ray prismatic fan volley
	{
		for (int32 i = -2; i <= 2; ++i)
		{
			SpawnBossProjectile(Muzzle, AimDir.RotateAngleAxis(i * 16.0f, FVector::UpVector), BaseDamage * 1.15f, 1750.0f, 45.0f, AetheriaMindColors::CrispAlabaster);
		}
		return;
	}
	case 6: // Sunken Astrolabe — Tidal Wave inflicting Wet + focused ray
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), MyLoc, 760.0f, AetheriaMindColors::MindCobalt, 0.65f);
		for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
		{
			if (AAetheriaWizardCharacter* Wiz = *It)
			{
				if (Wiz->bIsAlive && FVector::Dist2D(MyLoc, Wiz->GetActorLocation()) <= 760.0f)
				{
					Wiz->ApplyWizardWet(4.5f);
					Wiz->ApplyDamageToWizard(BaseDamage * 1.1f);
				}
			}
		}
		SpawnBossProjectile(Muzzle, AimDir, BaseDamage * 1.45f, 1950.0f, 55.0f, AetheriaMindColors::MindCyan);
		return;
	}
	case 7: // Obsidian Titan Forge — Double Anvil Seismic Rings
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), MyLoc, 480.0f, AetheriaMindColors::IgnisPlasma, 0.50f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), MyLoc, 920.0f, FLinearColor(1.0f, 0.42f, 0.08f, 1.0f), 0.78f);
		for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
		{
			if (AAetheriaWizardCharacter* Wiz = *It)
			{
				if (Wiz->bIsAlive && FVector::Dist2D(MyLoc, Wiz->GetActorLocation()) <= 920.0f)
				{
					const FVector Push = (Wiz->GetActorLocation() - MyLoc).GetSafeNormal2D();
					Wiz->LaunchCharacter(Push * 560.0f + FVector(0.0f, 0.0f, 220.0f), false, false);
					Wiz->ApplyDamageToWizard(BaseDamage * 1.4f);
				}
			}
		}
		return;
	}
	case 8: // Frozen Time Temple — Temporal Blink + Slowing Anomaly
	{
		const FVector BlinkDest = TargetLoc - AimDir * 450.0f;
		SetActorLocation(FVector(BlinkDest.X, BlinkDest.Y, MyLoc.Z), true);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), MyLoc, 420.0f, AetheriaMindColors::MindCyan, 0.5f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), BlinkDest, 520.0f, AetheriaMindColors::MindViolet, 0.55f);
		if (FVector::Dist2D(BlinkDest, TargetWizard->GetActorLocation()) <= 520.0f)
		{
			TargetWizard->ColdStacks = FMath::Min(5, TargetWizard->ColdStacks + 2);
			TargetWizard->ApplyDamageToWizard(BaseDamage * 1.3f);
		}
		return;
	}
	default: // 9: Heart of the Chrono-Seal — Primal Archmage Triad Cycle
		break;
	}

	if (SpellCycle == 0)
	{
		FVector SkyLoc = TargetLoc + FVector(0.0f, 0.0f, 1500.0f);
		FVector FallDir = FVector(0.0f, 0.0f, -1.0f);
		SpawnBossProjectile(SkyLoc, FallDir, BaseDamage * 2.0f, 1600.0f, 280.0f, AetheriaMindColors::IgnisPlasma, EAetheriaSpellId::ChaosMeteor);
	}
	else if (SpellCycle == 1)
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Owner = this;
		AAetheriaSpellZone* Hole = GetWorld()->SpawnActor<AAetheriaSpellZone>(TargetLoc, FRotator::ZeroRotator, SpawnParams);
		if (Hole)
		{
			Hole->InitializeZone(
				EAetheriaSpellId::BlackHole,
				this,
				TargetLoc,
				FVector::DownVector,
				500.0f,
				4.0f,
				BaseDamage * 0.4f,
				AetheriaMindColors::MindViolet,
				AetheriaMindColors::MindViolet,
				true);
		}
	}
	else
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), TargetLoc, 420.0f, AetheriaMindColors::MindCyan, 0.5f);
		for (int32 i = -1; i <= 1; ++i)
		{
			SpawnBossProjectile(Muzzle, AimDir.RotateAngleAxis(i * 18.0f, FVector::UpVector), BaseDamage * 1.25f, 1750.0f, 55.0f, AetheriaMindColors::MindCyan);
		}
		if (FVector::Dist2D(TargetLoc, TargetWizard->GetActorLocation()) <= 420.0f)
		{
			TargetWizard->ApplyDamageToWizard(BaseDamage * 1.5f);
		}
	}
}

void AAetheriaMobCharacter::Die()
{
	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::Death));
	if (bDying) return;
	bDying = true;
	DeathDissolveTimer = 0.95f;

	if (LastInstigator.IsValid())
	{
		const FVector ToInstigator = (LastInstigator->GetActorLocation() - GetActorLocation()).GetSafeNormal2D();
		const FVector LocalHitDir = GetActorQuat().UnrotateVector(ToInstigator);
		// Directional topple: if hit from front (LocalHitDir.X >= 0), topple backward; if hit from behind, topple forward
		DeathFallAxis = FVector(LocalHitDir.Y, (LocalHitDir.X >= 0.0f) ? 1.0f : -1.0f, 0.0f).GetSafeNormal();
		if (DeathFallAxis.IsNearlyZero())
		{
			DeathFallAxis = FVector::RightVector;
		}
	}
	else
	{
		DeathFallAxis = FVector::RightVector;
	}

	SetReplicateMovement(false);
	if (GetCapsuleComponent())
	{
		GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (GetCharacterMovement())
	{
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
	}

	UAetheriaMaterialLibrary::PlayGameSound2D(this, TEXT("SFX_MobDeath"), 0.80f);

	if (bExplodeOnDeath)
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), 450.0f, AccentColor, 0.7f);

		const float ExpDistSq = 450.0f * 450.0f;
		for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
		{
			AAetheriaWizardCharacter* Wiz = *It;
			if (Wiz && Wiz->bIsAlive && Wiz->HP > 0.0f)
			{
				if (FVector::DistSquared(GetActorLocation(), Wiz->GetActorLocation()) <= ExpDistSq)
				{
					Wiz->ApplyDamageToWizard(BaseDamage * 1.8f);
				}
			}
		}
	}

	if (AAetheriaGameMode* GM = Cast<AAetheriaGameMode>(GetWorld()->GetAuthGameMode()))
	{
		GM->NotifyMobKilled(this, LastInstigator.Get());
	}
}


void AAetheriaMobCharacter::PlayPresentationAction(uint8 Action)
{
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Skel->GetAnimInstance()))
		{
			Anim->PlayAction(static_cast<EAetheriaAnimAction>(Action));
		}
	}
}
