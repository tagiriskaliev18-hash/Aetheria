// Copyright Mind Interactive. All Rights Reserved.
// AETHERIA: Shards of the Chrono-Seal — Combat Core types.
// Target engine: UE 5.8 (BuildId 55116800). GAS is NOT used in this project.

#pragma once

#include "CoreMinimal.h"
#include "AetheriaCombatTypes.generated.h"

/** Exactly one combat state is active at any moment. */
UENUM(BlueprintType)
enum class EAetheriaCombatState : uint8
{
    Idle      UMETA(DisplayName = "Idle"),
    Windup    UMETA(DisplayName = "Windup"),
    Active    UMETA(DisplayName = "Active"),
    Recovery  UMETA(DisplayName = "Recovery"),
    Dodging   UMETA(DisplayName = "Dodging"),
    Parrying  UMETA(DisplayName = "Parrying"),
    Riposte   UMETA(DisplayName = "Riposte"),
    Staggered UMETA(DisplayName = "Staggered"),
    Dead      UMETA(DisplayName = "Dead")
};

UENUM(BlueprintType)
enum class EAetheriaHitReaction : uint8
{
    None      UMETA(DisplayName = "None"),
    Flinch    UMETA(DisplayName = "Flinch"),
    Stagger   UMETA(DisplayName = "Stagger"),
    Knockdown UMETA(DisplayName = "Knockdown")
};

UENUM(BlueprintType)
enum class EAetheriaCombatHitResult : uint8
{
    Ignored   UMETA(DisplayName = "Ignored"),
    Evaded    UMETA(DisplayName = "Evaded"),
    Parried   UMETA(DisplayName = "Parried"),
    Blocked   UMETA(DisplayName = "Blocked"),
    Damaged   UMETA(DisplayName = "Damaged"),
    Staggered UMETA(DisplayName = "Staggered"),
    Killed    UMETA(DisplayName = "Killed")
};

/** A time slice in seconds from action start. Half-open interval [Start, End). */
USTRUCT(BlueprintType)
struct FAetheriaCombatWindow
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window", meta = (ClampMin = "0.0"))
    float Start = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Window", meta = (ClampMin = "0.0"))
    float End = 0.f;

    FAetheriaCombatWindow() = default;
    FAetheriaCombatWindow(float InStart, float InEnd) : Start(InStart), End(InEnd) {}

    FORCEINLINE bool IsValid() const { return End > Start; }
    FORCEINLINE float Length() const { return FMath::Max(0.f, End - Start); }
    FORCEINLINE bool Contains(float T) const { return T >= Start && T < End; }

    /**
     * Swept test against [PrevT, CurT). Frame-rate independent: at 24 FPS a
     * 0.06 s parry window is still caught, because the whole step is tested
     * rather than a single sampled point.
     */
    FORCEINLINE bool Intersects(float PrevT, float CurT) const
    {
        return CurT > Start && PrevT < End;
    }
};

/** Timing + payload for one attack animation. */
USTRUCT(BlueprintType)
struct FAetheriaAttackTiming
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    FAetheriaCombatWindow Windup = FAetheriaCombatWindow(0.00f, 0.22f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    FAetheriaCombatWindow Active = FAetheriaCombatWindow(0.22f, 0.34f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    FAetheriaCombatWindow Recovery = FAetheriaCombatWindow(0.34f, 0.62f);

    /** Window inside Recovery where the next action may be buffered. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    FAetheriaCombatWindow Cancel = FAetheriaCombatWindow(0.40f, 0.62f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Payload", meta = (ClampMin = "0.0"))
    float Damage = 12.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Payload", meta = (ClampMin = "0.0"))
    float PoiseDamage = 18.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Payload")
    EAetheriaHitReaction Reaction = EAetheriaHitReaction::Flinch;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop", meta = (ClampMin = "0.0"))
    float HitstopOnHit = 0.055f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Hitstop", meta = (ClampMin = "0.0"))
    float HitstopOnParry = 0.150f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (ClampMin = "1.0"))
    float TraceRadius = 48.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace", meta = (ClampMin = "1.0"))
    float TraceReach = 190.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trace")
    FVector TraceOffset = FVector(0.f, 0.f, 40.f);

    FORCEINLINE float Duration() const { return FMath::Max(Recovery.End, Active.End); }
};

/** Timing for dodge and parry. One struct serves both. */
USTRUCT(BlueprintType)
struct FAetheriaDefenseTiming
{
    GENERATED_BODY()

    /** Invulnerability span. Hitstop extends it automatically (clock freezes). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    FAetheriaCombatWindow Invuln = FAetheriaCombatWindow(0.02f, 0.30f);

    /** Sub-window inside Invuln where a hit is deflected instead of ignored. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    FAetheriaCombatWindow Parry = FAetheriaCombatWindow(0.05f, 0.18f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing")
    FAetheriaCombatWindow Recovery = FAetheriaCombatWindow(0.30f, 0.46f);

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
    float TotalDuration = 0.46f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Timing", meta = (ClampMin = "0.0"))
    float Cooldown = 0.10f;

    FORCEINLINE bool HasParryWindow() const { return Parry.IsValid(); }
};

/** An incoming hit, resolved by the receiver. */
USTRUCT(BlueprintType)
struct FAetheriaHitPayload
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category = "Hit")
    float Damage = 10.f;

    UPROPERTY(BlueprintReadWrite, Category = "Hit")
    float PoiseDamage = 15.f;

    UPROPERTY(BlueprintReadWrite, Category = "Hit")
    EAetheriaHitReaction Reaction = EAetheriaHitReaction::Flinch;

    UPROPERTY(BlueprintReadWrite, Category = "Hit")
    FVector HitLocation = FVector::ZeroVector;

    UPROPERTY(BlueprintReadWrite, Category = "Hit")
    FVector HitDirection = FVector::ForwardVector;

    /** Hitstop applied to the attacker on connect. */
    UPROPERTY(BlueprintReadWrite, Category = "Hit", meta = (ClampMin = "0.0"))
    float AttackerHitstop = 0.055f;
};
