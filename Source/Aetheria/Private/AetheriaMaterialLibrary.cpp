#include "AetheriaMaterialLibrary.h"
#include "AetheriaTypes.h"
#include "UObject/Package.h"
#include "ImageUtils.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/CommandLine.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundWaveProcedural.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSampleParameter2D.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionFresnel.h"
#include "Materials/MaterialExpressionPanner.h"
#include "Materials/MaterialExpressionTime.h"
#include "Materials/MaterialExpressionSine.h"
#include "Materials/MaterialExpressionPower.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionNoise.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "Materials/MaterialExpressionAbs.h"
#include "Materials/MaterialExpressionClamp.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionDistance.h"
#include "Materials/MaterialExpressionDivide.h"
#include "Materials/MaterialExpressionDotProduct.h"
#include "Materials/MaterialExpressionOneMinus.h"
#include "Materials/MaterialExpressionSubtract.h"
#include "Materials/MaterialExpressionWorldPosition.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AutomatedAssetImportData.h"
#include "Misc/PackageName.h"
#endif

// ------------------------------------------------------------------------------------------------
// Texture, Custom FBX Mesh, Audio & Dynamic Material Helpers
// ------------------------------------------------------------------------------------------------

UTexture2D* UAetheriaMaterialLibrary::LoadOrImportTexture(const FString& AssetName, const FString& RawRelativePngPath, bool bNormalMap)
{
	static TMap<FString, TWeakObjectPtr<UTexture2D>> TextureCache;
	if (const TWeakObjectPtr<UTexture2D>* Found = TextureCache.Find(AssetName))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	const FString PkgPath = FString::Printf(TEXT("/Game/Textures/%s"), *AssetName);
	const FString CookedPath = FString::Printf(TEXT("/Game/Textures/%s.%s"), *AssetName, *AssetName);
	UTexture2D* Tex = FPackageName::DoesPackageExist(PkgPath)
		? LoadObject<UTexture2D>(nullptr, *CookedPath, nullptr, LOAD_NoWarn | LOAD_Quiet)
		: nullptr;

	auto TryLoadTexPath = [&Tex](const TCHAR* ObjectPath)
	{
		if (!Tex && ObjectPath && *ObjectPath)
		{
			Tex = LoadObject<UTexture2D>(nullptr, ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
	};

	if (AssetName == TEXT("T_GothicStone_Albedo"))
	{
		TryLoadTexPath(TEXT("/Game/Props/SM_PropPillar/prop_pillar/Textures/cathedral_brick_albedo.cathedral_brick_albedo"));
	}
	else if (AssetName == TEXT("T_GothicStone_Normal"))
	{
		TryLoadTexPath(TEXT("/Game/Props/SM_PropPillar/prop_pillar/Textures/cathedral_brick_normal.cathedral_brick_normal"));
	}
	else if (AssetName == TEXT("T_GothicStone_Roughness"))
	{
		TryLoadTexPath(TEXT("/Game/Props/SM_PropPillar/prop_pillar/Textures/cathedral_brick_roughness.cathedral_brick_roughness"));
	}
	else if (AssetName == TEXT("T_SanctumFlagstone_Albedo"))
	{
		TryLoadTexPath(TEXT("/Game/Props/SM_PropPillar/prop_pillar/Textures/stone_floor_albedo.stone_floor_albedo"));
	}
	else if (AssetName == TEXT("T_SanctumFlagstone_Normal"))
	{
		TryLoadTexPath(TEXT("/Game/Props/SM_PropPillar/prop_pillar/Textures/stone_floor_normal.stone_floor_normal"));
	}
	else if (AssetName == TEXT("T_SanctumFlagstone_Roughness"))
	{
		TryLoadTexPath(TEXT("/Game/Props/SM_PropPillar/prop_pillar/Textures/stone_floor_roughness.stone_floor_roughness"));
	}

	if (!Tex && !RawRelativePngPath.IsEmpty())
	{
		const FString FullPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RawRelativePngPath);
		if (FPaths::FileExists(FullPath))
		{
			Tex = FImageUtils::ImportFileAsTexture2D(FullPath);
			if (Tex)
			{
				Tex->AddToRoot();
				if (bNormalMap)
				{
					Tex->CompressionSettings = TC_Normalmap;
					Tex->SRGB = false;
					Tex->UpdateResource();
				}
			}
		}
	}

	if (!Tex)
	{
		FString GodotRel;
		if (AssetName == TEXT("T_GothicStone_Albedo")) GodotRel = TEXT("GodotAetheria/assets/textures/pbr/cathedral_brick_albedo.png");
		else if (AssetName == TEXT("T_GothicStone_Normal")) GodotRel = TEXT("GodotAetheria/assets/textures/pbr/cathedral_brick_normal.png");
		else if (AssetName == TEXT("T_GothicStone_Roughness")) GodotRel = TEXT("GodotAetheria/assets/textures/pbr/cathedral_brick_roughness.png");
		else if (AssetName == TEXT("T_SanctumFlagstone_Albedo")) GodotRel = TEXT("GodotAetheria/assets/textures/pbr/stone_floor_albedo.png");
		else if (AssetName == TEXT("T_SanctumFlagstone_Normal")) GodotRel = TEXT("GodotAetheria/assets/textures/pbr/stone_floor_normal.png");
		else if (AssetName == TEXT("T_SanctumFlagstone_Roughness")) GodotRel = TEXT("GodotAetheria/assets/textures/pbr/stone_floor_roughness.png");
		if (!GodotRel.IsEmpty())
		{
			const FString GodotFull = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / GodotRel);
			if (FPaths::FileExists(GodotFull))
			{
				Tex = FImageUtils::ImportFileAsTexture2D(GodotFull);
				if (Tex)
				{
					Tex->AddToRoot();
					if (bNormalMap)
					{
						Tex->CompressionSettings = TC_Normalmap;
						Tex->SRGB = false;
						Tex->UpdateResource();
					}
				}
			}
		}
	}

	if (Tex)
	{
		if (!bNormalMap && (AssetName.StartsWith(TEXT("T_UI_")) || AssetName.StartsWith(TEXT("SpellPlate_")) || AssetName.Contains(TEXT("Grimoire")) || AssetName.Contains(TEXT("Banner")) || AssetName.Contains(TEXT("Splash"))))
		{
			Tex->LODGroup = TEXTUREGROUP_UI;
			Tex->NeverStream = true;
		}
		TextureCache.Add(AssetName, Tex);
	}
	return Tex;
}

static float GGlobalAudioVolume = 0.75f;

void UAetheriaMaterialLibrary::SetGlobalAudioVolume(float NewVolume)
{
	GGlobalAudioVolume = FMath::Clamp(NewVolume, 0.0f, 1.0f);
}

float UAetheriaMaterialLibrary::GetGlobalAudioVolume()
{
	return GGlobalAudioVolume;
}

UStaticMesh* UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(const FString& MeshName, const FString& FallbackEnginePath)
{
	static TMap<FString, TWeakObjectPtr<UStaticMesh>> MeshCache;
	const FString CacheKey = MeshName + TEXT("|") + FallbackEnginePath;
	if (const TWeakObjectPtr<UStaticMesh>* Found = MeshCache.Find(CacheKey))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	auto TryStatic = [](const FString& PkgPath, const FString& ObjectName) -> UStaticMesh*
	{
		if (FPackageName::DoesPackageExist(PkgPath))
		{
			const FString CookedPath = FString::Printf(TEXT("%s.%s"), *PkgPath, *ObjectName);
			return LoadObject<UStaticMesh>(nullptr, *CookedPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		}
		return nullptr;
	};

	FString LookupName = MeshName;
	if (MeshName == TEXT("SM_GothicPillar")) LookupName = TEXT("SM_PropPillar");
	else if (MeshName == TEXT("SM_RuinedArch")) LookupName = TEXT("SM_PropArch");
	else if (MeshName == TEXT("SM_IronBrazier")) LookupName = TEXT("SM_PropBrazier");
	else if (MeshName == TEXT("SM_GothicStatue")) LookupName = TEXT("SM_PropStatue");
	else if (MeshName == TEXT("SM_TreasureChest")) LookupName = TEXT("SM_PropChest");
	else if (MeshName == TEXT("SM_LargeIronGate")) LookupName = TEXT("SM_PropIronGate");
	else if (MeshName == TEXT("SM_RunicAltar") || MeshName == TEXT("SM_SiphonAltar")) LookupName = TEXT("SM_PropShrineAltar");
	else if (MeshName == TEXT("SM_CliffBoulder")) LookupName = TEXT("SM_PropBoulder");

	UStaticMesh* ResultMesh = TryStatic(FString::Printf(TEXT("/Game/Props/%s"), *LookupName), LookupName);
	if (!ResultMesh)
	{
		ResultMesh = TryStatic(FString::Printf(TEXT("/Game/Props/%s/%s"), *LookupName, *LookupName), LookupName);
	}
	if (!ResultMesh)
	{
		ResultMesh = TryStatic(FString::Printf(TEXT("/Game/Meshes/%s"), *MeshName), MeshName);
	}
	if (!ResultMesh && LookupName != MeshName)
	{
		ResultMesh = TryStatic(FString::Printf(TEXT("/Game/Meshes/%s"), *LookupName), LookupName);
	}
	if (!ResultMesh && !FallbackEnginePath.IsEmpty())
	{
		ResultMesh = LoadObject<UStaticMesh>(nullptr, *FallbackEnginePath);
	}

	if (ResultMesh)
	{
		MeshCache.Add(CacheKey, ResultMesh);
	}
	return ResultMesh;
}

struct FAetheriaCachedRawWav
{
	uint32 SampleRate = 48000;
	uint16 NumChannels = 2;
	float Duration = 0.0f;
	TArray<uint8> PCMData;
	bool bValid = false;
};

static bool IsSilentAudioTestMode()
{
	return IsRunningCommandlet()
		|| GIsAutomationTesting
		|| FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))
		|| FParse::Param(FCommandLine::Get(), TEXT("nosound"))
		|| FParse::Param(FCommandLine::Get(), TEXT("autotest"));
}

static bool ParseWavRiffBytes(const TArray<uint8>& Bytes, FAetheriaCachedRawWav& OutWav)
{
	const int32 TotalLen = Bytes.Num();
	if (TotalLen < 44) return false;
	if (Bytes[0] != 'R' || Bytes[1] != 'I' || Bytes[2] != 'F' || Bytes[3] != 'F') return false;
	if (Bytes[8] != 'W' || Bytes[9] != 'A' || Bytes[10] != 'V' || Bytes[11] != 'E') return false;

	uint16 Channels = 2;
	uint32 SampleRate = 48000;
	uint16 BitsPerSample = 16;
	int32 Pos = 12;

	while (Pos + 8 <= TotalLen)
	{
		const uint32 ChunkId = *reinterpret_cast<const uint32*>(&Bytes[Pos]);
		uint32 ChunkSize = *reinterpret_cast<const uint32*>(&Bytes[Pos + 4]);
		Pos += 8;
		if (Pos + static_cast<int32>(ChunkSize) > TotalLen)
		{
			ChunkSize = static_cast<uint32>(TotalLen - Pos);
		}

		// "fmt " == 0x20746D66 LE, "data" == 0x61746164 LE
		if (Bytes[Pos - 8] == 'f' && Bytes[Pos - 7] == 'm' && Bytes[Pos - 6] == 't' && Bytes[Pos - 5] == ' ' && ChunkSize >= 16)
		{
			Channels = *reinterpret_cast<const uint16*>(&Bytes[Pos + 2]);
			SampleRate = *reinterpret_cast<const uint32*>(&Bytes[Pos + 4]);
			BitsPerSample = *reinterpret_cast<const uint16*>(&Bytes[Pos + 14]);
		}
		else if (Bytes[Pos - 8] == 'd' && Bytes[Pos - 7] == 'a' && Bytes[Pos - 6] == 't' && Bytes[Pos - 5] == 'a' && ChunkSize > 0)
		{
			if (BitsPerSample == 16 && Channels >= 1 && SampleRate >= 8000)
			{
				OutWav.SampleRate = SampleRate;
				OutWav.NumChannels = Channels;
				OutWav.PCMData.SetNumUninitialized(static_cast<int32>(ChunkSize));
				FMemory::Memcpy(OutWav.PCMData.GetData(), &Bytes[Pos], ChunkSize);
				const float BytesPerSec = static_cast<float>(SampleRate) * Channels * 2.0f;
				OutWav.Duration = BytesPerSec > 0.0f ? (static_cast<float>(ChunkSize) / BytesPerSec) : 0.0f;
				OutWav.bValid = true;
				return true;
			}
			return false;
		}
		(void)ChunkId;
		Pos += static_cast<int32>(ChunkSize);
		if ((ChunkSize & 1u) != 0u) Pos += 1;
	}
	return false;
}

static USoundBase* ResolvePlayableAudioAsset(const FString& SoundName)
{
	static TMap<FString, TWeakObjectPtr<USoundBase>> CookedSoundCache;
	if (const TWeakObjectPtr<USoundBase>* CachedCooked = CookedSoundCache.Find(SoundName))
	{
		if (CachedCooked->IsValid())
		{
			return CachedCooked->Get();
		}
	}

	const FString PkgPath = FString::Printf(TEXT("/Game/Audio/%s"), *SoundName);
	const FString CookedPath = FString::Printf(TEXT("/Game/Audio/%s.%s"), *SoundName, *SoundName);
	if (FPackageName::DoesPackageExist(PkgPath))
	{
		if (USoundBase* Cooked = LoadObject<USoundBase>(nullptr, *CookedPath))
		{
			CookedSoundCache.Add(SoundName, Cooked);
			return Cooked;
		}
	}

	static TMap<FString, FAetheriaCachedRawWav> RawWavCache;
	FAetheriaCachedRawWav* RawWav = RawWavCache.Find(SoundName);
	if (!RawWav)
	{
		FAetheriaCachedRawWav Parsed;
		const FString RawPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / TEXT("RawAssets/Audio") / (SoundName + TEXT(".wav")));
		if (FPaths::FileExists(RawPath))
		{
			TArray<uint8> Bytes;
			if (FFileHelper::LoadFileToArray(Bytes, *RawPath))
			{
				ParseWavRiffBytes(Bytes, Parsed);
			}
		}
		RawWav = &RawWavCache.Add(SoundName, MoveTemp(Parsed));
	}

	if (RawWav && RawWav->bValid && RawWav->PCMData.Num() > 0)
	{
		USoundWaveProcedural* Sw = NewObject<USoundWaveProcedural>(GetTransientPackage());
		if (Sw)
		{
			Sw->SetSampleRate(RawWav->SampleRate);
			Sw->NumChannels = RawWav->NumChannels;
			Sw->Duration = RawWav->Duration;
			Sw->bLooping = false;
			Sw->SoundGroup = SOUNDGROUP_Effects;
			Sw->QueueAudio(RawWav->PCMData.GetData(), RawWav->PCMData.Num());
			return Sw;
		}
	}
	return nullptr;
}

