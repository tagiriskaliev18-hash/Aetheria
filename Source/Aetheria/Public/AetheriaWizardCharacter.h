#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AetheriaTypes.h"
#include "AetheriaWizardCharacter.generated.h"

class USpringArmComponent;
class UCameraComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AETHERIA_API AAetheriaWizardCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	AAetheriaWizardCharacter();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	// -------------------------------------------------------------
	// Camera & Visual Components
	// -------------------------------------------------------------
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* TopDownCameraComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* CloakMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* MantleMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* HeadMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* StaffMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* StaffGemMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* CastSigilMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UPointLightComponent* StaffLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	USceneComponent* StaffTip;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* ShieldMesh;

	// Cosmetic & Familiar Visual Components
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* HeadCosmeticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* BackCosmeticMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* FloatingGrimoireMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* FamiliarSkullMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UPointLightComponent* FamiliarLight;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UPointLightComponent* HeroRimLight;

	// Orbiting elemental triad (Q, W, E)
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	USceneComponent* OrbPivot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* OrbMesh0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* OrbMesh1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UStaticMeshComponent* OrbMesh2;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UPointLightComponent* OrbLight0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UPointLightComponent* OrbLight1;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aetheria|Visuals")
	UPointLightComponent* OrbLight2;

	// -------------------------------------------------------------
	// Replicated Customization State
	// -------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_Customization, BlueprintReadOnly, Category = "Aetheria|Customization")
	FAetheriaCharacterCustomization Customization;

	UFUNCTION()
	void OnRep_Customization();

	UFUNCTION(Server, Reliable)
	void ServerSetCustomization(FAetheriaCharacterCustomization NewCustomization);

	void ApplyCustomizationVisuals();
	void SetEditorCameraMode(bool bOpen, float TurntableYaw);

	// -------------------------------------------------------------
	// Replicated Combat State
	// -------------------------------------------------------------
	UPROPERTY(ReplicatedUsing = OnRep_ActiveOrbs, BlueprintReadOnly, Category = "Aetheria|Wizard")
	TArray<EAetheriaOrb> ActiveOrbs;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	EAetheriaSpellId SlotD = EAetheriaSpellId::Fireball;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	EAetheriaSpellId SlotF = EAetheriaSpellId::Chronosphere;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float SlotDCooldownRemaining = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float SlotFCooldownRemaining = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	TArray<float> SpellCooldowns;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float HP = 450.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float MaxHP = 450.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float Mana = 250.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float MaxMana = 250.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float Shield = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float DashCooldownRemaining = 0.0f;

	UPROPERTY(ReplicatedUsing = OnRep_PartyColorIndex, BlueprintReadOnly, Category = "Aetheria|Wizard")
	int32 PartyColorIndex = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	bool bIsAlive = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	int32 ColdStacks = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	bool bInChronosphereBuff = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	bool bOnHighGround = false;

	bool bInSunkenTrench = false;
	float RespawnTimer = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	int32 Kills = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	float DamageDealt = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	FString WizardName = TEXT("Mage");

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	int32 WizardLevel = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	int32 CurrentXP = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Wizard")
	int32 ArcaneEssence = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Relics")
	float RelicIgnisBonus = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Relics")
	float RelicChronoCDR = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Relics")
	float RelicBlackHoleRadiusBonus = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Relics")
	float RelicPrismSplitBonus = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Relics")
	float RelicAllElementBonus = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Progression")
	int32 EssenceEmpowerRank = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Survival")
	int32 HealingFlaskCharges = 3;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Survival")
	int32 MaxHealingFlaskCharges = 3;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Survival")
	float FlaskCooldownRemaining = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Status")
	bool bIsWet = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Status")
	bool bIsBurning = false;

	// -------------------------------------------------------------
	// Server / Multicast RPCs
	// -------------------------------------------------------------
	UFUNCTION(Server, Reliable)
	void ServerPushOrb(EAetheriaOrb Orb);

	UFUNCTION(Server, Reliable)
	void ServerInvoke(bool bAutoCast, FVector AimPoint);

	UFUNCTION(Server, Reliable)
	void ServerCastSlot(int32 SlotIndex, FVector AimPoint);

	UFUNCTION(Server, Reliable)
	void ServerInstantCastCombo(FVector AimPoint);

	UFUNCTION(Server, Reliable)
	void ServerBasicAttack(FVector AimPoint);

	UFUNCTION(Server, Reliable)
	void ServerDash(FVector AimPoint);

	UFUNCTION(Server, Reliable)
	void ServerUseHealingFlask();

	UFUNCTION(Server, Reliable)
	void ServerSelfCast();

	UFUNCTION(Server, Reliable)
	void ServerEmpowerWithEssence();

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlaySpellFX(EAetheriaSpellId SpellId, FVector Origin, FVector Target, const TArray<FVector>& ChainPoints);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayDashFX(FVector StartPos, FVector EndPos, FVector DashDir);

	UFUNCTION(NetMulticast, Unreliable)
	void MulticastPlayInvokeFX();

	// -------------------------------------------------------------
	// Spell, Camera & Locomotion Helpers
	// -------------------------------------------------------------
	FVector GetSpellMuzzleLocation() const;
	void AttachHeroStaff();
	void PlaySpellCastAnimation(EAetheriaSpellId SpellId);
	void PlayBossIntroFocus(AActor* Target, float Duration = 2.4f);
	void FaceWorldPoint(const FVector& TargetPoint);
	void SetCursorAimPoint(const FVector& WorldPoint);
	void PlayPresentationAction(uint8 Action);
	bool bUsingSkeletalPresentation = false;
	void SpawnAfterimageGhost(const FVector& WorldPos, const FRotator& WorldRot, float AlphaScale);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Camera")
	void SetLobbyCameraMode(bool bLobbyActive);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Camera")
	bool IsInLobbyCameraMode() const { return bIsInLobbyMode; }

	UFUNCTION(BlueprintPure, Category = "Aetheria|Camera")
	bool IsInEditorCameraMode() const { return bIsInEditorMode; }

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Camera")
	void AdjustCameraZoom(float WheelDelta);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Camera")
	void AddCameraTrauma(float Amount);

	// -------------------------------------------------------------
	// Spell & Combat Execution
	// -------------------------------------------------------------
	UFUNCTION(BlueprintCallable, Category = "Aetheria|Combat")
	void ExecuteSpellServer(EAetheriaSpellId SpellId, const FVector& AimPoint);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Combat")
	void ApplyDamageToWizard(float Amount);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Combat")
	void AddShield(float Amount);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Combat")
	bool IsSpellOffCooldown(EAetheriaSpellId SpellId) const;

	UFUNCTION(BlueprintPure, Category = "Aetheria|Combat")
	float GetSpellCooldownRemaining(EAetheriaSpellId SpellId) const;

	UFUNCTION(BlueprintPure, Category = "Aetheria|Combat")
	float GetSlotCooldown(int32 SlotIndex) const;

	UFUNCTION()
	void OnRep_ActiveOrbs();

	UFUNCTION()
	void OnRep_PartyColorIndex();

	void AwardProgression(int32 XPDelta, int32 EssenceDelta);
	void GrantRelicBonus(const FString& RelicId, const FString& RelicName, float RarityPowerMult = 1.0f);
	void ApplyWizardBurn(float Duration, float DPS);
	void ApplyWizardWet(float Duration);
	void RefillFlaskFromKill(bool bIsEliteOrBoss);
	float GetSpellPowerMultiplier(EAetheriaSpellId SpellId) const;
	float GetEffectiveSpellCooldown(EAetheriaSpellId SpellId, float BaseCooldown) const;
	void ReviveWizard(float HealthFraction = 0.65f);
	void SetupSculptedMeshes();
	void UpdateOrbVisuals();
	void UpdateShieldVisuals();

	UStaticMeshComponent* GetOrbMesh(int32 Index) const;
	UPointLightComponent* GetOrbLight(int32 Index) const;

