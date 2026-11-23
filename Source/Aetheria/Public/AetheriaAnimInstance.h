#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "AetheriaTypes.h"
class ACharacter;

#include "AetheriaAnimInstance.generated.h"

class UAnimSequence;
class UAnimMontage;
class UBlendSpace;
class USkeletalMesh;

UENUM(BlueprintType)
enum class EAetheriaAnimAction : uint8
{
	CastPrimary,
	CastHeavy,
	Dash,
	AttackMelee,
	AttackSlam,
	HitReact,
	Death
};

/**
 * C++ AnimInstance used by the hero and all four mob types.
 * Drives 2D locomotion (Idle + four run directions) and plays action montages.
 * Works even if an AnimBlueprint graph is missing: locomotion is applied via
 * PlaySlotAnimationAsDynamicMontage / single-node fallback on the mesh.
 */
UCLASS()
class AETHERIA_API UAetheriaAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	virtual void NativeInitializeAnimation() override;
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	/** Load sequences / montages / optional blendspace from a Content folder. */
	UFUNCTION(BlueprintCallable, Category = "Aetheria|Anim")
	void ConfigureFromFolder(const FString& ContentFolder);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Anim")
	void PlayAction(EAetheriaAnimAction Action, float PlayRate = 1.0f);
	void PlayCastForSpell(EAetheriaSpellId SpellId);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Anim")
	bool IsActionPlaying() const;

	/** True once ConfigureFromFolder resolved at least idle or run clips. */
	UFUNCTION(BlueprintPure, Category = "Aetheria|Anim")
	bool HasLocomotionSequences() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|Anim")
	FString AssetFolder;

	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Anim")
	float Speed = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Anim")
	float Direction = 0.0f;

	/** Local-space forward component, -1..1 (Run_Fwd / Run_Back). */
	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Anim")
	float StrafeY = 0.0f;

	/** Local-space right component, -1..1 (Run_StrafeR / Run_StrafeL). */
	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Anim")
	float StrafeX = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Anim")
	bool bIsMoving = false;

	UPROPERTY(BlueprintReadOnly, Category = "Aetheria|Anim")
	bool bIsDead = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UBlendSpace> LocomotionBlendSpace = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimSequence> IdleSeq = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimSequence> RunFwdSeq = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimSequence> RunBackSeq = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimSequence> RunStrafeLSeq = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimSequence> RunStrafeRSeq = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimMontage> CastPrimaryMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimMontage> CastHeavyMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimMontage> DashMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimMontage> AttackMeleeMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimMontage> AttackSlamMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimMontage> HitReactMontage = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Anim")
	TObjectPtr<UAnimMontage> DeathMontage = nullptr;

	/** Binds a skeletal mesh + this anim class onto a character's GetMesh(). */
	static bool BindSkeletalPresentation(
		ACharacter* Character,
		const FString& CharacterFolder,
		const FString& PreferredMeshName,
		float CapsuleHalfHeight);

	/** Bind an explicit mesh (and optional Paragon AnimBP). Falls back to UAetheriaAnimInstance + AnimFolder clips. */
	static bool BindExplicitPresentation(
		ACharacter* Character,
		const FString& MeshPackagePath,
		const FString& AnimFolder,
		const FString& AnimBlueprintPath,
		float CapsuleHalfHeight,
		float MeshScale = 1.0f);

	/** Prefer a completed Fab/Paragon pack when its packages exist; otherwise the Godot import folder. */
	static bool BindBestPresentation(
		ACharacter* Character,
		float CapsuleHalfHeight,
		const TArray<FString>& ParagonRoots,
		const FString& HeroName,
		const FString& FallbackFolder,
		const FString& FallbackMeshName);

	/** Editor-only: fill BS_Locomotion 2D blendspaces (Idle/Fwd/Back/Strafe). */
	UFUNCTION(BlueprintCallable, Category = "Aetheria|Anim")
	static int32 FillLocomotionBlendSpaces();

private:
	void ResolveAssetsIfNeeded();
	void TickLocomotionPlayback();
	UAnimSequence* PickLocomotionSequence() const;
	UAnimSequence* FindSequenceByNames(const TArray<FString>& Names) const;
	UAnimMontage* FindMontageByNames(const TArray<FString>& Names) const;
	UAnimMontage* EnsureMontage(UAnimMontage* Montage, UAnimSequence* Source, const TCHAR* SlotName);
	TMap<FString, TWeakObjectPtr<UAnimationAsset>> FolderAssets;
	bool bAssetsResolved = false;
	TWeakObjectPtr<UAnimSequence> CurrentLocomotion;
	float LastLocomotionDirection = 0.0f;
};
