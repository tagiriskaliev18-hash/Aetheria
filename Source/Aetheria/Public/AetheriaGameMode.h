#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "AetheriaTypes.h"
#include "AetheriaCookRefs.h"
#include "AetheriaGameMode.generated.h"

class AAetheriaBiomeBuilder;
class AAetheriaMobCharacter;
class AAetheriaBotAIController;
class AAetheriaWizardCharacter;
class AAetheriaGameState;
class AAetheriaSanctumGate;
class UAudioComponent;
class USoundBase;

struct FAetheriaRarityEntry
{
	FString Id;
	FString NameRu;
	int32 Weight = 55;
	float PowerMult = 1.0f;
};

struct FAetheriaRelicEntry
{
	FString Id;
	FString Name;
	FString Bonus;
};

UCLASS()
class AETHERIA_API AAetheriaGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AAetheriaGameMode();

	/** Hard refs to dynamically-loaded content, keeps it in the cook. */
	UPROPERTY()
	TObjectPtr<UAetheriaCookRefs> CookRefs;

	virtual void BeginPlay() override;
	virtual void PostLogin(APlayerController* NewPlayer) override;
	virtual void Logout(AController* Exiting) override;

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void AddBotWizard();

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void RemoveBotWizard();

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void WarpToLocation(int32 NewLocationIndex, int32 OptionalAct = 1);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void SetPacingMode(EAetheriaPacingMode NewPacing);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void EnterAstralPortal();

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void NotifyMobKilled(AAetheriaMobCharacter* Mob, AActor* Killer);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void NotifyPylonDamaged(int32 PylonIndex, float Damage, AActor* InstigatorWizard = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void QueueAmbush(EAetheriaAmbushKind Kind, const FVector& Location, int32 SpawnCount, int32 ArchIndex, bool bElite, const FString& AlertText);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|GameMode")
	void ToggleFriendlyFire();

	UFUNCTION(BlueprintPure, Category = "Aetheria|GameMode")
	AAetheriaBiomeBuilder* GetBiomeBuilder() const;

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	void StartMainMenu();

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	void StartWorld(EAetheriaWorldId WorldId, bool bFromMenu = true);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	void OpenActiveGate();

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Audio")
	void PlayBiomeBGM(EAetheriaWorldId WorldId, bool bBossEncounter = false);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	bool IsInMainMenu() const { return bInMainMenu; }

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	bool IsCombatReleased() const { return bCombatReleased; }

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	EAetheriaWorldId GetActiveWorld() const { return ActiveWorld; }

	void ApplyWorldPresentation();
	void RestampCourtyardForMenu();


	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aetheria|GameMode")
	int32 MaxPlayers = 6;

protected:
	UPROPERTY()
	AAetheriaBiomeBuilder* BiomeBuilder = nullptr;

	UPROPERTY()
	TArray<AAetheriaBotAIController*> ActiveBotControllers;

	FTimerHandle SimulationTimerHandle;
	FTimerHandle PresentationTimerHandle;

	int32 CurrentAct = 1;
	int32 LegionKillsCurrent = 0;
	int32 LegionKillsTarget = 40;
	float MobSpawnTimer = 0.0f;
	int32 MaxActiveMobs = 26;

	float HazardTimer = 0.0f;
	float TriadPhaseTimer = 0.0f;
	float MirrorLaserAngle = 0.0f;

	TArray<FAetheriaPendingAmbush> PendingAmbushes;
	float GraceTimer = 0.0f;
	float BreatherTimer = 0.0f;
	float AmbushCooldownTimer = 14.0f;
	bool bConstructAwakeningTriggered = false;
	int32 LastTriggeredBossPhase = 1;
	TArray<int32> CounteredObjectiveIds;
	bool bBreatherReady = false;

	TWeakObjectPtr<AAetheriaMobCharacter> ActiveBossMob;

	int32 LootPityCounter = 0;
	int32 PityThresholdKills = 18;
	float PartyWipeResetTimer = 0.0f;
	TArray<FAetheriaRarityEntry> LoadedRarities;
	TArray<FAetheriaRelicEntry> LoadedRelics;

	void LoadLootTablesFromJson();
	void OnSimulationTick();
	void SetAct(int32 ActIndex);
	void SimulateHazards(const FAetheriaLocationDef& LocDef, float DeltaTime);
	void SpawnBoss(const FAetheriaLocationDef& LocDef, float BossHPScale);
	void SpawnPortalObjective();

	void SpawnWaveAroundActiveObjective(const FAetheriaLocationDef& LocDef, EAetheriaObjectiveKind ObjKind);
	void SpawnSiegeWave(const FAetheriaLocationDef& LocDef);
	void SpawnLegionWave(const FAetheriaLocationDef& LocDef);

	void TickPendingAmbushes(float DeltaTime);
	int32 SelectProgressiveArchetype(const FAetheriaLocationDef& LocDef, float ActProgress, int32 WaveNumber);
	void SpawnProgressiveWave(const FAetheriaLocationDef& LocDef, const FVector& AnchorLocation);

	EAetheriaWorldId ActiveWorld = EAetheriaWorldId::Bastion;
	bool bInMainMenu = true;
	bool bCombatReleased = false;
	TWeakObjectPtr<AAetheriaSanctumGate> ActiveGate;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> BiomeBGMComp = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<USoundBase> ActiveBGMTrack = nullptr;

	bool bStageEntryAmbushSpawned = false;
	bool bGateOpenVFXTriggered = false;
	bool bVictoryPortalVFXTriggered = false;

	void PlayAnnouncerVoice(const TCHAR* SoundWavePkg, float Volume = 0.85f);
	void SpawnStageWorldVFX(const TCHAR* NiagaraPkg, const FVector& Loc, const FRotator& Rot, const FVector& Scale, bool bPersistentStageTag = true);
	void ClearStageWorldVFX();
	FVector ResolveGroundedStagePoint(const FVector& DesiredXY, float DefaultZ = 0.0f) const;

public:
	/** Trace down (and search nearby) for walkable, pawn-blocking ground and
	 *  put the character on it. If nothing is found yet (collision still
	 *  loading) the character is held in place and retried by the watchdog. */
	void PlaceCharacterOnGround(class ACharacter* Character, const FVector& Desired, const TCHAR* Reason);

protected:
	FTimerHandle GroundWatchdogHandle;
	TMap<TWeakObjectPtr<class ACharacter>, FVector> LastSafeGround;
	TMap<TWeakObjectPtr<class ACharacter>, float> FallingTime;
	TMap<TWeakObjectPtr<class ACharacter>, FVector> PendingGround;
	float GroundCheckClock = 0.0f;
	float PendingGroundClock = 0.0f;
	int32 GroundRescueCount = 0;
	bool bGroundVerdictLogged = false;
	void TickGroundWatchdog();
};

