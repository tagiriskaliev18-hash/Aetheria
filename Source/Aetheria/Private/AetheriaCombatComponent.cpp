// Copyright Mind Interactive. All Rights Reserved.

#include "AetheriaCombatComponent.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CollisionQueryParams.h"
#include "Misc/App.h"

UAetheriaCombatComponent::UAetheriaCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.TickGroup = TG_PrePhysics;
    bAutoTraceOnActive = true;
}

void UAetheriaCombatComponent::BeginPlay()
{
    Super::BeginPlay();
    CurrentPoise = MaxPoise;
    SetState(EAetheriaCombatState::Idle);
}

// ---------------------------------------------------------------------------
// Tick
// ---------------------------------------------------------------------------

void UAetheriaCombatComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    if (State == EAetheriaCombatState::Dead)
    {
        return;
    }

    // Hitstop is counted in REAL time. If the project dilates world or actor
    // time (parry slow-mo, death cam), a 55 ms freeze must still last 55 ms
    // of wall clock, not 55 ms scaled by that dilation.
    const float RealDelta = FMath::Clamp(static_cast<float>(FApp::GetDeltaTime()), 0.f, 0.25f);

    if (HitstopRemaining > 0.f)
    {
        HitstopRemaining = FMath::Max(0.f, HitstopRemaining - RealDelta);

        // Clock freeze: action time does not advance, so i-frames and parry
        // windows are extended by exactly the hitstop duration. This is the
        // Wukong behaviour — the freeze is part of the feel, not lost time.
        return;
    }

    if (CooldownRemaining > 0.f)
    {
        CooldownRemaining = FMath::Max(0.f, CooldownRemaining - DeltaTime);
    }

    if (bRiposteOpen)
    {
        RiposteRemaining -= DeltaTime;
        if (RiposteRemaining <= 0.f)
        {
            bRiposteOpen = false;
            RiposteRemaining = 0.f;
        }
    }

    // Poise recovery pauses while staggered.
    if (State != EAetheriaCombatState::Staggered && CurrentPoise < MaxPoise)
    {
        if (PoiseRegenTimer > 0.f)
        {
            PoiseRegenTimer = FMath::Max(0.f, PoiseRegenTimer - DeltaTime);
        }
        else
        {
            CurrentPoise = FMath::Min(MaxPoise, CurrentPoise + PoiseRegenPerSecond * DeltaTime);
        }
    }

    if (State == EAetheriaCombatState::Idle)
    {
        return;
    }

    const float PrevTime = ActionTime;
    ActionTime += DeltaTime;

    switch (State)
    {
    case EAetheriaCombatState::Windup:
    case EAetheriaCombatState::Active:
    case EAetheriaCombatState::Recovery:
    {
        const FAetheriaAttackTiming* Timing = GetActiveAttackTiming();
        if (!Timing)
        {
            SetState(EAetheriaCombatState::Idle);
            break;
        }

        // Single sweep per swing, tested against the swept interval so a fast
        // step can never skip the Active window.
        if (bAutoTraceOnActive && !bTraceConsumed && Timing->Active.Intersects(PrevTime, ActionTime))
        {
            bTraceConsumed = true;
            PerformAttackTrace(*Timing);
        }

        if (ActionTime >= Timing->Recovery.End)
        {
            SetState(EAetheriaCombatState::Idle);
        }
        else if (ActionTime >= Timing->Active.End)
        {
            SetState(EAetheriaCombatState::Recovery);
        }
        else if (ActionTime >= Timing->Windup.End)
        {
            SetState(EAetheriaCombatState::Active);
        }
        else
        {
            SetState(EAetheriaCombatState::Windup);
        }
        break;
    }

    case EAetheriaCombatState::Dodging:
    case EAetheriaCombatState::Parrying:
    {
        const FAetheriaDefenseTiming* Timing = GetActiveDefenseTiming();
        if (!Timing || ActionTime >= Timing->TotalDuration)
        {
            SetState(EAetheriaCombatState::Idle);
        }
        break;
    }

    case EAetheriaCombatState::Staggered:
        if (ActionTime >= StaggerDuration)
        {
            SetState(EAetheriaCombatState::Idle);
        }
        break;

    case EAetheriaCombatState::Riposte:
        if (ActionTime >= ActionDuration)
        {
            SetState(EAetheriaCombatState::Idle);
        }
        break;

    default:
        break;
    }
}

// ---------------------------------------------------------------------------
// Actions
// ---------------------------------------------------------------------------

