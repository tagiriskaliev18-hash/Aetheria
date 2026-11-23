#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "AetheriaTypes.h"
#include "AetheriaPlayerController.generated.h"

class UStaticMeshComponent;
class UPointLightComponent;
class UMaterialInstanceDynamic;

UCLASS()
class AETHERIA_API AAetheriaTargetingReticle : public AActor
{
	GENERATED_BODY()

public:
	AAetheriaTargetingReticle();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reticle")
	UStaticMeshComponent* RingMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Reticle")
	UStaticMeshComponent* LineMesh;

	void UpdateReticle(
		bool bVisible,
		EAetheriaTargetShape Shape,
		const FVector& Origin,
		const FVector& AimPoint,
		float RadiusUU,
		float RangeUU,
		FLinearColor Color);

private:
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* ReticleDynamicMat = nullptr;
};

UCLASS()
class AETHERIA_API AAetheriaMoveMarker : public AActor
{
	GENERATED_BODY()

public:
	AAetheriaMoveMarker();

	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;

	UPROPERTY()
	UStaticMeshComponent* ArrowMeshes[4];

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MoveMarker")
	UStaticMeshComponent* GroundRingMesh = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "MoveMarker")
	UPointLightComponent* MarkerLight = nullptr;

	void PlayClickOrder(const FVector& TargetLocation);
	void SetMovingActive(bool bActive);

private:
	UPROPERTY(Transient)
	UMaterialInstanceDynamic* MarkerDynMat = nullptr;

	bool bUsingChevronMesh = false;
	bool bAnimConverging = false;
	bool bMoveDestinationActive = false;
	float ConvergeTimer = 0.0f;
	float ConvergeDuration = 0.38f;
};

UCLASS(Config = Game)
class AETHERIA_API AAetheriaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AAetheriaPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;

	void OnMouseWheelUp();
	void OnMouseWheelDown();

	UFUNCTION(BlueprintCallable, Category = "Aetheria|UI")
	void UI_SetHeroArchetype(int32 ArchetypeIndex);

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Settings")
	EAetheriaCastMode PreferredCastMode = EAetheriaCastMode::InvokeThenAim;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Settings")
	bool bAutoInvokeOnThree = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Settings")
	bool bReducedMotion = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Settings")
	float MasterVolume = 0.75f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Settings")
	bool bFullscreen = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Settings")
	int32 GraphicsPresetIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Input")
	int32 ArmedSlotIndex = -1;

	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Input")
	FVector CachedGroundAim = FVector::ZeroVector;

	// UI & Input Triggers
	void UI_PushOrb(EAetheriaOrb Orb);
	void UI_InvokeOrCast();
	void UI_SelectOrCastSlot(int32 SlotIndex);
	void UI_Dash();
	void UI_SetCastMode(EAetheriaCastMode NewMode);
	void UI_SetAutoInvoke(bool bEnabled);
	void UI_SetReducedMotion(bool bEnabled);
	void UI_SetVolume(float NewVolume);
	void UI_ToggleFullscreen();
	void UI_QuitToDesktop();
	void UI_ApplyGraphicsPreset(int32 PresetIdx);
	void UI_UpdateCustomization(const FAetheriaCharacterCustomization& NewCust, bool bPersistToDisk = true);
	void UI_SetEditorPreviewCamera(bool bOpen, float TurntableYaw);
	void UI_StartGame(bool bHostCoop, const FString& PlayerName, int32 StartLocIndex, EAetheriaPacingMode Pacing, int32 BotCount);
	void UI_JoinByAddress(const FString& IpAddress, const FString& PlayerName);

	// Server RPCs
	UFUNCTION(Server, Reliable)
	void ServerAddBot();

	UFUNCTION(Server, Reliable)
	void ServerRemoveBot();

	UFUNCTION(Server, Reliable)
	void ServerWarpLocation(int32 LocationIndex, int32 Act);

	UFUNCTION(Server, Reliable)
	void ServerSetPacing(EAetheriaPacingMode Pacing);

	UFUNCTION(Server, Reliable)
	void ServerEnterPortal();

	UFUNCTION(Server, Reliable)
	void ServerSetWizardName(const FString& NewName);

	UFUNCTION(Server, Reliable)
	void ServerConfigureMatch(const FString& PlayerName, int32 StartLocIndex, EAetheriaPacingMode Pacing, int32 BotCount);

	UFUNCTION(Server, Reliable)
	void ServerToggleFriendlyFire();

private:
	void OnKeyOrbQ();
	void OnKeyOrbW();
	void OnKeyOrbE();
	void OnKeyInvokeR();
	void OnKeySlotD();
	void OnKeySlotF();
	void OnKeyDashSpace();
	void OnKeyCharacterEditorC();
	void OnKeyUseFlask();
	void OnKeySelfCast();
	void OnKeyEmpowerEssence();
	void OnKeyToggleFriendlyFire();
	void OnLeftMouseDown();
	void OnRightMouseDown();
	void OnRightMouseUp();
	void OnKeyGrimoireTab();
	void OnKeyWorldMapM();
	void OnKeyEscape();
	void OnKeyEnterPortal();

	void UpdateGroundAimRay();
	void UpdateMovementInput(float DeltaTime);

	int32 InstantComboCount = 0;
	int32 BufferedSlotIndex = -1;
	bool bBufferedInvoke = false;
	double InputBufferExpireTime = 0.0;
	bool bClickMoveActive = false;
	bool bRightMouseHeld = false;
	FVector ClickMoveDestination = FVector::ZeroVector;

	UPROPERTY()
	AAetheriaTargetingReticle* TargetingReticle = nullptr;

	UPROPERTY()
	AAetheriaMoveMarker* MoveMarker = nullptr;
};
