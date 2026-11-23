// Copyright Mind Interactive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "AetheriaTypes.h"
#include "AetheriaCombatTypes.h"
#include "AetheriaCombatComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAetheriaOnCombatStateChanged, EAetheriaCombatState, OldState, EAetheriaCombatState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FAetheriaOnHitResolved, AActor*, Target, EAetheriaCombatHitResult, Result, float, Damage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAetheriaOnParrySuccess, AActor*, ParriedAttacker);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FAetheriaOnStaggered);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAetheriaOnHitstopApplied, float, Duration);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAetheriaOnOrbsChanged, const TArray<EAetheriaOrb>&, CurrentOrbs);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FAetheriaOnSpellInvoked, EAetheriaSpellId, SpellId, const FVector&, TargetLocation);

/**
 * Standalone combat resolver: states, i-frames, parry windows, hitstop, poise.
 *
 * Deliberately NOT built on Gameplay Ability System — Aetheria's Build.cs does
 * not link GameplayAbilities/GameplayTasks/GameplayTags, and this component
 * adds zero build dependencies. Attach it to any actor that fights.
 *
 * Damage application to health is the owner's responsibility. This component
 * resolves *combat state* and reports the verdict via delegates.
 */
UCLASS(ClassGroup = (Aetheria), meta = (BlueprintSpawnableComponent))
class AETHERIA_API UAetheriaCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UAetheriaCombatComponent();

    virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

    // ---- Tuning (exposed so balance is editable without recompiling) ----

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
    FAetheriaAttackTiming LightAttack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Attack")
    FAetheriaAttackTiming HeavyAttack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
    FAetheriaDefenseTiming Dodge;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Defense")
    FAetheriaDefenseTiming Parry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Poise", meta = (ClampMin = "1.0"))
    float MaxPoise = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
    float PoiseRegenPerSecond = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Poise", meta = (ClampMin = "0.0"))
    float PoiseRegenDelay = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stagger", meta = (ClampMin = "0.0"))
    float StaggerDuration = 0.85f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Stagger", meta = (ClampMin = "0.0"))
    float HitstopOnStagger = 0.110f;

    /** Opens a riposte opportunity for this long after a successful parry. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Riposte", meta = (ClampMin = "0.0"))
    float RiposteWindow = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat|Trace")
    bool bAutoTraceOnActive = true;

    // ---- Actions ----

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool TryStartAttack(bool bHeavy);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool TryStartDodge();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    bool TryStartParry();

    /** Resolve an incoming hit. Returns the verdict. */
    UFUNCTION(BlueprintCallable, Category = "Combat")
    EAetheriaCombatHitResult ReceiveHit(const FAetheriaHitPayload& Payload, AActor* Attacker);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ApplyHitstop(float Duration);

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void ResetCombat();

    UFUNCTION(BlueprintCallable, Category = "Combat")
    void MarkDead();

    // ---- Queries ----

    UFUNCTION(BlueprintPure, Category = "Combat")
    EAetheriaCombatState GetCombatState() const { return State; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    float GetCurrentPoise() const { return CurrentPoise; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsInvulnerable() const;

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool IsInParryWindow() const;

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool CanAct() const;

    UFUNCTION(BlueprintPure, Category = "Combat")
    bool HasRiposteOpportunity() const { return bRiposteOpen; }

    UFUNCTION(BlueprintPure, Category = "Combat")
    float GetHitstopRemaining() const { return HitstopRemaining; }

    // ---- Events ----

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FAetheriaOnCombatStateChanged OnCombatStateChanged;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FAetheriaOnHitResolved OnHitResolved;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FAetheriaOnParrySuccess OnParrySuccess;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FAetheriaOnStaggered OnStaggered;

    UPROPERTY(BlueprintAssignable, Category = "Combat|Events")
    FAetheriaOnHitstopApplied OnHitstopApplied;

protected:
    virtual void BeginPlay() override;

private:
    void SetState(EAetheriaCombatState NewState);
    void BeginAction(EAetheriaCombatState NewState, float Duration);
    void PerformAttackTrace(const FAetheriaAttackTiming& Timing);

    const FAetheriaAttackTiming* GetActiveAttackTiming() const;
    const FAetheriaDefenseTiming* GetActiveDefenseTiming() const;

    UPROPERTY(VisibleInstanceOnly, Category = "Combat|Runtime")
    EAetheriaCombatState State = EAetheriaCombatState::Idle;

    float ActionTime = 0.f;
    float ActionDuration = 0.f;
    float HitstopRemaining = 0.f;
    float CooldownRemaining = 0.f;
    float CurrentPoise = 100.f;
    float PoiseRegenTimer = 0.f;
    float RiposteRemaining = 0.f;

    bool bRiposteOpen = false;
    bool bTraceConsumed = false;
    bool bHeavyInFlight = false;
};
