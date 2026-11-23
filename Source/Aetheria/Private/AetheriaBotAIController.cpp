#include "AetheriaBotAIController.h"
#include "AetheriaWizardCharacter.h"
#include "AetheriaMobCharacter.h"
#include "AetheriaGameState.h"
#include "AetheriaPlayerState.h"
#include "AetheriaSpellDatabase.h"
#include "Engine/World.h"
#include "EngineUtils.h"

AAetheriaBotAIController::AAetheriaBotAIController()
{
	PrimaryActorTick.bCanEverTick = true;
	bWantsPlayerState = true;
}

void AAetheriaBotAIController::OnPossess(APawn* InPawn)
{
	Super::OnPossess(InPawn);
	PossessedWizard = Cast<AAetheriaWizardCharacter>(InPawn);
}

void AAetheriaBotAIController::OnUnPossess()
{
	Super::OnUnPossess();
	PossessedWizard = nullptr;
}

void AAetheriaBotAIController::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (HasAuthority())
	{
		TickBotBehavior(DeltaTime);
	}
}

void AAetheriaBotAIController::TickBotBehavior(float DeltaTime)
{
	if (!PossessedWizard.IsValid())
	{
		return;
	}

	AAetheriaWizardCharacter* MyWizard = PossessedWizard.Get();
	if (!MyWizard->bIsAlive || MyWizard->HP <= 0.0f)
	{
		return;
	}

	ActionCooldownTimer -= DeltaTime;
	DashCooldownTimer -= DeltaTime;
	ObjectiveSearchTimer -= DeltaTime;

	const FVector MyLoc = MyWizard->GetActorLocation();

	TArray<AAetheriaMobCharacter*> NearbyMobs;
	for (TActorIterator<AAetheriaMobCharacter> It(GetWorld()); It; ++It)
	{
		AAetheriaMobCharacter* Mob = *It;
		if (Mob && Mob->HP > 0.0f)
		{
			float Dist = FVector::Dist(MyLoc, Mob->GetActorLocation());
			if (Dist <= 2500.0f)
			{
				NearbyMobs.Add(Mob);

				// Dodge check
				if (DashCooldownTimer <= 0.0f)
				{
					if (Dist <= 220.0f || (Mob->MobKind == EAetheriaMobKind::Boss && Dist <= 500.0f))
					{
						FVector EscapeDir = MyLoc - Mob->GetActorLocation();
						EscapeDir.Z = 0.0f;
						EscapeDir = EscapeDir.GetSafeNormal();
						if (EscapeDir.IsNearlyZero())
						{
							EscapeDir = -MyWizard->GetActorForwardVector();
						}
						MyWizard->ServerDash(MyLoc + EscapeDir * 500.0f);
						DashCooldownTimer = 4.0f;
					}
				}
			}
		}
	}

	EvaluateMovementAndPositioning(MyWizard, NearbyMobs, DeltaTime);

	if (ActionCooldownTimer <= 0.0f)
	{
		EvaluateCombatSpells(MyWizard, NearbyMobs);
	}
}

