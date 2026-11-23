// Copyright Mind Interactive. All Rights Reserved.

#include "AetheriaCombatTuning.h"

#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
    bool ReadWindow(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, FAetheriaCombatWindow& Out)
    {
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
        if (!Obj.IsValid() || !Obj->TryGetArrayField(Field, Arr) || !Arr || Arr->Num() < 2)
        {
            return false;
        }
        Out.Start = static_cast<float>((*Arr)[0]->AsNumber());
        Out.End = static_cast<float>((*Arr)[1]->AsNumber());
        return true;
    }

    void ReadAttack(const TSharedPtr<FJsonObject>& Obj, FAetheriaAttackTiming& Out)
    {
        if (!Obj.IsValid())
        {
            return;
        }

        ReadWindow(Obj, TEXT("windup"), Out.Windup);
        ReadWindow(Obj, TEXT("active"), Out.Active);
        ReadWindow(Obj, TEXT("recovery"), Out.Recovery);
        ReadWindow(Obj, TEXT("cancel"), Out.Cancel);

        double Num = 0.0;
        if (Obj->TryGetNumberField(TEXT("damage"), Num))          { Out.Damage = static_cast<float>(Num); }
        if (Obj->TryGetNumberField(TEXT("poiseDamage"), Num))     { Out.PoiseDamage = static_cast<float>(Num); }
        if (Obj->TryGetNumberField(TEXT("hitstopOnHit"), Num))    { Out.HitstopOnHit = static_cast<float>(Num); }
        if (Obj->TryGetNumberField(TEXT("hitstopOnParry"), Num))  { Out.HitstopOnParry = static_cast<float>(Num); }
        if (Obj->TryGetNumberField(TEXT("traceRadius"), Num))     { Out.TraceRadius = static_cast<float>(Num); }
        if (Obj->TryGetNumberField(TEXT("traceReach"), Num))      { Out.TraceReach = static_cast<float>(Num); }

        FString Reaction;
        if (Obj->TryGetStringField(TEXT("reaction"), Reaction))
        {
            if (Reaction.Equals(TEXT("Stagger"), ESearchCase::IgnoreCase))        { Out.Reaction = EAetheriaHitReaction::Stagger; }
            else if (Reaction.Equals(TEXT("Knockdown"), ESearchCase::IgnoreCase)) { Out.Reaction = EAetheriaHitReaction::Knockdown; }
            else if (Reaction.Equals(TEXT("None"), ESearchCase::IgnoreCase))      { Out.Reaction = EAetheriaHitReaction::None; }
            else                                                                  { Out.Reaction = EAetheriaHitReaction::Flinch; }
        }
    }

    void ReadDefense(const TSharedPtr<FJsonObject>& Obj, FAetheriaDefenseTiming& Out)
    {
        if (!Obj.IsValid())
        {
            return;
        }

        ReadWindow(Obj, TEXT("invuln"), Out.Invuln);
        ReadWindow(Obj, TEXT("parry"), Out.Parry);
        ReadWindow(Obj, TEXT("recovery"), Out.Recovery);

        double Num = 0.0;
        if (Obj->TryGetNumberField(TEXT("totalDuration"), Num)) { Out.TotalDuration = static_cast<float>(Num); }
        if (Obj->TryGetNumberField(TEXT("cooldown"), Num))      { Out.Cooldown = static_cast<float>(Num); }
    }

    const TSharedPtr<FJsonObject>* GetObject(const TSharedPtr<FJsonObject>& Parent, const TCHAR* Field)
    {
        const TSharedPtr<FJsonObject>* Found = nullptr;
        if (Parent.IsValid() && Parent->TryGetObjectField(Field, Found) && Found)
        {
            return Found;
        }
        return nullptr;
    }
}

FString UAetheriaCombatTuning::GetDefaultTuningPath()
{
    return FPaths::Combine(FPaths::ProjectDir(), TEXT("Data"), TEXT("CombatTuning.json"));
}

UAetheriaCombatTuning* UAetheriaCombatTuning::LoadDefault(FString& OutError)
{
    return LoadFromFile(GetDefaultTuningPath(), OutError);
}

