#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AetheriaTypes.h"
#include "AetheriaMobCharacter.generated.h"

class UStaticMeshComponent;
class AAetheriaWizardCharacter;
class UMaterialInstanceDynamic;

UCLASS()
class AETHERIA_API AAetheriaMobCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAetheriaMobCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(ReplicatedUsing = OnRep_MobDisplayName, BlueprintReadOnly, Category = "Aetheria|Mob")
	FString MobDisplayName;

	UPROPERTY(ReplicatedUsing = OnRep_MobKind, BlueprintReadOnly, Category = "Aetheria|Mob")
	EAetheriaMobKind MobKind = EAetheriaMobKind::Swarm;

	UPROPERTY(ReplicatedUsing = OnRep_HP, BlueprintReadOnly, Category = "Aetheria|Mob")
	float HP = 150.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	float MaxHP = 150.0f;

	UPROPERTY(ReplicatedUsing = OnRep_IsFrozen, BlueprintReadOnly, Category = "Aetheria|Mob")
	bool bIsFrozen = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	int32 BossPhase = 1;

	UPROPERTY(ReplicatedUsing = OnRep_BossOrbs, BlueprintReadOnly, Category = "Aetheria|Mob")
	TArray<EAetheriaOrb> BossOrbs;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	bool bIsTelegraphingSlam = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	int32 BurnStacks = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	bool bIsStaggered = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	float StaggerGauge = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	bool bIsTaunted = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	EAetheriaAmbushKind SpawnAmbushKind = EAetheriaAmbushKind::None;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Mob")
	bool bIsEliteLeader = false;

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void InitAmbushEmergence(EAetheriaAmbushKind InKind, bool bInElite);
	void BeginTypedEntrance();
	bool IsEntranceLocked() const { return bEntranceLocked; }

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void ApplyBurnStack(float BaseSpellDamage, AActor* InstigatorWizard, int32 StacksToAdd = 1);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void ApplyStatusImpulse(const FVector& PushDir2D, float BaseImpulseUU, AActor* InstigatorWizard, bool bFromGravityOrBlast = true);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void ApplyAirborneLift(float BaseLaunchZ, float StunDuration, AActor* InstigatorWizard);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void ApplyTaunt(const FVector& DecoyLocation, float Duration);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void ApplyElectroShock(float Duration);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Mob")
	bool IsAirborneOrJuggled() const;

	void ApplyGravitationalTidalPull(const FVector& ToSingularityDir, float NormalizedProximity);

	float GravitationalStretchAlpha = 0.0f;
	FVector GravitationalPullDir = FVector::ZeroVector;

	float SlowRecoveryTimer = 0.0f;
	float RiftSpeedBurstTimer = 0.0f;
	bool bEntranceLocked = false;
	float EntranceStartZ = 0.0f;
	float EmergenceTimer = 0.0f;

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void InitFromArchetype(const FAetheriaMobArchetype& Archetype, float HPScale, float DamageScale, int32 InLocationIndex);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void ApplySpellDamage(float Amount, AActor* InstigatorWizard, bool bIgnite = false, float StunDuration = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void TriggerImpactHitstop(float Duration = 0.048f);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Mob")
	void SetChronoFrozen(bool bFrozen);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Mob")
	bool IsChronoFrozen() const { return bIsFrozen; }

	UFUNCTION()
	void OnRep_MobKind();

	UFUNCTION()
	void OnRep_MobDisplayName();

	UFUNCTION()
	void OnRep_HP();

	UFUNCTION()
	void OnRep_IsFrozen();

	UFUNCTION()
	void OnRep_BossOrbs();

	void UpdateVisualSilhouette();
	void PlayPresentationAction(uint8 Action);
	bool bUsingSkeletalPresentation = false;
	void UpdateMaterials();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* BodyMeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* HeadMeshComponent = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* ShoulderLeftMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* ShoulderRightMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* CoreMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* FinMesh1 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* FinMesh2 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* FinMesh3 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* FinMesh4 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* OrbMesh1 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* OrbMesh2 = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visual")
	UStaticMeshComponent* OrbMesh3 = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	float BaseDamage = 16.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	float AttackRangeUU = 200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	float AttackCooldown = 1.4f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	bool bExplodeOnDeath = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	bool bCastsOrbs = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	int32 LocationIndex = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	FLinearColor BodyColor = FLinearColor(0.12f, 0.14f, 0.22f, 1.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Combat")
	FLinearColor AccentColor = FLinearColor(0.48f, 0.18f, 0.97f, 1.0f);

private:
	float AttackTimer = 0.0f;
	float StunTimeRemaining = 0.0f;
	float FrozenTimeRemaining = 0.0f;
	float BurnTimeRemaining = 0.0f;
	float BurnDamagePerSec = 0.0f;
	float OrbitAngle = 0.0f;
	float SlamCooldownTimer = 8.0f;
	float SlamTelegraphTimer = 0.0f;
	FVector SlamTargetLocation = FVector::ZeroVector;
	float OrbCastTimer = 4.0f;

	float StaggerTimer = 0.0f;
	float UnstoppableTimer = 0.0f;
	float StunWindowTimer = 0.0f;
	int32 StunCountInWindow = 0;

	float ThermalShockCooldown = 0.0f;
	float WildfireSpreadTimer = 0.0f;
	float PanicWeavePhase = 0.0f;

	int32 BossSpellCycle = 0;
	float WindupAnimTimer = 0.0f;
	float LungeAnimTimer = 0.0f;
	float CurrentBankRoll = 0.0f;
	float PrevMobYaw = 0.0f;
	bool bDying = false;
	float DeathDissolveTimer = 0.0f;
	FVector DeathFallAxis = FVector::RightVector;
	float HitstopTimer = 0.0f;
	float PrevStrideSin = 0.0f;

	float AirborneTrackingTimer = 0.0f;
	bool bWasAirborneLastFrame = false;

	float ElectroShockTimer = 0.0f;

	float TauntTimer = 0.0f;
	FVector TauntTargetLoc = FVector::ZeroVector;

	// Procedural animation & materials
	float WalkPhase = 0.0f;
	float AttackAnimTimer = 0.0f;
	float HitFlashTimer = 0.0f;
	float BaseMeshZOffset = -72.0f;
	FVector BaseMeshScale = FVector(1.0f);
	FRotator BaseMeshRotation = FRotator::ZeroRotator;
	float HitRecoilTimer = 0.0f;
	float LandingSquashTimer = 0.0f;
	float SlamSnapTimer = 0.0f;
	float StaggerPoseAlpha = 0.0f;
	bool bPrevTelegraphingSlam = false;
	float LastSeenHP = -1.0f;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* DynamicBodyMaterial = nullptr;

	TWeakObjectPtr<AActor> LastInstigator;

	void ServerTickAI(float DeltaTime);
	void ExecuteMeleeAttack(AAetheriaWizardCharacter* TargetWizard);
	void ExecuteRangedAttack(AAetheriaWizardCharacter* TargetWizard);
	void ExecuteBossSlam();
	void ExecuteBossOrbSpell(AAetheriaWizardCharacter* TargetWizard);
	void Die();
};