static bool PassSoundAntiSpamThrottle(const FString& SoundName)
{
	static TMap<FString, double> LastPlaySec;
	const double NowSec = FPlatformTime::Seconds();
	const double CooldownSec = SoundName.StartsWith(TEXT("SFX_Banner")) ? 1.50 : 0.04;
	if (const double* PrevSec = LastPlaySec.Find(SoundName))
	{
		if ((NowSec - *PrevSec) < CooldownSec)
		{
			return false;
		}
	}
	LastPlaySec.Add(SoundName, NowSec);
	return true;
}

USoundBase* UAetheriaMaterialLibrary::LoadOrImportSound(const FString& SoundName)
{
	return ResolvePlayableAudioAsset(SoundName);
}

void UAetheriaMaterialLibrary::PlayGameSound2D(const UObject* WorldContextObject, const FString& SoundName, float VolumeMultiplier)
{
	if (!WorldContextObject || IsSilentAudioTestMode()) return;
	const float FinalVolume = VolumeMultiplier * GGlobalAudioVolume;
	if (FinalVolume <= 0.001f) return;
	if (!PassSoundAntiSpamThrottle(SoundName)) return;

	if (USoundBase* Sound = ResolvePlayableAudioAsset(SoundName))
	{
		const float Pitch = FMath::FRandRange(0.96f, 1.04f);
		UGameplayStatics::PlaySound2D(WorldContextObject, Sound, FinalVolume, Pitch);
	}
}

void UAetheriaMaterialLibrary::PlayGameSoundAtLocation(const UObject* WorldContextObject, const FString& SoundName, FVector Location, float VolumeMultiplier, float PitchMultiplier)
{
	if (!WorldContextObject || IsSilentAudioTestMode()) return;
	const float FinalVolume = VolumeMultiplier * GGlobalAudioVolume;
	if (FinalVolume <= 0.001f) return;
	if (!PassSoundAntiSpamThrottle(SoundName)) return;

	if (USoundBase* Sound = ResolvePlayableAudioAsset(SoundName))
	{
		const float OrganicPitch = PitchMultiplier * FMath::FRandRange(0.95f, 1.05f);
		UGameplayStatics::PlaySoundAtLocation(WorldContextObject, Sound, Location, FinalVolume, OrganicPitch);
	}
}

UMaterialInterface* UAetheriaMaterialLibrary::GetFallbackMaterial(bool bEmissive)
{
	const TCHAR* Path = bEmissive
		? TEXT("/Engine/EngineMaterials/EmissiveMeshMaterial.EmissiveMeshMaterial")
		: TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	UMaterialInterface* Fallback = LoadObject<UMaterialInterface>(nullptr, Path);
	return Fallback;
}

UMaterialInstanceDynamic* UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
	UObject* Outer,
	UMaterialInterface* Base,
	FLinearColor BaseColor,
	FLinearColor EmissiveColor,
	float EmissiveBoost,
	float Opacity)
{
	if (!Base)
	{
		Base = GetEmissiveCoreMaterial(Outer);
	}

	UMaterialInstanceDynamic* MID = Cast<UMaterialInstanceDynamic>(Base);
	if (!MID)
	{
		MID = UMaterialInstanceDynamic::Create(Base ? Base : GetFallbackMaterial(true), Outer);
	}

	if (MID)
	{
		FLinearColor SafeBaseColor = BaseColor;
		if (SafeBaseColor.A > 0.0f)
		{
			SafeBaseColor.R = FMath::Max(SafeBaseColor.R, 0.035f);
			SafeBaseColor.G = FMath::Max(SafeBaseColor.G, 0.035f);
			SafeBaseColor.B = FMath::Max(SafeBaseColor.B, 0.048f);
		}

		MID->SetVectorParameterValue(FName("BaseColor"), SafeBaseColor);
		MID->SetVectorParameterValue(FName("Color"), SafeBaseColor);
		MID->SetVectorParameterValue(FName("EmissiveColor"), EmissiveColor);
		MID->SetVectorParameterValue(FName("AccentColor"), EmissiveColor);
		MID->SetScalarParameterValue(FName("EmissiveBoost"), EmissiveBoost);
		MID->SetScalarParameterValue(FName("EmissivePower"), EmissiveBoost);
		MID->SetScalarParameterValue(FName("Opacity"), Opacity);
		MID->SetScalarParameterValue(FName("Alpha"), Opacity);
	}

	return MID;
}

UMaterialInstanceDynamic* UAetheriaMaterialLibrary::CreateModelPBRDynamicMaterial(
	UObject* Outer,
	const FString& TexStem,
	FLinearColor BaseTint,
	FLinearColor EmissiveColor,
	float EmissiveBoost)
{
	FString EffectiveStem = TexStem;
	FString AlbedoName = FString::Printf(TEXT("T_%s_Albedo"), *EffectiveStem);
	FString NormalName = FString::Printf(TEXT("T_%s_Normal"), *EffectiveStem);
	FString RoughName  = FString::Printf(TEXT("T_%s_Roughness"), *EffectiveStem);

	FString RawAlbedoPath = FString::Printf(TEXT("RawAssets/Textures/%s.png"), *AlbedoName);
	FString RawNormalPath = FString::Printf(TEXT("RawAssets/Textures/%s.png"), *NormalName);
	FString RawRoughPath  = FString::Printf(TEXT("RawAssets/Textures/%s.png"), *RoughName);

	UTexture2D* AlbedoTex = LoadOrImportTexture(AlbedoName, RawAlbedoPath, false);
	if (!AlbedoTex && EffectiveStem.StartsWith(TEXT("SM_")))
	{
		EffectiveStem = EffectiveStem.Mid(3);
		AlbedoName = FString::Printf(TEXT("T_%s_Albedo"), *EffectiveStem);
		NormalName = FString::Printf(TEXT("T_%s_Normal"), *EffectiveStem);
		RoughName  = FString::Printf(TEXT("T_%s_Roughness"), *EffectiveStem);

		RawAlbedoPath = FString::Printf(TEXT("RawAssets/Textures/%s.png"), *AlbedoName);
		RawNormalPath = FString::Printf(TEXT("RawAssets/Textures/%s.png"), *NormalName);
		RawRoughPath  = FString::Printf(TEXT("RawAssets/Textures/%s.png"), *RoughName);

		AlbedoTex = LoadOrImportTexture(AlbedoName, RawAlbedoPath, false);
	}
	if (!AlbedoTex)
	{
		return CreateTintedDynamicMaterial(Outer, GetDarkIronMaterial(Outer), BaseTint, EmissiveColor, EmissiveBoost, 1.0f);
	}

	UTexture2D* NormalTex = LoadOrImportTexture(NormalName, RawNormalPath, true);
	UTexture2D* RoughTex  = LoadOrImportTexture(RoughName, RawRoughPath, false);

	UMaterialInterface* MasterMat = GetMasterCharacterPBRMaterial(Outer);
	if (!MasterMat)
	{
		return CreateTintedDynamicMaterial(Outer, GetDarkIronMaterial(Outer), BaseTint, EmissiveColor, EmissiveBoost, 1.0f);
	}

	UMaterialInstanceDynamic* MID = UMaterialInstanceDynamic::Create(MasterMat, Outer);
	if (!MID)
	{
		return nullptr;
	}

	MID->SetTextureParameterValue(FName("AlbedoMap"), AlbedoTex);
	if (NormalTex)
	{
		MID->SetTextureParameterValue(FName("NormalMap"), NormalTex);
	}
	if (RoughTex)
	{
		MID->SetTextureParameterValue(FName("RoughnessMap"), RoughTex);
	}

	FLinearColor SafeBaseTint = BaseTint;
	if (SafeBaseTint.A > 0.0f)
	{
		SafeBaseTint.R = FMath::Max(SafeBaseTint.R, 0.035f);
		SafeBaseTint.G = FMath::Max(SafeBaseTint.G, 0.035f);
		SafeBaseTint.B = FMath::Max(SafeBaseTint.B, 0.048f);
	}

	MID->SetVectorParameterValue(FName("BaseColor"), SafeBaseTint);
	MID->SetVectorParameterValue(FName("EmissiveColor"), EmissiveColor);
	MID->SetScalarParameterValue(FName("EmissiveBoost"), EmissiveBoost);
	MID->SetScalarParameterValue(FName("HitFlash"), 0.0f);

	return MID;
}

// ------------------------------------------------------------------------------------------------
// Static Material Getters (Load -> Editor Procedural Build -> Engine Fallback)
// ------------------------------------------------------------------------------------------------

UMaterialInterface* UAetheriaMaterialLibrary::GetMasterCharacterPBRMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaMasterPBR.M_AetheriaMasterPBR"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaMasterPBR"));
		Mat = BuildMasterCharacterPBRShaderGraph(Package, TEXT("M_AetheriaMasterPBR"));
	}
#endif
	if (!Mat)
	{
		Mat = GetDarkIronMaterial(Outer);
	}
	if (!Mat)
	{
		Mat = GetFallbackMaterial(false);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetObsidianMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaObsidian.M_AetheriaObsidian"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaObsidian"));
		Mat = BuildObsidianShaderGraph(Package, TEXT("M_AetheriaObsidian"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(false);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetBiomeFloorMaterial(int32 LocationIndex, UObject* Outer)
{
	const bool bCaldera = (LocationIndex == 1 || LocationIndex == 7);
	const bool bGlacier = (LocationIndex == 2 || LocationIndex == 4 || LocationIndex == 6 || LocationIndex == 8);

	const FString MatName = bCaldera
		? TEXT("M_AetheriaCalderaFloor")
		: (bGlacier ? TEXT("M_AetheriaGlacierFloor") : TEXT("M_AetheriaSanctumFloor"));
	const FString MatPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), *MatName, *MatName);

	if (UMaterialInterface* Cooked = LoadObject<UMaterialInterface>(nullptr, *MatPath))
	{
		return Cooked;
	}

#if WITH_EDITOR
	UTexture2D* Alb = bCaldera
		? LoadOrImportTexture(TEXT("T_CalderaBasalt_Albedo"), TEXT("RawAssets/Textures/T_CalderaBasalt_Albedo.png"), false)
		: (bGlacier
			? LoadOrImportTexture(TEXT("T_GlacierSlate_Albedo"), TEXT("RawAssets/Textures/T_GlacierSlate_Albedo.png"), false)
			: LoadOrImportTexture(TEXT("T_SanctumFlagstone_Albedo"), TEXT("RawAssets/Textures/T_SanctumFlagstone_Albedo.png"), false));
	UTexture2D* Nrm = bCaldera
		? LoadOrImportTexture(TEXT("T_CalderaBasalt_Normal"), TEXT("RawAssets/Textures/T_CalderaBasalt_Normal.png"), true)
		: (bGlacier
			? LoadOrImportTexture(TEXT("T_GlacierSlate_Normal"), TEXT("RawAssets/Textures/T_GlacierSlate_Normal.png"), true)
			: LoadOrImportTexture(TEXT("T_SanctumFlagstone_Normal"), TEXT("RawAssets/Textures/T_SanctumFlagstone_Normal.png"), true));
	UTexture2D* Rgh = bCaldera
		? nullptr
		: (bGlacier
			? LoadOrImportTexture(TEXT("T_GlacierSlate_Roughness"), TEXT("RawAssets/Textures/T_GlacierSlate_Roughness.png"), false)
			: LoadOrImportTexture(TEXT("T_SanctumFlagstone_Roughness"), TEXT("RawAssets/Textures/T_SanctumFlagstone_Roughness.png"), false));
	UTexture2D* Ems = bCaldera
		? LoadOrImportTexture(TEXT("T_CalderaBasalt_Emissive"), TEXT("RawAssets/Textures/T_CalderaBasalt_Emissive.png"), false)
		: nullptr;

	if (Alb)
	{
		UPackage* Pkg = CreatePackage(*FString::Printf(TEXT("/Game/Materials/%s"), *MatName));
		if (UMaterial* Built = BuildTexturedPBRShaderGraph(Pkg, MatName, Alb, Nrm, Rgh, Ems, 22.0f, 0.08f))
		{
			return Built;
		}
	}
#endif

	return GetObsidianMaterial(Outer);
}

