#include "AetheriaSpellActors.h"
#include "AetheriaMaterialLibrary.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Character.h"
#include "UObject/ConstructorHelpers.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "AetheriaBiomeDatabase.h"
#include "AetheriaGameState.h"

#if __has_include("AetheriaMobCharacter.h")
#include "AetheriaMobCharacter.h"
#define HAS_MOB_CHARACTER 1
#else
#define HAS_MOB_CHARACTER 0
#endif

#if __has_include("AetheriaWizardCharacter.h")
#include "AetheriaWizardCharacter.h"
#define HAS_WIZARD_CHARACTER 1
#else
#define HAS_WIZARD_CHARACTER 0
#endif

#if __has_include("AetheriaGameMode.h")
#include "AetheriaGameMode.h"
#define HAS_GAMEMODE 1
#else
#define HAS_GAMEMODE 0
#endif

#if __has_include("AetheriaBiomeBuilder.h")
#include "AetheriaBiomeBuilder.h"
#define HAS_BIOME_BUILDER 1
#else
#define HAS_BIOME_BUILDER 0
#endif

// Cached basic shape meshes loaded in constructor helpers
static UStaticMesh* GEngineSphereMesh = nullptr;
static UStaticMesh* GEngineCylinderMesh = nullptr;
static UStaticMesh* GEngineConeMesh = nullptr;
static UStaticMesh* GEnginePlaneMesh = nullptr;

static void EnsureEngineMeshesLoaded()
{
	if (!IsValid(GEngineSphereMesh))
	{
		GEngineSphereMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		GEngineCylinderMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		GEngineConeMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone"));
		GEnginePlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
	}
}

static float GetNormalizedUnitMeshScale(const UStaticMesh* Mesh)
{
	if (!Mesh) return 1.0f;
	const float MaxDim = Mesh->GetBoundingBox().GetSize().GetMax();
	return (MaxDim > 5.0f) ? (100.0f / MaxDim) : 1.0f;
}

static float SampleArenaSurface(UWorld* World, const FVector2D& Pos2D, FVector* OutNormal = nullptr, int32* OutLocIdx = nullptr)
{
	int32 LocIdx = 0;
	if (World)
	{
		if (const AAetheriaGameState* GS = World->GetGameState<AAetheriaGameState>())
		{
			LocIdx = GS->CurrentLocationIndex;
		}
	}
	if (OutLocIdx) *OutLocIdx = LocIdx;

	const float Zc = UAetheriaBiomeDatabase::GroundZAt(World, LocIdx, Pos2D);
	if (OutNormal)
	{
		const float Zx = UAetheriaBiomeDatabase::GroundZAt(World, LocIdx, Pos2D + FVector2D(32.0f, 0.0f));
		const float Zy = UAetheriaBiomeDatabase::GroundZAt(World, LocIdx, Pos2D + FVector2D(0.0f, 32.0f));
		const FVector Tx(32.0f, 0.0f, Zx - Zc);
		const FVector Ty(0.0f, 32.0f, Zy - Zc);
		FVector N = FVector::CrossProduct(Tx, Ty).GetSafeNormal();
		*OutNormal = N.IsNearlyZero() ? FVector::UpVector : N;
	}
	return Zc;
}

static void AddNearbyWizardsCameraTrauma(UWorld* World, const FVector& Epicenter, float MaxTrauma, float MaxDistUU = 2600.0f)
{
	if (!World) return;
#if HAS_WIZARD_CHARACTER
	for (TActorIterator<AAetheriaWizardCharacter> It(World); It; ++It)
	{
		if (AAetheriaWizardCharacter* Wiz = *It)
		{
			const float Dist = FVector::Dist2D(Epicenter, Wiz->GetActorLocation());
			if (Dist < MaxDistUU)
			{
				const float Falloff = 1.0f - (Dist / MaxDistUU);
				Wiz->AddCameraTrauma(MaxTrauma * Falloff);
			}
		}
	}
#endif
}

static void DealDamageToMobOrActor(AActor* Target, float Amount, AActor* InstigatorWizard, bool bIgnite = false, float StunDuration = 0.0f)
{
	if (!Target || !Target->IsValidLowLevel()) return;

#if HAS_MOB_CHARACTER
	if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Target))
	{
		Mob->ApplySpellDamage(Amount, InstigatorWizard, bIgnite, StunDuration);
		return;
	}
#endif

#if HAS_WIZARD_CHARACTER
	if (AAetheriaWizardCharacter* AllyWiz = Cast<AAetheriaWizardCharacter>(Target))
	{
		if (InstigatorWizard && AllyWiz != InstigatorWizard && AllyWiz->bIsAlive)
		{
			const AAetheriaGameState* GS = AllyWiz->GetWorld() ? AllyWiz->GetWorld()->GetGameState<AAetheriaGameState>() : nullptr;
			const float FFScale = GS ? GS->GetEffectiveFriendlyFireScale() : 0.0f;
			if (FFScale > 0.0f)
			{
				AllyWiz->ApplyDamageToWizard(Amount * FFScale);
				if (bIgnite)
				{
					AllyWiz->ApplyWizardBurn(2.0f, FMath::Max(6.0f, Amount * FFScale * 0.12f));
				}
			}
		}
		return;
	}
#endif

	UGameplayStatics::ApplyDamage(Target, Amount, nullptr, InstigatorWizard, UDamageType::StaticClass());
}

static void DealDamageToWizard(AActor* Target, float Amount)
{
	if (!Target || !Target->IsValidLowLevel()) return;

#if HAS_WIZARD_CHARACTER
	if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(Target))
	{
		Wiz->ApplyDamageToWizard(Amount);
		return;
	}
#endif

	UGameplayStatics::ApplyDamage(Target, Amount, nullptr, nullptr, UDamageType::StaticClass());
}

// ================================================================================================
// AAetheriaSpellProjectile
// ================================================================================================

AAetheriaSpellProjectile::AAetheriaSpellProjectile()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	SetReplicateMovement(true);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	if (SphereFinder.Succeeded()) GEngineSphereMesh = SphereFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	if (CylinderFinder.Succeeded()) GEngineCylinderMesh = CylinderFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeFinder(TEXT("/Engine/BasicShapes/Cone.Cone"));
	if (ConeFinder.Succeeded()) GEngineConeMesh = ConeFinder.Object;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneFinder(TEXT("/Engine/BasicShapes/Plane.Plane"));
	if (PlaneFinder.Succeeded()) GEnginePlaneMesh = PlaneFinder.Object;

	CollisionComp = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionComp"));
	CollisionComp->InitSphereRadius(24.0f);
	CollisionComp->SetCollisionProfileName(TEXT("Projectile"));
	CollisionComp->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionComp->SetGenerateOverlapEvents(true);
	SetRootComponent(CollisionComp);

	CoreMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoreMesh"));
	CoreMesh->SetupAttachment(CollisionComp);
	if (GEngineSphereMesh)
	{
		CoreMesh->SetStaticMesh(GEngineSphereMesh);
	}
	CoreMesh->SetRelativeScale3D(FVector(0.35f));
	CoreMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoreMesh->SetCastShadow(false);

	CoronaMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("CoronaMesh"));
	CoronaMesh->SetupAttachment(CollisionComp);
	if (GEngineSphereMesh)
	{
		CoronaMesh->SetStaticMesh(GEngineSphereMesh);
	}
	CoronaMesh->SetRelativeScale3D(FVector(0.55f, 0.42f, 0.42f));
	CoronaMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CoronaMesh->SetCastShadow(false);

	TrailISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TrailISM"));
	TrailISM->SetupAttachment(CollisionComp);
	TrailISM->SetAbsolute(true, true, true);
	if (GEngineSphereMesh)
	{
		TrailISM->SetStaticMesh(GEngineSphereMesh);
	}
	TrailISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TrailISM->SetCastShadow(false);

	GlowLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("GlowLight"));
	GlowLight->SetupAttachment(CollisionComp);
	GlowLight->Intensity = 1800.0f;
	GlowLight->AttenuationRadius = 380.0f;
	GlowLight->LightColor = FColor(0, 245, 255);

	MovementComp = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("MovementComp"));
	MovementComp->UpdatedComponent = CollisionComp;
	MovementComp->InitialSpeed = 2200.0f;
	MovementComp->MaxSpeed = 2600.0f;
	MovementComp->bRotationFollowsVelocity = true;
	MovementComp->bShouldBounce = false;
	MovementComp->ProjectileGravityScale = 0.0f;
}

void AAetheriaSpellProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAetheriaSpellProjectile, SpellId);
	DOREPLIFETIME(AAetheriaSpellProjectile, Damage);
	DOREPLIFETIME(AAetheriaSpellProjectile, RadiusUU);
	DOREPLIFETIME(AAetheriaSpellProjectile, PrimaryColor);
	DOREPLIFETIME(AAetheriaSpellProjectile, AccentColor);
	DOREPLIFETIME(AAetheriaSpellProjectile, bHasSplitByPrism);
	DOREPLIFETIME(AAetheriaSpellProjectile, bIsEnemyProjectile);
	DOREPLIFETIME(AAetheriaSpellProjectile, bIsHoming);
}

void AAetheriaSpellProjectile::BeginPlay()
{
	Super::BeginPlay();

	CollisionComp->OnComponentBeginOverlap.AddDynamic(this, &AAetheriaSpellProjectile::OnOverlapBegin);
	CollisionComp->OnComponentHit.AddDynamic(this, &AAetheriaSpellProjectile::OnHit);

	const bool bFiery = (SpellId == EAetheriaSpellId::Fireball || SpellId == EAetheriaSpellId::ChaosMeteor);
	UMaterialInterface* BaseMat = bFiery
		? UAetheriaMaterialLibrary::GetFireballMaterial(this)
		: UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this);

	UMaterialInstanceDynamic* MID = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, BaseMat, PrimaryColor, AccentColor, bFiery ? 5.5f : 4.2f, 1.0f);
	if (CoreMesh)
	{
		if (bFiery)
		{
			if (UStaticMesh* Boulder = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CliffBoulder"), TEXT("")))
			{
				CoreMesh->SetStaticMesh(Boulder);
				const float NormScale = GetNormalizedUnitMeshScale(Boulder) * FMath::Clamp(RadiusUU / 115.0f, 0.35f, 1.8f);
				CoreMesh->SetRelativeScale3D(FVector(NormScale));
			}
		}
		if (MID) CoreMesh->SetMaterial(0, MID);
	}

	if (CoronaMesh)
	{
		UMaterialInstanceDynamic* CoronaMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this), PrimaryColor, AccentColor, bFiery ? 5.5f : 3.8f, 0.45f);
		if (CoronaMat) CoronaMesh->SetMaterial(0, CoronaMat);
		const float CoreS = FMath::Clamp(RadiusUU / 115.0f, 0.25f, 1.8f);
		CoronaMesh->SetRelativeScale3D(FVector(CoreS * 1.65f, CoreS * 1.15f, CoreS * 1.15f));
	}

	if (TrailISM)
	{
		TrailISM->ClearInstances();
		TrailHistory.Empty();
		const FVector InitLoc = GetActorLocation();
		UMaterialInstanceDynamic* TrailMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this), PrimaryColor, AccentColor, bFiery ? 5.5f : 4.0f, 0.85f);
		if (TrailMat) TrailISM->SetMaterial(0, TrailMat);

		const int32 TrailCount = bIsEnemyProjectile ? 8 : 14;
		for (int32 i = 0; i < TrailCount; ++i)
		{
			TrailHistory.Add(InitLoc);
			TrailISM->AddInstance(FTransform(FRotator::ZeroRotator, InitLoc, FVector(0.05f)));
		}
	}

	if (GlowLight)
	{
		GlowLight->SetLightColor(PrimaryColor);
		GlowLight->Intensity = bFiery ? 3200.0f : 2200.0f;
	}
}

void AAetheriaSpellProjectile::InitializeProjectile(
	EAetheriaSpellId InSpellId,
	AActor* InOwnerWizard,
	const FVector& Direction,
	float InDamage,
	float InSpeedUU,
	float InRadiusUU,
	FLinearColor InPrimaryColor,
	FLinearColor InAccentColor,
	bool bInEnemy,
	bool bSplitVariant)
{
	SpellId = InSpellId;
	OwnerWizard = InOwnerWizard;
	Damage = InDamage;
	RadiusUU = InRadiusUU;
	PrimaryColor = InPrimaryColor;
	AccentColor = InAccentColor;
	bIsEnemyProjectile = bInEnemy;
	bHasSplitByPrism = bSplitVariant;

	if (MovementComp)
	{
		MovementComp->InitialSpeed = InSpeedUU;
		MovementComp->MaxSpeed = InSpeedUU * 1.35f;
		MovementComp->Velocity = Direction.GetSafeNormal() * InSpeedUU;
	}

	const bool bFiery = (SpellId == EAetheriaSpellId::Fireball || SpellId == EAetheriaSpellId::ChaosMeteor);
	const float ScaleFactor = FMath::Clamp(RadiusUU / 115.0f, 0.25f, 1.8f);
	if (CoreMesh)
	{
		const float MeshNorm = GetNormalizedUnitMeshScale(CoreMesh->GetStaticMesh());
		CoreMesh->SetRelativeScale3D(FVector(ScaleFactor * MeshNorm));
	}
	if (CoronaMesh)
	{
		CoronaMesh->SetRelativeScale3D(FVector(ScaleFactor * (bFiery ? 1.75f : 1.4f), ScaleFactor * 1.15f, ScaleFactor * 1.15f));
	}
}

void AAetheriaSpellProjectile::SetHomingTarget(AActor* Target)
{
	HomingTargetActor = Target;
	bIsHoming = (Target != nullptr);
}

void AAetheriaSpellProjectile::UpdateTrailFX(float DeltaTime)
{
	SpinAngle += DeltaTime * 540.0f;
	if (CoreMesh)
	{
		CoreMesh->SetRelativeRotation(FRotator(SpinAngle, SpinAngle * 0.65f, 0.0f));
	}
	if (CoronaMesh)
	{
		const float Pulse = 1.0f + 0.14f * FMath::Sin(LifeTimer * 28.0f);
		const float BaseS = FMath::Clamp(RadiusUU / 115.0f, 0.25f, 1.8f);
		CoronaMesh->SetRelativeScale3D(FVector(BaseS * 1.65f * Pulse, BaseS * 1.12f, BaseS * 1.12f));
	}

	if (!TrailISM || TrailHistory.Num() == 0) return;

	const FVector CurLoc = GetActorLocation();
	const int32 N = TrailHistory.Num();
	for (int32 i = N - 1; i > 0; --i)
	{
		TrailHistory[i] = FMath::VInterpTo(TrailHistory[i], TrailHistory[i - 1], DeltaTime, 38.0f);
	}
	TrailHistory[0] = CurLoc;

	const float HeadScale = FMath::Clamp(RadiusUU / 115.0f, 0.22f, 1.6f) * 0.42f;
	for (int32 i = 0; i < N; ++i)
	{
		const float T = static_cast<float>(i) / static_cast<float>(FMath::Max(1, N - 1));
		const float S = HeadScale * FMath::Pow(1.0f - T, 1.45f);
		const FVector Jitter = (i > 1)
			? FVector(FMath::Sin(LifeTimer * 24.0f + i * 1.7f) * 8.0f * T, FMath::Cos(LifeTimer * 21.0f + i * 2.1f) * 8.0f * T, 0.0f)
			: FVector::ZeroVector;
		TrailISM->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, TrailHistory[i] + Jitter, FVector(FMath::Max(0.02f, S))), true, false, true);
	}
	TrailISM->MarkRenderStateDirty();
}

void AAetheriaSpellProjectile::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	LifeTimer += DeltaTime;
	if (LifeTimer >= MaxLifetime)
	{
		Detonate();
		return;
	}

	UpdateTrailFX(DeltaTime);

	if (MovementComp)
	{
		int32 LocIdx = 0;
		const FVector CurLoc = GetActorLocation();
		const FVector Velocity = MovementComp->Velocity;
		const FVector PrevLoc = CurLoc - Velocity * DeltaTime;
		const float SurfaceZ = SampleArenaSurface(GetWorld(), FVector2D(CurLoc.X, CurLoc.Y), nullptr, &LocIdx);

		// Skim smoothly above rising terrain/ramps
		if (!bIsHoming && CurLoc.Z < SurfaceZ + 38.0f)
		{
			SetActorLocation(FVector(CurLoc.X, CurLoc.Y, SurfaceZ + 44.0f), false);
		}

		// Parapet & boundary impact physics
		FVector2D Hit2D;
		if (UAetheriaBiomeDatabase::SegmentHitsCoverParapet(LocIdx, FVector2D(PrevLoc.X, PrevLoc.Y), FVector2D(CurLoc.X, CurLoc.Y), Hit2D))
		{
			const FVector WallPt(Hit2D.X, Hit2D.Y, SurfaceZ + 12.0f);
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(GetWorld(), TEXT("SFX_Hit_Stone"), WallPt, 0.55f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), WallPt, 190.0f, AccentColor, 0.35f);
			Detonate();
			return;
		}
	}

	// Auto-acquire nearest mob target for homing Primal Overload / arcane missiles
	if (bIsHoming && MovementComp)
	{
		if (!HomingTargetActor.IsValid() && GetWorld())
		{
			float BestDistSq = FMath::Square(2200.0f);
			AActor* BestMob = nullptr;
#if HAS_MOB_CHARACTER
			for (TActorIterator<AAetheriaMobCharacter> It(GetWorld()); It; ++It)
			{
				AAetheriaMobCharacter* Mob = *It;
				if (Mob && Mob->HP > 0.0f)
				{
					const float DSq = FVector::DistSquared(GetActorLocation(), Mob->GetActorLocation());
					if (DSq < BestDistSq)
					{
						BestDistSq = DSq;
						BestMob = Mob;
					}
				}
			}
#endif
			HomingTargetActor = BestMob;
		}

		if (HomingTargetActor.IsValid())
		{
			const FVector AimPos = HomingTargetActor->GetActorLocation() + FVector(0.0f, 0.0f, 40.0f);
			const FVector ToTarget = (AimPos - GetActorLocation()).GetSafeNormal();
			const FVector CurrentVel = MovementComp->Velocity;
			const float CurrentSpeed = CurrentVel.Size();
			const FVector NewVel = FMath::VInterpTo(CurrentVel.GetSafeNormal(), ToTarget, DeltaTime, 10.5f) * CurrentSpeed;
			MovementComp->Velocity = NewVel;
		}
	}
}

