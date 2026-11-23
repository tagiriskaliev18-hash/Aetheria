#include "AetheriaPlayerState.h"
#include "Net/UnrealNetwork.h"

AAetheriaPlayerState::AAetheriaPlayerState()
{
	bReplicates = true;
	NetUpdateFrequency = 20.0f;
}

void AAetheriaPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AAetheriaPlayerState, PartyColorIndex);
	DOREPLIFETIME(AAetheriaPlayerState, bIsBotMage);
	DOREPLIFETIME(AAetheriaPlayerState, WizardDisplayName);
}