protected:
	float OrbitAngle = 0.0f;
	float DashInvulnRemaining = 0.0f;
	bool bProgressionLoaded = false;

	float WetTimer = 0.0f;
	float WizardBurnTimer = 0.0f;
	float WizardBurnTickAccum = 0.0f;
	float WizardBurnDPS = 8.0f;
	float HasteBuffTimer = 0.0f;
	float SelfCastCooldownRemaining = 0.0f;
	int32 FlaskKillProgress = 0;
	TArray<FString> OwnedRelicIds;

	// Procedural animation state
	float WalkCyclePhase = 0.0f;
	float CastAnimTimer = 0.0f;
	float DashAnimTimer = 0.0f;
	float HitFlashTimer = 0.0f;
	float HitRecoilTimer = 0.0f;
	float InvokePulseTimer = 0.0f;
	float DownedAnimAlpha = 0.0f;
	float LastSeenWizardHP = -1.0f;

	// Editor turntable camera
	bool bIsInEditorMode = false;
	bool bIsInLobbyMode = false;
	bool bStaffAttachedToHand = false;
	float BossFocusTimer = 0.0f;
	TWeakObjectPtr<AActor> BossFocusTarget;
	float EditorTurntableYaw = 0.0f;
	float DesiredCameraArmLength = 1650.0f;
	float CameraShakeTrauma = 0.0f;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> CameraOccluders;
	float OccluderScanTimer = 0.0f;
	void UpdateCameraOccluders(float DeltaSeconds);
	float CurrentLookYawOffset = 0.0f;
	float CurrentLookPitchOffset = 0.0f;
	int32 LastPreviewArchetype = -1;

	// Locomotion banking, 3-phase cast & kinematic dash state
	float CurrentBankRoll = 0.0f;
	float PrevActorYaw = 0.0f;
	float DesiredAimYaw = 0.0f;
	bool bHasDesiredAimYaw = false;
	bool bGlideDashing = false;
	float GlideDashElapsed = 0.0f;
	float GlideDashDuration = 0.22f;
	FVector GlideDashVelocity = FVector::ZeroVector;
	int32 DashGhostStep = 0;

	// Advanced procedural animation & locomotion smoothing state (non-replicated visual)
	FVector SmoothedLocomotionVel = FVector::ZeroVector;
	float PrevStrideSin = 0.0f;
	float ClothFollowPitch = 0.0f;
	float ClothFollowRoll = 0.0f;
	float FamiliarBobVel = 0.0f;
	EAetheriaSpellId ActiveCastAnimSpell = EAetheriaSpellId::None;
	bool bUsingCustomSculptedHero = false;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* DynamicOrbMaterials[3] = { nullptr, nullptr, nullptr };

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* DynamicShieldMaterial = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* DynamicStaffGemMaterial = nullptr;

	UPROPERTY(Transient)
	UMaterialInstanceDynamic* DynamicCloakMaterial = nullptr;

	void SetupDynamicMaterials();
};
