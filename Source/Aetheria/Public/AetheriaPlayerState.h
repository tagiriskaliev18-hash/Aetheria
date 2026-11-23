#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AetheriaPlayerState.generated.h"

UCLASS()
class AETHERIA_API AAetheriaPlayerState : public APlayerState
{
	GENERATED_BODY()

public:
	AAetheriaPlayerState();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Player")
	int32 PartyColorIndex = 0;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Player")
	bool bIsBotMage = false;

	UPROPERTY(Replicated, BlueprintReadOnly, Category = "Aetheria|Player")
	FString WizardDisplayName;
};