void AAetheriaSpellProjectile::OnOverlapBegin(
	UPrimitiveComponent* OverlappedComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	AActor* SafeOwner = OwnerWizard.Get() ? OwnerWizard.Get() : (GetOwner() ? GetOwner() : GetInstigator());
	if (!OwnerWizard.IsValid() && SafeOwner)
	{
		OwnerWizard = SafeOwner;
	}
	if (!OtherActor || OtherActor == this || OtherActor == SafeOwner || OtherActor == GetOwner() || OtherActor == GetInstigator() || bHasDetonated)
	{
		return;
	}

	// 1. Check interaction with AAetheriaSpellZone
	if (AAetheriaSpellZone* Zone = Cast<AAetheriaSpellZone>(OtherActor))
	{
		// Fireball hitting allied BlackHole ignites Solar Singularity!
		if (SpellId == EAetheriaSpellId::Fireball && Zone->GetSpellId() == EAetheriaSpellId::BlackHole && !Zone->IsBlackHoleIgnited())
		{
			Zone->IgniteSolarSingularity();
			Detonate();
			return;
		}

		// PrismWall splitting allied projectile into 3 refracted fan rays
		if (Zone->GetSpellId() == EAetheriaSpellId::PrismWall)
		{
			if (bIsEnemyProjectile)
			{
				// Enemy projectile absorbed by Prism Wall
				UAetheriaMaterialLibrary::PlayGameSoundAtLocation(GetWorld(), TEXT("SFX_Prism_Absorb"), GetActorLocation(), 0.72f);
				Detonate();
				return;
			}
			else if (!bHasSplitByPrism)
			{
				bHasSplitByPrism = true;
				UAetheriaMaterialLibrary::PlayGameSoundAtLocation(GetWorld(), TEXT("SFX_Prism_Split"), GetActorLocation(), 0.72f);
				const FVector CurDir = MovementComp ? MovementComp->Velocity.GetSafeNormal() : GetActorForwardVector();
				const FVector Up = FVector::UpVector;
				const float Angles[3] = { -18.0f, 0.0f, 18.0f };

				for (float Angle : Angles)
				{
					FVector SplitDir = CurDir.RotateAngleAxis(Angle, Up);
					FActorSpawnParameters SpawnParams;
					SpawnParams.Owner = OwnerWizard.Get();
					SpawnParams.Instigator = Cast<APawn>(OwnerWizard.Get());
					SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

					AAetheriaSpellProjectile* SplitProj = GetWorld()->SpawnActor<AAetheriaSpellProjectile>(
						GetClass(), GetActorLocation() + SplitDir * 40.0f, SplitDir.Rotation(), SpawnParams);
					if (SplitProj)
					{
						float SplitMult = 0.65f;
#if HAS_WIZARD_CHARACTER
						if (AAetheriaWizardCharacter* OwnerWiz = Cast<AAetheriaWizardCharacter>(OwnerWizard.Get()))
						{
							SplitMult *= (1.0f + OwnerWiz->RelicPrismSplitBonus);
						}
#endif
						SplitProj->InitializeProjectile(
							SpellId,
							OwnerWizard.Get(),
							SplitDir,
							Damage * SplitMult,
							MovementComp ? MovementComp->InitialSpeed : 2200.0f,
							RadiusUU * 0.75f,
							PrimaryColor,
							AccentColor,
							false,
							true /* bSplitVariant */
						);
					}
				}
				bHasDetonated = true;
				SetActorEnableCollision(false);
				SetLifeSpan(0.01f);
				return;
			}
		}
	}

	// 2. Enemy projectile hitting Wizard
	if (bIsEnemyProjectile)
	{
		DealDamageToWizard(OtherActor, Damage);
		Detonate();
		return;
	}

	// 3. Allied projectile hitting Mobs
	if (SpellId == EAetheriaSpellId::None)
	{
#if HAS_WIZARD_CHARACTER
		if (AAetheriaWizardCharacter* OwnerWiz = Cast<AAetheriaWizardCharacter>(OwnerWizard.Get()))
		{
			OwnerWiz->Mana = FMath::Min(OwnerWiz->MaxMana, OwnerWiz->Mana + 6.0f);
		}
#endif
#if HAS_MOB_CHARACTER
		if (const AAetheriaMobCharacter* HitMob = Cast<AAetheriaMobCharacter>(OtherActor))
		{
			const bool bArmored = (HitMob->MobKind == EAetheriaMobKind::Bruiser || HitMob->MobKind == EAetheriaMobKind::Boss);
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(GetWorld(), bArmored ? TEXT("SFX_Hit_Armor") : TEXT("SFX_Hit_Bone"), HitMob->GetActorLocation(), 0.65f);
		}
#endif
	}
	DealDamageToMobOrActor(OtherActor, Damage, OwnerWizard.Get(), SpellId == EAetheriaSpellId::Fireball);
	Detonate();
}

void AAetheriaSpellProjectile::OnHit(
	UPrimitiveComponent* HitComp,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	FVector NormalImpulse,
	const FHitResult& Hit)
{
	if (bHasDetonated) return;
	Detonate();
}

void AAetheriaSpellProjectile::Detonate()
{
	if (bHasDetonated) return;
	bHasDetonated = true;

	const FVector BoomLoc = GetActorLocation();
	UWorld* World = GetWorld();

	// Dual chromatic explosion shockwaves + radial lightning sparks for Fireball
	AAetheriaLightningArcVFX::SpawnShockwaveRing(World, BoomLoc, RadiusUU * 1.65f, PrimaryColor, 0.48f);
	AAetheriaLightningArcVFX::SpawnShockwaveRing(World, BoomLoc, RadiusUU * 1.05f, AccentColor, 0.36f);

	const bool bFiery = (SpellId == EAetheriaSpellId::Fireball || SpellId == EAetheriaSpellId::ChaosMeteor);
	if (World)
	{
		if (bFiery && SpellId == EAetheriaSpellId::Fireball)
		{
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(World, TEXT("SFX_Fireball_Boom"), BoomLoc, 0.90f);
		}
		const int32 ArcCount = bFiery ? 4 : 2;
		for (int32 s = 0; s < ArcCount; ++s)
		{
			const float Ang = s * (TWO_PI / (float)ArcCount) + FMath::FRandRange(0.0f, 0.6f);
			const float ArcRadius = bFiery ? (RadiusUU * 0.95f) : FMath::Max(45.0f, RadiusUU * 0.70f);
			const FVector EndPt = BoomLoc + FVector(FMath::Cos(Ang) * ArcRadius, FMath::Sin(Ang) * ArcRadius, FMath::FRandRange(25.0f, bFiery ? 110.0f : 60.0f));
			TArray<FVector> Pts;
			Pts.Add(BoomLoc);
			Pts.Add(FMath::Lerp(BoomLoc, EndPt, 0.5f) + FVector(0.0f, 0.0f, bFiery ? 40.0f : 20.0f));
			Pts.Add(EndPt);
			AAetheriaLightningArcVFX::SpawnLightningArc(World, Pts, bFiery ? PrimaryColor : AccentColor, bFiery ? 0.20f : 0.14f);
		}
		AddNearbyWizardsCameraTrauma(World, BoomLoc, bFiery ? 0.18f : 0.08f, 1600.0f);
	}

	// Radial damage & blast impulse to nearby mobs
	if (!bIsEnemyProjectile && HasAuthority() && World)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(RadiusUU);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		if (OwnerWizard.IsValid()) QueryParams.AddIgnoredActor(OwnerWizard.Get());
		if (GetOwner()) QueryParams.AddIgnoredActor(GetOwner());
		if (GetInstigator()) QueryParams.AddIgnoredActor(GetInstigator());

		World->OverlapMultiByChannel(Overlaps, BoomLoc, FQuat::Identity, ECC_Pawn, SphereShape, QueryParams);
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (AActor* Victim = Overlap.GetActor())
			{
				DealDamageToMobOrActor(Victim, Damage, OwnerWizard.Get(), bFiery);
#if HAS_MOB_CHARACTER
				if (bFiery)
				{
					if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
					{
						const FVector BlastDir = (Mob->GetActorLocation() - BoomLoc).GetSafeNormal2D();
						Mob->ApplyStatusImpulse(BlastDir, 440.0f, OwnerWizard.Get(), true);
					}
				}
#endif
			}
		}
	}

	Destroy();
}

// ================================================================================================
// AAetheriaSpellZone
// ================================================================================================

AAetheriaSpellZone::AAetheriaSpellZone()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;

	EnsureEngineMeshesLoaded();

	AreaCollision = CreateDefaultSubobject<USphereComponent>(TEXT("AreaCollision"));
	AreaCollision->InitSphereRadius(500.0f);
	AreaCollision->SetCollisionProfileName(TEXT("OverlapAllDynamic"));
	AreaCollision->SetGenerateOverlapEvents(true);
	SetRootComponent(AreaCollision);

	DomeMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("DomeMesh"));
	DomeMesh->SetupAttachment(AreaCollision);
	if (GEngineSphereMesh) DomeMesh->SetStaticMesh(GEngineSphereMesh);
	DomeMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	SecondaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("SecondaryMesh"));
	SecondaryMesh->SetupAttachment(AreaCollision);
	if (GEngineCylinderMesh) SecondaryMesh->SetStaticMesh(GEngineCylinderMesh);
	SecondaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	TertiaryMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TertiaryMesh"));
	TertiaryMesh->SetupAttachment(AreaCollision);
	if (GEngineCylinderMesh) TertiaryMesh->SetStaticMesh(GEngineCylinderMesh);
	TertiaryMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TertiaryMesh->SetCastShadow(false);

	RingMeshA = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RingMeshA"));
	RingMeshA->SetupAttachment(AreaCollision);
	if (GEnginePlaneMesh) RingMeshA->SetStaticMesh(GEnginePlaneMesh);
	RingMeshA->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMeshA->SetCastShadow(false);
	RingMeshA->SetVisibility(false);

	RingMeshB = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("RingMeshB"));
	RingMeshB->SetupAttachment(AreaCollision);
	if (GEnginePlaneMesh) RingMeshB->SetStaticMesh(GEnginePlaneMesh);
	RingMeshB->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMeshB->SetCastShadow(false);
	RingMeshB->SetVisibility(false);

	GroundSealMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("GroundSealMesh"));
	GroundSealMesh->SetupAttachment(AreaCollision);
	if (GEnginePlaneMesh) GroundSealMesh->SetStaticMesh(GEnginePlaneMesh);
	GroundSealMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	GroundSealMesh->SetCastShadow(false);
	GroundSealMesh->SetVisibility(false);

	InstancedFX = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("InstancedFX"));
	InstancedFX->SetupAttachment(AreaCollision);
	if (GEngineSphereMesh) InstancedFX->SetStaticMesh(GEngineSphereMesh);
	InstancedFX->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	InstancedFX->SetCastShadow(false);

	TerrainDebrisISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("TerrainDebrisISM"));
	TerrainDebrisISM->SetupAttachment(AreaCollision);
	if (GEngineSphereMesh) TerrainDebrisISM->SetStaticMesh(GEngineSphereMesh);
	TerrainDebrisISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	TerrainDebrisISM->SetCastShadow(false);

	PlasmaTrailISM = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("PlasmaTrailISM"));
	PlasmaTrailISM->SetupAttachment(AreaCollision);
	if (GEngineSphereMesh) PlasmaTrailISM->SetStaticMesh(GEngineSphereMesh);
	PlasmaTrailISM->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	PlasmaTrailISM->SetCastShadow(false);

	ZoneLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("ZoneLight"));
	ZoneLight->SetupAttachment(AreaCollision);
	ZoneLight->Intensity = 3500.0f;
	ZoneLight->AttenuationRadius = 900.0f;
	ZoneLight->SetCastShadows(false);
}

void AAetheriaSpellZone::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAetheriaSpellZone, SpellId);
	DOREPLIFETIME(AAetheriaSpellZone, RadiusUU);
	DOREPLIFETIME(AAetheriaSpellZone, Duration);
	DOREPLIFETIME(AAetheriaSpellZone, TickDamage);
	DOREPLIFETIME(AAetheriaSpellZone, PrimaryColor);
	DOREPLIFETIME(AAetheriaSpellZone, AccentColor);
	DOREPLIFETIME(AAetheriaSpellZone, bIsEnemy);
	DOREPLIFETIME(AAetheriaSpellZone, bSolarSingularityIgnited);
	DOREPLIFETIME(AAetheriaSpellZone, TargetLocation);
	DOREPLIFETIME(AAetheriaSpellZone, CastDirection);
}

void AAetheriaSpellZone::BeginPlay()
{
	Super::BeginPlay();
	if (SpellId != EAetheriaSpellId::None && LastSetupSpellId != SpellId)
	{
		SetupMeshesForSpell();
	}
}

void AAetheriaSpellZone::InitializeZone(
	EAetheriaSpellId InSpellId,
	AActor* InOwnerWizard,
	const FVector& TargetPoint,
	const FVector& Direction,
	float InRadiusUU,
	float InDuration,
	float InTickDamage,
	FLinearColor InPrimaryColor,
	FLinearColor InAccentColor,
	bool bInEnemy)
{
	SpellId = InSpellId;
	OwnerWizard = InOwnerWizard;
	TargetLocation = TargetPoint;
	CastDirection = Direction.GetSafeNormal2D();
	if (CastDirection.IsNearlyZero())
	{
		CastDirection = FVector::ForwardVector;
	}
	RadiusUU = InRadiusUU;
	Duration = InDuration;
	TickDamage = InTickDamage;
	PrimaryColor = InPrimaryColor;
	AccentColor = InAccentColor;
	bIsEnemy = bInEnemy;

	SetActorLocation(TargetPoint);
	SetActorRotation(FRotator::ZeroRotator);
	if (AreaCollision)
	{
		AreaCollision->SetSphereRadius(RadiusUU);
	}

	SetupMeshesForSpell();
}

