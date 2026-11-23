#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AetheriaTypes.h"
#include "AetheriaSpellDatabase.generated.h"

UCLASS()
class AETHERIA_API UAetheriaSpellDatabase : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Aetheria|Spells")
	static FString NormalizeComboKey(const TArray<EAetheriaOrb>& Orbs);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Spells")
	static FAetheriaSpellDef ResolveSpellFromOrbs(const TArray<EAetheriaOrb>& Orbs);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Spells")
	static FAetheriaSpellDef GetSpellDef(EAetheriaSpellId SpellId);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Spells")
	static const TArray<FAetheriaSpellDef>& GetAllSpells();
};