bool UAetheriaCombatComponent::TryStartAttack(bool bHeavy)
{
    if (!CanAct())
    {
        return false;
    }

    bHeavyInFlight = bHeavy;
    bTraceConsumed = false;

    const FAetheriaAttackTiming& Timing = bHeavy ? HeavyAttack : LightAttack;
    BeginAction(EAetheriaCombatState::Windup, Timing.Duration());
    return true;
}

bool UAetheriaCombatComponent::TryStartDodge()
{
    if (!CanAct())
    {
        return false;
    }

    BeginAction(EAetheriaCombatState::Dodging, Dodge.TotalDuration);
    CooldownRemaining = Dodge.Cooldown;
    return true;
}

bool UAetheriaCombatComponent::TryStartParry()
{
    if (!CanAct())
    {
        return false;
    }

    BeginAction(EAetheriaCombatState::Parrying, Parry.TotalDuration);
    CooldownRemaining = Parry.Cooldown;
    return true;
}

EAetheriaCombatHitResult UAetheriaCombatComponent::ReceiveHit(const FAetheriaHitPayload& Payload, AActor* Attacker)
{
    if (State == EAetheriaCombatState::Dead)
    {
        return EAetheriaCombatHitResult::Ignored;
    }

    // --- 1. Invulnerability, with parry sub-window ---
    if (IsInvulnerable())
    {
        if (IsInParryWindow())
        {
            bRiposteOpen = true;
            RiposteRemaining = RiposteWindow;

            // Parry hitstop is the longest in the game: it is the reward.
            ApplyHitstop(0.150f);

            OnParrySuccess.Broadcast(Attacker);
            OnHitResolved.Broadcast(Attacker, EAetheriaCombatHitResult::Parried, 0.f);
            return EAetheriaCombatHitResult::Parried;
        }

        OnHitResolved.Broadcast(Attacker, EAetheriaCombatHitResult::Evaded, 0.f);
        return EAetheriaCombatHitResult::Evaded;
    }

    // --- 2. Poise ---
    CurrentPoise = FMath::Max(0.f, CurrentPoise - Payload.PoiseDamage);
    PoiseRegenTimer = PoiseRegenDelay;

    const bool bPoiseBroken = (CurrentPoise <= 0.f);
    const bool bHeavyReaction = (Payload.Reaction == EAetheriaHitReaction::Stagger)
                             || (Payload.Reaction == EAetheriaHitReaction::Knockdown);

    if (bPoiseBroken || bHeavyReaction)
    {
        if (bPoiseBroken)
        {
            CurrentPoise = MaxPoise * 0.5f;
        }

        bRiposteOpen = false;
        RiposteRemaining = 0.f;

        BeginAction(EAetheriaCombatState::Staggered, StaggerDuration);
        ApplyHitstop(HitstopOnStagger);

        OnStaggered.Broadcast();
        OnHitResolved.Broadcast(Attacker, EAetheriaCombatHitResult::Staggered, Payload.Damage);
        return EAetheriaCombatHitResult::Staggered;
    }

    // --- 3. Flinch ---
    ApplyHitstop(Payload.AttackerHitstop);
    OnHitResolved.Broadcast(Attacker, EAetheriaCombatHitResult::Damaged, Payload.Damage);
    return EAetheriaCombatHitResult::Damaged;
}

void UAetheriaCombatComponent::ApplyHitstop(float Duration)
{
    if (Duration <= 0.f || State == EAetheriaCombatState::Dead)
    {
        return;
    }

    // Overlapping hitstops take the longest, never the sum.
    HitstopRemaining = FMath::Max(HitstopRemaining, Duration);
    OnHitstopApplied.Broadcast(Duration);
}

void UAetheriaCombatComponent::ResetCombat()
{
    ActionTime = 0.f;
    ActionDuration = 0.f;
    HitstopRemaining = 0.f;
    CooldownRemaining = 0.f;
    PoiseRegenTimer = 0.f;
    RiposteRemaining = 0.f;
    CurrentPoise = MaxPoise;
    bRiposteOpen = false;
    bTraceConsumed = false;
    bHeavyInFlight = false;
    SetState(EAetheriaCombatState::Idle);
}

