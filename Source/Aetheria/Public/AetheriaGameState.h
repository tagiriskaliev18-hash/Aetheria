#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "AetheriaTypes.h"
#include "AetheriaGameState.generated.h"

UCLASS()
class AETHERIA_API AAetheriaGameState : public AGameStateBase
{
	GENERATED_BODY()

public:
	AAetheriaGameState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	int32 CurrentLocationIndex = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	int32 CurrentAct = 1;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	FString ActTitle;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	FString ActDescription;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	float ActProgress = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	float LocationElapsedSeconds = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	int32 EstimatedLocationMinutes = 6;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	EAetheriaPacingMode PacingMode = EAetheriaPacingMode::DemoBlitz;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	bool bBossSpawned = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	bool bBossDefeated = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	bool bPortalOpen = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	bool bCampaignComplete = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	FString BossName;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	float BossHP = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	float BossMaxHP = 0.0f;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	TArray<EAetheriaOrb> BossOrbs;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	FString ActiveSynergyBanner;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	TArray<FAetheriaObjectiveInfo> Objectives;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	int32 ActivePlayerCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	int32 ActiveBotCount = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	EAetheriaOrb ActiveTriadPhaseOrb = EAetheriaOrb::Ignis;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	EAetheriaWavePhase CurrentWavePhase = EAetheriaWavePhase::SanctuaryGrace;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	int32 CurrentWaveNumber = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	FString ActiveAmbushAlert;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	bool bFriendlyFireEnabled = true;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|State")
	float FriendlyFireScale = 0.30f;

	UFUNCTION(BlueprintPure, Category = "Aetheria|State")
	float GetEffectiveFriendlyFireScale() const { return bFriendlyFireEnabled ? FriendlyFireScale : 0.0f; }

	UFUNCTION(BlueprintCallable, Category = "Aetheria|State")
	void SetSynergyBanner(const FString& InBanner) { ActiveSynergyBanner = InBanner; }

	UFUNCTION(BlueprintCallable, Category = "Aetheria|State")
	void ClearSynergyBanner() { ActiveSynergyBanner.Empty(); }

	UFUNCTION(BlueprintCallable, Category = "Aetheria|State")
	void SetAmbushAlert(const FString& InAlert) { ActiveAmbushAlert = InAlert; }

	UFUNCTION(BlueprintCallable, Category = "Aetheria|State")
	void ClearAmbushAlert() { ActiveAmbushAlert.Empty(); }

	const FAetheriaObjectiveInfo* GetActiveObjective() const;

	FAetheriaObjectiveInfo* FindObjective(int32 ObjectiveId);

	UFUNCTION(BlueprintPure, Category = "Aetheria|State")
	float GetBossHPPercent() const
	{
		return BossMaxHP > 0.0f ? FMath::Clamp(BossHP / BossMaxHP, 0.0f, 1.0f) : 0.0f;
	}
};
