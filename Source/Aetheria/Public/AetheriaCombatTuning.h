// Copyright Mind Interactive. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "AetheriaCombatTypes.h"
#include "AetheriaCombatTuning.generated.h"

/** Party-size poise scaling. max = Solo + PerExtraPlayer * (N - 1). */
USTRUCT(BlueprintType)
struct FAetheriaPoiseScaling
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poise", meta = (ClampMin = "1.0"))
    float Solo = 100.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poise", meta = (ClampMin = "0.0"))
    float PerExtraPlayer = 55.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Poise", meta = (ClampMin = "1"))
    int32 MaxPlayers = 6;

    FORCEINLINE float ForParty(int32 PartySize) const
    {
        const int32 Extra = FMath::Clamp(PartySize - 1, 0, FMath::Max(0, MaxPlayers - 1));
        return Solo + PerExtraPlayer * static_cast<float>(Extra);
    }
};

/** One complete tuning profile (player, mob, boss...). */
USTRUCT(BlueprintType)
struct FAetheriaTuningProfile
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FString Id;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FAetheriaAttackTiming Light;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FAetheriaAttackTiming Heavy;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FAetheriaDefenseTiming Dodge;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FAetheriaDefenseTiming Parry;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile")
    FAetheriaPoiseScaling Poise;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0.0"))
    float PoiseRegenPerSecond = 25.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0.0"))
    float PoiseRegenDelay = 1.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0.0"))
    float StaggerDuration = 0.85f;

    /** After a stagger ends, poise cannot break again for this long. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0.0"))
    float StaggerImmunitySeconds = 1.2f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Profile", meta = (ClampMin = "0.0"))
    float RiposteWindow = 1.2f;
};

/**
 * Combat balance loaded from Data/CombatTuning.json.
 *
 * Uses the Json module only (already linked in Aetheria.Build.cs). FJsonObjectConverter
 * would need JsonUtilities, which is NOT linked, so parsing is done by hand.
 */
UCLASS(BlueprintType)
class AETHERIA_API UAetheriaCombatTuning : public UObject
{
    GENERATED_BODY()

public:
    /** Project-relative default: <ProjectDir>/Data/CombatTuning.json */
    UFUNCTION(BlueprintPure, Category = "Combat|Tuning")
    static FString GetDefaultTuningPath();

    /** Returns nullptr and fills OutError on failure. Never throws. */
    UFUNCTION(BlueprintCallable, Category = "Combat|Tuning")
    static UAetheriaCombatTuning* LoadFromFile(const FString& AbsolutePath, FString& OutError);

    UFUNCTION(BlueprintCallable, Category = "Combat|Tuning")
    static UAetheriaCombatTuning* LoadDefault(FString& OutError);

    const FAetheriaTuningProfile* FindProfile(const FString& ProfileId) const;

    UFUNCTION(BlueprintPure, Category = "Combat|Tuning")
    float GetScaledPoise(const FString& ProfileId, int32 PartySize) const;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Tuning")
    TMap<FString, FAetheriaTuningProfile> Profiles;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Tuning")
    FString DefaultProfile;

    UPROPERTY(BlueprintReadOnly, Category = "Combat|Tuning")
    int32 Version = 0;
};
