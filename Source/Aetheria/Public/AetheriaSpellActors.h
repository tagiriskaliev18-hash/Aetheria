#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "AetheriaTypes.h"
#include "AetheriaSpellActors.generated.h"

class AAetheriaSpellZone;

/**
 * AAetheriaSpellProjectile
 * Replicated projectile actor supporting Fireball, Arcane Bolt, Primal Overload missiles, and enemy orbs.
 * Supports refraction fan splitting via PrismWall, homing logic, and Solar Singularity ignition on Black Holes.
 */
UCLASS()
class AETHERIA_API AAetheriaSpellProjectile : public AActor
{
	GENERATED_BODY()

public:
	AAetheriaSpellProjectile();

	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* CollisionComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UProjectileMovementComponent* MovementComp;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CoreMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* CoronaMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* TrailISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* GlowLight;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	EAetheriaSpellId SpellId = EAetheriaSpellId::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	float Damage = 120.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	float RadiusUU = 200.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	FLinearColor PrimaryColor;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	FLinearColor AccentColor;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	bool bHasSplitByPrism = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	bool bIsEnemyProjectile = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	bool bIsHoming = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> HomingTargetActor;

	UPROPERTY()
	TWeakObjectPtr<AActor> OwnerWizard;

	void InitializeProjectile(
		EAetheriaSpellId InSpellId,
		AActor* InOwnerWizard,
		const FVector& Direction,
		float InDamage,
		float InSpeedUU,
		float InRadiusUU,
		FLinearColor InPrimaryColor,
		FLinearColor InAccentColor,
		bool bInEnemy = false,
		bool bSplitVariant = false);

	void SetHomingTarget(AActor* Target);

	UFUNCTION()
	void OnOverlapBegin(
		UPrimitiveComponent* OverlappedComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		int32 OtherBodyIndex,
		bool bFromSweep,
		const FHitResult& SweepResult);

	UFUNCTION()
	void OnHit(
		UPrimitiveComponent* HitComp,
		AActor* OtherActor,
		UPrimitiveComponent* OtherComp,
		FVector NormalImpulse,
		const FHitResult& Hit);

	void Detonate();

protected:
	virtual void BeginPlay() override;

private:
	void UpdateTrailFX(float DeltaTime);

	bool bHasDetonated = false;
	float LifeTimer = 0.0f;
	float MaxLifetime = 6.0f;
	float SpinAngle = 0.0f;
	TArray<FVector> TrailHistory;
};

struct FAetheriaAbsorbedChunk
{
	float OrbitAngle = 0.0f;
	float CurrentRadius = 400.0f;
	float StartRadius = 500.0f;
	float CurrentZ = 0.0f;
	float BaseScale = 0.35f;
	FRotator TumbleRot = FRotator::ZeroRotator;
	FVector BallisticPos = FVector::ZeroVector;
	FVector BallisticVel = FVector::ZeroVector;
	bool bActive = true;
};

/**
 * AAetheriaSpellZone
 * Replicated persistent area effect actor implementing all 10 Aetheria spells:
 * Black Hole (EEE), Chronosphere (WWW), Chaos Meteor (QQE), Gravity Cataclysm (QEE),
 * Prism Wall (WEE), Chrono-Blink (WWE), Supernova Beam (QQW), Primal Overload (QWE), etc.
 */
UCLASS()
class AETHERIA_API AAetheriaSpellZone : public AActor
{
	GENERATED_BODY()

public:
	AAetheriaSpellZone();

	virtual void Tick(float DeltaTime) override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	USphereComponent* AreaCollision;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* DomeMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* SecondaryMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* TertiaryMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* InstancedFX;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* TerrainDebrisISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UInstancedStaticMeshComponent* PlasmaTrailISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RingMeshA;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* RingMeshB;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UStaticMeshComponent* GroundSealMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	UPointLightComponent* ZoneLight;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	EAetheriaSpellId SpellId = EAetheriaSpellId::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	float RadiusUU = 500.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	float Duration = 5.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	float TickDamage = 25.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	FLinearColor PrimaryColor;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	FLinearColor AccentColor;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	bool bIsEnemy = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Spell")
	bool bSolarSingularityIgnited = false;

