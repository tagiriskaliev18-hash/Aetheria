#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Sound/SoundBase.h"
#include "Commandlets/Commandlet.h"
class USkeletalMesh;

#include "AetheriaMaterialLibrary.generated.h"

/**
 * UAetheriaMaterialLibrary
 * Mind x Diablo Dark-Fantasy PBR Material, Texture, Custom FBX Mesh & Audio Loader/Baker.
 * Uses 2K photogrammetry PBR textures (Albedo/DirectX Normal/Roughness/Emissive) and Blender-sculpted meshes
 * baked into /Game/* via UAetheriaShaderBakeCommandlet for native Win64 .exe packaging, with runtime fallbacks.
 */
UCLASS()
class AETHERIA_API UAetheriaMaterialLibrary : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetObsidianMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetMasterCharacterPBRMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Materials")
	static UMaterialInstanceDynamic* CreateModelPBRDynamicMaterial(
		UObject* Outer,
		const FString& TexStem,
		FLinearColor BaseTint = FLinearColor::White,
		FLinearColor EmissiveColor = FLinearColor::Black,
		float EmissiveBoost = 0.0f);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Audio")
	static void SetGlobalAudioVolume(float NewVolume);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Audio")
	static float GetGlobalAudioVolume();

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetBiomeFloorMaterial(int32 LocationIndex, UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetGothicStoneMaterial(UObject* Outer = nullptr);

	/** Build and save M_AetheriaGothicStone / M_AetheriaSanctumFloor if they are missing. */
	UFUNCTION(BlueprintCallable, Category = "Aetheria|Materials")
	static int32 EnsureBastionEnvironmentMaterials();

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetDarkIronMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetEmissiveCoreMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetFireballMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetBlackHoleHorizonMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetAccretionDiskMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetArcaneRingMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetChronosphereDomeMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetSolarBeamMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetPrismWallMaterial(UObject* Outer = nullptr);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Assets")
	static UTexture2D* LoadOrImportTexture(const FString& AssetName, const FString& RawRelativePngPath, bool bNormalMap = false);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Assets")
	static UStaticMesh* LoadCustomOrFallbackMesh(const FString& MeshName, const FString& FallbackEnginePath);
	static USkeletalMesh* LoadSkeletalMeshFromFolder(const FString& Folder, const FString& PreferredName);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Audio")
	static USoundBase* LoadOrImportSound(const FString& SoundName);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Audio")
	static void PlayGameSound2D(const UObject* WorldContextObject, const FString& SoundName, float VolumeMultiplier = 0.75f);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Audio")
	static void PlayGameSoundAtLocation(const UObject* WorldContextObject, const FString& SoundName, FVector Location, float VolumeMultiplier = 0.80f, float PitchMultiplier = 1.0f);

	UFUNCTION(BlueprintCallable, Category = "Aetheria|Materials")
	static UMaterialInstanceDynamic* CreateTintedDynamicMaterial(
		UObject* Outer,
		UMaterialInterface* Base,
		FLinearColor BaseColor,
		FLinearColor EmissiveColor,
		float EmissiveBoost = 1.0f,
		float Opacity = 1.0f);

	/** Cinematic biome set (gpt-image-2.6, RawAssets/Cinematic): floor or wall
	 *  PBR material for one of the four baked worlds. Falls back to the classic
	 *  biome floor material when the raw PNGs are missing. */
	UFUNCTION(BlueprintPure, Category = "Aetheria|Materials")
	static UMaterialInterface* GetCinematicBiomeMaterial(int32 WorldId, bool bWall, UObject* Outer = nullptr);

#if WITH_EDITOR
	static class UMaterial* BuildMasterCharacterPBRShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildObsidianShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildTexturedPBRShaderGraph(
		UPackage* Package,
		const FString& Name,
		UTexture2D* AlbedoTex,
		UTexture2D* NormalTex,
		UTexture2D* RoughTex,
		UTexture2D* EmissiveTex,
		float DefaultUVTiling,
		float DefaultMetallic);
	static class UMaterial* BuildEmissiveCoreShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildFireballShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildBlackHoleHorizonShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildAccretionDiskShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildChronosphereDomeShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildSolarBeamShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildPrismWallShaderGraph(UPackage* Package, const FString& Name);
	static class UMaterial* BuildArcaneRingShaderGraph(UPackage* Package, const FString& Name);
#endif

private:
	static UMaterialInterface* GetFallbackMaterial(bool bEmissive);
};

/**
 * UAetheriaShaderBakeCommandlet
 * Imports RawAssets (2K PBR PNGs, Blender FBX meshes, WAV SFX) into /Game/* with Nanite enabled on architecture,
 * then compiles and saves procedural shaders into /Game/Materials/M_Aetheria* packages for standalone .exe cooking.
 */
UCLASS()
class AETHERIA_API UAetheriaShaderBakeCommandlet : public UCommandlet
{
	GENERATED_BODY()

public:
	UAetheriaShaderBakeCommandlet();

	virtual int32 Main(const FString& Params) override;
};