void AAetheriaSpellZone::SetupMeshesForSpell()
{
	EnsureEngineMeshesLoaded();
	LastSetupSpellId = SpellId;

	if (ZoneLight)
	{
		ZoneLight->SetLightColor(PrimaryColor);
		ZoneLight->AttenuationRadius = RadiusUU * 1.55f;
		ZoneLight->Intensity = 4800.0f;
		const bool bIsMajorUltimate = (SpellId == EAetheriaSpellId::BlackHole ||
			SpellId == EAetheriaSpellId::ChaosMeteor ||
			SpellId == EAetheriaSpellId::Chronosphere ||
			SpellId == EAetheriaSpellId::SupernovaBeam ||
			SpellId == EAetheriaSpellId::PrimalOverload);
		ZoneLight->SetCastShadows(bIsMajorUltimate);
		if (bIsMajorUltimate)
		{
			ZoneLight->ContactShadowLength = 0.06f;
		}
	}

	if (DomeMesh)
	{
		DomeMesh->SetRelativeLocation(FVector::ZeroVector);
		DomeMesh->SetRelativeRotation(FRotator::ZeroRotator);
		DomeMesh->SetVisibility(true);
	}
	if (SecondaryMesh)
	{
		SecondaryMesh->SetRelativeLocation(FVector::ZeroVector);
		SecondaryMesh->SetRelativeRotation(FRotator::ZeroRotator);
		SecondaryMesh->SetVisibility(true);
	}
	if (TertiaryMesh)
	{
		TertiaryMesh->SetRelativeLocation(FVector::ZeroVector);
		TertiaryMesh->SetRelativeRotation(FRotator::ZeroRotator);
		TertiaryMesh->SetVisibility(true);
	}
	if (RingMeshA)
	{
		RingMeshA->SetRelativeLocation(FVector::ZeroVector);
		RingMeshA->SetRelativeRotation(FRotator::ZeroRotator);
		RingMeshA->SetVisibility(false);
	}
	if (RingMeshB)
	{
		RingMeshB->SetRelativeLocation(FVector::ZeroVector);
		RingMeshB->SetRelativeRotation(FRotator::ZeroRotator);
		RingMeshB->SetVisibility(false);
	}
	if (GroundSealMesh)
	{
		GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
		GroundSealMesh->SetRelativeRotation(FRotator::ZeroRotator);
		GroundSealMesh->SetVisibility(false);
	}
	if (TerrainDebrisISM) TerrainDebrisISM->ClearInstances();
	if (PlasmaTrailISM) PlasmaTrailISM->ClearInstances();
	if (InstancedFX) InstancedFX->ClearInstances();
	MatterChunks.Reset();
	StarFilamentParams.Reset();
	MeteorFurrowPoints.Reset();

	const float SphereScale = RadiusUU / 50.0f; // UE basic sphere is 100 units diameter (radius 50)
	int32 LocIdx = 0;
	if (GetWorld())
	{
		if (const AAetheriaGameState* GS = GetWorld()->GetGameState<AAetheriaGameState>())
		{
			LocIdx = GS->CurrentLocationIndex;
		}
	}

	switch (SpellId)
	{
	case EAetheriaSpellId::BlackHole:
	{
		const FVector HorizonOffset(0.0f, 0.0f, 55.0f);
		const float AccretionBoost = bSolarSingularityIgnited ? 12.0f : 9.5f;
		const FLinearColor AccretionInner = bSolarSingularityIgnited ? AetheriaMindColors::IgnisPlasma : FLinearColor(1.0f, 0.56f, 0.20f, 1.0f);
		const FLinearColor AccretionOuter = bSolarSingularityIgnited ? FLinearColor(1.0f, 0.22f, 0.05f, 1.0f) : AetheriaMindColors::MindViolet;

		// 1. Schwarzschild Pitch-Black Event Horizon + Sharp Photon Ring (compact readable cosmic core)
		if (DomeMesh)
		{
			DomeMesh->SetStaticMesh(GEngineSphereMesh);
			DomeMesh->SetRelativeLocation(HorizonOffset);
			DomeMesh->SetRelativeScale3D(FVector(SphereScale * 0.18f));
			if (UMaterialInstanceDynamic* HorizonMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetBlackHoleHorizonMaterial(this),
				AetheriaMindColors::MindViolet, FLinearColor(0.92f, 0.98f, 1.0f, 1.0f), 10.0f, 1.0f))
			{
				DomeMesh->SetMaterial(0, HorizonMat);
			}
			else
			{
				DomeMesh->SetMaterial(0, UAetheriaMaterialLibrary::GetBlackHoleHorizonMaterial(this));
			}
		}

		// 2. Equatorial Relativistic Accretion Disk (Hollow UV disk with ISCO & Doppler beaming)
		if (SecondaryMesh)
		{
			SecondaryMesh->SetStaticMesh(GEnginePlaneMesh);
			SecondaryMesh->SetRelativeLocation(HorizonOffset);
			SecondaryMesh->SetRelativeRotation(FRotator(16.0f, 0.0f, 12.0f));
			SecondaryMesh->SetRelativeScale3D(FVector(SphereScale * 1.50f, SphereScale * 1.50f, 1.0f));
			if (UMaterialInstanceDynamic* DiskMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetAccretionDiskMaterial(this),
				AccretionInner, AccretionOuter, AccretionBoost, 1.0f))
			{
				SecondaryMesh->SetMaterial(0, DiskMat);
			}
		}

		// 3. Gravitational Lensing Einstein Halo Arch (vertical bent disk light like Gargantua)
		if (RingMeshA)
		{
			RingMeshA->SetStaticMesh(GEnginePlaneMesh);
			RingMeshA->SetVisibility(true);
			RingMeshA->SetRelativeLocation(HorizonOffset);
			RingMeshA->SetRelativeRotation(FRotator(84.0f, -14.0f, 8.0f));
			RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.15f, SphereScale * 1.15f, 1.0f));
			if (UMaterialInstanceDynamic* LensMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetAccretionDiskMaterial(this),
				FLinearColor(0.88f, 0.94f, 1.0f, 1.0f), AetheriaMindColors::MindViolet, 8.5f, 1.0f))
			{
				RingMeshA->SetMaterial(0, LensMat);
			}
		}

		// 4. Keep outer sphere distortion shell hidden so nothing obscures view of the singularity core
		if (RingMeshB)
		{
			RingMeshB->SetVisibility(false);
		}

		// 5. Bi-Directional Relativistic Polar Astrophysical Jets
		if (TertiaryMesh)
		{
			TertiaryMesh->SetStaticMesh(GEngineCylinderMesh);
			TertiaryMesh->SetVisibility(true);
			TertiaryMesh->SetRelativeLocation(HorizonOffset);
			TertiaryMesh->SetRelativeRotation(FRotator(16.0f, 0.0f, 12.0f));
			TertiaryMesh->SetRelativeScale3D(FVector(0.18f, 0.18f, SphereScale * 1.10f));
			if (UMaterialInstanceDynamic* JetMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetSolarBeamMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 9.0f, 1.0f))
			{
				TertiaryMesh->SetMaterial(0, JetMat);
			}
		}

		// 6. Gravitational Abyssal Floor Fracture Seal
		if (GroundSealMesh)
		{
			GroundSealMesh->SetVisibility(true);
			GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
			GroundSealMesh->SetRelativeScale3D(FVector(SphereScale * 1.42f, SphereScale * 1.42f, 1.0f));
			if (UMaterialInstanceDynamic* GravSealMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::MindViolet, AetheriaMindColors::MindCyan, 5.2f, 0.88f))
			{
				GroundSealMesh->SetMaterial(0, GravSealMat);
			}
		}

		// 7. 56 Relativistic Star-Matter & Plasma Filaments spiraling inward from 1.25R -> horizon
		if (InstancedFX)
		{
			InstancedFX->SetStaticMesh(GEngineSphereMesh);
			if (UMaterialInstanceDynamic* StarFXMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				FLinearColor(0.95f, 0.72f, 1.0f, 1.0f), AetheriaMindColors::MindCyan, 4.5f, 1.0f))
			{
				InstancedFX->SetMaterial(0, StarFXMat);
			}
			for (int32 i = 0; i < 56; ++i)
			{
				const float Ang = (i / 56.0f) * TWO_PI + FMath::FRandRange(-0.15f, 0.15f);
				const float StartR = FMath::FRandRange(RadiusUU * 0.45f, RadiusUU * 1.28f);
				StarFilamentParams.Add(FVector(Ang, StartR, StartR));
				const FVector Pos(FMath::Cos(Ang) * StartR, FMath::Sin(Ang) * StartR, HorizonOffset.Z + FMath::FRandRange(-35.0f, 35.0f));
				InstancedFX->AddInstance(FTransform(FRotator::ZeroRotator, Pos, FVector(0.11f)));
			}
		}

		// 8. 28 Torn Landscape Slabs & Arena Boulders ripped out of floor, spaghettified and swallowed
		if (TerrainDebrisISM)
		{
			UStaticMesh* BoulderMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CliffBoulder"), TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			TerrainDebrisISM->SetStaticMesh(BoulderMesh);
			if (UMaterialInstanceDynamic* TerrainSlabMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetBiomeFloorMaterial(LocIdx, this),
				FLinearColor(0.84f, 0.86f, 0.94f, 1.0f), AetheriaMindColors::MindViolet, 3.2f, 1.0f))
			{
				TerrainDebrisISM->SetMaterial(0, TerrainSlabMat);
			}

			const float NormUnit = GetNormalizedUnitMeshScale(BoulderMesh);
			bExtractedBiomeProps = false;
			for (int32 i = 0; i < 28; ++i)
			{
				FAetheriaAbsorbedChunk Chunk;
				Chunk.OrbitAngle = (i / 28.0f) * TWO_PI + FMath::FRandRange(-0.2f, 0.2f);
				Chunk.StartRadius = FMath::FRandRange(RadiusUU * 0.62f, RadiusUU * 1.32f);
				Chunk.CurrentRadius = Chunk.StartRadius;
				Chunk.CurrentZ = 4.0f;
				Chunk.BaseScale = NormUnit * FMath::FRandRange(0.28f, 0.62f);
				Chunk.TumbleRot = FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f));
				Chunk.bActive = true;
				MatterChunks.Add(Chunk);

				const FVector LocalPos(FMath::Cos(Chunk.OrbitAngle) * Chunk.CurrentRadius, FMath::Sin(Chunk.OrbitAngle) * Chunk.CurrentRadius, Chunk.CurrentZ);
				TerrainDebrisISM->AddInstance(FTransform(Chunk.TumbleRot, LocalPos, FVector(Chunk.BaseScale, Chunk.BaseScale * 0.85f, Chunk.BaseScale * 0.55f)));
			}
		}

		if (ZoneLight)
		{
			ZoneLight->SetLightColor(AetheriaMindColors::MindViolet);
			ZoneLight->Intensity = 9500.0f;
			ZoneLight->AttenuationRadius = RadiusUU * 2.2f;
		}
		break;
	}

	case EAetheriaSpellId::Chronosphere:
	{
		// Magnificent translucent Space-Time Chronosphere Dome + Astrolabe Seal + Precessing Rings
		if (DomeMesh)
		{
			DomeMesh->SetStaticMesh(GEngineSphereMesh);
			DomeMesh->SetRelativeScale3D(FVector(SphereScale, SphereScale, SphereScale * 0.88f));
			if (UMaterialInstanceDynamic* SheerDomeMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetChronosphereDomeMaterial(this),
				FLinearColor(0.04f, 0.08f, 0.35f, 1.0f), AetheriaMindColors::MindCyan, 3.8f, 0.22f))
			{
				DomeMesh->SetMaterial(0, SheerDomeMat);
			}
			else
			{
				DomeMesh->SetMaterial(0, UAetheriaMaterialLibrary::GetChronosphereDomeMaterial(this));
			}
		}
		if (SecondaryMesh)
		{
			SecondaryMesh->SetVisibility(false);
		}
		if (TertiaryMesh)
		{
			TertiaryMesh->SetVisibility(false);
		}
		if (GroundSealMesh)
		{
			GroundSealMesh->SetVisibility(true);
			GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
			GroundSealMesh->SetRelativeScale3D(FVector(SphereScale * 1.95f, SphereScale * 1.95f, 1.0f));
			if (UMaterialInstanceDynamic* SealMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 4.8f, 0.92f))
			{
				GroundSealMesh->SetMaterial(0, SealMat);
			}
		}
		if (RingMeshA)
		{
			RingMeshA->SetStaticMesh(GEnginePlaneMesh);
			RingMeshA->SetVisibility(true);
			RingMeshA->SetRelativeRotation(FRotator(28.0f, 0.0f, 0.0f));
			RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.65f, SphereScale * 1.65f, 1.0f));
			if (UMaterialInstanceDynamic* RingAMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), AetheriaMindColors::MindCyan, AetheriaMindColors::MindCyan, 5.2f, 0.85f))
			{
				RingMeshA->SetMaterial(0, RingAMat);
			}
		}
		if (RingMeshB)
		{
			RingMeshB->SetStaticMesh(GEnginePlaneMesh);
			RingMeshB->SetVisibility(true);
			RingMeshB->SetRelativeRotation(FRotator(-34.0f, 45.0f, 0.0f));
			RingMeshB->SetRelativeScale3D(FVector(SphereScale * 1.38f, SphereScale * 1.38f, 1.0f));
			if (UMaterialInstanceDynamic* RingBMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), AetheriaMindColors::MindViolet, AetheriaMindColors::MindViolet, 5.2f, 0.82f))
			{
				RingMeshB->SetMaterial(0, RingBMat);
			}
		}
		if (InstancedFX)
		{
			InstancedFX->SetStaticMesh(GEngineSphereMesh);
			if (UMaterialInstanceDynamic* StarMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				FLinearColor(0.80f, 0.96f, 1.0f, 1.0f), AetheriaMindColors::MindCyan, 4.5f, 1.0f))
			{
				InstancedFX->SetMaterial(0, StarMat);
			}
			for (int32 i = 0; i < 72; ++i)
			{
				const float Theta = FMath::FRandRange(0.0f, TWO_PI);
				const float Phi = FMath::FRandRange(0.10f, PI * 0.46f);
				const float R = RadiusUU * FMath::Pow(FMath::FRandRange(0.18f, 0.96f), 0.65f);
				const FVector Pos(
					FMath::Cos(Theta) * FMath::Cos(Phi) * R,
					FMath::Sin(Theta) * FMath::Cos(Phi) * R,
					FMath::Sin(Phi) * R + 18.0f);
				const float StarScale = FMath::FRandRange(0.055f, 0.155f);
				InstancedFX->AddInstance(FTransform(FRotator::ZeroRotator, Pos, FVector(StarScale)));
			}
		}
		break;
	}

	case EAetheriaSpellId::ChaosMeteor:
	{
		const FVector Fwd2D = CastDirection.GetSafeNormal2D();
		const FVector SkyOffset = -Fwd2D * 920.0f + FVector(0.0f, 0.0f, 1480.0f);
		const FVector EntryDir = (-SkyOffset + FVector(0.0f, 0.0f, RadiusUU * 0.40f)).GetSafeNormal();
		const FRotator PortalFacingRot = EntryDir.Rotation() + FRotator(90.0f, 0.0f, 0.0f);

		// 1. Molten Obsidian Asteroid Core
		UStaticMesh* BoulderMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CliffBoulder"), TEXT("/Engine/BasicShapes/Sphere.Sphere"));
		const float CoreNorm = GetNormalizedUnitMeshScale(BoulderMesh);
		if (DomeMesh)
		{
			DomeMesh->SetStaticMesh(BoulderMesh ? BoulderMesh : GEngineSphereMesh);
			DomeMesh->SetRelativeLocation(SkyOffset);
			DomeMesh->SetRelativeScale3D(FVector(SphereScale * 0.68f * CoreNorm));
			DomeMesh->SetMaterial(0, UAetheriaMaterialLibrary::GetFireballMaterial(this));
		}

		// 2. Atmospheric Plasma Mach Corona Shell
		if (TertiaryMesh)
		{
			TertiaryMesh->SetStaticMesh(GEngineSphereMesh);
			TertiaryMesh->SetVisibility(true);
			TertiaryMesh->SetRelativeLocation(SkyOffset);
			TertiaryMesh->SetRelativeRotation(EntryDir.Rotation());
			TertiaryMesh->SetRelativeScale3D(FVector(SphereScale * 1.15f, SphereScale * 0.86f, SphereScale * 0.86f));
			if (UMaterialInstanceDynamic* CoronaMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindViolet, 4.8f, 0.52f))
			{
				TertiaryMesh->SetMaterial(0, CoronaMat);
			}
		}

		// 3. Celestial Tear Sky Rift Portal (2 counter-rotating rings tilted along entry trajectory)
		if (SecondaryMesh)
		{
			SecondaryMesh->SetStaticMesh(GEnginePlaneMesh);
			SecondaryMesh->SetRelativeLocation(SkyOffset);
			SecondaryMesh->SetRelativeRotation(PortalFacingRot);
			SecondaryMesh->SetRelativeScale3D(FVector(SphereScale * 2.55f, SphereScale * 2.55f, 1.0f));
			if (UMaterialInstanceDynamic* SkyPortalMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindViolet, 8.5f, 0.96f))
			{
				SecondaryMesh->SetMaterial(0, SkyPortalMat);
			}
		}
		if (RingMeshA)
		{
			RingMeshA->SetStaticMesh(GEnginePlaneMesh);
			RingMeshA->SetVisibility(true);
			RingMeshA->SetRelativeLocation(SkyOffset);
			RingMeshA->SetRelativeRotation(PortalFacingRot + FRotator(0.0f, 45.0f, 0.0f));
			RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.90f, SphereScale * 1.90f, 1.0f));
			if (UMaterialInstanceDynamic* InnerPortalMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::MindViolet, AetheriaMindColors::IgnisPlasma, 7.5f, 0.90f))
			{
				RingMeshA->SetMaterial(0, InnerPortalMat);
			}
		}

		// 4. Pre-Impact Ground Targeting & Crater Seals
		if (GroundSealMesh)
		{
			GroundSealMesh->SetVisibility(true);
			GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
			GroundSealMesh->SetRelativeScale3D(FVector(SphereScale * 2.15f, SphereScale * 2.15f, 1.0f));
			if (UMaterialInstanceDynamic* CraterSealMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindViolet, 6.2f, 0.94f))
			{
				GroundSealMesh->SetMaterial(0, CraterSealMat);
			}
		}
		if (RingMeshB)
		{
			RingMeshB->SetStaticMesh(GEnginePlaneMesh);
			RingMeshB->SetVisibility(true);
			RingMeshB->SetRelativeLocation(FVector(0.0f, 0.0f, 8.0f));
			RingMeshB->SetRelativeScale3D(FVector(SphereScale * 1.45f, SphereScale * 1.45f, 1.0f));
			if (UMaterialInstanceDynamic* InnerTargetMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				FLinearColor(1.0f, 0.85f, 0.35f, 1.0f), AetheriaMindColors::IgnisPlasma, 7.0f, 0.90f))
			{
				RingMeshB->SetMaterial(0, InnerTargetMat);
			}
		}

		// 5. 36 Atmospheric Plasma Plume & Burning Wake Fireballs
		if (PlasmaTrailISM)
		{
			PlasmaTrailISM->SetStaticMesh(GEngineSphereMesh);
			if (UMaterialInstanceDynamic* PlumeMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::IgnisPlasma, FLinearColor(1.0f, 0.82f, 0.25f, 1.0f), 5.2f, 0.92f))
			{
				PlasmaTrailISM->SetMaterial(0, PlumeMat);
			}
			for (int32 i = 0; i < 36; ++i)
			{
				PlasmaTrailISM->AddInstance(FTransform(FRotator::ZeroRotator, SkyOffset, FVector(0.01f)));
			}
		}

		// 6. 10 Ballistic Molten Shrapnel Boulders + 24 Persistent Burning Magma Furrow Slabs
		if (TerrainDebrisISM)
		{
			TerrainDebrisISM->SetStaticMesh(BoulderMesh ? BoulderMesh : GEngineSphereMesh);
			TerrainDebrisISM->SetMaterial(0, UAetheriaMaterialLibrary::GetFireballMaterial(this));
			for (int32 i = 0; i < 34; ++i)
			{
				FAetheriaAbsorbedChunk Frag;
				Frag.BaseScale = CoreNorm * FMath::FRandRange(0.22f, 0.44f);
				Frag.TumbleRot = FRotator(FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f), FMath::FRandRange(0.0f, 360.0f));
				Frag.bActive = false;
				MatterChunks.Add(Frag);
				TerrainDebrisISM->AddInstance(FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.001f)));
			}
		}

		if (ZoneLight)
		{
			ZoneLight->SetLightColor(AetheriaMindColors::IgnisPlasma);
			ZoneLight->Intensity = 12000.0f;
			ZoneLight->AttenuationRadius = RadiusUU * 2.4f;
		}
		MeteorSpinQuat = FQuat::Identity;
		bMeteorFinalEruption = false;
		NextFurrowDist = 0.0f;
		break;
	}

	case EAetheriaSpellId::GravityCataclysm:
	{
		if (InstancedFX)
		{
			UStaticMesh* SpireMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CliffBoulder"), TEXT("/Engine/BasicShapes/Cone.Cone"));
			const float Norm = GetNormalizedUnitMeshScale(SpireMesh);
			InstancedFX->SetStaticMesh(SpireMesh ? SpireMesh : GEngineConeMesh);
			if (UMaterialInstanceDynamic* SpireMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetObsidianMaterial(this),
				FLinearColor(0.16f, 0.08f, 0.24f, 1.0f), AetheriaMindColors::IgnisPlasma, 4.5f, 1.0f))
			{
				InstancedFX->SetMaterial(0, SpireMat);
			}
			else
			{
				InstancedFX->SetMaterial(0, UAetheriaMaterialLibrary::GetObsidianMaterial(this));
			}

			const FVector Offsets[7] = {
				FVector::ZeroVector,
				FVector(240.0f, 60.0f, 0.0f),
				FVector(-230.0f, -80.0f, 0.0f),
				FVector(80.0f, 250.0f, 0.0f),
				FVector(-90.0f, -240.0f, 0.0f),
				FVector(190.0f, -180.0f, 0.0f),
				FVector(-180.0f, 190.0f, 0.0f)
			};
			for (int32 k = 0; k < 7; ++k)
			{
				const FRotator RandRot(FMath::FRandRange(-10.0f, 10.0f), k * 51.4f, FMath::FRandRange(-10.0f, 10.0f));
				const float HeightScale = ((k == 0) ? 3.2f : FMath::FRandRange(2.0f, 2.7f)) * Norm;
				const float WidthScale = 1.1f * Norm;
				InstancedFX->AddInstance(FTransform(RandRot, Offsets[k] + FVector(0.0f, 0.0f, -350.0f), FVector(WidthScale, WidthScale, HeightScale)));
			}
		}

		// 24 Zero-G levitating glowing stones drifting skyward
		if (PlasmaTrailISM)
		{
			UStaticMesh* GravelMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CliffBoulder"), TEXT("/Engine/BasicShapes/Sphere.Sphere"));
			PlasmaTrailISM->SetStaticMesh(GravelMesh ? GravelMesh : GEngineSphereMesh);
			if (UMaterialInstanceDynamic* GravMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::MindViolet, AetheriaMindColors::IgnisPlasma, 4.8f, 1.0f))
			{
				PlasmaTrailISM->SetMaterial(0, GravMat);
			}
			const float GravelNorm = GetNormalizedUnitMeshScale(GravelMesh);
			for (int32 g = 0; g < 24; ++g)
			{
				const float Ang = (g / 24.0f) * TWO_PI + FMath::FRandRange(-0.2f, 0.2f);
				const float R = FMath::FRandRange(65.0f, RadiusUU * 0.92f);
				const float BaseZ = FMath::FRandRange(12.0f, 180.0f);
				const FVector Pos(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, BaseZ);
				const float S = GravelNorm * FMath::FRandRange(0.14f, 0.30f);
				StarFilamentParams.Add(FVector(Pos.X, Pos.Y, BaseZ));
				PlasmaTrailISM->AddInstance(FTransform(FRotator(g * 33.0f, g * 67.0f, 0.0f), Pos, FVector(S)));
			}
		}

		if (RingMeshA)
		{
			RingMeshA->SetStaticMesh(GEnginePlaneMesh);
			RingMeshA->SetVisibility(true);
			RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.9f, SphereScale * 1.9f, 1.0f));
			if (UMaterialInstanceDynamic* WaveRingMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 7.5f, 0.85f))
			{
				RingMeshA->SetMaterial(0, WaveRingMat);
			}
		}

		if (GroundSealMesh)
		{
			GroundSealMesh->SetVisibility(true);
			GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
			GroundSealMesh->SetRelativeScale3D(FVector(SphereScale * 2.05f, SphereScale * 2.05f, 1.0f));
			if (UMaterialInstanceDynamic* CrackMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindViolet, 6.0f, 0.92f))
			{
				GroundSealMesh->SetMaterial(0, CrackMat);
			}
		}
		if (DomeMesh) DomeMesh->SetVisibility(false);
		if (SecondaryMesh) SecondaryMesh->SetVisibility(false);
		if (TertiaryMesh) TertiaryMesh->SetVisibility(false);
		break;
	}

	case EAetheriaSpellId::PrismWall:
	{
		if (DomeMesh) DomeMesh->SetVisibility(false);
		if (SecondaryMesh) SecondaryMesh->SetVisibility(false);
		if (TertiaryMesh) TertiaryMesh->SetVisibility(false);

		const FVector Fwd = CastDirection.GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();

		if (InstancedFX)
		{
			UStaticMesh* CrystalMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_CrystalShard"), TEXT("/Engine/BasicShapes/Cone.Cone"));
			InstancedFX->SetStaticMesh(CrystalMesh ? CrystalMesh : GEngineConeMesh);
			if (UMaterialInstanceDynamic* CrystalMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetPrismWallMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 5.5f, 0.72f))
			{
				InstancedFX->SetMaterial(0, CrystalMat);
			}
			else
			{
				InstancedFX->SetMaterial(0, UAetheriaMaterialLibrary::GetPrismWallMaterial(this));
			}

			for (int32 i = -3; i <= 3; ++i)
			{
				const float Lateral = i * 135.0f;
				const float CurveBack = -FMath::Square(static_cast<float>(i)) * 12.0f;
				const FVector Pos = Right * Lateral + Fwd * CurveBack + FVector(0.0f, 0.0f, 130.0f);
				const float HeightMult = 3.2f - FMath::Abs(i) * 0.25f;
				InstancedFX->AddInstance(FTransform(FRotator(0.0f, i * 18.0f, 0.0f), Pos, FVector(0.95f, 0.95f, HeightMult)));
			}
		}

		// Floating refractive crystal motes orbiting along barrier crest
		if (PlasmaTrailISM)
		{
			PlasmaTrailISM->SetStaticMesh(GEngineSphereMesh);
			if (UMaterialInstanceDynamic* PrisMoteMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 4.6f, 0.95f))
			{
				PlasmaTrailISM->SetMaterial(0, PrisMoteMat);
			}
			for (int32 m = 0; m < 18; ++m)
			{
				const float Lateral = FMath::Lerp(-405.0f, 405.0f, static_cast<float>(m) / 17.0f);
				const FVector MotePos = Right * Lateral + FVector(0.0f, 0.0f, 180.0f + FMath::Sin(m * 1.1f) * 60.0f);
				StarFilamentParams.Add(MotePos);
				PlasmaTrailISM->AddInstance(FTransform(FRotator::ZeroRotator, MotePos, FVector(0.11f)));
			}
		}

		if (GroundSealMesh)
		{
			GroundSealMesh->SetVisibility(true);
			GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 4.0f));
			GroundSealMesh->SetRelativeScale3D(FVector(9.6f, 2.4f, 1.0f));
			GroundSealMesh->SetRelativeRotation(CastDirection.Rotation());
			if (UMaterialInstanceDynamic* WallSealMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), AetheriaMindColors::MindViolet, AetheriaMindColors::MindCyan, 5.5f, 0.85f))
			{
				GroundSealMesh->SetMaterial(0, WallSealMat);
			}
		}
		break;
	}

	case EAetheriaSpellId::ChronoBlink:
	{
		// Sculpted spectral mage hologram instead of plain cylinder
		UStaticMesh* HoloMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_ArchmageRobed"), TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
		if (DomeMesh)
		{
			DomeMesh->SetStaticMesh(HoloMesh ? HoloMesh : GEngineCylinderMesh);
			DomeMesh->SetRelativeLocation(HoloMesh ? FVector(0.0f, 0.0f, -86.0f) : FVector::ZeroVector);
			DomeMesh->SetRelativeRotation(CastDirection.Rotation());
			DomeMesh->SetRelativeScale3D(HoloMesh ? FVector(1.12f) : FVector(0.6f, 0.6f, 1.8f));
			if (UMaterialInstanceDynamic* HoloMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetChronosphereDomeMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 6.0f, 0.72f))
			{
				DomeMesh->SetMaterial(0, HoloMat);
			}
		}
		if (GroundSealMesh)
		{
			GroundSealMesh->SetVisibility(true);
			GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
			GroundSealMesh->SetRelativeScale3D(FVector(SphereScale * 2.0f, SphereScale * 2.0f, 1.0f));
			if (UMaterialInstanceDynamic* SealMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 6.2f, 0.90f))
			{
				GroundSealMesh->SetMaterial(0, SealMat);
			}
		}
		if (RingMeshA)
		{
			RingMeshA->SetStaticMesh(GEnginePlaneMesh);
			RingMeshA->SetVisibility(true);
			RingMeshA->SetRelativeLocation(FVector(0.0f, 0.0f, 65.0f));
			RingMeshA->SetRelativeRotation(FRotator(25.0f, 0.0f, 0.0f));
			RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.4f, SphereScale * 1.4f, 1.0f));
			if (UMaterialInstanceDynamic* RingMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), AetheriaMindColors::MindCyan, AetheriaMindColors::MindCyan, 8.0f, 0.88f))
			{
				RingMeshA->SetMaterial(0, RingMat);
			}
		}
		if (SecondaryMesh) SecondaryMesh->SetVisibility(false);
		if (TertiaryMesh) TertiaryMesh->SetVisibility(false);
		if (InstancedFX)
		{
			InstancedFX->SetStaticMesh(GEngineSphereMesh);
			if (UMaterialInstanceDynamic* MoteMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 4.2f, 1.0f))
			{
				InstancedFX->SetMaterial(0, MoteMat);
			}
			for (int32 i = 0; i < 24; ++i)
			{
				const float Ang = (i / 24.0f) * TWO_PI;
				const float R = RadiusUU * 0.85f;
				StarFilamentParams.Add(FVector(Ang, R, FMath::FRandRange(20.0f, 140.0f)));
				InstancedFX->AddInstance(FTransform(FRotator::ZeroRotator, FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 60.0f), FVector(0.11f)));
			}
		}
		break;
	}

	case EAetheriaSpellId::SupernovaBeam:
	{
		const FRotator BeamRot = CastDirection.Rotation() + FRotator(90.0f, 0.0f, 0.0f);
		const FVector BeamMidOffset = CastDirection * 1300.0f;

		// Inner white-gold plasma core beam spanning [0 .. 2600 UU] ahead of caster staff
		if (DomeMesh)
		{
			DomeMesh->SetStaticMesh(GEngineCylinderMesh);
			DomeMesh->SetRelativeLocation(BeamMidOffset);
			DomeMesh->SetRelativeRotation(BeamRot);
			DomeMesh->SetRelativeScale3D(FVector(0.75f, 0.75f, 26.0f));
			if (UMaterialInstanceDynamic* BeamMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetSolarBeamMaterial(this),
				AetheriaMindColors::IgnisPlasma, FLinearColor(1.0f, 0.96f, 0.82f, 1.0f), 26.0f, 1.0f))
			{
				DomeMesh->SetMaterial(0, BeamMat);
			}
		}
		// Outer solar-violet plasma sheath
		if (SecondaryMesh)
		{
			SecondaryMesh->SetStaticMesh(GEngineCylinderMesh);
			SecondaryMesh->SetRelativeLocation(BeamMidOffset);
			SecondaryMesh->SetRelativeRotation(BeamRot);
			SecondaryMesh->SetRelativeScale3D(FVector(1.55f, 1.55f, 26.0f));
			if (UMaterialInstanceDynamic* SheathMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindCyan, 8.5f, 0.36f))
			{
				SecondaryMesh->SetMaterial(0, SheathMat);
			}
		}
		// Scorching plasma endpoint flare sphere at 2560 UU
		if (TertiaryMesh)
		{
			TertiaryMesh->SetStaticMesh(GEngineSphereMesh);
			TertiaryMesh->SetVisibility(true);
			TertiaryMesh->SetRelativeLocation(CastDirection * 2560.0f);
			TertiaryMesh->SetRelativeScale3D(FVector(2.2f));
			if (UMaterialInstanceDynamic* FlareMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetFireballMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindCyan, 5.5f, 1.0f))
			{
				TertiaryMesh->SetMaterial(0, FlareMat);
			}
		}
		// Vertical astrolabe muzzle rings framing the beam emitter
		if (RingMeshA)
		{
			RingMeshA->SetStaticMesh(GEnginePlaneMesh);
			RingMeshA->SetVisibility(true);
			RingMeshA->SetRelativeLocation(CastDirection * 40.0f);
			RingMeshA->SetRelativeRotation(CastDirection.Rotation() + FRotator(90.0f, 0.0f, 0.0f));
			RingMeshA->SetRelativeScale3D(FVector(2.6f, 2.6f, 1.0f));
			if (UMaterialInstanceDynamic* MuzzleMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindCyan, 10.0f, 0.95f))
			{
				RingMeshA->SetMaterial(0, MuzzleMat);
			}
		}
		// 36 Double-Helix plasma nodes corkscrewing down the 2600 UU beam
		if (InstancedFX)
		{
			InstancedFX->SetStaticMesh(GEngineSphereMesh);
			if (UMaterialInstanceDynamic* HelixMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::IgnisPlasma, 5.0f, 1.0f))
			{
				InstancedFX->SetMaterial(0, HelixMat);
			}
			for (int32 h = 0; h < 36; ++h)
			{
				const float Dist = (static_cast<float>(h) / 35.0f) * 2550.0f;
				InstancedFX->AddInstance(FTransform(FRotator::ZeroRotator, CastDirection * Dist, FVector(0.18f)));
			}
		}
		if (ZoneLight)
		{
			ZoneLight->SetLightColor(AetheriaMindColors::IgnisPlasma);
			ZoneLight->Intensity = 10500.0f;
		}
		break;
	}

	case EAetheriaSpellId::PrimalOverload:
	{
		if (DomeMesh)
		{
			DomeMesh->SetStaticMesh(GEngineSphereMesh);
			DomeMesh->SetRelativeScale3D(FVector(0.15f));
			if (UMaterialInstanceDynamic* TriadMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetChronosphereDomeMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindCyan, 8.0f, 0.35f))
			{
				DomeMesh->SetMaterial(0, TriadMat);
			}
		}
		if (RingMeshA)
		{
			RingMeshA->SetStaticMesh(GEnginePlaneMesh);
			RingMeshA->SetVisibility(true);
			RingMeshA->SetRelativeRotation(FRotator(24.0f, 0.0f, 0.0f));
			RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.4f, SphereScale * 1.4f, 1.0f));
			if (UMaterialInstanceDynamic* RingAMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::IgnisPlasma, AetheriaMindColors::MindViolet, 10.0f, 0.92f))
			{
				RingMeshA->SetMaterial(0, RingAMat);
			}
		}
		if (RingMeshB)
		{
			RingMeshB->SetStaticMesh(GEnginePlaneMesh);
			RingMeshB->SetVisibility(true);
			RingMeshB->SetRelativeRotation(FRotator(-28.0f, 45.0f, 0.0f));
			RingMeshB->SetRelativeScale3D(FVector(SphereScale * 1.15f, SphereScale * 1.15f, 1.0f));
			if (UMaterialInstanceDynamic* RingBMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this),
				AetheriaMindColors::MindCyan, AetheriaMindColors::MindViolet, 10.0f, 0.92f))
			{
				RingMeshB->SetMaterial(0, RingBMat);
			}
		}
		if (InstancedFX)
		{
			InstancedFX->SetStaticMesh(GEngineSphereMesh);
			if (UMaterialInstanceDynamic* SparkMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
				this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
				AetheriaMindColors::CrispAlabaster, AetheriaMindColors::MindCyan, 4.5f, 1.0f))
			{
				InstancedFX->SetMaterial(0, SparkMat);
			}
			for (int32 p = 0; p < 48; ++p)
			{
				const float Ang = (p / 48.0f) * TWO_PI;
				const float R = RadiusUU * 0.72f;
				const float Phi = FMath::FRandRange(-0.55f, 0.75f);
				StarFilamentParams.Add(FVector(Ang, R, Phi));
				InstancedFX->AddInstance(FTransform(FRotator::ZeroRotator, FVector(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, 60.0f), FVector(0.14f)));
			}
		}
		if (SecondaryMesh) SecondaryMesh->SetVisibility(false);
		if (TertiaryMesh) TertiaryMesh->SetVisibility(false);
		break;
	}

	default:
		break;
	}
}