UMaterialInterface* UAetheriaMaterialLibrary::GetCinematicBiomeMaterial(int32 WorldId, bool bWall, UObject* Outer)
{
	// EAetheriaWorldId order: 0 Bastion, 1 RuinsTemple, 2 VoidRealm, 3 BossSanctum.
	static const TCHAR* Sets[] = {
		TEXT("pbr_bastion"), TEXT("pbr_ruins"), TEXT("pbr_void"), TEXT("pbr_sanctum")
	};
	const int32 SetIdx = (WorldId >= 0 && WorldId < 4) ? WorldId : 0;
	const FString Stem = FString(Sets[SetIdx]) + (bWall ? TEXT("_wall") : TEXT("_floor"));

	static TMap<FString, TWeakObjectPtr<UMaterialInterface>> Cache;
	if (const TWeakObjectPtr<UMaterialInterface>* Found = Cache.Find(Stem))
	{
		if (Found->IsValid()) return Found->Get();
	}

	const FString MatName = TEXT("M_Cine_") + Stem;
	const FString MatPath = FString::Printf(TEXT("/Game/Materials/%s.%s"), *MatName, *MatName);
	if (UMaterialInterface* Cooked = LoadObject<UMaterialInterface>(nullptr, *MatPath))
	{
		Cache.Add(Stem, Cooked);
		return Cooked;
	}

#if WITH_EDITOR
	UTexture2D* Alb = LoadOrImportTexture(TEXT("T_Cine_") + Stem + TEXT("_Albedo"),
		FString::Printf(TEXT("RawAssets/Cinematic/%s.png"), *Stem), false);
	if (Alb)
	{
		UTexture2D* Nrm = LoadOrImportTexture(TEXT("T_Cine_") + Stem + TEXT("_Normal"),
			FString::Printf(TEXT("RawAssets/Cinematic/%s_Normal.png"), *Stem), true);
		UTexture2D* Rgh = LoadOrImportTexture(TEXT("T_Cine_") + Stem + TEXT("_Roughness"),
			FString::Printf(TEXT("RawAssets/Cinematic/%s_Roughness.png"), *Stem), false);
		// Ember-crack glow on the two fiery floors; walls and cool worlds stay matte.
		UTexture2D* Ems = nullptr;
		if (!bWall && (SetIdx == 0 || SetIdx == 2))
		{
			Ems = LoadOrImportTexture(TEXT("T_Cine_") + Stem + TEXT("_Emissive"),
				FString::Printf(TEXT("RawAssets/Cinematic/%s_Emissive.png"), *Stem), false);
		}
		UPackage* Pkg = CreatePackage(*FString::Printf(TEXT("/Game/Materials/%s"), *MatName));
		// Floors tile tighter (world-aligned surfaces), walls slightly coarser.
		const float Tiling = bWall ? 14.0f : 22.0f;
		if (UMaterial* Built = BuildTexturedPBRShaderGraph(Pkg, MatName, Alb, Nrm, Rgh, Ems, Tiling, 0.02f))
		{
			Cache.Add(Stem, Built);
			return Built;
		}
	}
#endif

	UMaterialInterface* Fallback = GetBiomeFloorMaterial(WorldId, Outer);
	Cache.Add(Stem, Fallback);
	return Fallback;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetGothicStoneMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid()) return Cached.Get();

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaGothicStone.M_AetheriaGothicStone"));
#if WITH_EDITOR
	if (!Mat)
	{
		UTexture2D* Alb = LoadOrImportTexture(TEXT("T_GothicStone_Albedo"), TEXT("RawAssets/Textures/T_GothicStone_Albedo.png"), false);
		UTexture2D* Nrm = LoadOrImportTexture(TEXT("T_GothicStone_Normal"), TEXT("RawAssets/Textures/T_GothicStone_Normal.png"), true);
		UTexture2D* Rgh = LoadOrImportTexture(TEXT("T_GothicStone_Roughness"), TEXT("RawAssets/Textures/T_GothicStone_Roughness.png"), false);
		if (Alb)
		{
			UPackage* Pkg = CreatePackage(TEXT("/Game/Materials/M_AetheriaGothicStone"));
			Mat = BuildTexturedPBRShaderGraph(Pkg, TEXT("M_AetheriaGothicStone"), Alb, Nrm, Rgh, nullptr, 2.0f, 0.04f);
		}
	}
#endif
	if (!Mat) Mat = GetObsidianMaterial(Outer);
	Cached = Mat;
	return Mat;
}

int32 UAetheriaMaterialLibrary::EnsureBastionEnvironmentMaterials()
{
	int32 Saved = 0;
#if WITH_EDITOR
	auto SaveMat = [&Saved](UMaterialInterface* Mat)
	{
		if (!Mat) return;
		UPackage* Pkg = Mat->GetOutermost();
		if (!Pkg) return;
		Mat->MarkPackageDirty();
		const FString Filename = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		if (UPackage::SavePackage(Pkg, Mat, *Filename, SaveArgs))
		{
			++Saved;
			UE_LOG(LogTemp, Display, TEXT("[Aetheria] Saved material %s"), *Pkg->GetName());
		}
	};
	SaveMat(GetGothicStoneMaterial());
	SaveMat(GetBiomeFloorMaterial(0));
#endif
	return Saved;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetDarkIronMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid()) return Cached.Get();

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaDarkIron.M_AetheriaDarkIron"));
#if WITH_EDITOR
	if (!Mat)
	{
		UTexture2D* Alb = LoadOrImportTexture(TEXT("T_DarkIronCloth_Albedo"), TEXT("RawAssets/Textures/T_DarkIronCloth_Albedo.png"), false);
		UTexture2D* Nrm = LoadOrImportTexture(TEXT("T_DarkIronCloth_Normal"), TEXT("RawAssets/Textures/T_DarkIronCloth_Normal.png"), true);
		UTexture2D* Rgh = LoadOrImportTexture(TEXT("T_DarkIronCloth_Roughness"), TEXT("RawAssets/Textures/T_DarkIronCloth_Roughness.png"), false);
		if (Alb)
		{
			UPackage* Pkg = CreatePackage(TEXT("/Game/Materials/M_AetheriaDarkIron"));
			Mat = BuildTexturedPBRShaderGraph(Pkg, TEXT("M_AetheriaDarkIron"), Alb, Nrm, Rgh, nullptr, 1.5f, 0.35f);
		}
	}
#endif
	if (!Mat) Mat = GetObsidianMaterial(Outer);
	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetEmissiveCoreMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaEmissiveCore.M_AetheriaEmissiveCore"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaEmissiveCore"));
		Mat = BuildEmissiveCoreShaderGraph(Package, TEXT("M_AetheriaEmissiveCore"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(true);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetFireballMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaFireball.M_AetheriaFireball"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaFireball"));
		Mat = BuildFireballShaderGraph(Package, TEXT("M_AetheriaFireball"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(true);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetBlackHoleHorizonMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaBlackHole.M_AetheriaBlackHole"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaBlackHole"));
		Mat = BuildBlackHoleHorizonShaderGraph(Package, TEXT("M_AetheriaBlackHole"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(false);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetAccretionDiskMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaAccretionDisk.M_AetheriaAccretionDisk"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaAccretionDisk"));
		Mat = BuildAccretionDiskShaderGraph(Package, TEXT("M_AetheriaAccretionDisk"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(true);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetArcaneRingMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaArcaneRing.M_AetheriaArcaneRing"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaArcaneRing"));
		Mat = BuildArcaneRingShaderGraph(Package, TEXT("M_AetheriaArcaneRing"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(true);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetChronosphereDomeMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaChronosphere.M_AetheriaChronosphere"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaChronosphere"));
		Mat = BuildChronosphereDomeShaderGraph(Package, TEXT("M_AetheriaChronosphere"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(true);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetSolarBeamMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaSolarBeam.M_AetheriaSolarBeam"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaSolarBeam"));
		Mat = BuildSolarBeamShaderGraph(Package, TEXT("M_AetheriaSolarBeam"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(true);
	}

	Cached = Mat;
	return Mat;
}

UMaterialInterface* UAetheriaMaterialLibrary::GetPrismWallMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaPrismWall.M_AetheriaPrismWall"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaPrismWall"));
		Mat = BuildPrismWallShaderGraph(Package, TEXT("M_AetheriaPrismWall"));
	}
#endif
	if (!Mat)
	{
		Mat = GetFallbackMaterial(true);
	}

	Cached = Mat;
	return Mat;
}

// ------------------------------------------------------------------------------------------------
// Programmatic Editor Shader Graph Construction (Mind Design Aesthetics)
// ------------------------------------------------------------------------------------------------

#if WITH_EDITOR