void UAetheriaCombatComponent::MarkDead()
{
    ActionTime = 0.f;
    HitstopRemaining = 0.f;
    bRiposteOpen = false;
    SetState(EAetheriaCombatState::Dead);
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

bool UAetheriaCombatComponent::IsInvulnerable() const
{
    switch (State)
    {
    case EAetheriaCombatState::Dodging:
        return Dodge.Invuln.Contains(ActionTime);
    case EAetheriaCombatState::Parrying:
        return Parry.Invuln.Contains(ActionTime);
    default:
        return false;
    }
}

bool UAetheriaCombatComponent::IsInParryWindow() const
{
    return State == EAetheriaCombatState::Parrying && Parry.Parry.Contains(ActionTime);
}

bool UAetheriaCombatComponent::CanAct() const
{
    if (State == EAetheriaCombatState::Dead)
    {
        return false;
    }
    if (HitstopRemaining > 0.f || CooldownRemaining > 0.f)
    {
        return false;
    }
    if (State == EAetheriaCombatState::Idle)
    {
        return true;
    }

    // Input buffering: the tail of an attack may be cancelled into the next action.
    if (State == EAetheriaCombatState::Recovery)
    {
        const FAetheriaAttackTiming* Timing = GetActiveAttackTiming();
        return Timing && Timing->Cancel.Contains(ActionTime);
    }

    return false;
}

// ---------------------------------------------------------------------------
// Internals
// ---------------------------------------------------------------------------

void UAetheriaCombatComponent::SetState(EAetheriaCombatState NewState)
{
    if (State == NewState)
    {
        return;
    }

    const EAetheriaCombatState OldState = State;
    State = NewState;
    OnCombatStateChanged.Broadcast(OldState, NewState);
}

void UAetheriaCombatComponent::BeginAction(EAetheriaCombatState NewState, float Duration)
{
    ActionTime = 0.f;
    ActionDuration = FMath::Max(Duration, 0.f);
    SetState(NewState);
}

const FAetheriaAttackTiming* UAetheriaCombatComponent::GetActiveAttackTiming() const
{
    switch (State)
    {
    case EAetheriaCombatState::Windup:
    case EAetheriaCombatState::Active:
    case EAetheriaCombatState::Recovery:
        return bHeavyInFlight ? &HeavyAttack : &LightAttack;
    default:
        return nullptr;
    }
}

const FAetheriaDefenseTiming* UAetheriaCombatComponent::GetActiveDefenseTiming() const
{
    switch (State)
    {
    case EAetheriaCombatState::Dodging:
        return &Dodge;
    case EAetheriaCombatState::Parrying:
        return &Parry;
    default:
        return nullptr;
    }
}

void UAetheriaCombatComponent::PerformAttackTrace(const FAetheriaAttackTiming& Timing)
{
    AActor* Owner = GetOwner();
    UWorld* World = GetWorld();
    if (!Owner || !World)
    {
        return;
    }

    const FVector Origin = Owner->GetActorLocation()
                         + Owner->GetActorRotation().RotateVector(Timing.TraceOffset);
    const FVector Direction = Owner->GetActorForwardVector();
    const FVector End = Origin + Direction * Timing.TraceReach;

    FCollisionQueryParams Params(SCENE_QUERY_STAT(AetheriaMeleeTrace), false, Owner);
    Params.AddIgnoredActor(Owner);

    TArray<FHitResult> Hits;
    World->SweepMultiByChannel(
        Hits,
        Origin,
        End,
        FQuat::Identity,
        ECC_Visibility,
        FCollisionShape::MakeSphere(Timing.TraceRadius),
        Params);

    TSet<AActor*> AlreadyHit;
    bool bConnected = false;

    for (const FHitResult& Hit : Hits)
    {
        AActor* HitActor = Hit.GetActor();
        if (!HitActor || AlreadyHit.Contains(HitActor))
        {
            continue;
        }

        UAetheriaCombatComponent* Target = HitActor->FindComponentByClass<UAetheriaCombatComponent>();
        if (!Target || Target == this)
        {
            continue;
        }

        AlreadyHit.Add(HitActor);

        FAetheriaHitPayload Payload;
        Payload.Damage = Timing.Damage;
        Payload.PoiseDamage = Timing.PoiseDamage;
        Payload.Reaction = Timing.Reaction;
        Payload.HitLocation = Hit.ImpactPoint;
        Payload.HitDirection = Direction;
        Payload.AttackerHitstop = Timing.HitstopOnHit;

        const EAetheriaCombatHitResult Result = Target->ReceiveHit(Payload, Owner);

        OnHitResolved.Broadcast(HitActor, Result, Payload.Damage);

        if (Result != EAetheriaCombatHitResult::Ignored && Result != EAetheriaCombatHitResult::Evaded)
        {
            bConnected = true;
        }
    }

    // Attacker freezes only on contact — whiffing must stay responsive.
    if (bConnected)
    {
        ApplyHitstop(Timing.HitstopOnHit);
    }
}