void AAetheriaSpellZone::IgniteSolarSingularity()
{
	bSolarSingularityIgnited = true;
	TickDamage *= 1.35f;
	UAetheriaMaterialLibrary::PlayGameSoundAtLocation(GetWorld(), TEXT("SFX_BlackHole_SolarIgnite"), GetActorLocation(), 0.95f);

	const float SphereScale = RadiusUU / 50.0f;
	if (SecondaryMesh)
	{
		SecondaryMesh->SetRelativeScale3D(FVector(SphereScale * 1.80f, SphereScale * 1.80f, 1.0f));
		if (UMaterialInstanceDynamic* QuasarMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, UAetheriaMaterialLibrary::GetAccretionDiskMaterial(this),
			AetheriaMindColors::IgnisPlasma, FLinearColor(1.0f, 0.18f, 0.04f, 1.0f), 12.0f, 1.0f))
		{
			SecondaryMesh->SetMaterial(0, QuasarMat);
		}
	}
	if (RingMeshA)
	{
		RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.35f, SphereScale * 1.35f, 1.0f));
		if (UMaterialInstanceDynamic* QuasarHalo = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, UAetheriaMaterialLibrary::GetAccretionDiskMaterial(this),
			FLinearColor(1.0f, 0.85f, 0.35f, 1.0f), AetheriaMindColors::IgnisPlasma, 10.0f, 1.0f))
		{
			RingMeshA->SetMaterial(0, QuasarHalo);
		}
	}
	if (TertiaryMesh)
	{
		if (UMaterialInstanceDynamic* QuasarJet = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, UAetheriaMaterialLibrary::GetSolarBeamMaterial(this),
			AetheriaMindColors::IgnisPlasma, FLinearColor(1.0f, 0.90f, 0.55f, 1.0f), 5.8f, 1.0f))
		{
			TertiaryMesh->SetMaterial(0, QuasarJet);
		}
	}
	if (InstancedFX)
	{
		if (UMaterialInstanceDynamic* IgnisFX = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this),
			AetheriaMindColors::IgnisPlasma, FLinearColor(1.0f, 0.92f, 0.45f, 1.0f), 5.5f, 1.0f))
		{
			InstancedFX->SetMaterial(0, IgnisFX);
		}
	}

	if (ZoneLight)
	{
		ZoneLight->SetLightColor(AetheriaMindColors::IgnisPlasma);
		ZoneLight->Intensity = 15000.0f;
	}

	AddNearbyWizardsCameraTrauma(GetWorld(), GetActorLocation(), 0.28f, 2200.0f);
	AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.75f, AetheriaMindColors::IgnisPlasma, 0.62f);
}