void AAetheriaBotAIController::EvaluateMovementAndPositioning(AAetheriaWizardCharacter* MyWizard, const TArray<AAetheriaMobCharacter*>& NearbyMobs, float DeltaTime)
{
	const FVector MyLoc = MyWizard->GetActorLocation();
	FVector DesiredLoc = MyLoc;

	AAetheriaMobCharacter* NearestThreat = nullptr;
	float ClosestDist = MAX_FLT;
	for (AAetheriaMobCharacter* Mob : NearbyMobs)
	{
		if (Mob && Mob->HP > 0.0f)
		{
			float D = FVector::Dist(MyLoc, Mob->GetActorLocation());
			if (D < ClosestDist)
			{
				ClosestDist = D;
				NearestThreat = Mob;
			}
		}
	}

	if (NearestThreat && ClosestDist < 450.0f)
	{
		FVector AwayDir = MyLoc - NearestThreat->GetActorLocation();
		AwayDir.Z = 0.0f;
		AwayDir = AwayDir.GetSafeNormal();
		DesiredLoc = MyLoc + AwayDir * 400.0f;
	}
	else
	{
		const AAetheriaGameState* GS = GetWorld()->GetGameState<AAetheriaGameState>();
		const FAetheriaObjectiveInfo* TargetObj = FindTargetObjective(GS);

		if (TargetObj)
		{
			DesiredLoc = TargetObj->Location + FVector(FMath::Sin(GetWorld()->GetTimeSeconds()) * 120.0f, FMath::Cos(GetWorld()->GetTimeSeconds()) * 120.0f, 0.0f);
		}
		else
		{
			AAetheriaWizardCharacter* Human = FindNearestHumanWizard(MyLoc);
			if (Human)
			{
				int32 OffsetSign = (MyWizard->PartyColorIndex % 2 == 0) ? 1 : -1;
				DesiredLoc = Human->GetActorLocation() + Human->GetActorRightVector() * (220.0f * OffsetSign) - Human->GetActorForwardVector() * 180.0f;
			}
			else if (NearestThreat)
			{
				FVector ToThreat = (NearestThreat->GetActorLocation() - MyLoc).GetSafeNormal();
				DesiredLoc = NearestThreat->GetActorLocation() - ToThreat * 700.0f;
			}
		}
	}

	CachedGoalLocation = DesiredLoc;
	// Repath only when the goal actually moved: calling MoveToLocation every
	// tick resets path following and fights the AddMovementInput below.
	if (FVector::DistSquared(LastMoveToGoal, DesiredLoc) > 200.0f * 200.0f)
	{
		LastMoveToGoal = DesiredLoc;
		MoveToLocation(DesiredLoc, 120.0f);
	}

	FVector MoveVec = (CachedGoalLocation - MyLoc);
	MoveVec.Z = 0.0f;
	if (MoveVec.Size() > 100.0f)
	{
		MyWizard->AddMovementInput(MoveVec.GetSafeNormal(), 1.0f);
	}

	if (NearestThreat)
	{
		FVector AimDir = NearestThreat->GetActorLocation() - MyLoc;
		AimDir.Z = 0.0f;
		AimDir = AimDir.GetSafeNormal();
		if (!AimDir.IsNearlyZero())
		{
			FRotator AimRot = AimDir.Rotation();
			MyWizard->SetActorRotation(FMath::RInterpTo(MyWizard->GetActorRotation(), AimRot, DeltaTime, 10.0f));
		}
	}
}

