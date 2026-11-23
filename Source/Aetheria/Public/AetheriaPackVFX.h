#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AetheriaTypes.h"
#include "AetheriaPackVFX.generated.h"

UCLASS()
class AETHERIA_API UAetheriaPackVFX : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "Aetheria|VFX")
	static void PlaySpell(UObject* WorldContext, EAetheriaSpellId SpellId, FVector Origin, FVector Target, const TArray<FVector>& ChainPoints);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|VFX")
	static void PlayHit(UObject* WorldContext, FVector Location, EAetheriaSpellId SpellId);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|VFX")
	static void PlayCastFlourish(UObject* WorldContext, FVector Location, EAetheriaSpellId SpellId);

	/** One telegraph mark at a spawn point (1 s ground cue). Elite uses a summoning circle. */
	UFUNCTION(BlueprintCallable, Category = "Aetheria|VFX")
	static void PlaySpawnTelegraph(UObject* WorldContext, EAetheriaMobKind Kind, FVector Location, bool bElite);

	/** One entrance VFX for a mob kind. Never stacks systems. */
	UFUNCTION(BlueprintCallable, Category = "Aetheria|VFX")
	static void PlaySpawnEntrance(UObject* WorldContext, EAetheriaMobKind Kind, FVector Location, bool bElite, bool bIsBoss);

	static void PlayTaggedLoop(UObject* WorldContext, const TCHAR* NiagaraPath, FVector Location, FRotator Rotation, FVector Scale, FName Tag);
	static void PlayNiagara(UWorld* World, const TCHAR* Path, FVector Location, FRotator Rotation, FVector Scale, bool bAutoDestroy);
	/** Play exactly one system: Niagara if the asset exists, otherwise Cascade. Never both. */
	static bool PlayOne(UWorld* World, const TCHAR* Path, FVector Location, FRotator Rotation, FVector Scale, bool bAutoDestroy = true);
	static void PlayCascade(UWorld* World, const TCHAR* Path, FVector Location, FRotator Rotation, FVector Scale);
};
