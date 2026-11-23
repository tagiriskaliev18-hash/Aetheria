#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "AetheriaTypes.h"
#include "AetheriaBotAIController.generated.h"

class AAetheriaWizardCharacter;
class AAetheriaMobCharacter;
class AAetheriaGameState;

UCLASS()
class AETHERIA_API AAetheriaBotAIController : public AAIController
{
	GENERATED_BODY()

public:
	AAetheriaBotAIController();

	virtual void Tick(float DeltaTime) override;

protected:
	virtual void OnPossess(APawn* InPawn) override;
	virtual void OnUnPossess() override;

private:
	TWeakObjectPtr<AAetheriaWizardCharacter> PossessedWizard;

	float ActionCooldownTimer = 0.0f;
	float DashCooldownTimer = 0.0f;
	float ObjectiveSearchTimer = 0.0f;
	FVector CachedGoalLocation = FVector::ZeroVector;
	FVector LastMoveToGoal = FVector::ZeroVector;

	void TickBotBehavior(float DeltaTime);
	void EvaluateMovementAndPositioning(AAetheriaWizardCharacter* MyWizard, const TArray<AAetheriaMobCharacter*>& NearbyMobs, float DeltaTime);
	void EvaluateCombatSpells(AAetheriaWizardCharacter* MyWizard, const TArray<AAetheriaMobCharacter*>& NearbyMobs);
	void SynthesizeAndCastSpell(AAetheriaWizardCharacter* MyWizard, EAetheriaSpellId SpellId, const FVector& TargetLocation);
	AAetheriaWizardCharacter* FindNearestHumanWizard(const FVector& MyLoc) const;
	const FAetheriaObjectiveInfo* FindTargetObjective(const AAetheriaGameState* GameState) const;
};