UAetheriaCombatTuning* UAetheriaCombatTuning::LoadFromFile(const FString& AbsolutePath, FString& OutError)
{
    OutError.Reset();

    FString Raw;
    if (!FFileHelper::LoadFileToString(Raw, *AbsolutePath))
    {
        OutError = FString::Printf(TEXT("CombatTuning: cannot read '%s'"), *AbsolutePath);
        return nullptr;
    }

    TSharedPtr<FJsonObject> Root;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Raw);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        OutError = FString::Printf(TEXT("CombatTuning: malformed JSON in '%s'"), *AbsolutePath);
        return nullptr;
    }

    const TSharedPtr<FJsonObject>* ProfilesObj = nullptr;
    if (!Root->TryGetObjectField(TEXT("profiles"), ProfilesObj) || !ProfilesObj)
    {
        OutError = TEXT("CombatTuning: missing 'profiles' object");
        return nullptr;
    }

    UAetheriaCombatTuning* Result = NewObject<UAetheriaCombatTuning>();
    Result->Version = Root->GetIntegerField(TEXT("version"));
    Root->TryGetStringField(TEXT("defaultProfile"), Result->DefaultProfile);

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Entry : (*ProfilesObj)->Values)
    {
        const TSharedPtr<FJsonObject>* ProfileObj = nullptr;
        if (!Entry.Value.IsValid() || !Entry.Value->TryGetObject(ProfileObj) || !ProfileObj)
        {
            continue;
        }

        FAetheriaTuningProfile Profile;
        Profile.Id = Entry.Key;

        if (const TSharedPtr<FJsonObject>* AttackObj = GetObject(*ProfileObj, TEXT("light")))
        {
            ReadAttack(*AttackObj, Profile.Light);
        }
        if (const TSharedPtr<FJsonObject>* AttackObj = GetObject(*ProfileObj, TEXT("heavy")))
        {
            ReadAttack(*AttackObj, Profile.Heavy);
        }
        if (const TSharedPtr<FJsonObject>* DefenseObj = GetObject(*ProfileObj, TEXT("dodge")))
        {
            ReadDefense(*DefenseObj, Profile.Dodge);
        }
        if (const TSharedPtr<FJsonObject>* DefenseObj = GetObject(*ProfileObj, TEXT("parry")))
        {
            ReadDefense(*DefenseObj, Profile.Parry);
        }

        if (const TSharedPtr<FJsonObject>* PoiseObj = GetObject(*ProfileObj, TEXT("poise")))
        {
            double Num = 0.0;
            if ((*PoiseObj)->TryGetNumberField(TEXT("solo"), Num))           { Profile.Poise.Solo = static_cast<float>(Num); }
            if ((*PoiseObj)->TryGetNumberField(TEXT("perExtraPlayer"), Num)) { Profile.Poise.PerExtraPlayer = static_cast<float>(Num); }
            if ((*PoiseObj)->TryGetNumberField(TEXT("maxPlayers"), Num))     { Profile.Poise.MaxPlayers = static_cast<int32>(Num); }
        }

        double Num = 0.0;
        if ((*ProfileObj)->TryGetNumberField(TEXT("poiseRegenPerSecond"), Num))   { Profile.PoiseRegenPerSecond = static_cast<float>(Num); }
        if ((*ProfileObj)->TryGetNumberField(TEXT("poiseRegenDelay"), Num))       { Profile.PoiseRegenDelay = static_cast<float>(Num); }
        if ((*ProfileObj)->TryGetNumberField(TEXT("staggerDuration"), Num))       { Profile.StaggerDuration = static_cast<float>(Num); }
        if ((*ProfileObj)->TryGetNumberField(TEXT("staggerImmunitySeconds"), Num)){ Profile.StaggerImmunitySeconds = static_cast<float>(Num); }
        if ((*ProfileObj)->TryGetNumberField(TEXT("riposteWindow"), Num))         { Profile.RiposteWindow = static_cast<float>(Num); }

        Result->Profiles.Add(Profile.Id, Profile);
    }

    if (Result->Profiles.Num() == 0)
    {
        OutError = TEXT("CombatTuning: 'profiles' is empty");
        return nullptr;
    }

    if (Result->DefaultProfile.IsEmpty() || !Result->Profiles.Contains(Result->DefaultProfile))
    {
        Result->DefaultProfile = Result->Profiles.CreateConstIterator()->Key;
    }

    return Result;
}

const FAetheriaTuningProfile* UAetheriaCombatTuning::FindProfile(const FString& ProfileId) const
{
    return Profiles.Find(ProfileId);
}

float UAetheriaCombatTuning::GetScaledPoise(const FString& ProfileId, int32 PartySize) const
{
    const FAetheriaTuningProfile* Profile = FindProfile(ProfileId);
    return Profile ? Profile->Poise.ForParty(PartySize) : 0.f;
}
