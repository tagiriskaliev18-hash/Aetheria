#include "AetheriaGameState.h"
#include "Net/UnrealNetwork.h"

AAetheriaGameState::AAetheriaGameState()
{
	bReplicates = true;
	NetUpdateFrequency = 20.0f;
}

void AAetheriaGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAetheriaGameState, CurrentLocationIndex);
	DOREPLIFETIME(AAetheriaGameState, CurrentAct);
	DOREPLIFETIME(AAetheriaGameState, ActTitle);
	DOREPLIFETIME(AAetheriaGameState, ActDescription);
	DOREPLIFETIME(AAetheriaGameState, ActProgress);
	DOREPLIFETIME(AAetheriaGameState, LocationElapsedSeconds);
	DOREPLIFETIME(AAetheriaGameState, EstimatedLocationMinutes);
	DOREPLIFETIME(AAetheriaGameState, PacingMode);
	DOREPLIFETIME(AAetheriaGameState, bBossSpawned);
	DOREPLIFETIME(AAetheriaGameState, bBossDefeated);
	DOREPLIFETIME(AAetheriaGameState, bPortalOpen);
	DOREPLIFETIME(AAetheriaGameState, bCampaignComplete);
	DOREPLIFETIME(AAetheriaGameState, BossName);
	DOREPLIFETIME(AAetheriaGameState, BossHP);
	DOREPLIFETIME(AAetheriaGameState, BossMaxHP);
	DOREPLIFETIME(AAetheriaGameState, BossOrbs);
	DOREPLIFETIME(AAetheriaGameState, ActiveSynergyBanner);
	DOREPLIFETIME(AAetheriaGameState, Objectives);
	DOREPLIFETIME(AAetheriaGameState, ActivePlayerCount);
	DOREPLIFETIME(AAetheriaGameState, ActiveBotCount);
	DOREPLIFETIME(AAetheriaGameState, ActiveTriadPhaseOrb);
	DOREPLIFETIME(AAetheriaGameState, CurrentWavePhase);
	DOREPLIFETIME(AAetheriaGameState, CurrentWaveNumber);
	DOREPLIFETIME(AAetheriaGameState, ActiveAmbushAlert);
	DOREPLIFETIME(AAetheriaGameState, bFriendlyFireEnabled);
	DOREPLIFETIME(AAetheriaGameState, FriendlyFireScale);
}

const FAetheriaObjectiveInfo* AAetheriaGameState::GetActiveObjective() const
{
	for (const FAetheriaObjectiveInfo& Obj : Objectives)
	{
		if (Obj.bActive && !Obj.bCompleted)
		{
			return &Obj;
		}
	}
	return nullptr;
}

FAetheriaObjectiveInfo* AAetheriaGameState::FindObjective(int32 ObjectiveId)
{
	for (FAetheriaObjectiveInfo& Obj : Objectives)
	{
		if (Obj.ObjectiveId == ObjectiveId)
		{
			return &Obj;
		}
	}
	return nullptr;
}