void AAetheriaSpellZone::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (SpellId != EAetheriaSpellId::None && LastSetupSpellId != SpellId)
	{
		SetupMeshesForSpell();
	}

	Age += DeltaTime;
	TickTimer += DeltaTime;

	switch (SpellId)
	{
	case EAetheriaSpellId::BlackHole:
		TickBlackHole(DeltaTime);
		break;
	case EAetheriaSpellId::Chronosphere:
		TickChronosphere(DeltaTime);
		break;
	case EAetheriaSpellId::ChaosMeteor:
		TickChaosMeteor(DeltaTime);
		break;
	case EAetheriaSpellId::GravityCataclysm:
		TickGravityCataclysm(DeltaTime);
		break;
	case EAetheriaSpellId::PrismWall:
		TickPrismWall(DeltaTime);
		break;
	case EAetheriaSpellId::ChronoBlink:
		TickChronoBlink(DeltaTime);
		break;
	case EAetheriaSpellId::SupernovaBeam:
		TickSupernovaBeam(DeltaTime);
		break;
	case EAetheriaSpellId::PrimalOverload:
		TickPrimalOverload(DeltaTime);
		break;
	case EAetheriaSpellId::Macropyre:
	case EAetheriaSpellId::EyeOfTheStorm:
		if (TickTimer >= 0.35f && HasAuthority() && GetWorld())
		{
			TickTimer = 0.0f;
			TArray<FOverlapResult> Overlaps;
			GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(RadiusUU));
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AActor* Victim = Overlap.GetActor())
				{
					DealDamageToMobOrActor(Victim, TickDamage, OwnerWizard.Get(), SpellId == EAetheriaSpellId::Macropyre);
				}
			}
		}
		break;
	case EAetheriaSpellId::PlasmaField:
	case EAetheriaSpellId::CataclysmNova:
	case EAetheriaSpellId::SunStrike:
	case EAetheriaSpellId::RequiemBlast:
		if (TickTimer >= 0.50f && HasAuthority() && GetWorld())
		{
			TickTimer = 0.0f;
			TArray<FOverlapResult> Overlaps;
			GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(RadiusUU));
			for (const FOverlapResult& Overlap : Overlaps)
			{
				if (AActor* Victim = Overlap.GetActor())
				{
					DealDamageToMobOrActor(Victim, TickDamage * 0.5f, OwnerWizard.Get(), SpellId == EAetheriaSpellId::CataclysmNova || SpellId == EAetheriaSpellId::SunStrike);
				}
			}
		}
		break;
	default:
		break;
	}

	if (Age >= Duration)
	{
		if (SpellId == EAetheriaSpellId::BlackHole)
		{
			UAetheriaMaterialLibrary::PlayGameSoundAtLocation(GetWorld(), TEXT("SFX_BlackHole_Collapse"), GetActorLocation(), 0.95f);
			// Relativistic Supernova Collapse Shockwaves + Hawking arcs
			const FLinearColor BlastColor = bSolarSingularityIgnited ? AetheriaMindColors::IgnisPlasma : PrimaryColor;
			AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.95f, BlastColor, 0.60f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.35f, AetheriaMindColors::MindCyan, 0.48f);
			for (int32 b = 0; b < 6; ++b)
			{
				const float Ang = b * (TWO_PI / 6.0f);
				const FVector ArcEnd = GetActorLocation() + FVector(FMath::Cos(Ang) * RadiusUU * 0.95f, FMath::Sin(Ang) * RadiusUU * 0.95f, 110.0f);
				EmitCosmicBolt(GetActorLocation() + FVector(0.0f, 0.0f, 118.0f), ArcEnd, AetheriaMindColors::MindCyan, 0.24f);
			}
			AddNearbyWizardsCameraTrauma(GetWorld(), GetActorLocation(), 0.35f, 2200.0f);
		}
		else if (SpellId == EAetheriaSpellId::ChaosMeteor && !bMeteorFinalEruption)
		{
			bMeteorFinalEruption = true;
			const FVector EndLoc = GetActorLocation();
			AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), EndLoc, RadiusUU * 1.75f, AetheriaMindColors::IgnisPlasma, 0.62f);
			AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), EndLoc, RadiusUU * 1.20f, AetheriaMindColors::MindViolet, 0.48f);
			for (int32 b = 0; b < 4; ++b)
			{
				const float Ang = b * HALF_PI + 0.35f;
				EmitCosmicBolt(EndLoc + FVector(0.0f, 0.0f, 90.0f), EndLoc + FVector(FMath::Cos(Ang) * RadiusUU * 0.9f, FMath::Sin(Ang) * RadiusUU * 0.9f, 20.0f), AetheriaMindColors::IgnisPlasma, 0.22f);
			}
			AddNearbyWizardsCameraTrauma(GetWorld(), EndLoc, 0.30f, 2000.0f);

			if (HasAuthority() && GetWorld())
			{
				TArray<FOverlapResult> Overlaps;
				GetWorld()->OverlapMultiByChannel(Overlaps, EndLoc, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(RadiusUU * 1.15f));
				for (const FOverlapResult& Overlap : Overlaps)
				{
					AActor* Victim = Overlap.GetActor();
					if (!Victim || Victim == this) continue;
					DealDamageToMobOrActor(Victim, TickDamage * 1.6f, OwnerWizard.Get(), true, 0.45f);
#if HAS_MOB_CHARACTER
					if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
					{
						Mob->ApplyStatusImpulse((Mob->GetActorLocation() - EndLoc).GetSafeNormal2D(), 560.0f, OwnerWizard.Get(), true);
					}
#endif
				}
			}
		}
		Destroy();
	}
}

void AAetheriaSpellZone::TickBlackHole(float DeltaTime)
{
	UWorld* World = GetWorld();
	if (!World) return;
	const FVector Center = GetActorLocation();
	const FVector HorizonOffset(0.0f, 0.0f, 55.0f);
	const float SphereScale = RadiusUU / 50.0f;

	// Final 0.28s Stillness/Implosion contraction before Supernova collapse flash
	float CollapseScaleMult = 1.0f;
	const float TimeLeft = Duration - Age;
	if (TimeLeft < 0.28f)
	{
		const float ImplodeT = FMath::Clamp(TimeLeft / 0.28f, 0.0f, 1.0f);
		CollapseScaleMult = FMath::Lerp(0.18f, 1.0f, ImplodeT * ImplodeT);
	}
	else if (Age < 0.22f)
	{
		const float OpenT = FMath::Clamp(Age / 0.22f, 0.0f, 1.0f);
		CollapseScaleMult = FMath::Lerp(0.15f, 1.0f, 1.0f - FMath::Pow(1.0f - OpenT, 3.0f));
	}

	const float MassGrowth = 1.0f + FMath::Min(0.24f, ConsumedMatterCount * 0.004f);
	if (DomeMesh)
	{
		DomeMesh->SetRelativeScale3D(FVector(SphereScale * 0.18f * MassGrowth * CollapseScaleMult));
	}
	if (SecondaryMesh)
	{
		SecondaryMesh->AddLocalRotation(FRotator(0.0f, (bSolarSingularityIgnited ? 340.0f : 230.0f) * DeltaTime, 0.0f));
		const float DiskBase = bSolarSingularityIgnited ? 1.65f : 1.35f;
		SecondaryMesh->SetRelativeScale3D(FVector(SphereScale * DiskBase * CollapseScaleMult, SphereScale * DiskBase * CollapseScaleMult, 1.0f));
	}
	if (RingMeshA)
	{
		const float Pulse = 1.0f + 0.04f * FMath::Sin(Age * 6.5f);
		const float LensBase = bSolarSingularityIgnited ? 1.35f : 1.15f;
		RingMeshA->SetRelativeScale3D(FVector(SphereScale * LensBase * CollapseScaleMult * Pulse, SphereScale * LensBase * CollapseScaleMult * Pulse, 1.0f));
	}
	if (TertiaryMesh)
	{
		TertiaryMesh->AddLocalRotation(FRotator(0.0f, 180.0f * DeltaTime, 0.0f));
		const float JetPulse = 1.0f + 0.25f * FMath::Sin(Age * 14.0f);
		TertiaryMesh->SetRelativeScale3D(FVector(0.18f * JetPulse, 0.18f * JetPulse, SphereScale * 1.10f * CollapseScaleMult));
	}
	if (GroundSealMesh)
	{
		GroundSealMesh->AddLocalRotation(FRotator(0.0f, -48.0f * DeltaTime, 0.0f));
	}

	// Extract real nearby arena boulders & props from AAetheriaBiomeBuilder on first tick!
	if (!bExtractedBiomeProps && World)
	{
		bExtractedBiomeProps = true;
#if HAS_BIOME_BUILDER
		TArray<FTransform> RippedTransforms;
		for (TActorIterator<AAetheriaBiomeBuilder> BBIt(World); BBIt; ++BBIt)
		{
			if (AAetheriaBiomeBuilder* BB = *BBIt)
			{
				BB->RipNearbyMatterForSingularity(Center, RadiusUU, 8, RippedTransforms);
				break;
			}
		}

		for (int32 r = 0; r < RippedTransforms.Num() && r < MatterChunks.Num(); ++r)
		{
			const FVector RelPos = RippedTransforms[r].GetLocation() - Center;
			const float Dist2D = FMath::Max(RadiusUU * 0.35f, RelPos.Size2D());
			MatterChunks[r].OrbitAngle = FMath::Atan2(RelPos.Y, RelPos.X);
			MatterChunks[r].StartRadius = Dist2D;
			MatterChunks[r].CurrentRadius = Dist2D;
			MatterChunks[r].CurrentZ = FMath::Max(0.0f, RelPos.Z);
			MatterChunks[r].BaseScale = FMath::Clamp(RippedTransforms[r].GetScale3D().GetMax() * 0.42f, 0.28f, 0.72f);
		}
#endif
	}

	// Update 56 Relativistic Star-Matter & Plasma Filaments spiraling smoothly down Keplerian orbits
	if (InstancedFX && StarFilamentParams.Num() > 0)
	{
		const int32 Count = StarFilamentParams.Num();
		for (int32 i = 0; i < Count; ++i)
		{
			FVector& P = StarFilamentParams[i]; // X=Angle, Y=CurRadius, Z=StartRadius
			const float Ratio = FMath::Clamp(P.Y / FMath::Max(1.0f, P.Z), 0.12f, 1.0f);
			const float AngVel = 2.6f + 11.5f * FMath::Pow(1.0f - Ratio, 1.45f);
			const float InfallSpeed = (180.0f + 520.0f * FMath::Square(1.0f - Ratio)) * DeltaTime;
			P.X += AngVel * DeltaTime;
			P.Y -= InfallSpeed;

			if (P.Y <= RadiusUU * 0.11f)
			{
				if (TimeLeft > 0.45f)
				{
					P.Z = FMath::FRandRange(RadiusUU * 0.72f, RadiusUU * 1.15f);
					P.Y = P.Z;
					P.X = FMath::FRandRange(0.0f, TWO_PI);
				}
				else
				{
					P.Y = RadiusUU * 0.05f;
				}
			}

			const float RNow = P.Y * CollapseScaleMult;
			const float DiskZ = HorizonOffset.Z + FMath::Sin(P.X * 1.0f) * (RNow * 0.22f) + FMath::Cos(P.X * 2.0f + i) * 14.0f;
			const FVector Pos(FMath::Cos(P.X) * RNow, FMath::Sin(P.X) * RNow, DiskZ);

			// Tangent velocity direction for relativistic streak elongation
			const FVector RadialVec(FMath::Cos(P.X), FMath::Sin(P.X), 0.0f);
			const FVector TangentVec(-FMath::Sin(P.X), FMath::Cos(P.X), 0.0f);
			const FVector FlowDir = (TangentVec * 0.78f - RadialVec * 0.62f).GetSafeNormal();
			const float Streak = FMath::Lerp(0.14f, 0.85f, 1.0f - Ratio);
			InstancedFX->UpdateInstanceTransform(i, FTransform(FlowDir.Rotation(), Pos, FVector(Streak, 0.065f, 0.065f)), false, false, true);
		}
		InstancedFX->MarkRenderStateDirty();
	}

	// Update 28 Torn Landscape Slabs & Boulders: lift off floor, spiral inward, spaghettify & cross Event Horizon
	int32 SwallowedThisFrame = 0;
	if (TerrainDebrisISM && MatterChunks.Num() > 0)
	{
		for (int32 i = 0; i < MatterChunks.Num(); ++i)
		{
			FAetheriaAbsorbedChunk& C = MatterChunks[i];
			const float Ratio = FMath::Clamp(C.CurrentRadius / FMath::Max(1.0f, C.StartRadius), 0.08f, 1.0f);
			const float Prox = 1.0f - Ratio;
			const float AngVel = 1.9f + 9.8f * FMath::Pow(Prox, 1.55f);
			const float InfallSpeed = (145.0f + 490.0f * Prox * Prox) * DeltaTime;

			C.OrbitAngle += AngVel * DeltaTime;
			C.CurrentRadius -= InfallSpeed;
			C.TumbleRot.Pitch += (120.0f + 360.0f * Prox) * DeltaTime;
			C.TumbleRot.Yaw += (95.0f + 290.0f * Prox) * DeltaTime;

			// Horizon crossing!
			if (C.CurrentRadius <= RadiusUU * 0.12f)
			{
				SwallowedThisFrame++;
				ConsumedMatterCount++;

				if (TimeLeft > 0.55f)
				{
					// Rip another slab out of the outer arena floor
					C.StartRadius = FMath::FRandRange(RadiusUU * 0.65f, RadiusUU * 1.15f);
					C.CurrentRadius = C.StartRadius;
					C.OrbitAngle = FMath::FRandRange(0.0f, TWO_PI);
					C.CurrentZ = 4.0f;
				}
				else
				{
					C.CurrentRadius = RadiusUU * 0.04f;
				}
			}

			const float RNow = C.CurrentRadius * CollapseScaleMult;
			// Smoothly lift off terrain floor up into the tilted accretion plane as it spirals inward
			const float LiftAlpha = FMath::SmoothStep(0.05f, 0.75f, Prox);
			const float OrbitZ = HorizonOffset.Z + FMath::Sin(C.OrbitAngle) * (RNow * 0.24f);
			C.CurrentZ = FMath::Lerp(4.0f, OrbitZ, LiftAlpha);

			const FVector Pos(FMath::Cos(C.OrbitAngle) * RNow, FMath::Sin(C.OrbitAngle) * RNow, C.CurrentZ);

			// Relativistic Spaghettification: stretch strongly along infall+orbital vector, thin cross-section
			const FVector Inward(-FMath::Cos(C.OrbitAngle), -FMath::Sin(C.OrbitAngle), (OrbitZ - C.CurrentZ) * 0.01f);
			const FVector OrbitTan(-FMath::Sin(C.OrbitAngle), FMath::Cos(C.OrbitAngle), 0.0f);
			const FVector TidalAxis = (Inward * 0.65f + OrbitTan * 0.75f).GetSafeNormal();

			const float Stretch = FMath::Lerp(1.0f, 4.2f, FMath::Pow(Prox, 2.1f));
			const float Thin = 1.0f / FMath::Sqrt(Stretch);
			const float ShrinkAtHorizon = (RNow < RadiusUU * 0.16f) ? FMath::Clamp(RNow / (RadiusUU * 0.16f), 0.15f, 1.0f) : 1.0f;
			const FVector ChunkScale = FVector(C.BaseScale * Stretch, C.BaseScale * Thin, C.BaseScale * Thin * 0.7f) * ShrinkAtHorizon;

			const FQuat FinalRot = TidalAxis.Rotation().Quaternion() * FRotator(0.0f, 0.0f, C.TumbleRot.Pitch).Quaternion();
			TerrainDebrisISM->UpdateInstanceTransform(i, FTransform(FinalRot, Pos, ChunkScale), false, false, true);
		}
		TerrainDebrisISM->MarkRenderStateDirty();
	}

	if (SwallowedThisFrame > 0 && World)
	{
		if (ZoneLight)
		{
			ZoneLight->Intensity = bSolarSingularityIgnited ? 18500.0f : 13500.0f;
		}
		if (ConsumedMatterCount % 4 == 0)
		{
			const float JetSide = FMath::RandBool() ? 1.0f : -1.0f;
			const FVector PoleStart = Center + HorizonOffset + FVector(0.0f, 0.0f, JetSide * RadiusUU * 0.22f);
			const FVector PoleEnd = Center + HorizonOffset + FVector(FMath::FRandRange(-85.0f, 85.0f), FMath::FRandRange(-85.0f, 85.0f), JetSide * RadiusUU * 0.92f);
			EmitCosmicBolt(PoleStart, PoleEnd, bSolarSingularityIgnited ? AetheriaMindColors::IgnisPlasma : AetheriaMindColors::MindCyan, 0.18f);
		}
	}
	else if (ZoneLight)
	{
		const float TargetLux = bSolarSingularityIgnited ? 15000.0f : 9500.0f;
		ZoneLight->Intensity = FMath::FInterpTo(ZoneLight->Intensity, TargetLux, DeltaTime, 6.0f);
	}

	// Relativistic 1/r^2 + Keplerian Vortex pull & Mob Spaghettification + Authority Damage
	if (World)
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(RadiusUU * 1.15f);
		World->OverlapMultiByChannel(Overlaps, Center, FQuat::Identity, ECC_Pawn, SphereShape);

		bool bAnyBurningInVortex = false;
		TArray<AAetheriaMobCharacter*> MobsInVortex;

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Victim = Overlap.GetActor();
			if (!Victim || Victim == this || Victim == OwnerWizard.Get()) continue;

			float MassScale = 1.0f;
			AAetheriaMobCharacter* MobPtr = nullptr;
#if HAS_MOB_CHARACTER
			if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
			{
				MobPtr = Mob;
				MobsInVortex.Add(Mob);
				if (Mob->BurnStacks > 0)
				{
					bAnyBurningInVortex = true;
				}

				switch (Mob->MobKind)
				{
				case EAetheriaMobKind::Swarm:   MassScale = 1.35f; break;
				case EAetheriaMobKind::Ranged:  MassScale = 1.10f; break;
				case EAetheriaMobKind::Bruiser: MassScale = 0.65f; break;
				case EAetheriaMobKind::Boss:    MassScale = 0.25f; break;
				}
			}
#endif
#if HAS_WIZARD_CHARACTER
			if (Cast<AAetheriaWizardCharacter>(Victim))
			{
				MassScale = 0.35f;
			}
#endif

			const FVector ToCenter = (Center - Victim->GetActorLocation());
			const float Dist2D = ToCenter.Size2D();
			const float NormProx = FMath::Clamp(1.0f - (Dist2D / FMath::Max(1.0f, RadiusUU * 1.15f)), 0.0f, 1.0f);

#if HAS_MOB_CHARACTER
			if (MobPtr)
			{
				MobPtr->ApplyGravitationalTidalPull((Center + HorizonOffset - Victim->GetActorLocation()).GetSafeNormal(), NormProx);
			}
#endif

			if (HasAuthority())
			{
				if (Dist2D > 24.0f)
				{
					const FVector Inward2D = FVector(ToCenter.X, ToCenter.Y, 0.0f).GetSafeNormal();
					const FVector Tangent2D = FVector::CrossProduct(Inward2D, FVector::UpVector);
					// Relativistic inward surge + Keplerian orbital swirl
					const float RadialPull = (260.0f + 840.0f * NormProx * NormProx) * MassScale;
					const float SwirlSpeed = (220.0f + 360.0f * NormProx) * MassScale;
					const FVector PullStep = (Inward2D * RadialPull + Tangent2D * SwirlSpeed) * DeltaTime;
					Victim->SetActorLocation(Victim->GetActorLocation() + PullStep, true);
				}

				if (TickTimer >= 0.25f && Dist2D <= RadiusUU * 1.02f)
				{
					const float CenterDmgMult = 0.85f + 0.45f * NormProx;
					DealDamageToMobOrActor(Victim, TickDamage * CenterDmgMult, OwnerWizard.Get());
				}
			}
		}

		// Firestorm Contagion: spreads 1 burn stack every 0.5s if any mob has BurnStacks > 0
		if (HasAuthority())
		{
			const bool bCrossedHalfSec = FMath::FloorToInt(Age / 0.5f) != FMath::FloorToInt((Age - DeltaTime) / 0.5f);
			if (bAnyBurningInVortex && bCrossedHalfSec)
			{
				if (!bSolarSingularityIgnited)
				{
					IgniteSolarSingularity();
				}

				for (AAetheriaMobCharacter* Mob : MobsInVortex)
				{
					if (Mob)
					{
						Mob->ApplyBurnStack(TickDamage * 4.0f, OwnerWizard.Get(), 1);
					}
				}
			}
		}
	}

	if (TickTimer >= 0.25f)
	{
		TickTimer = 0.0f;
	}
}

