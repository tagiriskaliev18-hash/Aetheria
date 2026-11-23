#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "AetheriaCookRefs.generated.h"

/**
 * Hard-reference bundle for content the game loads dynamically at runtime
 * (Paragon hero/mob meshes+anims, VFX packs, spell materials). The cooker
 * walks these hard references, so everything the bind/spell code
 * LoadObject()s at runtime exists in the packaged build.
 */
UCLASS()
class AETHERIA_API UAetheriaCookRefs : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Aetheria|Cook")
	TArray<TObjectPtr<UObject>> Refs;
};