void AAetheriaBotAIController::EvaluateCombatSpells(AAetheriaWizardCharacter* MyWizard, const TArray<AAetheriaMobCharacter*>& NearbyMobs)
{
	if (NearbyMobs.Num() == 0)
	{
		return;
	}

	const FVector MyLoc = MyWizard->GetActorLocation();

	int32 CloseMobCount = 0;
	int32 MediumMobCount = 0;
	FVector ClusterCenter = FVector::ZeroVector;
	AAetheriaMobCharacter* PriorityTarget = nullptr;
	float HighestHP = -1.0f;

	for (AAetheriaMobCharacter* Mob : NearbyMobs)
	{
		if (!Mob || Mob->HP <= 0.0f)
		{
			continue;
		}

		float D = FVector::Dist(MyLoc, Mob->GetActorLocation());
		if (D <= 750.0f)
		{
			CloseMobCount++;
		}
		if (D <= 1600.0f)
		{
			MediumMobCount++;
			ClusterCenter += Mob->GetActorLocation();
		}

		if (Mob->HP > HighestHP)
		{
			HighestHP = Mob->HP;
			PriorityTarget = Mob;
		}
	}

	if (MediumMobCount > 0)
	{
		ClusterCenter /= (float)MediumMobCount;
	}
	else if (PriorityTarget)
	{
		ClusterCenter = PriorityTarget->GetActorLocation();
	}

	if (CloseMobCount >= 3 && MyWizard->IsSpellOffCooldown(EAetheriaSpellId::Chronosphere))
	{
		SynthesizeAndCastSpell(MyWizard, EAetheriaSpellId::Chronosphere, MyLoc);
		ActionCooldownTimer = 1.0f;
		return;
	}

	if (MediumMobCount >= 3 && MyWizard->IsSpellOffCooldown(EAetheriaSpellId::BlackHole))
	{
		SynthesizeAndCastSpell(MyWizard, EAetheriaSpellId::BlackHole, ClusterCenter);
		ActionCooldownTimer = 1.0f;
		return;
	}

	if (PriorityTarget && (PriorityTarget->MobKind == EAetheriaMobKind::Boss || PriorityTarget->HP > 500.0f) &&
		MyWizard->IsSpellOffCooldown(EAetheriaSpellId::PrimalOverload))
	{
		SynthesizeAndCastSpell(MyWizard, EAetheriaSpellId::PrimalOverload, PriorityTarget->GetActorLocation());
		ActionCooldownTimer = 0.9f;
		return;
	}

	if (PriorityTarget && PriorityTarget->HP > 200.0f && MyWizard->IsSpellOffCooldown(EAetheriaSpellId::SupernovaBeam))
	{
		SynthesizeAndCastSpell(MyWizard, EAetheriaSpellId::SupernovaBeam, PriorityTarget->GetActorLocation());
		ActionCooldownTimer = 0.8f;
		return;
	}

	if (MediumMobCount >= 2 && MyWizard->IsSpellOffCooldown(EAetheriaSpellId::ChainLightning))
	{
		SynthesizeAndCastSpell(MyWizard, EAetheriaSpellId::ChainLightning, ClusterCenter);
		ActionCooldownTimer = 0.7f;
		return;
	}

	if (PriorityTarget && MyWizard->IsSpellOffCooldown(EAetheriaSpellId::Fireball))
	{
		SynthesizeAndCastSpell(MyWizard, EAetheriaSpellId::Fireball, PriorityTarget->GetActorLocation());
		ActionCooldownTimer = 0.6f;
		return;
	}

	if (PriorityTarget)
	{
		MyWizard->ServerBasicAttack(PriorityTarget->GetActorLocation());
		ActionCooldownTimer = 0.4f;
	}
}

void AAetheriaBotAIController::SynthesizeAndCastSpell(AAetheriaWizardCharacter* MyWizard, EAetheriaSpellId SpellId, const FVector& TargetLocation)
{
	if (!MyWizard)
	{
		return;
	}

	FAetheriaSpellDef Def = UAetheriaSpellDatabase::GetSpellDef(SpellId);

	for (EAetheriaOrb Orb : Def.RequiredOrbs)
	{
		MyWizard->ServerPushOrb(Orb);
	}

	MyWizard->ServerInvoke(true, TargetLocation);
}

AAetheriaWizardCharacter* AAetheriaBotAIController::FindNearestHumanWizard(const FVector& MyLoc) const
{
	AAetheriaWizardCharacter* BestHuman = nullptr;
	float MinDistSq = MAX_FLT;

	for (TActorIterator<AAetheriaWizardCharacter> It(GetWorld()); It; ++It)
	{
		AAetheriaWizardCharacter* Wiz = *It;
		if (Wiz && Wiz->bIsAlive && Wiz->HP > 0.0f)
		{
			AAetheriaPlayerState* PS = Wiz->GetPlayerState<AAetheriaPlayerState>();
			if (PS && !PS->bIsBotMage)
			{
				float Dsq = FVector::DistSquared(MyLoc, Wiz->GetActorLocation());
				if (Dsq < MinDistSq)
				{
					MinDistSq = Dsq;
					BestHuman = Wiz;
				}
			}
		}
	}
	return BestHuman;
}

const FAetheriaObjectiveInfo* AAetheriaBotAIController::FindTargetObjective(const AAetheriaGameState* GameState) const
{
	if (!GameState)
	{
		return nullptr;
	}

	return GameState->GetActiveObjective();
}