void AAetheriaSpellZone::EmitCosmicBolt(const FVector& From, const FVector& To, const FLinearColor& Color, float LifeTime, float WidthScale)
{
	UWorld* World = GetWorld();
	if (!World) return;
	TArray<FVector> Pts;
	Pts.Add(From);
	Pts.Add(FMath::Lerp(From, To, 0.5f) + FVector(FMath::FRandRange(-45.0f, 45.0f), FMath::FRandRange(-45.0f, 45.0f), FMath::FRandRange(-20.0f, 35.0f)));
	Pts.Add(To);
	AAetheriaLightningArcVFX::SpawnLightningArc(World, Pts, Color, LifeTime);
}

void AAetheriaSpellZone::TickChronosphere(float DeltaTime)
{
	if (!GetWorld()) return;
	const float SphereScale = RadiusUU / 50.0f;

	// Smooth dome expand (0.22s ease-out) & collapse (final 0.20s)
	float DomeMult = 1.0f;
	if (Age < 0.22f)
	{
		const float E = Age / 0.22f;
		DomeMult = FMath::Lerp(0.08f, 1.0f, 1.0f - FMath::Pow(1.0f - E, 3.0f));
	}
	else if (Duration - Age < 0.20f)
	{
		const float Rem = FMath::Max(0.0f, (Duration - Age) / 0.20f);
		DomeMult = FMath::Lerp(0.05f, 1.0f, Rem * Rem);
	}

	if (DomeMesh)
	{
		DomeMesh->SetRelativeScale3D(FVector(SphereScale * DomeMult, SphereScale * DomeMult, SphereScale * 0.88f * DomeMult));
		DomeMesh->AddLocalRotation(FRotator(0.0f, 14.0f * DeltaTime, 0.0f));
	}
	if (TertiaryMesh)
	{
		TertiaryMesh->SetVisibility(false);
	}
	if (GroundSealMesh)
	{
		GroundSealMesh->SetRelativeScale3D(FVector(SphereScale * 1.95f * DomeMult, SphereScale * 1.95f * DomeMult, 1.0f));
		GroundSealMesh->AddLocalRotation(FRotator(0.0f, 32.0f * DeltaTime, 0.0f));
	}
	if (RingMeshA)
	{
		RingMeshA->SetRelativeScale3D(FVector(SphereScale * 1.65f * DomeMult, SphereScale * 1.65f * DomeMult, 1.0f));
		RingMeshA->AddLocalRotation(FRotator(18.0f * DeltaTime, 72.0f * DeltaTime, 12.0f * DeltaTime));
	}
	if (RingMeshB)
	{
		RingMeshB->SetRelativeScale3D(FVector(SphereScale * 1.38f * DomeMult, SphereScale * 1.38f * DomeMult, 1.0f));
		RingMeshB->AddLocalRotation(FRotator(-24.0f * DeltaTime, -96.0f * DeltaTime, 20.0f * DeltaTime));
	}
	if (InstancedFX)
	{
		InstancedFX->AddLocalRotation(FRotator(0.0f, 7.5f * DeltaTime, 0.0f));
	}

	// Crackling cosmic lightning arcs from dome rim/apex into frozen interior
	BoltSpawnCooldown -= DeltaTime;
	if (BoltSpawnCooldown <= 0.0f)
	{
		BoltSpawnCooldown = 0.09f;
		const FVector Center = GetActorLocation();
		const float AngA = FMath::FRandRange(0.0f, TWO_PI);
		const float AngB = AngA + FMath::FRandRange(-1.4f, 1.4f);
		const FVector RimPt = Center + FVector(FMath::Cos(AngA) * RadiusUU * 0.85f, FMath::Sin(AngA) * RadiusUU * 0.85f, RadiusUU * FMath::FRandRange(0.32f, 0.72f));
		const FVector FloorPt = Center + FVector(FMath::Cos(AngB) * RadiusUU * FMath::FRandRange(0.25f, 0.75f), FMath::Sin(AngB) * RadiusUU * FMath::FRandRange(0.25f, 0.75f), 14.0f);
		const FLinearColor BoltCol = (FMath::RandBool()) ? AetheriaMindColors::MindCyan : FLinearColor(0.68f, 0.42f, 1.0f, 1.0f);
		EmitCosmicBolt(RimPt, FloorPt, BoltCol, 0.14f);
	}

	TSet<TWeakObjectPtr<AActor>> CurrentOverlaps;

	TArray<FOverlapResult> Overlaps;
	FCollisionShape SphereShape = FCollisionShape::MakeSphere(RadiusUU);
	GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_WorldDynamic, SphereShape);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Actor = Overlap.GetActor();
		if (!Actor || Actor == this) continue;

		CurrentOverlaps.Add(Actor);

		// Wizard speed buff
		bool bIsWizard = false;
#if HAS_WIZARD_CHARACTER
		if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(Actor))
		{
			bIsWizard = true;
			Wiz->CustomTimeDilation = 1.4f;
			Wiz->bInChronosphereBuff = true;
		}
#endif

		if (!bIsWizard)
		{
			// Mobs and enemy projectiles frozen in time
			Actor->CustomTimeDilation = 0.0001f;
#if HAS_MOB_CHARACTER
			if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Actor))
			{
				Mob->SetChronoFrozen(true);
			}
#endif
		}

		ChronoFrozenActors.Add(Actor);
	}

	// Restore dilation for actors that left the Chronosphere
	TArray<TWeakObjectPtr<AActor>> ToRemove;
	for (const TWeakObjectPtr<AActor>& OldActorPtr : ChronoFrozenActors)
	{
		if (!OldActorPtr.IsValid())
		{
			ToRemove.Add(OldActorPtr);
			continue;
		}

		if (!CurrentOverlaps.Contains(OldActorPtr))
		{
			AActor* OldActor = OldActorPtr.Get();
			OldActor->CustomTimeDilation = 1.0f;

#if HAS_MOB_CHARACTER
			if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(OldActor))
			{
				Mob->SetChronoFrozen(false);
			}
#endif
#if HAS_WIZARD_CHARACTER
			if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(OldActor))
			{
				Wiz->bInChronosphereBuff = false;
			}
#endif
			ToRemove.Add(OldActorPtr);
		}
	}

	for (const auto& Rem : ToRemove)
	{
		ChronoFrozenActors.Remove(Rem);
	}
}

void AAetheriaSpellZone::TickChaosMeteor(float DeltaTime)
{
	const float DropTime = 0.44f;
	const float SphereScale = RadiusUU / 50.0f;
	const FVector Fwd2D = CastDirection.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : CastDirection.GetSafeNormal2D();
	const FVector SkyOffset = -Fwd2D * 920.0f + FVector(0.0f, 0.0f, 1480.0f);
	const FVector TouchdownLocal(0.0f, 0.0f, RadiusUU * 0.42f);
	UWorld* World = GetWorld();

	if (Age < DropTime)
	{
		// Phase 1: Blazing diagonal atmospheric entry + crackling sky rift + 36-node plasma wake
		const float Alpha = FMath::Clamp(Age / DropTime, 0.0f, 1.0f);
		const float Accel = Alpha * Alpha; // gravitational entry acceleration
		const FVector CurLocalPos = FMath::Lerp(SkyOffset, TouchdownLocal, Accel);
		const FVector EntryDir = (TouchdownLocal - SkyOffset).GetSafeNormal();

		if (DomeMesh)
		{
			DomeMesh->SetRelativeLocation(CurLocalPos);
			DomeMesh->AddLocalRotation(FRotator(-480.0f * DeltaTime, 220.0f * DeltaTime, 0.0f));
		}
		if (TertiaryMesh)
		{
			TertiaryMesh->SetRelativeLocation(CurLocalPos + EntryDir * 18.0f);
			TertiaryMesh->SetRelativeRotation(EntryDir.Rotation());
			const float FlarePulse = 1.0f + 0.16f * FMath::Sin(Age * 42.0f);
			TertiaryMesh->SetRelativeScale3D(FVector(SphereScale * 1.25f * FlarePulse, SphereScale * 0.90f, SphereScale * 0.90f));
		}
		if (SecondaryMesh)
		{
			SecondaryMesh->AddLocalRotation(FRotator(0.0f, 240.0f * DeltaTime, 0.0f));
		}
		if (RingMeshA)
		{
			RingMeshA->AddLocalRotation(FRotator(0.0f, -320.0f * DeltaTime, 0.0f));
		}
		if (GroundSealMesh)
		{
			const float TargetScale = FMath::Lerp(2.45f, 1.55f, Accel) * SphereScale;
			GroundSealMesh->SetRelativeScale3D(FVector(TargetScale, TargetScale, 1.0f));
			GroundSealMesh->AddLocalRotation(FRotator(0.0f, 180.0f * DeltaTime, 0.0f));
		}
		if (RingMeshB)
		{
			const float InnerTarget = FMath::Lerp(0.45f, 1.65f, Accel) * SphereScale;
			RingMeshB->SetRelativeScale3D(FVector(InnerTarget, InnerTarget, 1.0f));
			RingMeshB->AddLocalRotation(FRotator(0.0f, -240.0f * DeltaTime, 0.0f));
		}

		// Plasma comet tail stretching up along entry vector
		if (PlasmaTrailISM && PlasmaTrailISM->GetInstanceCount() >= 36)
		{
			for (int32 i = 0; i < 36; ++i)
			{
				const float Lag = static_cast<float>(i) / 35.0f;
				const float BackDist = Lag * 780.0f;
				const FVector Wobble(
					FMath::Sin(Age * 26.0f + i * 1.3f) * (18.0f + Lag * 52.0f),
					FMath::Cos(Age * 22.0f + i * 1.7f) * (18.0f + Lag * 52.0f),
					FMath::Sin(Age * 19.0f + i) * (12.0f + Lag * 35.0f));
				const FVector TrailLoc = CurLocalPos - EntryDir * BackDist + Wobble;
				const float Scale = SphereScale * 0.42f * FMath::Pow(1.0f - Lag, 1.25f) * (0.85f + 0.35f * FMath::Sin(Age * 30.0f + i));
				PlasmaTrailISM->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, TrailLoc, FVector(FMath::Max(0.04f, Scale))), false, false, true);
			}
			PlasmaTrailISM->MarkRenderStateDirty();
		}

		// Celestial Rift Lightning snapping down into ground impact seal
		BoltSpawnCooldown -= DeltaTime;
		if (BoltSpawnCooldown <= 0.0f)
		{
			BoltSpawnCooldown = 0.09f;
			const FVector Center = GetActorLocation();
			const float Ang = FMath::FRandRange(0.0f, TWO_PI);
			EmitCosmicBolt(Center + CurLocalPos, Center + FVector(FMath::Cos(Ang) * RadiusUU * 0.7f, FMath::Sin(Ang) * RadiusUU * 0.7f, 12.0f), AetheriaMindColors::IgnisPlasma, 0.14f);
		}
		return;
	}

	// Phase 2: Cataclysmic Touchdown!
	if (!bMeteorLanded)
	{
		bMeteorLanded = true;
		const FVector InitLoc = GetActorLocation();
		const float InitGroundZ = SampleArenaSurface(World, FVector2D(InitLoc.X, InitLoc.Y));
		MeteorRollLocation = FVector(InitLoc.X, InitLoc.Y, InitGroundZ);
		SetActorLocation(MeteorRollLocation);

		MeteorFurrowPoints.Add(MeteorRollLocation + FVector(0.0f, 0.0f, 6.0f));
		NextFurrowDist = 65.0f;

		// Triple-layer seismic shockwaves + magma arc lightning + camera shake trauma
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, MeteorRollLocation, RadiusUU * 0.95f, AetheriaMindColors::CrispAlabaster, 0.38f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, MeteorRollLocation, RadiusUU * 1.65f, AetheriaMindColors::IgnisPlasma, 0.58f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(World, MeteorRollLocation, RadiusUU * 2.15f, AetheriaMindColors::MindViolet, 0.72f);
		AddNearbyWizardsCameraTrauma(World, MeteorRollLocation, 0.48f, 2600.0f);

		for (int32 b = 0; b < 5; ++b)
		{
			const float Ang = (b / 5.0f) * TWO_PI + FMath::FRandRange(-0.2f, 0.2f);
			EmitCosmicBolt(
				MeteorRollLocation + FVector(0.0f, 0.0f, 60.0f),
				MeteorRollLocation + FVector(FMath::Cos(Ang) * RadiusUU * 1.15f, FMath::Sin(Ang) * RadiusUU * 1.15f, 18.0f),
				(b % 2 == 0) ? AetheriaMindColors::IgnisPlasma : AetheriaMindColors::MindViolet,
				0.26f);
		}

		// Launch 10 Ballistic Molten Shrapnel Boulders (MatterChunks 0..9)
		for (int32 k = 0; k < 10 && k < MatterChunks.Num(); ++k)
		{
			const float Ang = (k / 10.0f) * TWO_PI + FMath::FRandRange(-0.22f, 0.22f);
			const float SpeedXY = FMath::FRandRange(380.0f, 760.0f);
			MatterChunks[k].bActive = true;
			MatterChunks[k].BallisticPos = MeteorRollLocation + FVector(FMath::Cos(Ang) * 45.0f, FMath::Sin(Ang) * 45.0f, 55.0f);
			MatterChunks[k].BallisticVel = FVector(FMath::Cos(Ang) * SpeedXY, FMath::Sin(Ang) * SpeedXY, FMath::FRandRange(540.0f, 920.0f)) + Fwd2D * 220.0f;
		}

		// Impact burst damage + radial knockback impulse
		if (HasAuthority() && World)
		{
			TArray<FOverlapResult> Overlaps;
			World->OverlapMultiByChannel(Overlaps, MeteorRollLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(RadiusUU * 1.10f));
			for (const FOverlapResult& Overlap : Overlaps)
			{
				AActor* Victim = Overlap.GetActor();
				if (!Victim || Victim == this) continue;
				DealDamageToMobOrActor(Victim, TickDamage * 3.5f, OwnerWizard.Get(), true, 0.6f);
#if HAS_MOB_CHARACTER
				if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
				{
					const FVector PushOut = (Mob->GetActorLocation() - MeteorRollLocation).GetSafeNormal2D();
					Mob->ApplyStatusImpulse(PushOut, 640.0f, OwnerWizard.Get(), true);
				}
#endif
			}
		}

		// Hide overhead sky portal after boulder bursts through
		if (SecondaryMesh) SecondaryMesh->SetVisibility(false);
		if (RingMeshA) RingMeshA->SetVisibility(false);
		if (RingMeshB) RingMeshB->SetVisibility(false);
	}

	// Phase 3: Terrain-Conforming Heavy Roll + Persistent Burning Magma Furrow + Ballistic Shrapnel
	const float RemainingTime = FMath::Max(0.15f, Duration - DropTime);
	const float RollSpeed = 1520.0f / RemainingTime;
	const float StepDist = RollSpeed * DeltaTime;

	MeteorRollLocation.X += Fwd2D.X * StepDist;
	MeteorRollLocation.Y += Fwd2D.Y * StepDist;

	FVector SurfaceNormal = FVector::UpVector;
	const float GroundZ = SampleArenaSurface(World, FVector2D(MeteorRollLocation.X, MeteorRollLocation.Y), &SurfaceNormal);
	MeteorRollLocation.Z = GroundZ;
	SetActorLocation(MeteorRollLocation);

	// Carve persistent burning lava furrow every 62 UU behind meteor
	NextFurrowDist -= StepDist;
	if (NextFurrowDist <= 0.0f && MeteorFurrowPoints.Num() < 24)
	{
		NextFurrowDist = 62.0f;
		MeteorFurrowPoints.Add(MeteorRollLocation + FVector(0.0f, 0.0f, 6.0f));
	}

	// Exact forward quaternion rotation: nose-down pitch along CastDirection world frame (no gimbal or backwards roll!)
	if (DomeMesh)
	{
		const float MeteorRadUU = FMath::Max(65.0f, RadiusUU * 0.42f);
		const float RollDeg = FMath::RadiansToDegrees(StepDist / MeteorRadUU);
		const FQuat DirFrame = FRotationMatrix::MakeFromX(Fwd2D).ToQuat();
		const FQuat ForwardSpin = DirFrame * FRotator(-RollDeg, 0.0f, 0.0f).Quaternion() * DirFrame.Inverse();
		MeteorSpinQuat = (ForwardSpin * MeteorSpinQuat).GetNormalized();

		DomeMesh->SetRelativeLocation(FVector(0.0f, 0.0f, MeteorRadUU * 0.92f));
		DomeMesh->SetRelativeRotation(MeteorSpinQuat);
	}
	if (TertiaryMesh)
	{
		TertiaryMesh->SetRelativeLocation(FVector(0.0f, 0.0f, RadiusUU * 0.42f));
		TertiaryMesh->SetRelativeRotation(Fwd2D.Rotation());
		const float CoronaPulse = 1.0f + 0.12f * FMath::Sin(Age * 24.0f);
		TertiaryMesh->SetRelativeScale3D(FVector(SphereScale * 1.10f * CoronaPulse, SphereScale * 0.92f, SphereScale * 0.85f));
	}
	if (GroundSealMesh)
	{
		GroundSealMesh->SetRelativeLocation(FVector(0.0f, 0.0f, 5.0f));
		GroundSealMesh->SetRelativeScale3D(FVector(SphereScale * 1.75f, SphereScale * 1.75f, 1.0f));
		GroundSealMesh->AddLocalRotation(FRotator(0.0f, 160.0f * DeltaTime, 0.0f));
	}

	// Animate 10 bouncing molten boulders + 24 ground magma furrow crust slabs anchored in world space
	if (TerrainDebrisISM && MatterChunks.Num() >= 34)
	{
		for (int32 k = 0; k < 10; ++k)
		{
			FAetheriaAbsorbedChunk& Frag = MatterChunks[k];
			if (!Frag.bActive) continue;

			Frag.BallisticVel.Z -= 1850.0f * DeltaTime;
			Frag.BallisticPos += Frag.BallisticVel * DeltaTime;
			Frag.TumbleRot.Pitch += 420.0f * DeltaTime;
			Frag.TumbleRot.Yaw += 310.0f * DeltaTime;

			const float FragGroundZ = SampleArenaSurface(World, FVector2D(Frag.BallisticPos.X, Frag.BallisticPos.Y));
			if (Frag.BallisticPos.Z <= FragGroundZ + 12.0f && Frag.BallisticVel.Z < 0.0f)
			{
				Frag.BallisticPos.Z = FragGroundZ + 12.0f;
				Frag.BallisticVel.Z = -Frag.BallisticVel.Z * 0.44f;
				Frag.BallisticVel.X *= 0.65f;
				Frag.BallisticVel.Y *= 0.65f;
				if (FMath::Abs(Frag.BallisticVel.Z) < 60.0f)
				{
					Frag.BallisticVel = FVector::ZeroVector;
				}
			}

			const FVector RelLoc = Frag.BallisticPos - MeteorRollLocation;
			TerrainDebrisISM->UpdateInstanceTransform(k, FTransform(Frag.TumbleRot, RelLoc, FVector(Frag.BaseScale)), false, false, true);
		}

		for (int32 f = 0; f < 24; ++f)
		{
			const int32 InstIdx = 10 + f;
			if (f < MeteorFurrowPoints.Num())
			{
				const FVector RelFurrow = MeteorFurrowPoints[f] - MeteorRollLocation;
				const float NormUnit = GetNormalizedUnitMeshScale(TerrainDebrisISM->GetStaticMesh());
				const float WideS = NormUnit * 0.85f * (0.75f + 0.25f * FMath::Sin(f * 1.9f));
				TerrainDebrisISM->UpdateInstanceTransform(InstIdx, FTransform(FRotator(0.0f, f * 37.0f, 0.0f), RelFurrow, FVector(WideS, WideS * 0.72f, WideS * 0.22f)), false, false, true);
			}
			else
			{
				TerrainDebrisISM->UpdateInstanceTransform(InstIdx, FTransform(FRotator::ZeroRotator, FVector::ZeroVector, FVector(0.001f)), false, false, true);
			}
		}
		TerrainDebrisISM->MarkRenderStateDirty();
	}

	// Animate 36 fire-plume & furrow flame embers
	if (PlasmaTrailISM && PlasmaTrailISM->GetInstanceCount() >= 36)
	{
		for (int32 p = 0; p < 36; ++p)
		{
			if (p < 14)
			{
				// Flames whipping backwards over rolling boulder
				const float Ang = Age * 14.0f + p * 0.9f;
				const float Back = static_cast<float>(p) * 18.0f;
				const FVector LocalPos = -Fwd2D * Back + FVector(FMath::Cos(Ang) * 45.0f, FMath::Sin(Ang) * 58.0f, RadiusUU * 0.35f + p * 8.0f);
				const float S = SphereScale * FMath::Lerp(0.32f, 0.08f, static_cast<float>(p) / 14.0f);
				PlasmaTrailISM->UpdateInstanceTransform(p, FTransform(FRotator::ZeroRotator, LocalPos, FVector(S)), false, false, true);
			}
			else if (MeteorFurrowPoints.Num() > 0)
			{
				// Dancing flame pillars rising along the carved magma furrow
				const int32 FurIdx = (p - 14) % MeteorFurrowPoints.Num();
				const float Phase = FMath::Fmod(Age * 3.8f + p * 0.37f, 1.0f);
				const FVector BaseWorld = MeteorFurrowPoints[FurIdx];
				const FVector FlameLoc = (BaseWorld - MeteorRollLocation) + FVector(
					FMath::Sin(Age * 10.0f + p) * 28.0f,
					FMath::Cos(Age * 9.0f + p) * 28.0f,
					Phase * 155.0f);
				const float FlameS = SphereScale * 0.24f * (1.0f - Phase);
				PlasmaTrailISM->UpdateInstanceTransform(p, FTransform(FRotator::ZeroRotator, FlameLoc, FVector(FMath::Max(0.03f, FlameS))), false, false, true);
			}
		}
		PlasmaTrailISM->MarkRenderStateDirty();
	}

	// Continuous magma & furrow burning ticks
	if (TickTimer >= 0.25f && HasAuthority() && World)
	{
		TickTimer = 0.0f;
		TSet<AActor*> BurnedThisTick;

		TArray<FOverlapResult> Overlaps;
		World->OverlapMultiByChannel(Overlaps, MeteorRollLocation, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(RadiusUU * 0.85f));
		for (const FOverlapResult& Overlap : Overlaps)
		{
			if (AActor* Victim = Overlap.GetActor())
			{
				BurnedThisTick.Add(Victim);
				DealDamageToMobOrActor(Victim, TickDamage, OwnerWizard.Get(), true);
			}
		}

		// Persistent magma furrow damages enemies standing in the scorched trench
		for (int32 f = 0; f < MeteorFurrowPoints.Num(); f += 2)
		{
			TArray<FOverlapResult> FurOverlaps;
			World->OverlapMultiByChannel(FurOverlaps, MeteorFurrowPoints[f], FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(135.0f));
			for (const FOverlapResult& Overlap : FurOverlaps)
			{
				if (AActor* Victim = Overlap.GetActor())
				{
					if (!BurnedThisTick.Contains(Victim))
					{
						BurnedThisTick.Add(Victim);
						DealDamageToMobOrActor(Victim, TickDamage * 0.45f, OwnerWizard.Get(), true);
					}
				}
			}
		}
	}
}