UMaterial* UAetheriaMaterialLibrary::BuildMasterCharacterPBRShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Opaque;
	Mat->SetShadingModel(MSM_DefaultLit);
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	// 1. Albedo Map Parameter
	UMaterialExpressionTextureSampleParameter2D* AlbedoParam = Cast<UMaterialExpressionTextureSampleParameter2D>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -600, -300));
	if (AlbedoParam)
	{
		AlbedoParam->ParameterName = FName("AlbedoMap");
		AlbedoParam->SamplerType = SAMPLERTYPE_Color;
		UTexture2D* DefAlbedo = LoadOrImportTexture(TEXT("T_DarkIronCloth_Albedo"), TEXT("RawAssets/Textures/T_DarkIronCloth_Albedo.png"), false);
		if (DefAlbedo)
		{
			AlbedoParam->Texture = DefAlbedo;
		}
		else
		{
			AlbedoParam->SetDefaultTexture();
		}
	}

	// Base Color Vector Parameter (Tint)
	UMaterialExpressionVectorParameter* BaseColParam = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -600, -100));
	if (BaseColParam)
	{
		BaseColParam->ParameterName = FName("BaseColor");
		BaseColParam->DefaultValue = FLinearColor::White;
	}

	// AlbedoMap(RGB) * BaseColor -> MP_BaseColor
	UMaterialExpressionMultiply* AlbedoMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -250, -200));
	if (AlbedoParam && BaseColParam && AlbedoMult)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(AlbedoParam, TEXT("RGB"), AlbedoMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BaseColParam, TEXT(""), AlbedoMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(AlbedoMult, TEXT(""), MP_BaseColor);
	}

	// 2. Normal Map Parameter
	UMaterialExpressionTextureSampleParameter2D* NormalParam = Cast<UMaterialExpressionTextureSampleParameter2D>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -600, 50));
	if (NormalParam)
	{
		NormalParam->ParameterName = FName("NormalMap");
		NormalParam->SamplerType = SAMPLERTYPE_Normal;
		UTexture2D* DefNormal = LoadOrImportTexture(TEXT("T_DarkIronCloth_Normal"), TEXT("RawAssets/Textures/T_DarkIronCloth_Normal.png"), true);
		if (DefNormal)
		{
			NormalParam->Texture = DefNormal;
		}
		else
		{
			NormalParam->SetDefaultTexture();
		}
		UMaterialEditingLibrary::ConnectMaterialProperty(NormalParam, TEXT("RGB"), MP_Normal);
	}

	// 3. Roughness Map Parameter
	UMaterialExpressionTextureSampleParameter2D* RoughParam = Cast<UMaterialExpressionTextureSampleParameter2D>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSampleParameter2D::StaticClass(), -600, 300));
	if (RoughParam)
	{
		RoughParam->ParameterName = FName("RoughnessMap");
		RoughParam->SamplerType = SAMPLERTYPE_LinearColor;
		UTexture2D* DefRough = LoadOrImportTexture(TEXT("T_DarkIronCloth_Roughness"), TEXT("RawAssets/Textures/T_DarkIronCloth_Roughness.png"), false);
		if (DefRough)
		{
			RoughParam->Texture = DefRough;
		}
		else
		{
			RoughParam->SetDefaultTexture();
		}
		UMaterialEditingLibrary::ConnectMaterialProperty(RoughParam, TEXT("R"), MP_Roughness);
	}

	// 4. Metallic Scalar Parameter
	UMaterialExpressionScalarParameter* MetalParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -400, 500));
	if (MetalParam)
	{
		MetalParam->ParameterName = FName("Metallic");
		MetalParam->DefaultValue = 0.18f;
		UMaterialEditingLibrary::ConnectMaterialProperty(MetalParam, TEXT(""), MP_Metallic);
	}

	// 4b. Specular Scalar Parameter (Cavity / Fresnel contrast)
	UMaterialExpressionScalarParameter* SpecParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -400, 600));
	if (SpecParam)
	{
		SpecParam->ParameterName = FName("Specular");
		SpecParam->DefaultValue = 0.52f;
		UMaterialEditingLibrary::ConnectMaterialProperty(SpecParam, TEXT(""), MP_Specular);
	}

	// 5. Emissive: (Fresnel * EmissiveColor * EmissiveBoost) + (HitFlash * EmissiveColor * 4.0)
	UMaterialExpressionFresnel* FresnelExpr = Cast<UMaterialExpressionFresnel>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionFresnel::StaticClass(), -800, 700));
	UMaterialExpressionVectorParameter* EmissColParam = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -800, 900));
	if (EmissColParam)
	{
		EmissColParam->ParameterName = FName("EmissiveColor");
		EmissColParam->DefaultValue = FLinearColor::Black;
	}

	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -800, 1100));
	if (BoostParam)
	{
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 0.0f;
	}

	UMaterialExpressionScalarParameter* HitFlashParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -800, 1300));
	if (HitFlashParam)
	{
		HitFlashParam->ParameterName = FName("HitFlash");
		HitFlashParam->DefaultValue = 0.0f;
	}

	// Fresnel * EmissiveColor
	UMaterialExpressionMultiply* FresnelColorMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -500, 750));
	// (Fresnel * EmissiveColor) * EmissiveBoost
	UMaterialExpressionMultiply* FresnelTermMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -300, 800));

	if (FresnelExpr && EmissColParam && BoostParam && FresnelColorMult && FresnelTermMult)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelExpr, TEXT(""), FresnelColorMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(EmissColParam, TEXT(""), FresnelColorMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelColorMult, TEXT(""), FresnelTermMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), FresnelTermMult, TEXT("B"));
	}

	// HitFlash * EmissiveColor
	UMaterialExpressionMultiply* HitColorMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -500, 1150));
	// (HitFlash * EmissiveColor) * 4.0
	UMaterialExpressionMultiply* HitScaleMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -300, 1200));

	if (HitFlashParam && EmissColParam && HitColorMult && HitScaleMult)
	{
		HitScaleMult->ConstB = 4.0f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(HitFlashParam, TEXT(""), HitColorMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(EmissColParam, TEXT(""), HitColorMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(HitColorMult, TEXT(""), HitScaleMult, TEXT("A"));
	}

	// Final Emissive: FresnelTerm + HitTerm
	UMaterialExpressionAdd* EmissAdd = Cast<UMaterialExpressionAdd>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), -100, 950));
	if (FresnelTermMult && HitScaleMult && EmissAdd)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelTermMult, TEXT(""), EmissAdd, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(HitScaleMult, TEXT(""), EmissAdd, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(EmissAdd, TEXT(""), MP_EmissiveColor);
	}

	// 6. Living Sanctuary Cloth Wind & Breathing WorldPositionOffset (gated by ClothWind = 0 for static stone, 1 for characters)
	UMaterialExpressionScalarParameter* ClothWindParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -800, 1520));
	UMaterialExpressionWorldPosition* WpoWorldPos = Cast<UMaterialExpressionWorldPosition>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionWorldPosition::StaticClass(), -1250, 1460));
	UMaterialExpressionTime* WpoTime = Cast<UMaterialExpressionTime>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTime::StaticClass(), -1250, 1620));
	UMaterialExpressionConstant3Vector* WpoDriftDir = Cast<UMaterialExpressionConstant3Vector>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant3Vector::StaticClass(), -1250, 1740));
	UMaterialExpressionMultiply* WpoTimeDrift = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -1050, 1660));
	UMaterialExpressionAdd* WpoSamplePos = Cast<UMaterialExpressionAdd>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), -900, 1540));
	UMaterialExpressionNoise* ClothWaveNoise = Cast<UMaterialExpressionNoise>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionNoise::StaticClass(), -720, 1540));
	UMaterialExpressionConstant3Vector* ClothAmpVec = Cast<UMaterialExpressionConstant3Vector>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant3Vector::StaticClass(), -520, 1680));
	UMaterialExpressionMultiply* ClothDisplace = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -420, 1540));
	UMaterialExpressionMultiply* FinalClothWPO = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -180, 1540));

	if (ClothWindParam)
	{
		ClothWindParam->ParameterName = FName("ClothWind");
		ClothWindParam->DefaultValue = 0.0f;
	}
	if (WpoDriftDir)
	{
		WpoDriftDir->Constant = FLinearColor(42.0f, -31.0f, 18.0f, 0.0f);
	}
	if (ClothWaveNoise)
	{
		ClothWaveNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
		ClothWaveNoise->Scale = 0.024f;
		ClothWaveNoise->Quality = 1;
		ClothWaveNoise->bTurbulence = false;
		ClothWaveNoise->Levels = 2;
		ClothWaveNoise->OutputMin = -1.0f;
		ClothWaveNoise->OutputMax = 1.0f;
	}
	if (ClothAmpVec)
	{
		ClothAmpVec->Constant = FLinearColor(2.4f, 1.9f, 0.75f, 0.0f);
	}
	if (ClothWindParam && WpoWorldPos && WpoTime && WpoDriftDir && WpoTimeDrift &&
		WpoSamplePos && ClothWaveNoise && ClothAmpVec && ClothDisplace && FinalClothWPO)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(WpoTime, TEXT(""), WpoTimeDrift, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WpoDriftDir, TEXT(""), WpoTimeDrift, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WpoWorldPos, TEXT(""), WpoSamplePos, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WpoTimeDrift, TEXT(""), WpoSamplePos, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WpoSamplePos, TEXT(""), ClothWaveNoise, TEXT("Position"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ClothWaveNoise, TEXT(""), ClothDisplace, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ClothAmpVec, TEXT(""), ClothDisplace, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ClothDisplace, TEXT(""), FinalClothWPO, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ClothWindParam, TEXT(""), FinalClothWPO, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalClothWPO, TEXT(""), MP_WorldPositionOffset);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildObsidianShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Opaque;
	Mat->SetShadingModel(MSM_DefaultLit);
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	// Base Obsidian Color (#09090F)
	UMaterialExpressionVectorParameter* BaseCol = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -400, -100));
	if (BaseCol)
	{
		BaseCol->ParameterName = FName("BaseColor");
		BaseCol->DefaultValue = AetheriaMindColors::ObsidianVoid;
		UMaterialEditingLibrary::ConnectMaterialProperty(BaseCol, TEXT(""), MP_BaseColor);
	}

	// Neutral Specular (#F2F4FA)
	UMaterialExpressionScalarParameter* SpecParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -400, 100));
	if (SpecParam)
	{
		SpecParam->ParameterName = FName("Specular");
		SpecParam->DefaultValue = 0.85f;
		UMaterialEditingLibrary::ConnectMaterialProperty(SpecParam, TEXT(""), MP_Specular);
	}

	// Mirror Polished Roughness
	UMaterialExpressionScalarParameter* RoughParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -400, 200));
	if (RoughParam)
	{
		RoughParam->ParameterName = FName("Roughness");
		RoughParam->DefaultValue = 0.22f;
		UMaterialEditingLibrary::ConnectMaterialProperty(RoughParam, TEXT(""), MP_Roughness);
	}

	// Metallic Specular Accent
	UMaterialExpressionScalarParameter* MetalParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -400, 300));
	if (MetalParam)
	{
		MetalParam->ParameterName = FName("Metallic");
		MetalParam->DefaultValue = 0.15f;
		UMaterialEditingLibrary::ConnectMaterialProperty(MetalParam, TEXT(""), MP_Metallic);
	}

	// Rim Fresnel with Mind Violet Emissive Accent
	UMaterialExpressionFresnel* FresnelExpr = Cast<UMaterialExpressionFresnel>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionFresnel::StaticClass(), -400, 450));
	UMaterialExpressionVectorParameter* AccentColor = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -400, 600));
	UMaterialExpressionMultiply* EmissMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -150, 500));
	if (FresnelExpr && AccentColor && EmissMult)
	{
		AccentColor->ParameterName = FName("AccentColor");
		AccentColor->DefaultValue = AetheriaMindColors::MindViolet * 0.45f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelExpr, TEXT(""), EmissMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(AccentColor, TEXT(""), EmissMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(EmissMult, TEXT(""), MP_EmissiveColor);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildTexturedPBRShaderGraph(
	UPackage* Package,
	const FString& Name,
	UTexture2D* AlbedoTex,
	UTexture2D* NormalTex,
	UTexture2D* RoughTex,
	UTexture2D* EmissiveTex,
	float DefaultUVTiling,
	float DefaultMetallic)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Opaque;
	Mat->SetShadingModel(MSM_DefaultLit);
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	UMaterialExpressionTextureCoordinate* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureCoordinate::StaticClass(), -900, 0));
	if (TexCoord)
	{
		TexCoord->UTiling = DefaultUVTiling;
		TexCoord->VTiling = DefaultUVTiling;
	}

	if (AlbedoTex)
	{
		UMaterialExpressionTextureSample* AlbSample = Cast<UMaterialExpressionTextureSample>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), -600, -250));
		UMaterialExpressionVectorParameter* TintParam = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -600, -50));
		UMaterialExpressionMultiply* AlbMult = Cast<UMaterialExpressionMultiply>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -260, -180));
		if (AlbSample && TintParam && AlbMult)
		{
			AlbSample->Texture = AlbedoTex;
			AlbSample->SamplerType = SAMPLERTYPE_Color;
			TintParam->ParameterName = FName("BaseColor");
			TintParam->DefaultValue = FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
			if (TexCoord)
			{
				UMaterialEditingLibrary::ConnectMaterialExpressions(TexCoord, TEXT(""), AlbSample, TEXT("UVs"));
			}
			UMaterialEditingLibrary::ConnectMaterialExpressions(AlbSample, TEXT("RGB"), AlbMult, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(TintParam, TEXT(""), AlbMult, TEXT("B"));
			UMaterialEditingLibrary::ConnectMaterialProperty(AlbMult, TEXT(""), MP_BaseColor);
		}
	}

	if (NormalTex)
	{
		UMaterialExpressionTextureSample* NrmSample = Cast<UMaterialExpressionTextureSample>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), -600, 120));
		if (NrmSample)
		{
			NrmSample->Texture = NormalTex;
			NrmSample->SamplerType = SAMPLERTYPE_Normal;
			if (TexCoord)
			{
				UMaterialEditingLibrary::ConnectMaterialExpressions(TexCoord, TEXT(""), NrmSample, TEXT("UVs"));
			}
			UMaterialEditingLibrary::ConnectMaterialProperty(NrmSample, TEXT("RGB"), MP_Normal);
		}
	}

	if (RoughTex)
	{
		UMaterialExpressionTextureSample* RghSample = Cast<UMaterialExpressionTextureSample>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), -600, 360));
		if (RghSample)
		{
			RghSample->Texture = RoughTex;
			RghSample->SamplerType = SAMPLERTYPE_LinearColor;
			if (TexCoord)
			{
				UMaterialEditingLibrary::ConnectMaterialExpressions(TexCoord, TEXT(""), RghSample, TEXT("UVs"));
			}
			UMaterialEditingLibrary::ConnectMaterialProperty(RghSample, TEXT("R"), MP_Roughness);
		}
	}

	UMaterialExpressionScalarParameter* MetalParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -300, 320));
	if (MetalParam)
	{
		MetalParam->ParameterName = FName("Metallic");
		MetalParam->DefaultValue = DefaultMetallic;
		UMaterialEditingLibrary::ConnectMaterialProperty(MetalParam, TEXT(""), MP_Metallic);
	}

	UMaterialExpressionScalarParameter* SpecParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -300, 450));
	if (SpecParam)
	{
		SpecParam->ParameterName = FName("Specular");
		SpecParam->DefaultValue = 0.50f;
		UMaterialEditingLibrary::ConnectMaterialProperty(SpecParam, TEXT(""), MP_Specular);
	}

	if (EmissiveTex)
	{
		UMaterialExpressionTextureSample* EmSample = Cast<UMaterialExpressionTextureSample>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), -600, 560));
		UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -600, 740));
		UMaterialExpressionMultiply* EmMult = Cast<UMaterialExpressionMultiply>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -260, 620));
		if (EmSample && BoostParam && EmMult)
		{
			EmSample->Texture = EmissiveTex;
			EmSample->SamplerType = SAMPLERTYPE_Color;
			BoostParam->ParameterName = FName("EmissiveBoost");
			BoostParam->DefaultValue = 3.5f;
			if (TexCoord)
			{
				UMaterialEditingLibrary::ConnectMaterialExpressions(TexCoord, TEXT(""), EmSample, TEXT("UVs"));
			}
			UMaterialEditingLibrary::ConnectMaterialExpressions(EmSample, TEXT("RGB"), EmMult, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), EmMult, TEXT("B"));
			UMaterialEditingLibrary::ConnectMaterialProperty(EmMult, TEXT(""), MP_EmissiveColor);
		}
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildEmissiveCoreShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	UMaterialExpressionVectorParameter* EmissCol = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -400, 0));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -400, 150));
	UMaterialExpressionMultiply* Mult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -150, 50));

	if (EmissCol && BoostParam && Mult)
	{
		EmissCol->ParameterName = FName("EmissiveColor");
		EmissCol->DefaultValue = AetheriaMindColors::MindCyan;
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 8.0f;

		UMaterialEditingLibrary::ConnectMaterialExpressions(EmissCol, TEXT(""), Mult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), Mult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(Mult, TEXT(""), MP_EmissiveColor);
	}

	UMaterialExpressionScalarParameter* OpacityParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -200, 300));
	if (OpacityParam)
	{
		OpacityParam->ParameterName = FName("Opacity");
		OpacityParam->DefaultValue = 1.0f;
		UMaterialEditingLibrary::ConnectMaterialProperty(OpacityParam, TEXT(""), MP_Opacity);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildFireballShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	// Solid obsidian-slag plates floated on blinding white-orange magma fissures + solar Fresnel corona
	Mat->BlendMode = BLEND_Opaque;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	UMaterialExpressionVectorParameter* CoreCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, -180));
	UMaterialExpressionVectorParameter* RimCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, -10));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -700, 140));

	if (CoreCol)
	{
		CoreCol->ParameterName = FName("BaseColor");
		CoreCol->DefaultValue = AetheriaMindColors::IgnisPlasma;
	}
	if (RimCol)
	{
		RimCol->ParameterName = FName("AccentColor");
		RimCol->DefaultValue = AetheriaMindColors::MindViolet;
	}
	if (BoostParam)
	{
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 18.0f;
	}

	// Turbulent 3D churning magma convection cells
	UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(
		CreateExpr(UMaterialExpressionWorldPosition::StaticClass(), -1350, -280));
	UMaterialExpressionTime* TimeExpr = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -1350, -80));
	UMaterialExpressionConstant3Vector* FlowDir = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -1350, 60));
	if (FlowDir) FlowDir->Constant = FLinearColor(65.0f, -52.0f, 44.0f, 0.0f);

	UMaterialExpressionMultiply* TimeFlow = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1150, -20));
	UMaterialExpressionAdd* FlowPos = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -980, -180));
	UMaterialExpressionNoise* MagmaNoise = Cast<UMaterialExpressionNoise>(
		CreateExpr(UMaterialExpressionNoise::StaticClass(), -800, -220));
	if (MagmaNoise)
	{
		MagmaNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
		MagmaNoise->Scale = 0.018f;
		MagmaNoise->Quality = 2;
		MagmaNoise->bTurbulence = true;
		MagmaNoise->Levels = 3;
		MagmaNoise->OutputMin = 0.0f;
		MagmaNoise->OutputMax = 1.0f;
	}

	UMaterialExpressionPower* MagmaVeins = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -600, -320));
	if (MagmaVeins) MagmaVeins->ConstExponent = 1.65f;

	UMaterialExpressionPower* WhiteCore = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -600, -450));
	if (WhiteCore) WhiteCore->ConstExponent = 4.2f;

	UMaterialExpressionConstant3Vector* ObsidianCrustCol = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -600, -580));
	if (ObsidianCrustCol) ObsidianCrustCol->Constant = FLinearColor(0.025f, 0.008f, 0.006f, 1.0f);

	UMaterialExpressionConstant3Vector* WhiteHotCol = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -420, -580));
	if (WhiteHotCol) WhiteHotCol->Constant = FLinearColor(2.4f, 2.15f, 1.65f, 1.0f);

	UMaterialExpressionLinearInterpolate* CrustToMagma = Cast<UMaterialExpressionLinearInterpolate>(
		CreateExpr(UMaterialExpressionLinearInterpolate::StaticClass(), -400, -260));
	UMaterialExpressionMultiply* WhiteCoreCol = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -260, -480));
	UMaterialExpressionAdd* SurfacePlusCore = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -180, -320));

	// Chromatic rim corona
	UMaterialExpressionFresnel* FresnelExpr = Cast<UMaterialExpressionFresnel>(
		CreateExpr(UMaterialExpressionFresnel::StaticClass(), -800, 260));
	if (FresnelExpr)
	{
		FresnelExpr->Exponent = 2.6f;
		FresnelExpr->BaseReflectFraction = 0.04f;
	}
	UMaterialExpressionMultiply* RimCorona = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -360, 220));
	UMaterialExpressionAdd* FullColor = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -40, -120));

	// High-frequency solar flare shimmer (0.85 .. 1.15) * EmissiveBoost
	UMaterialExpressionMultiply* PulseFreq = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1150, 380));
	if (PulseFreq) PulseFreq->ConstB = 7.5f;
	UMaterialExpressionSine* SineExpr = Cast<UMaterialExpressionSine>(
		CreateExpr(UMaterialExpressionSine::StaticClass(), -980, 380));
	UMaterialExpressionMultiply* PulseAmp = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -820, 380));
	if (PulseAmp) PulseAmp->ConstB = 0.16f;
	UMaterialExpressionAdd* PulseBias = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -660, 380));
	if (PulseBias) PulseBias->ConstB = 0.92f;
	UMaterialExpressionMultiply* PulseBoost = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -340, 360));
	UMaterialExpressionMultiply* FinalEmiss = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 140, 40));

	if (WorldPos && TimeExpr && FlowDir && TimeFlow && FlowPos && MagmaNoise &&
		MagmaVeins && WhiteCore && ObsidianCrustCol && WhiteHotCol && CrustToMagma && WhiteCoreCol &&
		SurfacePlusCore && CoreCol && RimCol && BoostParam && FresnelExpr && RimCorona && FullColor &&
		PulseFreq && SineExpr && PulseAmp && PulseBias && PulseBoost && FinalEmiss)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeExpr, TEXT(""), TimeFlow, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(FlowDir, TEXT(""), TimeFlow, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), FlowPos, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeFlow, TEXT(""), FlowPos, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(FlowPos, TEXT(""), MagmaNoise, TEXT("Position"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(MagmaNoise, TEXT(""), MagmaVeins, TEXT("Base"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(MagmaNoise, TEXT(""), WhiteCore, TEXT("Base"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(ObsidianCrustCol, TEXT(""), CrustToMagma, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(CoreCol, TEXT(""), CrustToMagma, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(MagmaVeins, TEXT(""), CrustToMagma, TEXT("Alpha"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(WhiteCore, TEXT(""), WhiteCoreCol, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WhiteHotCol, TEXT(""), WhiteCoreCol, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(CrustToMagma, TEXT(""), SurfacePlusCore, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WhiteCoreCol, TEXT(""), SurfacePlusCore, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RimCol, TEXT(""), RimCorona, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelExpr, TEXT(""), RimCorona, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(SurfacePlusCore, TEXT(""), FullColor, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RimCorona, TEXT(""), FullColor, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeExpr, TEXT(""), PulseFreq, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PulseFreq, TEXT(""), SineExpr, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(SineExpr, TEXT(""), PulseAmp, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PulseAmp, TEXT(""), PulseBias, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PulseBias, TEXT(""), PulseBoost, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), PulseBoost, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(FullColor, TEXT(""), FinalEmiss, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PulseBoost, TEXT(""), FinalEmiss, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalEmiss, TEXT(""), MP_EmissiveColor);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildBlackHoleHorizonShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	// True Schwarzschild pitch-black Event Horizon (Unlit 0,0,0 interior) + blinding Photon Ring & bent redshift halo
	Mat->BlendMode = BLEND_Opaque;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	UMaterialExpressionVectorParameter* HaloCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, -140));
	UMaterialExpressionVectorParameter* PhotonCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, 20));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -700, 180));
	if (HaloCol)
	{
		HaloCol->ParameterName = FName("EmissiveColor");
		HaloCol->DefaultValue = AetheriaMindColors::MindViolet;
	}
	if (PhotonCol)
	{
		PhotonCol->ParameterName = FName("AccentColor");
		PhotonCol->DefaultValue = FLinearColor(0.95f, 0.98f, 1.0f, 1.0f);
	}
	if (BoostParam)
	{
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 20.0f;
	}

	UMaterialExpressionFresnel* HorizonFresnel = Cast<UMaterialExpressionFresnel>(
		CreateExpr(UMaterialExpressionFresnel::StaticClass(), -1050, 0));
	if (HorizonFresnel)
	{
		HorizonFresnel->Exponent = 5.5f;
		HorizonFresnel->BaseReflectFraction = 0.0f;
	}

	UMaterialExpressionPower* OuterRedshiftPow = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -820, -120));
	if (OuterRedshiftPow) OuterRedshiftPow->ConstExponent = 1.6f;

	UMaterialExpressionPower* PhotonRingPow = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -820, 100));
	if (PhotonRingPow) PhotonRingPow->ConstExponent = 4.5f;

	// Swirling warped light shimmer around shadow edge
	UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(
		CreateExpr(UMaterialExpressionWorldPosition::StaticClass(), -1350, -320));
	UMaterialExpressionTime* TimeExpr = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -1350, -180));
	UMaterialExpressionConstant3Vector* WarpDir = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -1350, -40));
	if (WarpDir) WarpDir->Constant = FLinearColor(48.0f, -38.0f, 22.0f, 0.0f);
	UMaterialExpressionMultiply* TimeWarp = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1150, -140));
	UMaterialExpressionAdd* WarpedPos = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -980, -240));
	UMaterialExpressionNoise* ShimmerNoise = Cast<UMaterialExpressionNoise>(
		CreateExpr(UMaterialExpressionNoise::StaticClass(), -800, -300));
	if (ShimmerNoise)
	{
		ShimmerNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
		ShimmerNoise->Scale = 0.011f;
		ShimmerNoise->Quality = 2;
		ShimmerNoise->bTurbulence = true;
		ShimmerNoise->Levels = 2;
		ShimmerNoise->OutputMin = 0.55f;
		ShimmerNoise->OutputMax = 1.45f;
	}

	UMaterialExpressionMultiply* ShimmerMask = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -520, -220));
	UMaterialExpressionMultiply* HaloTerm = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -340, -140));
	UMaterialExpressionMultiply* PhotonScaled = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -520, 120));
	if (PhotonScaled) PhotonScaled->ConstB = 1.85f;
	UMaterialExpressionMultiply* PhotonTerm = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -340, 80));
	UMaterialExpressionAdd* CombinedRing = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -160, -20));
	UMaterialExpressionMultiply* FinalEmiss = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 80, 20));

	if (HaloCol && PhotonCol && BoostParam && HorizonFresnel && OuterRedshiftPow && PhotonRingPow &&
		WorldPos && TimeExpr && WarpDir && TimeWarp && WarpedPos && ShimmerNoise &&
		ShimmerMask && HaloTerm && PhotonScaled && PhotonTerm && CombinedRing && FinalEmiss)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeExpr, TEXT(""), TimeWarp, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WarpDir, TEXT(""), TimeWarp, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), WarpedPos, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeWarp, TEXT(""), WarpedPos, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WarpedPos, TEXT(""), ShimmerNoise, TEXT("Position"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(HorizonFresnel, TEXT(""), OuterRedshiftPow, TEXT("Base"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(HorizonFresnel, TEXT(""), PhotonRingPow, TEXT("Base"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(OuterRedshiftPow, TEXT(""), ShimmerMask, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ShimmerNoise, TEXT(""), ShimmerMask, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(HaloCol, TEXT(""), HaloTerm, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ShimmerMask, TEXT(""), HaloTerm, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(PhotonRingPow, TEXT(""), PhotonScaled, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PhotonCol, TEXT(""), PhotonTerm, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PhotonScaled, TEXT(""), PhotonTerm, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(HaloTerm, TEXT(""), CombinedRing, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PhotonTerm, TEXT(""), CombinedRing, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(CombinedRing, TEXT(""), FinalEmiss, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), FinalEmiss, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalEmiss, TEXT(""), MP_EmissiveColor);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildAccretionDiskShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	// Hollow Relativistic Accretion Disk (0 at center hole & UV square corners) with Doppler beaming & spiral plasma lanes
	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	UMaterialExpressionVectorParameter* DiskCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, -320));
	UMaterialExpressionVectorParameter* OuterCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, -160));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -300, 420));
	if (DiskCol)
	{
		DiskCol->ParameterName = FName("EmissiveColor");
		DiskCol->DefaultValue = AetheriaMindColors::IgnisPlasma;
	}
	if (OuterCol)
	{
		OuterCol->ParameterName = FName("AccentColor");
		OuterCol->DefaultValue = AetheriaMindColors::MindViolet;
	}
	if (BoostParam)
	{
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 22.0f;
	}

	// Hollow circular band in UV space: peak r = 0.31, zero inside r <= 0.145 & outside r >= 0.475
	UMaterialExpressionTextureCoordinate* UV = Cast<UMaterialExpressionTextureCoordinate>(
		CreateExpr(UMaterialExpressionTextureCoordinate::StaticClass(), -1600, 120));
	UMaterialExpressionConstant2Vector* Center = Cast<UMaterialExpressionConstant2Vector>(
		CreateExpr(UMaterialExpressionConstant2Vector::StaticClass(), -1600, 260));
	if (Center) { Center->R = 0.5f; Center->G = 0.5f; }

	UMaterialExpressionDistance* RadialDist = Cast<UMaterialExpressionDistance>(
		CreateExpr(UMaterialExpressionDistance::StaticClass(), -1400, 140));

	UMaterialExpressionSubtract* BandSub = Cast<UMaterialExpressionSubtract>(
		CreateExpr(UMaterialExpressionSubtract::StaticClass(), -1200, 40));
	if (BandSub) BandSub->ConstB = 0.31f;
	UMaterialExpressionAbs* BandAbs = Cast<UMaterialExpressionAbs>(
		CreateExpr(UMaterialExpressionAbs::StaticClass(), -1040, 40));
	UMaterialExpressionDivide* BandDiv = Cast<UMaterialExpressionDivide>(
		CreateExpr(UMaterialExpressionDivide::StaticClass(), -880, 40));
	if (BandDiv) BandDiv->ConstB = 0.165f;
	UMaterialExpressionOneMinus* BandInv = Cast<UMaterialExpressionOneMinus>(
		CreateExpr(UMaterialExpressionOneMinus::StaticClass(), -720, 40));
	UMaterialExpressionClamp* BandClamp = Cast<UMaterialExpressionClamp>(
		CreateExpr(UMaterialExpressionClamp::StaticClass(), -560, 40));
	UMaterialExpressionPower* RingMask = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -400, 40));
	if (RingMask) RingMask->ConstExponent = 1.45f;

	// Blinding white-gold ISCO inner rim at r = 0.21
	UMaterialExpressionSubtract* InnerSub = Cast<UMaterialExpressionSubtract>(
		CreateExpr(UMaterialExpressionSubtract::StaticClass(), -1200, 220));
	if (InnerSub) InnerSub->ConstB = 0.21f;
	UMaterialExpressionAbs* InnerAbs = Cast<UMaterialExpressionAbs>(
		CreateExpr(UMaterialExpressionAbs::StaticClass(), -1040, 220));
	UMaterialExpressionDivide* InnerDiv = Cast<UMaterialExpressionDivide>(
		CreateExpr(UMaterialExpressionDivide::StaticClass(), -880, 220));
	if (InnerDiv) InnerDiv->ConstB = 0.065f;
	UMaterialExpressionOneMinus* InnerInv = Cast<UMaterialExpressionOneMinus>(
		CreateExpr(UMaterialExpressionOneMinus::StaticClass(), -720, 220));
	UMaterialExpressionClamp* InnerClamp = Cast<UMaterialExpressionClamp>(
		CreateExpr(UMaterialExpressionClamp::StaticClass(), -560, 220));
	UMaterialExpressionPower* InnerHot = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -400, 220));
	if (InnerHot) InnerHot->ConstExponent = 2.0f;

	// Concentric density wave ripples traveling inward
	UMaterialExpressionMultiply* WaveFreq = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1200, 420));
	if (WaveFreq) WaveFreq->ConstB = 54.0f;
	UMaterialExpressionTime* TimeExpr = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -1450, 560));
	UMaterialExpressionMultiply* TimeSpeed = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1200, 560));
	if (TimeSpeed) TimeSpeed->ConstB = 6.5f;
	UMaterialExpressionSubtract* WavePhase = Cast<UMaterialExpressionSubtract>(
		CreateExpr(UMaterialExpressionSubtract::StaticClass(), -1000, 460));
	UMaterialExpressionSine* WaveSine = Cast<UMaterialExpressionSine>(
		CreateExpr(UMaterialExpressionSine::StaticClass(), -840, 460));
	UMaterialExpressionAbs* WaveAbs = Cast<UMaterialExpressionAbs>(
		CreateExpr(UMaterialExpressionAbs::StaticClass(), -680, 460));
	UMaterialExpressionMultiply* WaveAmp = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -520, 460));
	if (WaveAmp) WaveAmp->ConstB = 0.42f;
	UMaterialExpressionAdd* WaveMask = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -360, 460));
	if (WaveMask) WaveMask->ConstB = 0.65f;

	// Relativistic turbulent plasma lanes
	UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(
		CreateExpr(UMaterialExpressionWorldPosition::StaticClass(), -1450, -420));
	UMaterialExpressionConstant3Vector* SwirlDrift = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -1450, -280));
	if (SwirlDrift) SwirlDrift->Constant = FLinearColor(95.0f, -75.0f, 30.0f, 0.0f);
	UMaterialExpressionMultiply* DriftMult = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1220, -280));
	UMaterialExpressionAdd* DriftPos = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -1040, -360));
	UMaterialExpressionNoise* PlasmaNoise = Cast<UMaterialExpressionNoise>(
		CreateExpr(UMaterialExpressionNoise::StaticClass(), -860, -400));
	if (PlasmaNoise)
	{
		PlasmaNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
		PlasmaNoise->Scale = 0.0085f;
		PlasmaNoise->Quality = 2;
		PlasmaNoise->bTurbulence = true;
		PlasmaNoise->Levels = 3;
		PlasmaNoise->OutputMin = 0.38f;
		PlasmaNoise->OutputMax = 1.42f;
	}

	// Doppler asymmetric beaming via U axis (0.55 .. 1.45)
	UMaterialExpressionConstant2Vector* UAxis = Cast<UMaterialExpressionConstant2Vector>(
		CreateExpr(UMaterialExpressionConstant2Vector::StaticClass(), -1400, -40));
	if (UAxis) { UAxis->R = 1.0f; UAxis->G = 0.0f; }
	UMaterialExpressionDotProduct* UDot = Cast<UMaterialExpressionDotProduct>(
		CreateExpr(UMaterialExpressionDotProduct::StaticClass(), -1220, -40));
	UMaterialExpressionMultiply* DopplerAmp = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1040, -40));
	if (DopplerAmp) DopplerAmp->ConstB = 0.90f;
	UMaterialExpressionAdd* DopplerBias = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -880, -40));
	if (DopplerBias) DopplerBias->ConstB = 0.55f;

	// Blackbody radial gradient + white-gold ISCO highlight
	UMaterialExpressionLinearInterpolate* BandLerp = Cast<UMaterialExpressionLinearInterpolate>(
		CreateExpr(UMaterialExpressionLinearInterpolate::StaticClass(), -360, -220));
	UMaterialExpressionConstant3Vector* WhiteGoldCol = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -480, -380));
	if (WhiteGoldCol) WhiteGoldCol->Constant = FLinearColor(1.9f, 1.82f, 1.55f, 1.0f);
	UMaterialExpressionMultiply* ISCOGlow = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -220, -100));
	UMaterialExpressionAdd* CombinedColor = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -60, -160));

	UMaterialExpressionMultiply* MaskNoise = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -180, 120));
	UMaterialExpressionMultiply* MaskWave = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -40, 160));
	UMaterialExpressionMultiply* MaskDoppler = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 100, 160));
	UMaterialExpressionMultiply* ColMask = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 220, 20));
	UMaterialExpressionMultiply* FinalEmiss = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 380, 60));

	if (DiskCol && OuterCol && BoostParam && UV && Center && RadialDist &&
		BandSub && BandAbs && BandDiv && BandInv && BandClamp && RingMask &&
		InnerSub && InnerAbs && InnerDiv && InnerInv && InnerClamp && InnerHot &&
		WaveFreq && TimeExpr && TimeSpeed && WavePhase && WaveSine && WaveAbs && WaveAmp && WaveMask &&
		WorldPos && SwirlDrift && DriftMult && DriftPos && PlasmaNoise &&
		UAxis && UDot && DopplerAmp && DopplerBias &&
		BandLerp && WhiteGoldCol && ISCOGlow && CombinedColor &&
		MaskNoise && MaskWave && MaskDoppler && ColMask && FinalEmiss)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), RadialDist, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Center, TEXT(""), RadialDist, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RadialDist, TEXT(""), BandSub, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BandSub, TEXT(""), BandAbs, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BandAbs, TEXT(""), BandDiv, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BandDiv, TEXT(""), BandInv, TEXT(""));
		BandClamp->Input.Connect(0, BandInv);
		UMaterialEditingLibrary::ConnectMaterialExpressions(BandClamp, TEXT(""), RingMask, TEXT("Base"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RadialDist, TEXT(""), InnerSub, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerSub, TEXT(""), InnerAbs, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerAbs, TEXT(""), InnerDiv, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerDiv, TEXT(""), InnerInv, TEXT(""));
		InnerClamp->Input.Connect(0, InnerInv);
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerClamp, TEXT(""), InnerHot, TEXT("Base"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RadialDist, TEXT(""), WaveFreq, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeExpr, TEXT(""), TimeSpeed, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WaveFreq, TEXT(""), WavePhase, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeSpeed, TEXT(""), WavePhase, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WavePhase, TEXT(""), WaveSine, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WaveSine, TEXT(""), WaveAbs, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WaveAbs, TEXT(""), WaveAmp, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WaveAmp, TEXT(""), WaveMask, TEXT("A"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeExpr, TEXT(""), DriftMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(SwirlDrift, TEXT(""), DriftMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), DriftPos, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(DriftMult, TEXT(""), DriftPos, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(DriftPos, TEXT(""), PlasmaNoise, TEXT("Position"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), UDot, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(UAxis, TEXT(""), UDot, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(UDot, TEXT(""), DopplerAmp, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(DopplerAmp, TEXT(""), DopplerBias, TEXT("A"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(OuterCol, TEXT(""), BandLerp, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(DiskCol, TEXT(""), BandLerp, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BandClamp, TEXT(""), BandLerp, TEXT("Alpha"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(WhiteGoldCol, TEXT(""), ISCOGlow, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerHot, TEXT(""), ISCOGlow, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(BandLerp, TEXT(""), CombinedColor, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ISCOGlow, TEXT(""), CombinedColor, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RingMask, TEXT(""), MaskNoise, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PlasmaNoise, TEXT(""), MaskNoise, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(MaskNoise, TEXT(""), MaskWave, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WaveMask, TEXT(""), MaskWave, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(MaskWave, TEXT(""), MaskDoppler, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(DopplerBias, TEXT(""), MaskDoppler, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(CombinedColor, TEXT(""), ColMask, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(MaskDoppler, TEXT(""), ColMask, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(ColMask, TEXT(""), FinalEmiss, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), FinalEmiss, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalEmiss, TEXT(""), MP_EmissiveColor);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildChronosphereDomeShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Translucent;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	// Colors: Deep sapphire void, violet nebula, #00F5FF cyan rim/stars
	UMaterialExpressionVectorParameter* VoidCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, -350));
	UMaterialExpressionVectorParameter* NebulaCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, -180));
	UMaterialExpressionVectorParameter* RimCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -700, 0));
	UMaterialExpressionScalarParameter* OpacityParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -450, 520));

	if (VoidCol)
	{
		VoidCol->ParameterName = FName("Color");
		VoidCol->DefaultValue = FLinearColor(0.025f, 0.06f, 0.22f, 1.0f);
	}
	if (NebulaCol)
	{
		NebulaCol->ParameterName = FName("NebulaColor");
		NebulaCol->DefaultValue = FLinearColor(0.45f, 0.14f, 0.92f, 1.0f);
	}
	if (RimCol)
	{
		RimCol->ParameterName = FName("EmissiveColor");
		RimCol->DefaultValue = FLinearColor(0.18f, 0.96f, 1.0f, 1.0f) * 3.8f;
	}
	if (OpacityParam)
	{
		OpacityParam->ParameterName = FName("Opacity");
		OpacityParam->DefaultValue = 1.0f;
	}

	// Fresnel edge glow
	UMaterialExpressionFresnel* Fresnel = Cast<UMaterialExpressionFresnel>(
		CreateExpr(UMaterialExpressionFresnel::StaticClass(), -950, 120));
	if (Fresnel)
	{
		Fresnel->Exponent = 2.6f;
		Fresnel->BaseReflectFraction = 0.06f;
	}

	// Drifting 3D world-space Nebula + Pinpoint Voronoi Starfield
	UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(
		CreateExpr(UMaterialExpressionWorldPosition::StaticClass(), -1450, -250));
	UMaterialExpressionTime* TimeExpr = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -1450, -50));
	UMaterialExpressionConstant3Vector* DriftDir = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -1450, 80));
	UMaterialExpressionMultiply* TimeDrift = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -1250, -10));
	UMaterialExpressionAdd* DriftingPos = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -1100, -160));
	if (DriftDir) DriftDir->Constant = FLinearColor(28.0f, -19.0f, 14.0f, 0.0f);

	UMaterialExpressionNoise* NebulaNoise = Cast<UMaterialExpressionNoise>(
		CreateExpr(UMaterialExpressionNoise::StaticClass(), -900, -220));
	if (NebulaNoise)
	{
		NebulaNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
		NebulaNoise->Scale = 0.0045f;
		NebulaNoise->Quality = 2;
		NebulaNoise->bTurbulence = true;
		NebulaNoise->Levels = 3;
		NebulaNoise->OutputMin = 0.0f;
		NebulaNoise->OutputMax = 1.0f;
	}

	UMaterialExpressionNoise* StarNoise = Cast<UMaterialExpressionNoise>(
		CreateExpr(UMaterialExpressionNoise::StaticClass(), -900, -400));
	if (StarNoise)
	{
		StarNoise->NoiseFunction = NOISEFUNCTION_VoronoiALU;
		StarNoise->Scale = 0.032f;
		StarNoise->Quality = 1;
		StarNoise->bTurbulence = false;
		StarNoise->Levels = 1;
		StarNoise->OutputMin = 0.0f;
		StarNoise->OutputMax = 1.0f;
	}

	UMaterialExpressionOneMinus* StarInv = Cast<UMaterialExpressionOneMinus>(
		CreateExpr(UMaterialExpressionOneMinus::StaticClass(), -700, -400));
	UMaterialExpressionPower* StarPinpoint = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -520, -400));
	UMaterialExpressionMultiply* StarBoost = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -340, -400));
	if (StarPinpoint) StarPinpoint->ConstExponent = 12.0f;
	if (StarBoost) StarBoost->ConstB = 5.5f;

	// Geodesic lattice meridian grid
	UMaterialExpressionTextureCoordinate* UV = Cast<UMaterialExpressionTextureCoordinate>(
		CreateExpr(UMaterialExpressionTextureCoordinate::StaticClass(), -1450, 340));
	UMaterialExpressionPanner* GridPan = Cast<UMaterialExpressionPanner>(
		CreateExpr(UMaterialExpressionPanner::StaticClass(), -1250, 340));
	if (GridPan)
	{
		GridPan->SpeedX = 0.025f;
		GridPan->SpeedY = -0.014f;
	}
	UMaterialExpressionConstant2Vector* MeridianFreq = Cast<UMaterialExpressionConstant2Vector>(
		CreateExpr(UMaterialExpressionConstant2Vector::StaticClass(), -1250, 480));
	if (MeridianFreq) { MeridianFreq->R = 14.0f; MeridianFreq->G = 14.0f; }

	UMaterialExpressionDotProduct* GridDot = Cast<UMaterialExpressionDotProduct>(
		CreateExpr(UMaterialExpressionDotProduct::StaticClass(), -1050, 380));
	UMaterialExpressionSine* GridSine = Cast<UMaterialExpressionSine>(
		CreateExpr(UMaterialExpressionSine::StaticClass(), -880, 380));
	UMaterialExpressionAbs* GridAbs = Cast<UMaterialExpressionAbs>(
		CreateExpr(UMaterialExpressionAbs::StaticClass(), -720, 380));
	UMaterialExpressionPower* GridLines = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -560, 380));
	if (GridLines) GridLines->ConstExponent = 16.0f;

	// Combine Void -> Nebula -> Stars + Fresnel Rim + Geodesic Grid
	UMaterialExpressionLinearInterpolate* VoidNebulaLerp = Cast<UMaterialExpressionLinearInterpolate>(
		CreateExpr(UMaterialExpressionLinearInterpolate::StaticClass(), -420, -220));
	UMaterialExpressionAdd* RimPlusGrid = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -380, 220));
	UMaterialExpressionMultiply* GridWeighted = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -480, 280));
	if (GridWeighted) GridWeighted->ConstB = 0.45f;

	UMaterialExpressionAdd* EnergyMask = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -220, 40));
	UMaterialExpressionMultiply* EmissiveTint = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -80, -40));
	UMaterialExpressionAdd* FinalEmissive = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), 100, -120));

	// Translucent Opacity: airy cosmic refraction interior + crisp rim & celestial lines
	UMaterialExpressionMultiply* RimOpacity = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -340, 120));
	UMaterialExpressionAdd* OpacitySum1 = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -180, 180));
	UMaterialExpressionAdd* OpacitySum2 = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -40, 240));
	UMaterialExpressionClamp* OpacityClamp = Cast<UMaterialExpressionClamp>(
		CreateExpr(UMaterialExpressionClamp::StaticClass(), 110, 240));
	UMaterialExpressionMultiply* FinalOpacity = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 260, 280));
	if (RimOpacity) RimOpacity->ConstB = 0.52f;
	if (OpacitySum1) OpacitySum1->ConstB = 0.03f;
	if (OpacityClamp) { OpacityClamp->MinDefault = 0.03f; OpacityClamp->MaxDefault = 0.58f; }

	if (WorldPos && TimeExpr && DriftDir && TimeDrift && DriftingPos && NebulaNoise && StarNoise &&
		StarInv && StarPinpoint && StarBoost && UV && GridPan && MeridianFreq && GridDot && GridSine &&
		GridAbs && GridLines && VoidCol && NebulaCol && RimCol && OpacityParam && Fresnel &&
		VoidNebulaLerp && GridWeighted && RimPlusGrid && EnergyMask && EmissiveTint && FinalEmissive &&
		RimOpacity && OpacitySum1 && OpacitySum2 && OpacityClamp && FinalOpacity)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeExpr, TEXT(""), TimeDrift, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(DriftDir, TEXT(""), TimeDrift, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), DriftingPos, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeDrift, TEXT(""), DriftingPos, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(DriftingPos, TEXT(""), NebulaNoise, TEXT("Position"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), StarNoise, TEXT("Position"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(StarNoise, TEXT(""), StarInv, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(StarInv, TEXT(""), StarPinpoint, TEXT("Base"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(StarPinpoint, TEXT(""), StarBoost, TEXT("A"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), GridPan, TEXT("Coordinate"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GridPan, TEXT(""), GridDot, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(MeridianFreq, TEXT(""), GridDot, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GridDot, TEXT(""), GridSine, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GridSine, TEXT(""), GridAbs, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GridAbs, TEXT(""), GridLines, TEXT("Base"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GridLines, TEXT(""), GridWeighted, TEXT("A"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(VoidCol, TEXT(""), VoidNebulaLerp, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(NebulaCol, TEXT(""), VoidNebulaLerp, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(NebulaNoise, TEXT(""), VoidNebulaLerp, TEXT("Alpha"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(Fresnel, TEXT(""), RimPlusGrid, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GridWeighted, TEXT(""), RimPlusGrid, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RimPlusGrid, TEXT(""), EnergyMask, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(StarBoost, TEXT(""), EnergyMask, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RimCol, TEXT(""), EmissiveTint, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(EnergyMask, TEXT(""), EmissiveTint, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(VoidNebulaLerp, TEXT(""), FinalEmissive, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(EmissiveTint, TEXT(""), FinalEmissive, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalEmissive, TEXT(""), MP_EmissiveColor);

		UMaterialEditingLibrary::ConnectMaterialExpressions(Fresnel, TEXT(""), RimOpacity, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RimOpacity, TEXT(""), OpacitySum1, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GridWeighted, TEXT(""), OpacitySum2, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OpacitySum1, TEXT(""), OpacitySum2, TEXT("B"));
		OpacityClamp->Input.Connect(0, OpacitySum2);
		UMaterialEditingLibrary::ConnectMaterialExpressions(OpacityClamp, TEXT(""), FinalOpacity, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OpacityParam, TEXT(""), FinalOpacity, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalOpacity, TEXT(""), MP_Opacity);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildSolarBeamShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	UMaterialExpressionVectorParameter* BeamCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -520, -80));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -520, 80));

	// High-velocity surging plasma filaments along beam
	UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(
		CreateExpr(UMaterialExpressionWorldPosition::StaticClass(), -1150, -180));
	UMaterialExpressionTime* TimeExpr = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -1150, 0));
	UMaterialExpressionConstant3Vector* SurgeDir = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -1150, 140));
	if (SurgeDir) SurgeDir->Constant = FLinearColor(260.0f, -180.0f, 120.0f, 0.0f);
	UMaterialExpressionMultiply* SurgeFlow = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -950, 40));
	UMaterialExpressionAdd* SurgePos = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -780, -80));
	UMaterialExpressionNoise* PlasmaNoise = Cast<UMaterialExpressionNoise>(
		CreateExpr(UMaterialExpressionNoise::StaticClass(), -600, -220));
	if (PlasmaNoise)
	{
		PlasmaNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
		PlasmaNoise->Scale = 0.015f;
		PlasmaNoise->Quality = 2;
		PlasmaNoise->bTurbulence = true;
		PlasmaNoise->Levels = 2;
		PlasmaNoise->OutputMin = 0.52f;
		PlasmaNoise->OutputMax = 1.48f;
	}

	UMaterialExpressionMultiply* ColBoost = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -260, 0));
	UMaterialExpressionMultiply* FinalEmiss = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -60, 0));

	if (BeamCol && BoostParam && WorldPos && TimeExpr && SurgeDir && SurgeFlow && SurgePos && PlasmaNoise && ColBoost && FinalEmiss)
	{
		BeamCol->ParameterName = FName("EmissiveColor");
		BeamCol->DefaultValue = FLinearColor(1.0f, 0.94f, 0.78f, 1.0f);
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 24.0f;

		UMaterialEditingLibrary::ConnectMaterialExpressions(TimeExpr, TEXT(""), SurgeFlow, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(SurgeDir, TEXT(""), SurgeFlow, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), SurgePos, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(SurgeFlow, TEXT(""), SurgePos, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(SurgePos, TEXT(""), PlasmaNoise, TEXT("Position"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(BeamCol, TEXT(""), ColBoost, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), ColBoost, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(ColBoost, TEXT(""), FinalEmiss, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(PlasmaNoise, TEXT(""), FinalEmiss, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalEmiss, TEXT(""), MP_EmissiveColor);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildPrismWallShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Translucent;
	Mat->SetShadingModel(MSM_DefaultLit);
	Mat->TwoSided = true;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	UMaterialExpressionVectorParameter* BaseCol = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -400, -100));
	if (BaseCol)
	{
		BaseCol->ParameterName = FName("BaseColor");
		BaseCol->DefaultValue = AetheriaMindColors::MindCyan;
		UMaterialEditingLibrary::ConnectMaterialProperty(BaseCol, TEXT(""), MP_BaseColor);
	}

	UMaterialExpressionScalarParameter* Rough = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -400, 100));
	if (Rough)
	{
		Rough->ParameterName = FName("Roughness");
		Rough->DefaultValue = 0.08f;
		UMaterialEditingLibrary::ConnectMaterialProperty(Rough, TEXT(""), MP_Roughness);
	}

	UMaterialExpressionFresnel* FresnelExpr = Cast<UMaterialExpressionFresnel>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionFresnel::StaticClass(), -500, 250));
	UMaterialExpressionScalarParameter* OpacityParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -500, 400));
	UMaterialExpressionMultiply* OpacityMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -200, 300));
	if (FresnelExpr && OpacityParam && OpacityMult)
	{
		OpacityParam->ParameterName = FName("Opacity");
		OpacityParam->DefaultValue = 0.65f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelExpr, TEXT(""), OpacityMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OpacityParam, TEXT(""), OpacityMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(OpacityMult, TEXT(""), MP_Opacity);
	}

	UMaterialExpressionVectorParameter* RimCol = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -400, 550));
	UMaterialExpressionMultiply* EmissMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -150, 480));
	if (RimCol && FresnelExpr && EmissMult)
	{
		RimCol->ParameterName = FName("EmissiveColor");
		RimCol->DefaultValue = AetheriaMindColors::MindViolet * 3.5f;
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelExpr, TEXT(""), EmissMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RimCol, TEXT(""), EmissMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(EmissMult, TEXT(""), MP_EmissiveColor);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildArcaneRingShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Translucent;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	UMaterialExpressionVectorParameter* RingCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -300, -180));
	UMaterialExpressionScalarParameter* OpacityParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -300, 320));
	UMaterialExpressionScalarParameter* GlowParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -300, -40));
	if (RingCol)
	{
		RingCol->ParameterName = FName("Color");
		RingCol->DefaultValue = FLinearColor(0.22f, 0.92f, 1.0f, 1.0f);
	}
	if (GlowParam)
	{
		GlowParam->ParameterName = FName("EmissiveBoost");
		GlowParam->DefaultValue = 3.6f;
	}
	if (OpacityParam)
	{
		OpacityParam->ParameterName = FName("Opacity");
		OpacityParam->DefaultValue = 0.90f;
	}

	// Hollow concentric arcane rings inside unit UV circle (0 at edges -> zero rectangle artifacts)
	UMaterialExpressionTextureCoordinate* UV = Cast<UMaterialExpressionTextureCoordinate>(
		CreateExpr(UMaterialExpressionTextureCoordinate::StaticClass(), -1450, 80));
	UMaterialExpressionConstant2Vector* Center = Cast<UMaterialExpressionConstant2Vector>(
		CreateExpr(UMaterialExpressionConstant2Vector::StaticClass(), -1450, 220));
	if (Center) { Center->R = 0.5f; Center->G = 0.5f; }

	UMaterialExpressionDistance* RadialDist = Cast<UMaterialExpressionDistance>(
		CreateExpr(UMaterialExpressionDistance::StaticClass(), -1250, 120));

	// Outer Crisp Ring at r = 0.43, half-width 0.042
	UMaterialExpressionSubtract* OuterSub = Cast<UMaterialExpressionSubtract>(
		CreateExpr(UMaterialExpressionSubtract::StaticClass(), -1050, 0));
	if (OuterSub) OuterSub->ConstB = 0.43f;
	UMaterialExpressionAbs* OuterAbs = Cast<UMaterialExpressionAbs>(
		CreateExpr(UMaterialExpressionAbs::StaticClass(), -900, 0));
	UMaterialExpressionDivide* OuterDiv = Cast<UMaterialExpressionDivide>(
		CreateExpr(UMaterialExpressionDivide::StaticClass(), -760, 0));
	if (OuterDiv) OuterDiv->ConstB = 0.042f;
	UMaterialExpressionOneMinus* OuterInv = Cast<UMaterialExpressionOneMinus>(
		CreateExpr(UMaterialExpressionOneMinus::StaticClass(), -620, 0));
	UMaterialExpressionClamp* OuterClamp = Cast<UMaterialExpressionClamp>(
		CreateExpr(UMaterialExpressionClamp::StaticClass(), -480, 0));
	UMaterialExpressionPower* OuterRing = Cast<UMaterialExpressionPower>(
		CreateExpr(UMaterialExpressionPower::StaticClass(), -340, 0));
	if (OuterRing) OuterRing->ConstExponent = 1.6f;

	// Inner Fine Seal Ring at r = 0.34, half-width 0.020
	UMaterialExpressionSubtract* InnerSub = Cast<UMaterialExpressionSubtract>(
		CreateExpr(UMaterialExpressionSubtract::StaticClass(), -1050, 180));
	if (InnerSub) InnerSub->ConstB = 0.34f;
	UMaterialExpressionAbs* InnerAbs = Cast<UMaterialExpressionAbs>(
		CreateExpr(UMaterialExpressionAbs::StaticClass(), -900, 180));
	UMaterialExpressionDivide* InnerDiv = Cast<UMaterialExpressionDivide>(
		CreateExpr(UMaterialExpressionDivide::StaticClass(), -760, 180));
	if (InnerDiv) InnerDiv->ConstB = 0.020f;
	UMaterialExpressionOneMinus* InnerInv = Cast<UMaterialExpressionOneMinus>(
		CreateExpr(UMaterialExpressionOneMinus::StaticClass(), -620, 180));
	UMaterialExpressionClamp* InnerClamp = Cast<UMaterialExpressionClamp>(
		CreateExpr(UMaterialExpressionClamp::StaticClass(), -480, 180));
	UMaterialExpressionMultiply* InnerWeighted = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -340, 180));
	if (InnerWeighted) InnerWeighted->ConstB = 0.55f;

	// Soft circular inner fill inside r < 0.43 (whisper-thin dark-fantasy seal veil)
	UMaterialExpressionSubtract* FillSub = Cast<UMaterialExpressionSubtract>(
		CreateExpr(UMaterialExpressionSubtract::StaticClass(), -900, 340));
	if (FillSub) FillSub->ConstA = 0.44f;
	UMaterialExpressionClamp* FillClamp = Cast<UMaterialExpressionClamp>(
		CreateExpr(UMaterialExpressionClamp::StaticClass(), -720, 340));
	UMaterialExpressionMultiply* FillSoft = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -540, 340));
	if (FillSoft) FillSoft->ConstB = 0.06f;

	// Animated eldritch flame shimmer across the ring contour
	UMaterialExpressionWorldPosition* RingWorldPos = Cast<UMaterialExpressionWorldPosition>(
		CreateExpr(UMaterialExpressionWorldPosition::StaticClass(), -720, -260));
	UMaterialExpressionTime* RingTime = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -720, -140));
	UMaterialExpressionConstant3Vector* RingFlow = Cast<UMaterialExpressionConstant3Vector>(
		CreateExpr(UMaterialExpressionConstant3Vector::StaticClass(), -720, -20));
	if (RingFlow) RingFlow->Constant = FLinearColor(55.0f, -45.0f, 35.0f, 0.0f);
	UMaterialExpressionMultiply* RingFlowMult = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), -540, -100));
	UMaterialExpressionAdd* RingFlowPos = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -380, -200));
	UMaterialExpressionNoise* EldritchShimmer = Cast<UMaterialExpressionNoise>(
		CreateExpr(UMaterialExpressionNoise::StaticClass(), -220, -220));
	if (EldritchShimmer)
	{
		EldritchShimmer->NoiseFunction = NOISEFUNCTION_SimplexTex;
		EldritchShimmer->Scale = 0.016f;
		EldritchShimmer->Quality = 1;
		EldritchShimmer->bTurbulence = true;
		EldritchShimmer->Levels = 2;
		EldritchShimmer->OutputMin = 0.58f;
		EldritchShimmer->OutputMax = 1.42f;
	}

	UMaterialExpressionAdd* RingsAdd = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -180, 90));
	UMaterialExpressionAdd* MaskAdd = Cast<UMaterialExpressionAdd>(
		CreateExpr(UMaterialExpressionAdd::StaticClass(), -40, 160));
	UMaterialExpressionClamp* MaskClamp = Cast<UMaterialExpressionClamp>(
		CreateExpr(UMaterialExpressionClamp::StaticClass(), 100, 160));
	UMaterialExpressionMultiply* ShimmerMask = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 180, 40));

	UMaterialExpressionMultiply* EmissiveBoost = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 120, -100));
	UMaterialExpressionMultiply* EmissiveCol = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 320, -60));
	UMaterialExpressionMultiply* FinalOpacity = Cast<UMaterialExpressionMultiply>(
		CreateExpr(UMaterialExpressionMultiply::StaticClass(), 260, 220));

	if (UV && Center && RadialDist && OuterSub && OuterAbs && OuterDiv && OuterInv && OuterClamp && OuterRing &&
		InnerSub && InnerAbs && InnerDiv && InnerInv && InnerClamp && InnerWeighted &&
		FillSub && FillClamp && FillSoft && RingsAdd && MaskAdd && MaskClamp &&
		RingWorldPos && RingTime && RingFlow && RingFlowMult && RingFlowPos && EldritchShimmer && ShimmerMask &&
		RingCol && GlowParam && OpacityParam && EmissiveBoost && EmissiveCol && FinalOpacity)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(UV, TEXT(""), RadialDist, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(Center, TEXT(""), RadialDist, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RadialDist, TEXT(""), OuterSub, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OuterSub, TEXT(""), OuterAbs, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OuterAbs, TEXT(""), OuterDiv, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OuterDiv, TEXT(""), OuterInv, TEXT(""));
		OuterClamp->Input.Connect(0, OuterInv);
		UMaterialEditingLibrary::ConnectMaterialExpressions(OuterClamp, TEXT(""), OuterRing, TEXT("Base"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RadialDist, TEXT(""), InnerSub, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerSub, TEXT(""), InnerAbs, TEXT(""));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerAbs, TEXT(""), InnerDiv, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerDiv, TEXT(""), InnerInv, TEXT(""));
		InnerClamp->Input.Connect(0, InnerInv);
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerClamp, TEXT(""), InnerWeighted, TEXT("A"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RadialDist, TEXT(""), FillSub, TEXT("B"));
		FillClamp->Input.Connect(0, FillSub);
		UMaterialEditingLibrary::ConnectMaterialExpressions(FillClamp, TEXT(""), FillSoft, TEXT("A"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(OuterRing, TEXT(""), RingsAdd, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(InnerWeighted, TEXT(""), RingsAdd, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RingsAdd, TEXT(""), MaskAdd, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(FillSoft, TEXT(""), MaskAdd, TEXT("B"));
		MaskClamp->Input.Connect(0, MaskAdd);

		UMaterialEditingLibrary::ConnectMaterialExpressions(RingTime, TEXT(""), RingFlowMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RingFlow, TEXT(""), RingFlowMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RingWorldPos, TEXT(""), RingFlowPos, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RingFlowMult, TEXT(""), RingFlowPos, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RingFlowPos, TEXT(""), EldritchShimmer, TEXT("Position"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(MaskClamp, TEXT(""), ShimmerMask, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(EldritchShimmer, TEXT(""), ShimmerMask, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RingCol, TEXT(""), EmissiveBoost, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(GlowParam, TEXT(""), EmissiveBoost, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(EmissiveBoost, TEXT(""), EmissiveCol, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(ShimmerMask, TEXT(""), EmissiveCol, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(EmissiveCol, TEXT(""), MP_EmissiveColor);

		UMaterialEditingLibrary::ConnectMaterialExpressions(MaskClamp, TEXT(""), FinalOpacity, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(OpacityParam, TEXT(""), FinalOpacity, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalOpacity, TEXT(""), MP_Opacity);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

#endif // WITH_EDITOR

// ------------------------------------------------------------------------------------------------
// Shader Bake Commandlet
// ------------------------------------------------------------------------------------------------

UAetheriaShaderBakeCommandlet::UAetheriaShaderBakeCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
}

int32 UAetheriaShaderBakeCommandlet::Main(const FString& Params)
{
#if WITH_EDITOR
	UE_LOG(LogTemp, Display, TEXT("UAetheriaShaderBakeCommandlet: Importing RawAssets (PNG, FBX, WAV) and baking PBR shaders..."));

	IFileManager::Get().MakeDirectory(*(FPaths::ProjectContentDir() / TEXT("Materials")), true);
	IFileManager::Get().MakeDirectory(*(FPaths::ProjectContentDir() / TEXT("Meshes")), true);
	IFileManager::Get().MakeDirectory(*(FPaths::ProjectContentDir() / TEXT("Textures")), true);
	IFileManager::Get().MakeDirectory(*(FPaths::ProjectContentDir() / TEXT("Audio")), true);

	IAssetTools& AssetTools = FAssetToolsModule::GetModule().Get();

	auto BatchImportDir = [&](const FString& RelDir, const FString& ExtPattern, const FString& DestGamePath, bool bEnableNanite, bool bMarkNormalIfSuffix)
	{
		const FString FullDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / RelDir);
		TArray<FString> FoundFiles;
		IFileManager::Get().FindFiles(FoundFiles, *(FullDir / ExtPattern), true, false);
		if (FoundFiles.Num() == 0) return;

		TArray<FString> FullPaths;
		for (const FString& F : FoundFiles)
		{
			FullPaths.Add(FullDir / F);
		}

		UAutomatedAssetImportData* ImportData = NewObject<UAutomatedAssetImportData>();
		ImportData->bReplaceExisting = true;
		ImportData->bSkipReadOnly = true;
		ImportData->DestinationPath = DestGamePath;
		ImportData->Filenames = FullPaths;

		TArray<UObject*> Imported = AssetTools.ImportAssetsAutomated(ImportData);
		for (UObject* Obj : Imported)
		{
			if (!Obj) continue;
			if (UTexture2D* Tex = Cast<UTexture2D>(Obj))
			{
				if (RelDir.Contains(TEXT("UI")) || RelDir.Contains(TEXT("Grimoire")) || RelDir.Contains(TEXT("Design")))
				{
					Tex->LODGroup = TEXTUREGROUP_UI;
					Tex->MipGenSettings = TMGS_NoMipmaps;
					Tex->CompressionSettings = TC_EditorIcon;
					Tex->NeverStream = true;
					Tex->SRGB = true;
					Tex->PostEditChange();
				}
				else if (bMarkNormalIfSuffix)
				{
					if (Tex->GetName().Contains(TEXT("_Normal")))
					{
						Tex->CompressionSettings = TC_Normalmap;
						Tex->SRGB = false;
						Tex->PostEditChange();
					}
					else if (Tex->GetName().Contains(TEXT("_Roughness")))
					{
						Tex->SRGB = false;
						Tex->PostEditChange();
					}
				}
			}
			if (bEnableNanite)
			{
				if (UStaticMesh* SM = Cast<UStaticMesh>(Obj))
				{
					const FString MeshName = SM->GetName();
					const bool bIsDynamicEntity = MeshName.Contains(TEXT("SM_Hero_")) ||
						MeshName.Contains(TEXT("SM_Boss_")) ||
						MeshName.Contains(TEXT("SM_Mob_")) ||
						MeshName.Contains(TEXT("SM_Head_")) ||
						MeshName.Contains(TEXT("SM_Staff_")) ||
						MeshName.Contains(TEXT("SM_Familiar_")) ||
						MeshName.Contains(TEXT("SM_Back_")) ||
						MeshName.Contains(TEXT("Arrow")) ||
						MeshName.Contains(TEXT("Archmage")) ||
						MeshName.Contains(TEXT("VoidFiend")) ||
						MeshName.Contains(TEXT("Colossus")) ||
						MeshName.Contains(TEXT("ArchonBoss"));

					if (!bIsDynamicEntity)
					{
						SM->NaniteSettings.bEnabled = true;
						SM->PostEditChange();
					}
				}
			}
			UPackage* Pkg = Obj->GetOutermost();
			if (Pkg)
			{
				Pkg->MarkPackageDirty();
				FString PkgFilename = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());
				IFileManager::Get().MakeDirectory(*FPaths::GetPath(PkgFilename), true);
				FSavePackageArgs SaveArgs;
				SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
				SaveArgs.SaveFlags = SAVE_NoError;
				UPackage::SavePackage(Pkg, Obj, *PkgFilename, SaveArgs);
			}
		}
	};

	// 1. Import 2K Textures, Grimoire Illustrations, Volumetric UI Kit, Blender FBX Meshes & WAV Audio
	BatchImportDir(TEXT("RawAssets/Textures"), TEXT("*.png"), TEXT("/Game/Textures"), false, true);
	BatchImportDir(TEXT("RawAssets/Grimoire"), TEXT("*.png"), TEXT("/Game/Textures"), false, false);
	BatchImportDir(TEXT("RawAssets/UI"), TEXT("*.png"), TEXT("/Game/Textures"), false, false);
	BatchImportDir(TEXT("RawAssets/Meshes"), TEXT("*.fbx"), TEXT("/Game/Meshes"), true, false);
	BatchImportDir(TEXT("RawAssets/Audio"), TEXT("*.wav"), TEXT("/Game/Audio"), false, false);

	struct FShaderSpec
	{
		FString AssetName;
		TFunction<UMaterial*(UPackage*, const FString&)> Builder;
	};

	TArray<FShaderSpec> Specs = {
		{ TEXT("M_AetheriaMasterPBR"), &UAetheriaMaterialLibrary::BuildMasterCharacterPBRShaderGraph },
		{ TEXT("M_AetheriaObsidian"), &UAetheriaMaterialLibrary::BuildObsidianShaderGraph },
		{ TEXT("M_AetheriaEmissiveCore"), &UAetheriaMaterialLibrary::BuildEmissiveCoreShaderGraph },
		{ TEXT("M_AetheriaFireball"), &UAetheriaMaterialLibrary::BuildFireballShaderGraph },
		{ TEXT("M_AetheriaBlackHole"), &UAetheriaMaterialLibrary::BuildBlackHoleHorizonShaderGraph },
		{ TEXT("M_AetheriaAccretionDisk"), &UAetheriaMaterialLibrary::BuildAccretionDiskShaderGraph },
		{ TEXT("M_AetheriaChronosphere"), &UAetheriaMaterialLibrary::BuildChronosphereDomeShaderGraph },
		{ TEXT("M_AetheriaSolarBeam"), &UAetheriaMaterialLibrary::BuildSolarBeamShaderGraph },
		{ TEXT("M_AetheriaPrismWall"), &UAetheriaMaterialLibrary::BuildPrismWallShaderGraph },
		{ TEXT("M_AetheriaArcaneRing"), &UAetheriaMaterialLibrary::BuildArcaneRingShaderGraph },
	};

	for (const FShaderSpec& Spec : Specs)
	{
		const FString PackagePath = FString::Printf(TEXT("/Game/Materials/%s"), *Spec.AssetName);
		UPackage* Package = CreatePackage(*PackagePath);
		Package->FullyLoad();

		UMaterial* Mat = Spec.Builder(Package, Spec.AssetName);
		if (Mat)
		{
			Mat->PostEditChange();
			Package->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(Mat);

			FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFileName), true);
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			UPackage::SavePackage(Package, Mat, *PackageFileName, SaveArgs);
		}
	}

	// 2. Bake Real 2K Textured PBR Materials
	auto BakeTextured = [&](const FString& MatName, UTexture2D* Alb, UTexture2D* Nrm, UTexture2D* Rgh, UTexture2D* Ems, float Tiling, float Metal)
	{
		if (!Alb) return;
		const FString PackagePath = FString::Printf(TEXT("/Game/Materials/%s"), *MatName);
		UPackage* Package = CreatePackage(*PackagePath);
		Package->FullyLoad();
		if (UMaterial* Mat = UAetheriaMaterialLibrary::BuildTexturedPBRShaderGraph(Package, MatName, Alb, Nrm, Rgh, Ems, Tiling, Metal))
		{
			Mat->PostEditChange();
			Package->MarkPackageDirty();
			FAssetRegistryModule::AssetCreated(Mat);
			FString PackageFileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(PackageFileName), true);
			FSavePackageArgs SaveArgs;
			SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
			SaveArgs.SaveFlags = SAVE_NoError;
			UPackage::SavePackage(Package, Mat, *PackageFileName, SaveArgs);
		}
	};

	BakeTextured(TEXT("M_AetheriaSanctumFloor"),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_SanctumFlagstone_Albedo"), TEXT("RawAssets/Textures/T_SanctumFlagstone_Albedo.png"), false),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_SanctumFlagstone_Normal"), TEXT("RawAssets/Textures/T_SanctumFlagstone_Normal.png"), true),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_SanctumFlagstone_Roughness"), TEXT("RawAssets/Textures/T_SanctumFlagstone_Roughness.png"), false),
		nullptr, 22.0f, 0.08f);

	BakeTextured(TEXT("M_AetheriaCalderaFloor"),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_CalderaBasalt_Albedo"), TEXT("RawAssets/Textures/T_CalderaBasalt_Albedo.png"), false),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_CalderaBasalt_Normal"), TEXT("RawAssets/Textures/T_CalderaBasalt_Normal.png"), true),
		nullptr,
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_CalderaBasalt_Emissive"), TEXT("RawAssets/Textures/T_CalderaBasalt_Emissive.png"), false),
		20.0f, 0.06f);

	BakeTextured(TEXT("M_AetheriaGlacierFloor"),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_GlacierSlate_Albedo"), TEXT("RawAssets/Textures/T_GlacierSlate_Albedo.png"), false),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_GlacierSlate_Normal"), TEXT("RawAssets/Textures/T_GlacierSlate_Normal.png"), true),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_GlacierSlate_Roughness"), TEXT("RawAssets/Textures/T_GlacierSlate_Roughness.png"), false),
		nullptr, 22.0f, 0.05f);

	BakeTextured(TEXT("M_AetheriaGothicStone"),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_GothicStone_Albedo"), TEXT("RawAssets/Textures/T_GothicStone_Albedo.png"), false),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_GothicStone_Normal"), TEXT("RawAssets/Textures/T_GothicStone_Normal.png"), true),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_GothicStone_Roughness"), TEXT("RawAssets/Textures/T_GothicStone_Roughness.png"), false),
		nullptr, 2.0f, 0.04f);

	BakeTextured(TEXT("M_AetheriaDarkIron"),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_DarkIronCloth_Albedo"), TEXT("RawAssets/Textures/T_DarkIronCloth_Albedo.png"), false),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_DarkIronCloth_Normal"), TEXT("RawAssets/Textures/T_DarkIronCloth_Normal.png"), true),
		UAetheriaMaterialLibrary::LoadOrImportTexture(TEXT("T_DarkIronCloth_Roughness"), TEXT("RawAssets/Textures/T_DarkIronCloth_Roughness.png"), false),
		nullptr, 1.5f, 0.35f);

	UE_LOG(LogTemp, Display, TEXT("UAetheriaShaderBakeCommandlet: Full PBR/FBX/Audio bake complete."));
	return 0;
