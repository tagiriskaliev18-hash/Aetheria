#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AetheriaTypes.h"
#include "AetheriaBiomeDatabase.generated.h"

UCLASS()
class AETHERIA_API UAetheriaBiomeDatabase : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static const TArray<FAetheriaLocationDef>& GetAllLocations();

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static FAetheriaLocationDef GetLocation(int32 LocationIndex);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static FAetheriaActTimingConfig GetActTiming(EAetheriaPacingMode Pacing, int32 PlayerCount);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static float SampleTerrainHeightAt(int32 LocationIndex, const FVector2D& Pos2D);

	/** Real ground Z for baked maps: line-traces down on the Pawn channel
	 *  (what the capsule actually rests on) and falls back to the procedural
	 *  sampler when there is no collision (loading, void, old worlds). */
	UFUNCTION(BlueprintCallable, Category = "Aetheria|World", meta = (WorldContext = "WorldContextObject"))
	static float GroundZAt(const UObject* WorldContextObject, int32 LocationIndex, const FVector2D& Pos2D, float BiasUU = 0.0f);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static bool IsOnHighGround(int32 LocationIndex, const FVector& WorldPos);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static bool IsInSunkenTrench(int32 LocationIndex, const FVector& WorldPos);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static bool IsInSanctuary(int32 LocationIndex, const FVector& WorldPos);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static bool SegmentHitsCoverParapet(int32 LocationIndex, const FVector2D& Start2D, const FVector2D& End2D, FVector2D& OutHit2D);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static FVector2D ResolveCoverAvoidance2D(int32 LocationIndex, const FVector2D& Pos2D, float ActorRadiusUU);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	static FVector FindNearestPlateauLocation(int32 LocationIndex, const FVector& NearPos);
};