void AAetheriaSpellZone::TickGravityCataclysm(float DeltaTime)
{
	const float RiseDuration = 0.35f;
	const float SphereScale = RadiusUU / 50.0f;

	if (Age <= RiseDuration + 0.14f && InstancedFX)
	{
		const FVector Offsets[7] = {
			FVector::ZeroVector,
			FVector(240.0f, 60.0f, 0.0f),
			FVector(-230.0f, -80.0f, 0.0f),
			FVector(80.0f, 250.0f, 0.0f),
			FVector(-90.0f, -240.0f, 0.0f),
			FVector(190.0f, -180.0f, 0.0f),
			FVector(-180.0f, 190.0f, 0.0f)
		};

		for (int32 i = 0; i < 7; ++i)
		{
			// Staggered radial spire eruption (center bursts first, outer spires follow wave)
			const float Delay = (i == 0) ? 0.0f : 0.04f + (i % 3) * 0.03f;
			const float LocalAlpha = FMath::Clamp((Age - Delay) / RiseDuration, 0.0f, 1.0f);
			const float Eased = 1.0f - FMath::Pow(1.0f - LocalAlpha, 3.0f);
			const float Height = FMath::Lerp(-350.0f, 125.0f, Eased);
			FTransform T;
			if (InstancedFX->GetInstanceTransform(i, T))
			{
				T.SetLocation(Offsets[i] + FVector(0.0f, 0.0f, Height));
				InstancedFX->UpdateInstanceTransform(i, T, false, false, true);
			}
		}
		InstancedFX->MarkRenderStateDirty();
	}

	// Rising anti-gravity shock halo & floating zero-G glowing gravel stones
	if (RingMeshA)
	{
		const float RingAlpha = FMath::Clamp(Age / FMath::Max(0.1f, Duration), 0.0f, 1.0f);
		RingMeshA->SetRelativeLocation(FVector(0.0f, 0.0f, 25.0f + RingAlpha * 260.0f));
		RingMeshA->SetRelativeScale3D(FVector(SphereScale * (1.2f + RingAlpha * 0.9f), SphereScale * (1.2f + RingAlpha * 0.9f), 1.0f));
		RingMeshA->AddLocalRotation(FRotator(0.0f, 140.0f * DeltaTime, 0.0f));
	}
	if (GroundSealMesh)
	{
		GroundSealMesh->AddLocalRotation(FRotator(0.0f, -45.0f * DeltaTime, 0.0f));
	}
	if (PlasmaTrailISM && StarFilamentParams.Num() > 0)
	{
		for (int32 g = 0; g < StarFilamentParams.Num(); ++g)
		{
			const FVector& BaseP = StarFilamentParams[g];
			const float LiftZ = BaseP.Z + Age * (110.0f + (g % 4) * 35.0f);
			FTransform GT;
			if (PlasmaTrailISM->GetInstanceTransform(g, GT))
			{
				GT.SetLocation(FVector(BaseP.X, BaseP.Y, LiftZ));
				FRotator R = GT.GetRotation().Rotator();
				R.Pitch += 95.0f * DeltaTime;
				R.Yaw += 75.0f * DeltaTime;
				GT.SetRotation(R.Quaternion());
				PlasmaTrailISM->UpdateInstanceTransform(g, GT, false, false, true);
			}
		}
		PlasmaTrailISM->MarkRenderStateDirty();
	}

	// Airborne launch & stun on first tick
	if (Age <= DeltaTime * 1.5f && HasAuthority())
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.45f, PrimaryColor, 0.55f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 0.95f, AetheriaMindColors::IgnisPlasma, 0.42f);
		AddNearbyWizardsCameraTrauma(GetWorld(), GetActorLocation(), 0.25f, 1800.0f);

		TArray<FOverlapResult> Overlaps;
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(RadiusUU);
		GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Victim = Overlap.GetActor();
			if (!Victim || Victim == this || Victim == OwnerWizard.Get()) continue;

			DealDamageToMobOrActor(Victim, TickDamage * 2.8f, OwnerWizard.Get(), false, 1.2f /* 1.2s stun */);

#if HAS_MOB_CHARACTER
			if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
			{
				Mob->ApplyAirborneLift(950.0f, 1.2f, OwnerWizard.Get());
			}
			else
#endif
			if (ACharacter* Char = Cast<ACharacter>(Victim))
			{
				const AAetheriaGameState* GS = GetWorld() ? GetWorld()->GetGameState<AAetheriaGameState>() : nullptr;
				if (!GS || GS->GetEffectiveFriendlyFireScale() > 0.0f)
				{
					Char->LaunchCharacter(FVector(0.0f, 0.0f, 950.0f), true, true);
				}
			}
		}
	}
}

void AAetheriaSpellZone::TickPrismWall(float DeltaTime)
{
	if (!GetWorld()) return;
	// Shimmering crystal motes & periodic prismatic arcs across barrier crest
	if (PlasmaTrailISM && StarFilamentParams.Num() > 0)
	{
		const float TimeSec = GetWorld() ? GetWorld()->GetTimeSeconds() : Age;
		for (int32 m = 0; m < StarFilamentParams.Num(); ++m)
		{
			const FVector& BasePos = StarFilamentParams[m];
			const FVector BobPos = BasePos + FVector(0.0f, 0.0f, FMath::Sin(TimeSec * 4.5f + m * 0.7f) * 28.0f);
			FTransform MT;
			if (PlasmaTrailISM->GetInstanceTransform(m, MT))
			{
				MT.SetLocation(BobPos);
				PlasmaTrailISM->UpdateInstanceTransform(m, MT, false, false, true);
			}
		}
		PlasmaTrailISM->MarkRenderStateDirty();
	}

	BoltSpawnCooldown -= DeltaTime;
	if (BoltSpawnCooldown <= 0.0f)
	{
		BoltSpawnCooldown = 0.28f;
		const FVector Fwd = CastDirection.GetSafeNormal2D();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
		const FVector Center = GetActorLocation();
		const int32 A = FMath::RandRange(-3, 2);
		EmitCosmicBolt(Center + Right * (A * 135.0f) + FVector(0.0f, 0.0f, 230.0f), Center + Right * ((A + 1) * 135.0f) + FVector(0.0f, 0.0f, 230.0f), AetheriaMindColors::MindCyan, 0.16f);
	}

	// Slow mobs by 75%
	TArray<FOverlapResult> Overlaps;
	FCollisionShape BoxShape = FCollisionShape::MakeBox(FVector(60.0f, 480.0f, 200.0f));
	GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), CastDirection.Rotation().Quaternion(), ECC_Pawn, BoxShape);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Victim = Overlap.GetActor();
		if (!Victim || Victim == this || Victim == OwnerWizard.Get()) continue;

		Victim->CustomTimeDilation = 0.25f;
		ChronoFrozenActors.Add(Victim);

#if HAS_MOB_CHARACTER
		if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
		{
			Mob->SlowRecoveryTimer = 1.0f;
			Mob->RiftSpeedBurstTimer = 0.0f;
		}
#endif
	}
}

void AAetheriaSpellZone::TickChronoBlink(float DeltaTime)
{
	if (!GetWorld()) return;
	// Temporal glitch flicker & converging clockwork motes before 0.65s detonation
	const float DetonateTime = 0.65f;
	const float Alpha = FMath::Clamp(Age / DetonateTime, 0.0f, 1.0f);

	if (DomeMesh)
	{
		const float Flicker = 0.5f + 0.5f * FMath::Sin(Age * 36.0f);
		DomeMesh->SetVisibility(Flicker > 0.15f);
	}
	if (RingMeshA)
	{
		const float RingScale = FMath::Lerp(RadiusUU / 35.0f, 0.4f, Alpha * Alpha);
		RingMeshA->SetRelativeScale3D(FVector(RingScale, RingScale, 1.0f));
		RingMeshA->AddLocalRotation(FRotator(0.0f, 420.0f * DeltaTime, 0.0f));
	}
	if (GroundSealMesh)
	{
		GroundSealMesh->AddLocalRotation(FRotator(0.0f, -180.0f * DeltaTime, 0.0f));
	}
	if (InstancedFX && StarFilamentParams.Num() > 0)
	{
		for (int32 i = 0; i < StarFilamentParams.Num(); ++i)
		{
			const FVector& P = StarFilamentParams[i];
			const float Ang = P.X + Age * 8.5f;
			const float R = FMath::Lerp(P.Y, 18.0f, Alpha * Alpha);
			const FVector Pos(FMath::Cos(Ang) * R, FMath::Sin(Ang) * R, FMath::Lerp(P.Z, 55.0f, Alpha));
			InstancedFX->UpdateInstanceTransform(i, FTransform(FRotator::ZeroRotator, Pos, FVector(0.11f)), false, false, true);
		}
		InstancedFX->MarkRenderStateDirty();
	}

	// Pull mobs toward decoy
	if (HasAuthority())
	{
		TArray<FOverlapResult> Overlaps;
		FCollisionShape SphereShape = FCollisionShape::MakeSphere(RadiusUU);
		GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);

		for (const FOverlapResult& Overlap : Overlaps)
		{
			AActor* Victim = Overlap.GetActor();
			if (!Victim || Victim == this || Victim == OwnerWizard.Get()) continue;

#if HAS_MOB_CHARACTER
			if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
			{
				Mob->ApplyTaunt(GetActorLocation(), Duration);
			}
#endif

			FVector ToDecoy = (GetActorLocation() - Victim->GetActorLocation()).GetSafeNormal2D();
			Victim->SetActorLocation(Victim->GetActorLocation() + ToDecoy * (240.0f * DeltaTime), true);
		}
	}

	// Detonate after 0.65s
	if (Age >= DetonateTime)
	{
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.55f, AetheriaMindColors::MindCyan, 0.48f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.0f, AetheriaMindColors::MindViolet, 0.36f);

		if (HasAuthority())
		{
			TArray<FOverlapResult> Overlaps;
			FCollisionShape SphereShape = FCollisionShape::MakeSphere(RadiusUU);
			GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);
			for (const FOverlapResult& Overlap : Overlaps)
			{
				DealDamageToMobOrActor(Overlap.GetActor(), TickDamage * 2.5f, OwnerWizard.Get());
			}
		}
		Destroy();
	}
}

void AAetheriaSpellZone::TickSupernovaBeam(float DeltaTime)
{
	if (!GetWorld()) return;
	if (SecondaryMesh)
	{
		const float Pulse = 1.45f + 0.25f * FMath::Sin(Age * 32.0f);
		SecondaryMesh->SetRelativeScale3D(FVector(Pulse, Pulse, 26.0f));
	}
	if (RingMeshA)
	{
		RingMeshA->AddLocalRotation(FRotator(0.0f, 380.0f * DeltaTime, 0.0f));
	}
	if (TertiaryMesh)
	{
		const float TipPulse = 2.0f + 0.5f * FMath::Sin(Age * 28.0f);
		TertiaryMesh->SetRelativeScale3D(FVector(TipPulse));
	}

	// Animate 36 double-helix plasma coils spiraling along the 2600 UU beam
	if (InstancedFX && InstancedFX->GetInstanceCount() >= 36)
	{
		const FVector Fwd = CastDirection.GetSafeNormal();
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Fwd).GetSafeNormal();
		const FVector UpOrtho = FVector::CrossProduct(Right, Fwd).GetSafeNormal();

		for (int32 h = 0; h < 36; ++h)
		{
			const float T = static_cast<float>(h) / 35.0f;
			const float Dist = FMath::Fmod(T * 2550.0f + Age * 2200.0f, 2550.0f);
			const float HelixSign = (h % 2 == 0) ? 1.0f : -1.0f;
			const float Theta = (Dist * 0.012f) - Age * 15.0f + (HelixSign < 0.0f ? PI : 0.0f);
			const float HelixRad = 68.0f;
			const FVector Pos = Fwd * Dist + (Right * FMath::Cos(Theta) + UpOrtho * FMath::Sin(Theta)) * HelixRad;
			const float NodeScale = 0.14f + 0.08f * FMath::Sin(Age * 24.0f + h);
			InstancedFX->UpdateInstanceTransform(h, FTransform(FRotator::ZeroRotator, Pos, FVector(NodeScale)), false, false, true);
		}
		InstancedFX->MarkRenderStateDirty();
	}

	// Periodic electric plasma arcs shooting off beam axis
	BoltSpawnCooldown -= DeltaTime;
	if (BoltSpawnCooldown <= 0.0f)
	{
		BoltSpawnCooldown = 0.10f;
		const FVector Start = GetActorLocation();
		const FVector End = Start + CastDirection * 2550.0f;
		const float RandDist = FMath::FRandRange(200.0f, 2350.0f);
		const FVector Anchor = Start + CastDirection * RandDist;
		const FVector Offset(FMath::FRandRange(-160.0f, 160.0f), FMath::FRandRange(-160.0f, 160.0f), FMath::FRandRange(-80.0f, 140.0f));
		EmitCosmicBolt(Anchor, Anchor + Offset, AetheriaMindColors::MindCyan, 0.14f);
	}

	// Magicka-style Beam Crossing in co-op: two allied SupernovaBeams intersecting erupt into a resonant explosion
	if (HasAuthority() && !bBeamCrossTriggered && GetWorld())
	{
		const FVector StartA = GetActorLocation();
		const FVector EndA = StartA + CastDirection * 2600.0f;

		for (TActorIterator<AAetheriaSpellZone> It(GetWorld()); It; ++It)
		{
			AAetheriaSpellZone* Other = *It;
			if (!Other || Other == this || Other->bBeamCrossTriggered) continue;
			if (Other->GetSpellId() != EAetheriaSpellId::SupernovaBeam) continue;
			if (Other->OwnerWizard.Get() == OwnerWizard.Get()) continue;

			const FVector StartB = Other->GetActorLocation();
			const FVector EndB = StartB + Other->CastDirection * 2600.0f;
			FVector PtA = FVector::ZeroVector;
			FVector PtB = FVector::ZeroVector;
			FMath::SegmentDistToSegmentSafe(StartA, EndA, StartB, EndB, PtA, PtB);

			if (FVector::Dist(PtA, PtB) <= 240.0f &&
				FVector::Dist(PtA, StartA) > 180.0f &&
				FVector::Dist(PtB, StartB) > 180.0f)
			{
				bBeamCrossTriggered = true;
				Other->bBeamCrossTriggered = true;

				const FVector CrossPoint = 0.5f * (PtA + PtB);
				AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), CrossPoint, 920.0f, AetheriaMindColors::IgnisPlasma, 0.72f);
				AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), CrossPoint, 680.0f, AetheriaMindColors::MindCyan, 0.58f);
				for (int32 b = 0; b < 4; ++b)
				{
					const float Ang = b * HALF_PI + 0.4f;
					EmitCosmicBolt(CrossPoint, CrossPoint + FVector(FMath::Cos(Ang) * 480.0f, FMath::Sin(Ang) * 480.0f, 95.0f), AetheriaMindColors::MindCyan, 0.28f);
				}

				if (AAetheriaGameState* GS = GetWorld()->GetGameState<AAetheriaGameState>())
				{
					GS->SetSynergyBanner(TEXT("СКРЕЩИВАНИЕ ЛУЧЕЙ: Резонансная Сверхновая!"));
				}