#else
	return 0;
#endif
}


#include "Engine/SkeletalMesh.h"
#include "AssetRegistry/AssetRegistryModule.h"

USkeletalMesh* UAetheriaMaterialLibrary::LoadSkeletalMeshFromFolder(const FString& Folder, const FString& PreferredName)
{
	static TMap<FString, TWeakObjectPtr<USkeletalMesh>> Cache;
	const FString CacheKey = Folder + TEXT("|") + PreferredName;
	if (const TWeakObjectPtr<USkeletalMesh>* Found = Cache.Find(CacheKey))
	{
		if (Found->IsValid())
		{
			return Found->Get();
		}
	}

	auto TryLoad = [](const FString& Path) -> USkeletalMesh*
	{
		if (FPackageName::DoesPackageExist(Path))
		{
			const FString ObjectPath = Path + TEXT(".") + FPackageName::GetShortName(Path);
			return LoadObject<USkeletalMesh>(nullptr, *ObjectPath);
		}
		return nullptr;
	};

	TArray<FString> Candidates;
	if (!PreferredName.IsEmpty())
	{
		Candidates.Add(Folder / PreferredName);
		Candidates.Add(Folder / FString::Printf(TEXT("SKM_%s"), *PreferredName));
		Candidates.Add(FString::Printf(TEXT("/Game/Meshes/%s"), *PreferredName));
	}
	for (const FString& Path : Candidates)
	{
		if (USkeletalMesh* Mesh = TryLoad(Path))
		{
			Cache.Add(CacheKey, Mesh);
			return Mesh;
		}
	}

	FAssetRegistryModule& ARM = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> Assets;
	ARM.Get().GetAssetsByPath(FName(*Folder), Assets, true);
	USkeletalMesh* First = nullptr;
	for (const FAssetData& Data : Assets)
	{
		if (Data.GetClass() && Data.GetClass()->IsChildOf(USkeletalMesh::StaticClass()))
		{
			if (USkeletalMesh* Mesh = Cast<USkeletalMesh>(Data.GetAsset()))
			{
				if (!PreferredName.IsEmpty() && Data.AssetName.ToString().Contains(PreferredName))
				{
					Cache.Add(CacheKey, Mesh);
					return Mesh;
				}
				if (!First)
				{
					First = Mesh;
				}
			}
		}
	}
	if (First)
	{
		Cache.Add(CacheKey, First);
	}
	return First;
}
