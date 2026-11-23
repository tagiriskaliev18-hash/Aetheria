#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "AetheriaTypes.h"
#include "AetheriaBiomeBuilder.generated.h"

class APostProcessVolume;
class ADirectionalLight;
class ASkyLight;
class AExponentialHeightFog;

USTRUCT()
struct FAetheriaObjectiveVisualSlot
{
	GENERATED_BODY()

	UPROPERTY()
	int32 ObjectiveId = -1;

	UPROPERTY()
	EAetheriaObjectiveKind Kind = EAetheriaObjectiveKind::Beacon;

	UPROPERTY()
	UStaticMeshComponent* BaseMesh = nullptr;

	UPROPERTY()
	UStaticMeshComponent* CoreMesh = nullptr;

	UPROPERTY()
	UStaticMeshComponent* RingMesh = nullptr;

	UPROPERTY()
	UPointLightComponent* ObjectiveLight = nullptr;
};

/**
 * AAetheriaBiomeBuilder
 * Diablo IV-inspired dark-fantasy procedural biome builder with 2K photogrammetry PBR stone/basalt/glacier floors,
 * sculpted Blender Gothic architecture (Pillars, Ruined Arches, Cliff Boulders, Dead Trees, Runic Altars),
 * Volumetric Fog, Chiaroscuro moonlight + flickering wrought-iron Braziers.
 */
UCLASS()
class AETHERIA_API AAetheriaBiomeBuilder : public AActor
{
	GENERATED_BODY()

public:
	AAetheriaBiomeBuilder();

	virtual void Tick(float DeltaTime) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UStaticMeshComponent* ArenaFloorMesh;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* PillarsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* CrystalsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* RingsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* HazardSpireISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* BouldersISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* BraziersISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* StatuesISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* GatesISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* ChestsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* LanternsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* CitadelVistaISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* TerrainPlatformsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* CoverParapetsISM;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Arena")
	UInstancedStaticMeshComponent* TrenchFloorISM;

	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	APostProcessVolume* PostProcessVol = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	ADirectionalLight* KeyLight = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	ASkyLight* SkyLight = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Environment")
	AExponentialHeightFog* HeightFog = nullptr;

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	void BuildLocation(int32 LocationIndex);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	void SyncObjectives(const TArray<FAetheriaObjectiveInfo>& Objectives, float DeltaTime);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	void TriggerBossSlamVisual(const FVector& Location, float RadiusUU, FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	void TriggerAmbushTelegraphVisual(const FVector& Location, float RadiusUU, FLinearColor Color);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|World")
	int32 RipNearbyMatterForSingularity(const FVector& WorldCenter, float PullRadiusUU, int32 MaxChunks, TArray<FTransform>& OutRippedTransforms);

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	float GetCurrentTriadMultiplier(EAetheriaSpellId SpellId) const;

	UFUNCTION(BlueprintPure, Category = "Aetheria|World")
	int32 GetCurrentLocationIndex() const { return CurrentLocationIndex; }

protected:
	virtual void BeginPlay() override;

private:
	void EnsureEnvironmentActorsExist();
	void RefreshBiomeMeshes();
	void SpawnSanctuaryBraziers(float ArenaRadius, FLinearColor FireColor);
	void UpdateBrazierFlicker(float TimeSeconds);

	void BuildBastionArchitecture(float ArenaRadius);
	void BuildCalderaArchitecture(float ArenaRadius);
	void BuildGlacierArchitecture(float ArenaRadius);
	void BuildGravityGardensArchitecture(float ArenaRadius);
	void BuildStormNecropolisArchitecture(float ArenaRadius);
	void BuildMirrorLabyrinthArchitecture(float ArenaRadius);
	void BuildSunkenAstrolabeArchitecture(float ArenaRadius);
	void BuildTitanForgeArchitecture(float ArenaRadius);
	void BuildTimeTempleArchitecture(float ArenaRadius);
	void BuildChronoSealArchitecture(float ArenaRadius);
	void PopulatePerimeterVistasAndGates(float ArenaRadius);

	void UpdateAstrolabeRings(float DeltaTime);

	int32 CurrentLocationIndex = 0;
	float AstrolabeRotationAngle = 0.0f;

	UPROPERTY()
	TArray<FAetheriaObjectiveVisualSlot> ObjectiveSlots;

	UPROPERTY()
	TArray<UPointLightComponent*> BrazierLights;

	TArray<FTransform> BrazierFlameBaseTransforms;

	UPROPERTY()
	UMaterialInstanceDynamic* FloorDynamicMat = nullptr;
};