#if HAS_WIZARD_CHARACTER
				if (AAetheriaWizardCharacter* WizA = Cast<AAetheriaWizardCharacter>(OwnerWizard.Get()))
				{
					WizA->AddCameraTrauma(0.45f);
				}
				if (AAetheriaWizardCharacter* WizB = Cast<AAetheriaWizardCharacter>(Other->OwnerWizard.Get()))
				{
					WizB->AddCameraTrauma(0.45f);
				}
#endif

				TArray<FOverlapResult> Overlaps;
				GetWorld()->OverlapMultiByChannel(Overlaps, CrossPoint, FQuat::Identity, ECC_Pawn, FCollisionShape::MakeSphere(900.0f));
				for (const FOverlapResult& Overlap : Overlaps)
				{
					AActor* Victim = Overlap.GetActor();
					if (!Victim || Victim == this || Victim == Other) continue;

#if HAS_MOB_CHARACTER
					if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Victim))
					{
						Mob->ApplySpellDamage(440.0f, OwnerWizard.Get(), true, 1.4f);
						Mob->ApplyStatusImpulse((Mob->GetActorLocation() - CrossPoint).GetSafeNormal2D(), 950.0f, OwnerWizard.Get(), true);
						continue;
					}
#endif
#if HAS_WIZARD_CHARACTER
					if (AAetheriaWizardCharacter* AllyWiz = Cast<AAetheriaWizardCharacter>(Victim))
					{
						if (FVector::Dist(CrossPoint, AllyWiz->GetActorLocation()) <= 450.0f)
						{
							DealDamageToMobOrActor(AllyWiz, 440.0f, OwnerWizard.Get(), true);
						}
					}
#endif
				}

				Other->Age = Other->Duration;
				Age = Duration;
				return;
			}
		}
	}

	// Ray damage sweep along 2600 UU beam cylinder
	if (TickTimer >= 0.15f && HasAuthority())
	{
		TickTimer = 0.0f;

		const FVector Start = GetActorLocation();
		const FVector End = Start + CastDirection * 2600.0f;

		TArray<FHitResult> Hits;
		FCollisionShape SweepCapsule = FCollisionShape::MakeCapsule(90.0f, 1300.0f);
		FCollisionQueryParams QueryParams;
		QueryParams.AddIgnoredActor(this);
		if (OwnerWizard.IsValid()) QueryParams.AddIgnoredActor(OwnerWizard.Get());
		if (GetOwner()) QueryParams.AddIgnoredActor(GetOwner());
		if (GetInstigator()) QueryParams.AddIgnoredActor(GetInstigator());

		GetWorld()->SweepMultiByChannel(Hits, Start, End, CastDirection.Rotation().Quaternion(), ECC_Pawn, SweepCapsule, QueryParams);

		for (const FHitResult& Hit : Hits)
		{
			DealDamageToMobOrActor(Hit.GetActor(), TickDamage, OwnerWizard.Get(), true);
		}
	}
}

void AAetheriaSpellZone::TickPrimalOverload(float DeltaTime)
{
	if (!GetWorld()) return;
	const float StillnessDuration = 0.35f;
	const float SphereScale = RadiusUU / 50.0f;

	// Phase 1: 0.35s cinematic contraction ("stillness before climax") with 48 imploding triad streaks
	if (Age < StillnessDuration)
	{
		const float ImplodeT = Age / StillnessDuration;
		const float ContractionAlpha = 1.0f - ImplodeT;
		const float Scale = FMath::Lerp(0.05f, 0.38f, ContractionAlpha * ContractionAlpha);
		if (DomeMesh)
		{
			DomeMesh->SetRelativeScale3D(FVector(Scale * SphereScale));
		}
		if (RingMeshA)
		{
			RingMeshA->SetRelativeScale3D(FVector(SphereScale * FMath::Lerp(0.18f, 1.45f, ContractionAlpha), SphereScale * FMath::Lerp(0.18f, 1.45f, ContractionAlpha), 1.0f));
			RingMeshA->AddLocalRotation(FRotator(0.0f, 420.0f * DeltaTime, 0.0f));
		}
		if (RingMeshB)
		{
			RingMeshB->SetRelativeScale3D(FVector(SphereScale * FMath::Lerp(0.18f, 1.2f, ContractionAlpha), SphereScale * FMath::Lerp(0.18f, 1.2f, ContractionAlpha), 1.0f));
			RingMeshB->AddLocalRotation(FRotator(0.0f, -480.0f * DeltaTime, 0.0f));
		}
		if (InstancedFX && StarFilamentParams.Num() > 0)
		{
			for (int32 p = 0; p < StarFilamentParams.Num(); ++p)
			{
				const FVector& P = StarFilamentParams[p];
				const float Ang = P.X + ImplodeT * 5.0f;
				const float R = P.Y * (1.0f - ImplodeT * ImplodeT);
				const FVector Pos(FMath::Cos(Ang) * FMath::Cos(P.Z) * R, FMath::Sin(Ang) * FMath::Cos(P.Z) * R, FMath::Sin(P.Z) * R + 40.0f);
				InstancedFX->UpdateInstanceTransform(p, FTransform(Pos.Rotation(), Pos, FVector(0.32f, 0.06f, 0.06f)), false, false, true);
			}
			InstancedFX->MarkRenderStateDirty();
		}
		if (ZoneLight)
		{
			ZoneLight->Intensity = 14000.0f * (1.0f - ContractionAlpha);
		}
	}
	// Phase 2: Massive 1100 UU triple-triad eruption shockwaves + shield + 6 homing missiles
	else if (!bOverloadExploded)
	{
		bOverloadExploded = true;

		if (DomeMesh)
		{
			DomeMesh->SetVisibility(false);
		}
		if (RingMeshA) RingMeshA->SetVisibility(false);
		if (RingMeshB) RingMeshB->SetVisibility(false);
		if (InstancedFX) InstancedFX->ClearInstances();

		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.65f, AetheriaMindColors::IgnisPlasma, 0.78f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 1.30f, AetheriaMindColors::MindCyan, 0.62f);
		AAetheriaLightningArcVFX::SpawnShockwaveRing(GetWorld(), GetActorLocation(), RadiusUU * 0.95f, AetheriaMindColors::MindViolet, 0.48f);
		AddNearbyWizardsCameraTrauma(GetWorld(), GetActorLocation(), 0.42f, 2200.0f);

		if (HasAuthority())
		{
			TArray<FOverlapResult> Overlaps;
			FCollisionShape SphereShape = FCollisionShape::MakeSphere(RadiusUU);
			GetWorld()->OverlapMultiByChannel(Overlaps, GetActorLocation(), FQuat::Identity, ECC_Pawn, SphereShape);

			for (const FOverlapResult& Overlap : Overlaps)
			{
				AActor* Target = Overlap.GetActor();
				if (!Target || Target == this) continue;

#if HAS_WIZARD_CHARACTER
				if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(Target))
				{
					if (Wiz == OwnerWizard.Get())
					{
						Wiz->AddShield(120.0f);
					}
					continue;
				}
#endif
				{
					// Push & damage enemy mobs
					DealDamageToMobOrActor(Target, TickDamage * 4.0f, OwnerWizard.Get());
					FVector PushDir = (Target->GetActorLocation() - GetActorLocation()).GetSafeNormal();

#if HAS_MOB_CHARACTER
					if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Target))
					{
						Mob->ApplyStatusImpulse(PushDir, 1200.0f, OwnerWizard.Get(), true);
					}
					else
#endif
					if (ACharacter* Char = Cast<ACharacter>(Target))
					{
						Char->LaunchCharacter(PushDir * 1200.0f + FVector(0.0f, 0.0f, 300.0f), true, true);
					}
				}
			}

			// Spawn 6 homing missiles fanning out 360 degrees
			for (int32 i = 0; i < 6; ++i)
			{
				float AngleDeg = i * 60.0f;
				FVector LaunchDir = FVector::ForwardVector.RotateAngleAxis(AngleDeg, FVector::UpVector);

				FActorSpawnParameters SpawnParams;
				SpawnParams.Owner = OwnerWizard.Get();
				SpawnParams.Instigator = Cast<APawn>(OwnerWizard.Get());
				SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

				AAetheriaSpellProjectile* Missile = GetWorld()->SpawnActor<AAetheriaSpellProjectile>(
					AAetheriaSpellProjectile::StaticClass(), GetActorLocation() + LaunchDir * 75.0f, LaunchDir.Rotation(), SpawnParams);

				if (Missile)
				{
					Missile->InitializeProjectile(
						EAetheriaSpellId::PrimalOverload,
						OwnerWizard.Get(),
						LaunchDir,
						TickDamage * 1.5f,
						1600.0f,
						120.0f,
						PrimaryColor,
						AccentColor,
						false,
						true
					);
					Missile->bIsHoming = true;
				}
			}
		}
	}
}

void AAetheriaSpellZone::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Cleanly restore CustomTimeDilation for all actors when Chronosphere or zone is destroyed
	for (const TWeakObjectPtr<AActor>& ActorPtr : ChronoFrozenActors)
	{
		if (ActorPtr.IsValid())
		{
			AActor* Actor = ActorPtr.Get();
			Actor->CustomTimeDilation = 1.0f;

#if HAS_MOB_CHARACTER
			if (AAetheriaMobCharacter* Mob = Cast<AAetheriaMobCharacter>(Actor))
			{
				Mob->SetChronoFrozen(false);
			}
#endif
#if HAS_WIZARD_CHARACTER
			if (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(Actor))
			{
				Wiz->bInChronosphereBuff = false;
			}
#endif
		}
	}
	ChronoFrozenActors.Empty();

	Super::EndPlay(EndPlayReason);
}

// ================================================================================================
// AAetheriaLightningArcVFX
// ================================================================================================

AAetheriaLightningArcVFX::AAetheriaLightningArcVFX()
{
	PrimaryActorTick.bCanEverTick = true;
	EnsureEngineMeshesLoaded();

	USceneComponent* ArcRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ArcRoot"));
	SetRootComponent(ArcRoot);
}

void AAetheriaLightningArcVFX::SpawnLightningArc(UWorld* World, const TArray<FVector>& Points, FLinearColor Color, float InLifetime)
{
	if (!World || Points.Num() < 2) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAetheriaLightningArcVFX* Arc = World->SpawnActor<AAetheriaLightningArcVFX>(
		AAetheriaLightningArcVFX::StaticClass(), Points[0], FRotator::ZeroRotator, Params);
	if (Arc)
	{
		Arc->InitializeSegments(Points, Color, InLifetime);
	}
}

void AAetheriaLightningArcVFX::SpawnShockwaveRing(UWorld* World, const FVector& Location, float RadiusUU, FLinearColor Color, float Duration)
{
	if (!World) return;

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AAetheriaLightningArcVFX* Ring = World->SpawnActor<AAetheriaLightningArcVFX>(
		AAetheriaLightningArcVFX::StaticClass(), Location, FRotator::ZeroRotator, Params);
	if (Ring)
	{
		Ring->InitializeRing(Location, RadiusUU, Color, Duration);
	}
}

void AAetheriaLightningArcVFX::InitializeSegments(const TArray<FVector>& Points, FLinearColor InColor, float InLifetime)
{
	EnsureEngineMeshesLoaded();

	bIsRing = false;
	ArcColor = InColor;
	Lifetime = FMath::Max(0.05f, InLifetime);
	Age = 0.0f;

	DynamicMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(this), ArcColor, ArcColor, 6.0f, 1.0f);

	auto SpawnCylinderSeg = [this](const FVector& A, const FVector& B, float WidthScale)
	{
		const FVector Diff = B - A;
		const float Dist = Diff.Size();
		if (Dist < 2.0f) return;

		UStaticMeshComponent* SegMesh = NewObject<UStaticMeshComponent>(this);
		SegMesh->RegisterComponent();
		SegMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
		SegMesh->SetStaticMesh(GEngineCylinderMesh);
		SegMesh->SetWorldLocation(A + Diff * 0.5f);
		SegMesh->SetWorldRotation(Diff.Rotation() + FRotator(90.0f, 0.0f, 0.0f));
		SegMesh->SetWorldScale3D(FVector(WidthScale, WidthScale, Dist / 100.0f));
		SegMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SegMesh->SetCastShadow(false);
		if (DynamicMat) SegMesh->SetMaterial(0, DynamicMat);
		MeshSegments.Add(SegMesh);
	};

	const int32 SubSegs = 6;
	for (int32 i = 0; i < Points.Num() - 1; ++i)
	{
		const FVector Start = Points[i];
		const FVector End = Points[i + 1];
		const FVector Dir = (End - Start).GetSafeNormal();
		FVector Side = FVector::CrossProduct(Dir, FVector::UpVector).GetSafeNormal();
		if (Side.IsNearlyZero()) Side = FVector::RightVector;
		const FVector UpOrtho = FVector::CrossProduct(Side, Dir).GetSafeNormal();
		const float SpanLen = FVector::Dist(Start, End);
		const float JitterAmp = FMath::Clamp(SpanLen * 0.085f, 14.0f, 44.0f);

		FVector Prev = Start;
		for (int32 k = 1; k <= SubSegs; ++k)
		{
			const float T = static_cast<float>(k) / static_cast<float>(SubSegs);
			FVector Next = FMath::Lerp(Start, End, T);
			if (k < SubSegs)
			{
				Next += Side * FMath::FRandRange(-JitterAmp, JitterAmp);
				Next += UpOrtho * FMath::FRandRange(-JitterAmp * 0.55f, JitterAmp * 0.55f);
			}
			SpawnCylinderSeg(Prev, Next, (k == 1 || k == SubSegs) ? 0.085f : 0.065f);

			// Micro branching fork off middle vertices
			if (k == 2 || k == 4)
			{
				const FVector BranchTip = Next + (Dir * 0.45f + Side * FMath::FRandRange(-0.9f, 0.9f) + UpOrtho * FMath::FRandRange(-0.5f, 0.5f)).GetSafeNormal() * FMath::FRandRange(35.0f, 85.0f);
				SpawnCylinderSeg(Next, BranchTip, 0.032f);
			}
			Prev = Next;
		}
	}
}

void AAetheriaLightningArcVFX::InitializeRing(const FVector& InCenter, float InMaxRadius, FLinearColor InColor, float InDuration)
{
	EnsureEngineMeshesLoaded();

	bIsRing = true;
	MaxRadius = InMaxRadius;
	CurrentRadius = 28.0f;
	ArcColor = InColor;
	Lifetime = FMath::Max(0.05f, InDuration);
	Age = 0.0f;

	SetActorLocation(InCenter + FVector(0.0f, 0.0f, 4.0f));

	DynamicMat = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
		this, UAetheriaMaterialLibrary::GetArcaneRingMaterial(this), ArcColor, ArcColor, 6.5f, 0.95f);

	UStaticMeshComponent* RingMesh = NewObject<UStaticMeshComponent>(this);
	RingMesh->RegisterComponent();
	if (GetRootComponent())
	{
		RingMesh->AttachToComponent(GetRootComponent(), FAttachmentTransformRules::KeepRelativeTransform);
	}
	else
	{
		SetRootComponent(RingMesh);
	}
	RingMesh->SetRelativeLocation(FVector::ZeroVector);
	RingMesh->SetStaticMesh(GEnginePlaneMesh);
	RingMesh->SetRelativeScale3D(FVector(0.4f, 0.4f, 1.0f));
	RingMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	RingMesh->SetCastShadow(false);
	if (DynamicMat) RingMesh->SetMaterial(0, DynamicMat);

	MeshSegments.Add(RingMesh);
}

void AAetheriaLightningArcVFX::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	Age += DeltaTime;
	const float Alpha = FMath::Clamp(Age / FMath::Max(0.01f, Lifetime), 0.0f, 1.0f);

	if (bIsRing)
	{
		const float Eased = 1.0f - FMath::Pow(1.0f - Alpha, 3.0f);
		CurrentRadius = FMath::Lerp(28.0f, MaxRadius, Eased);
		const float Diameter = CurrentRadius * 2.0f;
		const float ScaleXY = Diameter / 100.0f;

		if (MeshSegments.Num() > 0 && MeshSegments[0])
		{
			MeshSegments[0]->SetRelativeScale3D(FVector(ScaleXY, ScaleXY, 1.0f));
			MeshSegments[0]->SetRelativeRotation(FRotator(0.0f, Age * 90.0f, 0.0f));
		}
	}

	if (DynamicMat)
	{
		const float InitialFlash = (Age < 0.05f) ? 1.4f : 1.0f;
		const FLinearColor ActiveTint = (Age < 0.05f) ? FLinearColor::LerpUsingHSV(ArcColor, FLinearColor(1.0f, 0.96f, 0.88f, 1.0f), 0.25f) : ArcColor;
		DynamicMat->SetVectorParameterValue(FName("Color"), ActiveTint);
		DynamicMat->SetVectorParameterValue(FName("BaseColor"), ActiveTint);
		DynamicMat->SetVectorParameterValue(FName("EmissiveColor"), ActiveTint);
		DynamicMat->SetVectorParameterValue(FName("AccentColor"), ActiveTint);
		DynamicMat->SetScalarParameterValue(FName("Opacity"), (1.0f - Alpha) * 0.95f);
		DynamicMat->SetScalarParameterValue(FName("EmissiveBoost"), (1.0f - Alpha) * 6.5f * InitialFlash);
	}

	if (Age >= Lifetime)
	{
		Destroy();
	}
}
