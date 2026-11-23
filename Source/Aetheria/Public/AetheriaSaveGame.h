#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "AetheriaTypes.h"
#include "AetheriaSaveGame.generated.h"

UCLASS()
class AETHERIA_API UAetheriaSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 SaveVersion = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 WizardLevel = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 CurrentXP = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 ArcaneEssence = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 UnlockedHighestLocation = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 TotalKills = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	TArray<FString> EquippedRelicNames;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	FAetheriaCharacterCustomization SavedCustomization;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 UnlockedCosmeticsMask = 0xFFFF;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 SavedGraphicsPreset = 1;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = "Aetheria|Save")
	int32 EssenceEmpowerRank = 0;

	bool UnlockCosmeticWithEssence(int32 BitIndex, int32 Cost);

	static int32 GetXPForNextLevel(int32 CurrentLevel);
	static UAetheriaSaveGame* LoadOrCreateSave(const FString& SlotName = TEXT("AetheriaProfile"));
	static bool PersistSave(UAetheriaSaveGame* SaveObj, const FString& SlotName = TEXT("AetheriaProfile"));
};