	UPROPERTY()
	TWeakObjectPtr<AActor> OwnerWizard;

	void InitializeZone(
		EAetheriaSpellId InSpellId,
		AActor* InOwnerWizard,
		const FVector& TargetPoint,
		const FVector& Direction,
		float InRadiusUU,
		float InDuration,
		float InTickDamage,
		FLinearColor InPrimaryColor,
		FLinearColor InAccentColor,
		bool bInEnemy = false);

	EAetheriaSpellId GetSpellId() const { return SpellId; }
	bool IsBlackHoleIgnited() const { return bSolarSingularityIgnited; }
	void IgniteSolarSingularity();

protected:
	virtual void BeginPlay() override;

private:
	void EmitCosmicBolt(const FVector& From, const FVector& To, const FLinearColor& Color, float LifeTime = 0.14f, float WidthScale = 1.0f);
	float BoltSpawnCooldown = 0.0f;

	void TickBlackHole(float DeltaTime);
	void TickChronosphere(float DeltaTime);
	void TickChaosMeteor(float DeltaTime);
	void TickGravityCataclysm(float DeltaTime);
	void TickPrismWall(float DeltaTime);
	void TickChronoBlink(float DeltaTime);
	void TickSupernovaBeam(float DeltaTime);
	void TickPrimalOverload(float DeltaTime);

	void SetupMeshesForSpell();

	float Age = 0.0f;
	float TickTimer = 0.0f;
	UPROPERTY(Replicated)
	FVector TargetLocation = FVector::ZeroVector;
	UPROPERTY(Replicated)
	FVector CastDirection = FVector::ForwardVector;

	// Overload contraction phase tracking
	bool bOverloadExploded = false;
	bool bBeamCrossTriggered = false;

	// Meteor drop, terrain-conforming roll & persistent magma furrow
	bool bMeteorLanded = false;
	FVector MeteorRollLocation = FVector::ZeroVector;
	FQuat MeteorSpinQuat = FQuat::Identity;
	float NextFurrowDist = 0.0f;
	float NextFireTrailVfxDist = 0.0f;
	float PlungeTrailVfxCooldown = 0.0f;
	int32 FireTrailVfxStep = 0;
	TArray<FVector> MeteorFurrowPoints;

	// Black Hole matter & landscape absorption + Meteor ballistic shrapnel
	TArray<FAetheriaAbsorbedChunk> MatterChunks;
	TArray<FVector> StarFilamentParams; // X=AngleRad, Y=CurrentRadius, Z=StartRadius
	int32 ConsumedMatterCount = 0;
	bool bExtractedBiomeProps = false;
	bool bBlackHoleCollapseFlash = false;
	bool bMeteorFinalEruption = false;
	EAetheriaSpellId LastSetupSpellId = EAetheriaSpellId::None;

	// Set of actors currently affected by Chronosphere to cleanly restore dilation
	TSet<TWeakObjectPtr<AActor>> ChronoFrozenActors;
};

/**
 * AAetheriaLightningArcVFX
 * Procedural electric arcs & shockwave ring visualizer actor.
 */
UCLASS()
class AETHERIA_API AAetheriaLightningArcVFX : public AActor
{
	GENERATED_BODY()

public:
	AAetheriaLightningArcVFX();

	virtual void Tick(float DeltaTime) override;

	static void SpawnLightningArc(UWorld* World, const TArray<FVector>& Points, FLinearColor Color, float InLifetime = 0.45f);
	static void SpawnShockwaveRing(UWorld* World, const FVector& Location, float RadiusUU, FLinearColor Color, float Duration = 0.5f);

	void InitializeSegments(const TArray<FVector>& Points, FLinearColor InColor, float InLifetime = 0.45f);
	void InitializeRing(const FVector& InCenter, float InMaxRadius, FLinearColor InColor, float InDuration = 0.5f);

private:
	UPROPERTY()
	TArray<UStaticMeshComponent*> MeshSegments;

	UPROPERTY()
	UMaterialInstanceDynamic* DynamicMat = nullptr;

	bool bIsRing = false;
	float MaxRadius = 500.0f;
	float CurrentRadius = 10.0f;
	float Lifetime = 0.45f;
	float Age = 0.0f;
	FLinearColor ArcColor;
};
