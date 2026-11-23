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
#include "Sound/SoundConcurrency.h"
#include "Sound/SoundAttenuation.h"

#if WITH_EDITOR
#include "MaterialEditingLibrary.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
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
#include "Materials/MaterialExpressionAppendVector.h"
#include "Materials/MaterialExpressionComponentMask.h"
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

	const FString CookedPath = FString::Printf(TEXT("/Game/Textures/%s.%s"), *AssetName, *AssetName);
	static TSet<FString> FailedCookedTexPaths;
	UTexture2D* Tex = nullptr;
	if (!FailedCookedTexPaths.Contains(CookedPath))
	{
		Tex = LoadObject<UTexture2D>(nullptr, *CookedPath, nullptr, LOAD_NoWarn | LOAD_Quiet);
		if (!Tex)
		{
			FailedCookedTexPaths.Add(CookedPath);
		}
	}

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
		static TSet<FString> FailedCookedMeshPaths;
		const FString CookedPath = FString::Printf(TEXT("%s.%s"), *PkgPath, *ObjectName);
		if (FailedCookedMeshPaths.Contains(CookedPath))
		{
			return nullptr;
		}
		if (UStaticMesh* Loaded = LoadObject<UStaticMesh>(nullptr, *CookedPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Loaded;
		}
		FailedCookedMeshPaths.Add(CookedPath);
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

static bool IsSilentAudioTestMode()
{
	return IsRunningCommandlet()
		|| GIsAutomationTesting
		|| FParse::Param(FCommandLine::Get(), TEXT("nullrhi"))
		|| FParse::Param(FCommandLine::Get(), TEXT("nosound"))
		|| FParse::Param(FCommandLine::Get(), TEXT("autotest"));
}

static USoundConcurrency* GetAetheriaSharedSFXConcurrency()
{
	static TWeakObjectPtr<USoundConcurrency> CachedConcurrency;
	if (!CachedConcurrency.IsValid())
	{
		USoundConcurrency* Obj = NewObject<USoundConcurrency>(GetTransientPackage(), TEXT("AetheriaSharedSFXConcurrency"));
		if (Obj)
		{
			Obj->AddToRoot();
			Obj->Concurrency.MaxCount = 3;
			Obj->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
			Obj->Concurrency.VoiceStealReleaseTime = 0.08f;
			CachedConcurrency = Obj;
		}
	}
	return CachedConcurrency.Get();
}

static USoundAttenuation* GetAetheriaSharedSFXAttenuation()
{
	static TWeakObjectPtr<USoundAttenuation> CachedAtten;
	if (!CachedAtten.IsValid())
	{
		USoundAttenuation* Obj = NewObject<USoundAttenuation>(GetTransientPackage(), TEXT("AetheriaSharedSFXAttenuation"));
		if (Obj)
		{
			Obj->AddToRoot();
			Obj->Attenuation.bAttenuate = true;
			Obj->Attenuation.bSpatialize = true;
			Obj->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::Logarithmic;
			Obj->Attenuation.AttenuationShape = EAttenuationShape::Sphere;
			Obj->Attenuation.AttenuationShapeExtents = FVector(450.0f, 450.0f, 450.0f);
			Obj->Attenuation.FalloffDistance = 2400.0f;
			CachedAtten = Obj;
		}
	}
	return CachedAtten.Get();
}

static const TCHAR* PickStudioFabSoundPath(const FString& SoundName)
{
	static uint32 VariantCursor = 0;
	const uint32 Pick = ++VariantCursor;

	auto Choose2 = [Pick](const TCHAR* A, const TCHAR* B) -> const TCHAR*
	{
		return ((Pick & 1u) == 0u) ? A : B;
	};
	auto Choose3 = [Pick](const TCHAR* A, const TCHAR* B, const TCHAR* C) -> const TCHAR*
	{
		const uint32 M = Pick % 3u;
		return (M == 0u) ? A : ((M == 1u) ? B : C);
	};

	// --- Fire & Meteor Spells ---
	if (SoundName.Contains(TEXT("ChaosMeteor_Impact")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_RockExplosion1_1.SW_MagicSpell_RockExplosion1_1"),
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_MagmaEruption_1.SW_MagicSpell_MagmaEruption_1"));
	}
	if (SoundName.Contains(TEXT("ChaosMeteor")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_MagmaEruption_1.SW_MagicSpell_MagmaEruption_1"),
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_ThrowHeavy_1.SW_MagicCast_ThrowHeavy_1"));
	}
	if (SoundName.Contains(TEXT("Fireball_Boom")) || SoundName.Contains(TEXT("Fireball_Impact")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_Fire_1.SW_MagicImpact_Fire_1"),
			TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_Fire_3.SW_MagicImpact_Fire_3"));
	}
	if (SoundName.Contains(TEXT("Fireball")) || SoundName.Contains(TEXT("BasicBolt_Ignis")) || SoundName.Contains(TEXT("SelfCast_Ignis")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_FireBall1_1.SW_MagicCast_FireBall1_1"),
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_FireBall2_1.SW_MagicCast_FireBall2_1"));
	}
	if (SoundName.Contains(TEXT("Macropyre")))
	{
		return TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_FireTrail_1.SW_MagicSpell_FireTrail_1");
	}
	if (SoundName.Contains(TEXT("SunStrike")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D2_Beam/Mono/SW_MagicBeam_Light1_1.SW_MagicBeam_Light1_1"),
			TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_Fire_1.SW_MagicImpact_Fire_1"));
	}

	// --- Black Hole, Void & Gravity Spells ---
	if (SoundName.Contains(TEXT("BlackHole_Collapse")) || SoundName.Contains(TEXT("BlackHole_SolarIgnite")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_EarthQuake_1.SW_MagicSpell_EarthQuake_1"),
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_RockExplosion1_1.SW_MagicSpell_RockExplosion1_1"));
	}
	if (SoundName.Contains(TEXT("BlackHole")) || SoundName.Contains(TEXT("RequiemBlast")))
	{
		return Choose3(
			TEXT("/Game/FreeModularMagicSFX/B_Transition/Mono/SW_MagicFade_Bass1_1.SW_MagicFade_Bass1_1"),
			TEXT("/Game/RPG_Fantasy_SFX/Dark/1__1_.1__1_"),
			TEXT("/Game/RPG_Fantasy_SFX/Dark/1__2_.1__2_"));
	}
	if (SoundName.Contains(TEXT("GravityCataclysm")) || SoundName.Contains(TEXT("CataclysmNova")) || SoundName.Contains(TEXT("PrimalOverload")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_EarthQuake_1.SW_MagicSpell_EarthQuake_1"),
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_GroundStrike_1.SW_MagicCast_GroundStrike_1"));
	}

	// --- Lightning, Storm & Beam Spells ---
	if (SoundName.Contains(TEXT("ChainLightning")) || SoundName.Contains(TEXT("Lightning")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_LightningStrike_1.SW_MagicImpact_LightningStrike_1"),
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Lightning1_1.SW_MagicCast_Lightning1_1"));
	}
	if (SoundName.Contains(TEXT("PlasmaField")) || SoundName.Contains(TEXT("EyeOfTheStorm")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_LightningStrikesLine_1.SW_MagicSpell_LightningStrikesLine_1"),
			TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_LightningStrikeMulti_1.SW_MagicImpact_LightningStrikeMulti_1"));
	}
	if (SoundName.Contains(TEXT("SupernovaBeam")) || SoundName.Contains(TEXT("Beam")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D2_Beam/Mono/SW_MagicBeam_HoldStrongBlast_1.SW_MagicBeam_HoldStrongBlast_1"),
			TEXT("/Game/FreeModularMagicSFX/D2_Beam/Mono/SW_MagicBeam_Light1_1.SW_MagicBeam_Light1_1"));
	}

	// --- Chrono, Frost, Barrier & Portal Spells ---
	if (SoundName.Contains(TEXT("Chronosphere")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/D1_Spell/Mono/SW_MagicSpell_FreezingWind_1.SW_MagicSpell_FreezingWind_1"),
			TEXT("/Game/FreeModularMagicSFX/B_Transition/Mono/SW_MagicFade_Appear1_1.SW_MagicFade_Appear1_1"));
	}
	if (SoundName.Contains(TEXT("ChronoBlink")) || SoundName.Contains(TEXT("Dash")) || SoundName.Contains(TEXT("Portal")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/B_Transition/Mono/SW_MagicFade_Appear2_1.SW_MagicFade_Appear2_1"),
			TEXT("/Game/FreeModularMagicSFX/A_Chant/Mono/SW_MagicChant_WindCharge_1.SW_MagicChant_WindCharge_1"));
	}
	if (SoundName.Contains(TEXT("Prism")) || SoundName.Contains(TEXT("CryoShatter")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_CreatingBarrier1_1.SW_MagicCast_CreatingBarrier1_1"),
			TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_Crystal_1.SW_MagicImpact_Crystal_1"));
	}
	if (SoundName.Contains(TEXT("ThermalShock")))
	{
		return TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_LightningStrike_1.SW_MagicImpact_LightningStrike_1");
	}

	// --- Basic Bolts, Orbs & Invoke ---
	if (SoundName.Contains(TEXT("BasicBolt")) || SoundName.Contains(TEXT("Enemy_Ranged_Cast")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Projectile1_1.SW_MagicCast_Projectile1_1"),
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Projectile2_1.SW_MagicCast_Projectile2_1"));
	}
	if (SoundName.Contains(TEXT("OrbQ")) || SoundName.Contains(TEXT("OrbW")) || SoundName.Contains(TEXT("OrbE")) || SoundName.Contains(TEXT("SelfCast")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/A_Chant/Mono/SW_MagicChant_Charge1_1.SW_MagicChant_Charge1_1"),
			TEXT("/Game/FreeModularMagicSFX/A_Chant/Mono/SW_MagicChant_ChargePulse1_1.SW_MagicChant_ChargePulse1_1"));
	}
	if (SoundName.Contains(TEXT("Invoke")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Buff1_1.SW_MagicCast_Buff1_1"),
			TEXT("/Game/RPG_Fantasy_SFX/Enchantment/1__1_.1__1_"));
	}

	// --- Combat Hits, Shields, Boss & Mob SFX ---
	if (SoundName.Contains(TEXT("Hit_Shield")) || SoundName.Contains(TEXT("Hit_Armor")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_Barrier_1.SW_MagicImpact_Barrier_1"),
			TEXT("/Game/RPG_Fantasy_SFX/Shields/10__1_.10__1_"));
	}
	if (SoundName.Contains(TEXT("Hit_")) || SoundName.Contains(TEXT("HitImpact")) || SoundName.Contains(TEXT("Wall_Slam")) || SoundName.Contains(TEXT("Swarm_Slash")))
	{
		return Choose2(
			TEXT("/Game/RPG_Fantasy_SFX/Combat/5__1_.5__1_"),
			TEXT("/Game/RPG_Fantasy_SFX/Combat/5__2_.5__2_"));
	}
	if (SoundName.Contains(TEXT("BossRoar")) || SoundName.Contains(TEXT("Boss_Roar")))
	{
		return TEXT("/Game/RPG_Fantasy_SFX/Dragons/1__1_.1__1_");
	}
	if (SoundName.Contains(TEXT("Boss_Slam")) || SoundName.Contains(TEXT("Bruiser_Slam")) || SoundName.Contains(TEXT("Boss_Melee")))
	{
		return Choose2(
			TEXT("/Game/RPG_Fantasy_SFX/Giant/1__1_.1__1_"),
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_StaffStrikeGround_1.SW_MagicCast_StaffStrikeGround_1"));
	}
	if (SoundName.Contains(TEXT("Boss_Death")) || SoundName.Contains(TEXT("MobDeath")))
	{
		return Choose2(
			TEXT("/Game/RPG_Fantasy_SFX/Undead/1__1_.1__1_"),
			TEXT("/Game/RPG_Fantasy_SFX/Creature/1__1_.1__1_"));
	}

	// --- Healing, Progression, Objectives & UI ---
	if (SoundName.Contains(TEXT("Flask")) || SoundName.Contains(TEXT("Revive")) || SoundName.Contains(TEXT("Healing")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Healing_1.SW_MagicCast_Healing_1"),
			TEXT("/Game/RPG_Fantasy_SFX/Healing/7__1_.7__1_"));
	}
	if (SoundName.Contains(TEXT("LevelUp")) || SoundName.Contains(TEXT("Beacon")) || SoundName.Contains(TEXT("Obelisk")) || SoundName.Contains(TEXT("Relic")))
	{
		return Choose2(
			TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Buff2_1.SW_MagicCast_Buff2_1"),
			TEXT("/Game/RPG_Fantasy_SFX/Enchantment/1__2_.1__2_"));
	}
	if (SoundName.Contains(TEXT("Pylon_Shatter")))
	{
		return TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_Crystal_1.SW_MagicImpact_Crystal_1");
	}
	if (SoundName.Contains(TEXT("Forge_Anvil")) || SoundName.Contains(TEXT("Anvil")))
	{
		return TEXT("/Game/FreeModularMagicSFX/E_Impact/Mono/SW_MagicImpact_Metallic_1.SW_MagicImpact_Metallic_1");
	}
	if (SoundName.Contains(TEXT("PageTurn")))
	{
		return TEXT("/Game/RPG_Fantasy_SFX/Books/1.1");
	}
	if (SoundName.Contains(TEXT("Step_")) || SoundName.Contains(TEXT("Footstep")))
	{
		return TEXT("/Game/RPG_Fantasy_SFX/Swing/1__1_.1__1_");
	}
	if (SoundName.Contains(TEXT("Deny")) || SoundName.Contains(TEXT("Party_Wipe")))
	{
		return TEXT("/Game/FreeModularMagicSFX/B_Transition/Mono/SW_MagicFade_FadeOut1_1.SW_MagicFade_FadeOut1_1");
	}
	if (SoundName.Contains(TEXT("UIClick")) || SoundName.Contains(TEXT("UI_Hover")) || SoundName.Contains(TEXT("MoveClick")))
	{
		return Choose2(
			TEXT("/Game/RPG_Fantasy_SFX/Inventory/1.1"),
			TEXT("/Game/RPG_Fantasy_SFX/Inventory/2.2"));
	}
	if (SoundName.Contains(TEXT("Banner")))
	{
		return TEXT("/Game/RPG_Fantasy_SFX/Magical/3__1_.3__1_");
	}

	return Choose2(
		TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Generic1_1.SW_MagicCast_Generic1_1"),
		TEXT("/Game/RPG_Fantasy_SFX/Spell/8__1_.8__1_"));
}

static USoundBase* ResolvePlayableAudioAsset(const FString& SoundName)
{
	static TMap<FString, TWeakObjectPtr<USoundBase>> SoundPathCache;
	static TSet<FString> LoggedAudioNames;

	auto LoadCachedPath = [&](const FString& ObjPath) -> USoundBase*
	{
		if (const TWeakObjectPtr<USoundBase>* Cached = SoundPathCache.Find(ObjPath))
		{
			if (Cached->IsValid())
			{
				return Cached->Get();
			}
		}
		if (USoundBase* Loaded = LoadObject<USoundBase>(nullptr, *ObjPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			SoundPathCache.Add(ObjPath, Loaded);
			return Loaded;
		}
		return nullptr;
	};

	// Keep BGM tracks routed cleanly to cooked /Game/Audio/BGM_* packages if requested
	if (SoundName.StartsWith(TEXT("BGM_")))
	{
		const FString BgmPath = FString::Printf(TEXT("/Game/Audio/%s.%s"), *SoundName, *SoundName);
		return LoadCachedPath(BgmPath);
	}

	// Route ALL SFX strictly to clean studio-mastered Fab USoundWave packs (NEVER raw crackling USoundWaveProcedural!)
	const FString FabPath = PickStudioFabSoundPath(SoundName);
	if (USoundBase* FabSound = LoadCachedPath(FabPath))
	{
		if (LoggedAudioNames.Num() < 16 && !LoggedAudioNames.Contains(SoundName))
		{
			LoggedAudioNames.Add(SoundName);
			UE_LOG(LogTemp, Display, TEXT("[AetheriaAudio] Routed %s -> Fab Studio %s"), *SoundName, *FabPath);
		}
		return FabSound;
	}

	// Verified studio fallback if a specific asset wasn't cooked yet
	static const FString FallbackFabPath(TEXT("/Game/FreeModularMagicSFX/C_Cast/Mono/SW_MagicCast_Generic1_1.SW_MagicCast_Generic1_1"));
	return LoadCachedPath(FallbackFabPath);
}

static bool PassSoundAntiSpamThrottle(const FString& SoundName)
{
	struct FCueThrottleState
	{
		double LastPlaySec = -100.0;
		double WindowStartSec = -100.0;
		int32 WindowCount = 0;
	};
	static TMap<FString, FCueThrottleState> ThrottleMap;

	const double NowSec = FPlatformTime::Seconds();
	double CooldownSec = 0.09;
	if (SoundName.StartsWith(TEXT("SFX_Banner")))
	{
		CooldownSec = 1.50;
	}
	else if (SoundName.Contains(TEXT("Step_")) || SoundName.Contains(TEXT("Footstep")) || SoundName.Contains(TEXT("Hit_")) || SoundName.Contains(TEXT("UI")))
	{
		CooldownSec = 0.14;
	}

	FCueThrottleState& State = ThrottleMap.FindOrAdd(SoundName);
	if ((NowSec - State.LastPlaySec) < CooldownSec)
	{
		return false;
	}
	if ((NowSec - State.WindowStartSec) > 0.45)
	{
		State.WindowStartSec = NowSec;
		State.WindowCount = 0;
	}
	// Hard per-cue concurrency cap: max 3 triggers per 0.45s window to prevent voice stacking / clipping!
	if (State.WindowCount >= 3)
	{
		return false;
	}

	State.LastPlaySec = NowSec;
	State.WindowCount += 1;
	return true;
}

USoundBase* UAetheriaMaterialLibrary::LoadOrImportSound(const FString& SoundName)
{
	return ResolvePlayableAudioAsset(SoundName);
}

void UAetheriaMaterialLibrary::PlayGameSound2D(const UObject* WorldContextObject, const FString& SoundName, float VolumeMultiplier)
{
	if (!WorldContextObject || IsSilentAudioTestMode()) return;
	// Soft volume cap (<= 0.62) eliminates ear-piercing spikes across UI and 2D combat cues
	const float SoftVolume = FMath::Clamp(VolumeMultiplier * GGlobalAudioVolume * 0.72f, 0.0f, 0.62f);
	if (SoftVolume <= 0.001f) return;
	if (!PassSoundAntiSpamThrottle(SoundName)) return;

	if (USoundBase* Sound = ResolvePlayableAudioAsset(SoundName))
	{
		const float Pitch = FMath::FRandRange(0.94f, 1.06f);
		UGameplayStatics::PlaySound2D(WorldContextObject, Sound, SoftVolume, Pitch, 0.0f, GetAetheriaSharedSFXConcurrency());
	}
}

void UAetheriaMaterialLibrary::PlayGameSoundAtLocation(const UObject* WorldContextObject, const FString& SoundName, FVector Location, float VolumeMultiplier, float PitchMultiplier)
{
	if (!WorldContextObject || IsSilentAudioTestMode()) return;
	// Soft volume cap (<= 0.62) + smooth 3D logarithmic attenuation + MaxCount=3 concurrency
	const float SoftVolume = FMath::Clamp(VolumeMultiplier * GGlobalAudioVolume * 0.72f, 0.0f, 0.62f);
	if (SoftVolume <= 0.001f) return;
	if (!PassSoundAntiSpamThrottle(SoundName)) return;

	if (USoundBase* Sound = ResolvePlayableAudioAsset(SoundName))
	{
		const float ClampedBasePitch = FMath::Clamp(PitchMultiplier, 0.88f, 1.12f);
		const float OrganicPitch = ClampedBasePitch * FMath::FRandRange(0.94f, 1.06f);
		UGameplayStatics::PlaySoundAtLocation(
			WorldContextObject,
			Sound,
			Location,
			SoftVolume,
			OrganicPitch,
			0.0f,
			GetAetheriaSharedSFXAttenuation(),
			GetAetheriaSharedSFXConcurrency());
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

UMaterialInterface* UAetheriaMaterialLibrary::GetMobRimOverlayMaterial(UObject* Outer)
{
	static TWeakObjectPtr<UMaterialInterface> Cached;
	if (Cached.IsValid())
	{
		return Cached.Get();
	}

	UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Materials/M_AetheriaMobRim.M_AetheriaMobRim"));
#if WITH_EDITOR
	if (!Mat)
	{
		UPackage* Package = CreatePackage(TEXT("/Game/Materials/M_AetheriaMobRim"));
		Mat = BuildMobRimOverlayShaderGraph(Package, TEXT("M_AetheriaMobRim"));
	}
#endif
	if (!Mat)
	{
		Mat = GetEmissiveCoreMaterial(Outer);
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
	Mat->bUsedWithSkeletalMesh = true;
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
	Mat->bUsedWithSkeletalMesh = true;
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
	Mat->bUsedWithSkeletalMesh = true;
	Mat->bUsedWithStaticLighting = false;

	UMaterialExpressionTextureCoordinate* TexCoord = Cast<UMaterialExpressionTextureCoordinate>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureCoordinate::StaticClass(), -900, 0));
	if (TexCoord)
	{
		TexCoord->UTiling = DefaultUVTiling;
		TexCoord->VTiling = DefaultUVTiling;
	}

	const bool bApplyMacroBreakup = (DefaultUVTiling >= 2.0f);
	UMaterialExpressionAdd* MacroUV = nullptr;
	UMaterialExpressionNoise* MacroBlendNoise = nullptr;
	UMaterialExpressionNoise* GrungeNoise = nullptr;
	if (bApplyMacroBreakup)
	{
		UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionWorldPosition::StaticClass(), -1550, -320));
		UMaterialExpressionComponentMask* WorldXY = Cast<UMaterialExpressionComponentMask>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionComponentMask::StaticClass(), -1380, -320));
		UMaterialExpressionConstant2Vector* RotRow0 = Cast<UMaterialExpressionConstant2Vector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant2Vector::StaticClass(), -1380, -180));
		UMaterialExpressionConstant2Vector* RotRow1 = Cast<UMaterialExpressionConstant2Vector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant2Vector::StaticClass(), -1380, -60));
		UMaterialExpressionDotProduct* DotU = Cast<UMaterialExpressionDotProduct>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionDotProduct::StaticClass(), -1180, -240));
		UMaterialExpressionDotProduct* DotV = Cast<UMaterialExpressionDotProduct>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionDotProduct::StaticClass(), -1180, -100));
		UMaterialExpressionAppendVector* RotUV = Cast<UMaterialExpressionAppendVector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAppendVector::StaticClass(), -1020, -170));
		UMaterialExpressionConstant2Vector* MacroOffset = Cast<UMaterialExpressionConstant2Vector>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant2Vector::StaticClass(), -1020, -40));
		MacroUV = Cast<UMaterialExpressionAdd>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), -860, -150));

		if (WorldXY)
		{
			WorldXY->R = 1;
			WorldXY->G = 1;
			WorldXY->B = 0;
			WorldXY->A = 0;
		}
		if (RotRow0) { RotRow0->R = 0.00068f; RotRow0->G = -0.00051f; }
		if (RotRow1) { RotRow1->R = 0.00051f; RotRow1->G = 0.00068f; }
		if (MacroOffset) { MacroOffset->R = 0.37f; MacroOffset->G = 0.61f; }

		if (WorldPos && WorldXY && RotRow0 && RotRow1 && DotU && DotV && RotUV && MacroOffset && MacroUV)
		{
			WorldXY->Input.Connect(0, WorldPos);
			UMaterialEditingLibrary::ConnectMaterialExpressions(WorldXY, TEXT(""), DotU, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(RotRow0, TEXT(""), DotU, TEXT("B"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(WorldXY, TEXT(""), DotV, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(RotRow1, TEXT(""), DotV, TEXT("B"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(DotU, TEXT(""), RotUV, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(DotV, TEXT(""), RotUV, TEXT("B"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(RotUV, TEXT(""), MacroUV, TEXT("A"));
			UMaterialEditingLibrary::ConnectMaterialExpressions(MacroOffset, TEXT(""), MacroUV, TEXT("B"));
		}

		MacroBlendNoise = Cast<UMaterialExpressionNoise>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionNoise::StaticClass(), -1050, -480));
		if (MacroBlendNoise && WorldPos)
		{
			MacroBlendNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
			MacroBlendNoise->Scale = 0.0022f;
			MacroBlendNoise->Quality = 1;
			MacroBlendNoise->bTurbulence = false;
			MacroBlendNoise->Levels = 3;
			MacroBlendNoise->OutputMin = 0.0f;
			MacroBlendNoise->OutputMax = 0.48f;
			UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), MacroBlendNoise, TEXT("Position"));
		}

		GrungeNoise = Cast<UMaterialExpressionNoise>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionNoise::StaticClass(), -1050, 220));
		if (GrungeNoise && WorldPos)
		{
			GrungeNoise->NoiseFunction = NOISEFUNCTION_SimplexTex;
			GrungeNoise->Scale = 0.0075f;
			GrungeNoise->Quality = 1;
			GrungeNoise->bTurbulence = true;
			GrungeNoise->Levels = 2;
			GrungeNoise->OutputMin = 0.76f;
			GrungeNoise->OutputMax = 1.15f;
			UMaterialEditingLibrary::ConnectMaterialExpressions(WorldPos, TEXT(""), GrungeNoise, TEXT("Position"));
		}
	}

	if (AlbedoTex)
	{
		UMaterialExpressionTextureSample* AlbSample = Cast<UMaterialExpressionTextureSample>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), -600, -250));
		UMaterialExpressionVectorParameter* TintParam = Cast<UMaterialExpressionVectorParameter>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -600, -50));
		UMaterialExpressionMultiply* AlbMult = Cast<UMaterialExpressionMultiply>(
			UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -140, -180));
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

			UMaterialExpression* FinalAlbExpr = AlbSample;
			const TCHAR* FinalAlbOut = TEXT("RGB");

			if (bApplyMacroBreakup && MacroUV && MacroBlendNoise && GrungeNoise)
			{
				UMaterialExpressionTextureSample* AlbMacroSample = Cast<UMaterialExpressionTextureSample>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), -600, -520));
				UMaterialExpressionMultiply* PrimaryWeight = Cast<UMaterialExpressionMultiply>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -420, -380));
				UMaterialExpressionMultiply* MacroWeight = Cast<UMaterialExpressionMultiply>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -420, -520));
				UMaterialExpressionAdd* MixedTone = Cast<UMaterialExpressionAdd>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), -300, -450));
				UMaterialExpressionLinearInterpolate* MacroLerp = Cast<UMaterialExpressionLinearInterpolate>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionLinearInterpolate::StaticClass(), -260, -320));
				UMaterialExpressionMultiply* GrungeMult = Cast<UMaterialExpressionMultiply>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -200, -260));

				if (AlbMacroSample && PrimaryWeight && MacroWeight && MixedTone && MacroLerp && GrungeMult)
				{
					AlbMacroSample->Texture = AlbedoTex;
					AlbMacroSample->SamplerType = SAMPLERTYPE_Color;
					UMaterialEditingLibrary::ConnectMaterialExpressions(MacroUV, TEXT(""), AlbMacroSample, TEXT("UVs"));

					PrimaryWeight->ConstB = 0.52f;
					MacroWeight->ConstB = 0.52f;
					UMaterialEditingLibrary::ConnectMaterialExpressions(AlbSample, TEXT("RGB"), PrimaryWeight, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(AlbMacroSample, TEXT("RGB"), MacroWeight, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(PrimaryWeight, TEXT(""), MixedTone, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(MacroWeight, TEXT(""), MixedTone, TEXT("B"));

					UMaterialEditingLibrary::ConnectMaterialExpressions(AlbSample, TEXT("RGB"), MacroLerp, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(MixedTone, TEXT(""), MacroLerp, TEXT("B"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(MacroBlendNoise, TEXT(""), MacroLerp, TEXT("Alpha"));

					UMaterialEditingLibrary::ConnectMaterialExpressions(MacroLerp, TEXT(""), GrungeMult, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(GrungeNoise, TEXT(""), GrungeMult, TEXT("B"));

					FinalAlbExpr = GrungeMult;
					FinalAlbOut = TEXT("");
				}
			}

			UMaterialEditingLibrary::ConnectMaterialExpressions(FinalAlbExpr, FinalAlbOut, AlbMult, TEXT("A"));
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
			if (bApplyMacroBreakup && MacroUV && GrungeNoise)
			{
				UMaterialExpressionTextureSample* RghMacroSample = Cast<UMaterialExpressionTextureSample>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), -600, 520));
				UMaterialExpressionConstant* RoughAlphaConst = Cast<UMaterialExpressionConstant>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant::StaticClass(), -480, 620));
				UMaterialExpressionLinearInterpolate* RghLerp = Cast<UMaterialExpressionLinearInterpolate>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionLinearInterpolate::StaticClass(), -380, 440));
				UMaterialExpressionSubtract* GrungeCenter = Cast<UMaterialExpressionSubtract>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionSubtract::StaticClass(), -520, 260));
				UMaterialExpressionMultiply* GrungeScale = Cast<UMaterialExpressionMultiply>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -380, 260));
				UMaterialExpressionAdd* RghMod = Cast<UMaterialExpressionAdd>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), -240, 380));
				UMaterialExpressionClamp* RghClamp = Cast<UMaterialExpressionClamp>(
					UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionClamp::StaticClass(), -120, 380));

				if (RghMacroSample && RoughAlphaConst && RghLerp && GrungeCenter && GrungeScale && RghMod && RghClamp)
				{
					RghMacroSample->Texture = RoughTex;
					RghMacroSample->SamplerType = SAMPLERTYPE_LinearColor;
					UMaterialEditingLibrary::ConnectMaterialExpressions(MacroUV, TEXT(""), RghMacroSample, TEXT("UVs"));

					RoughAlphaConst->R = 0.30f;
					UMaterialEditingLibrary::ConnectMaterialExpressions(RghSample, TEXT("R"), RghLerp, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(RghMacroSample, TEXT("R"), RghLerp, TEXT("B"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(RoughAlphaConst, TEXT(""), RghLerp, TEXT("Alpha"));

					GrungeCenter->ConstB = 0.95f;
					GrungeScale->ConstB = 0.65f;
					UMaterialEditingLibrary::ConnectMaterialExpressions(GrungeNoise, TEXT(""), GrungeCenter, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(GrungeCenter, TEXT(""), GrungeScale, TEXT("A"));

					UMaterialEditingLibrary::ConnectMaterialExpressions(RghLerp, TEXT(""), RghMod, TEXT("A"));
					UMaterialEditingLibrary::ConnectMaterialExpressions(GrungeScale, TEXT(""), RghMod, TEXT("B"));

					RghClamp->MinDefault = 0.22f;
					RghClamp->MaxDefault = 0.96f;
					RghClamp->Input.Connect(0, RghMod);
					UMaterialEditingLibrary::ConnectMaterialProperty(RghClamp, TEXT(""), MP_Roughness);
				}
				else
				{
					UMaterialEditingLibrary::ConnectMaterialProperty(RghSample, TEXT("R"), MP_Roughness);
				}
			}
			else
			{
				UMaterialEditingLibrary::ConnectMaterialProperty(RghSample, TEXT("R"), MP_Roughness);
			}
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
	Mat->bUsedWithSkeletalMesh = true;
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

UMaterial* UAetheriaMaterialLibrary::BuildMobRimOverlayShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithSkeletalMesh = true;
	Mat->bUsedWithStaticLighting = false;

	UMaterialExpressionFresnel* FresnelExpr = Cast<UMaterialExpressionFresnel>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionFresnel::StaticClass(), -720, -140));
	if (FresnelExpr)
	{
		FresnelExpr->Exponent = 2.4f;
		FresnelExpr->BaseReflectFraction = 0.04f;
	}

	UMaterialExpressionVectorParameter* EmissCol = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -720, 40));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -720, 200));
	UMaterialExpressionMultiply* RimColorMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -460, -40));
	UMaterialExpressionMultiply* RimBoostMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -260, 40));

	if (EmissCol)
	{
		EmissCol->ParameterName = FName("EmissiveColor");
		EmissCol->DefaultValue = FLinearColor(1.0f, 0.56f, 0.20f, 1.0f);
	}
	if (BoostParam)
	{
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 2.1f;
	}

	UMaterialExpressionVectorParameter* BaseCol = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -720, 360));
	UMaterialExpressionScalarParameter* BaseLift = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -720, 520));
	UMaterialExpressionMultiply* LiftMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -420, 420));

	if (BaseCol)
	{
		BaseCol->ParameterName = FName("BaseColor");
		BaseCol->DefaultValue = FLinearColor(1.0f, 0.56f, 0.20f, 1.0f);
	}
	if (BaseLift)
	{
		BaseLift->ParameterName = FName("BaseLift");
		BaseLift->DefaultValue = 0.09f;
	}

	UMaterialExpressionVectorParameter* HitCol = Cast<UMaterialExpressionVectorParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), -720, 680));
	UMaterialExpressionScalarParameter* HitFlash = Cast<UMaterialExpressionScalarParameter>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), -720, 840));
	UMaterialExpressionMultiply* HitColorMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -460, 740));
	UMaterialExpressionMultiply* HitScaleMult = Cast<UMaterialExpressionMultiply>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), -260, 740));

	if (HitCol)
	{
		HitCol->ParameterName = FName("HitColor");
		HitCol->DefaultValue = FLinearColor(1.0f, 0.35f, 0.14f, 1.0f);
	}
	if (HitFlash)
	{
		HitFlash->ParameterName = FName("HitFlash");
		HitFlash->DefaultValue = 0.0f;
	}
	if (HitScaleMult)
	{
		HitScaleMult->ConstB = 5.0f;
	}

	UMaterialExpressionAdd* RimPlusLift = Cast<UMaterialExpressionAdd>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), -80, 200));
	UMaterialExpressionAdd* FinalEmiss = Cast<UMaterialExpressionAdd>(
		UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), 100, 380));

	if (FresnelExpr && EmissCol && BoostParam && RimColorMult && RimBoostMult &&
		BaseCol && BaseLift && LiftMult && HitCol && HitFlash && HitColorMult && HitScaleMult &&
		RimPlusLift && FinalEmiss)
	{
		UMaterialEditingLibrary::ConnectMaterialExpressions(FresnelExpr, TEXT(""), RimColorMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(EmissCol, TEXT(""), RimColorMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(RimColorMult, TEXT(""), RimBoostMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BoostParam, TEXT(""), RimBoostMult, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(BaseCol, TEXT(""), LiftMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(BaseLift, TEXT(""), LiftMult, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(HitCol, TEXT(""), HitColorMult, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(HitFlash, TEXT(""), HitColorMult, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(HitColorMult, TEXT(""), HitScaleMult, TEXT("A"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RimBoostMult, TEXT(""), RimPlusLift, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(LiftMult, TEXT(""), RimPlusLift, TEXT("B"));

		UMaterialEditingLibrary::ConnectMaterialExpressions(RimPlusLift, TEXT(""), FinalEmiss, TEXT("A"));
		UMaterialEditingLibrary::ConnectMaterialExpressions(HitScaleMult, TEXT(""), FinalEmiss, TEXT("B"));
		UMaterialEditingLibrary::ConnectMaterialProperty(FinalEmiss, TEXT(""), MP_EmissiveColor);
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
	Mat->bUsedWithSkeletalMesh = true;
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

	// Interstellar Gargantua Event Horizon:
	// Absolute light-trapping #000000 interior + razor Fresnel^5.5 multi-band incandescent photon ring
	// (blazing white-gold inner rim -> fiery amber-orange mid band -> deep violet gravitational redshift edge).
	Mat->BlendMode = BLEND_Opaque;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = false;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithSkeletalMesh = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	UMaterialExpressionVectorParameter* CoreCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -760, -240));
	UMaterialExpressionVectorParameter* MidCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -760, -80));
	UMaterialExpressionVectorParameter* OuterCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -760, 80));
	UMaterialExpressionScalarParameter* ExpParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -1060, 120));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -760, 240));

	if (CoreCol)
	{
		CoreCol->ParameterName = FName("RingCoreColor");
		CoreCol->DefaultValue = FLinearColor(1.00f, 0.96f, 0.80f, 1.0f); // blazing white-gold
	}
	if (MidCol)
	{
		MidCol->ParameterName = FName("RingMidColor");
		MidCol->DefaultValue = FLinearColor(1.00f, 0.52f, 0.10f, 1.0f); // fiery amber-orange
	}
	if (OuterCol)
	{
		OuterCol->ParameterName = FName("RingOuterColor");
		OuterCol->DefaultValue = FLinearColor(0.36f, 0.06f, 0.62f, 1.0f); // deep violet redshift
	}
	if (ExpParam)
	{
		ExpParam->ParameterName = FName("PhotonRingExponent");
		ExpParam->DefaultValue = 5.5f;
	}
	if (BoostParam)
	{
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 16.0f;
	}

	UMaterialExpressionFresnel* HorizonFresnel = Cast<UMaterialExpressionFresnel>(
		CreateExpr(UMaterialExpressionFresnel::StaticClass(), -760, -400));
	if (HorizonFresnel)
	{
		HorizonFresnel->Exponent = 5.5f;
		HorizonFresnel->BaseReflectFraction = 0.0f;
		if (ExpParam)
		{
			HorizonFresnel->ExponentIn.Connect(0, ExpParam);
		}
	}

	UMaterialExpressionWorldPosition* WorldPos = Cast<UMaterialExpressionWorldPosition>(
		CreateExpr(UMaterialExpressionWorldPosition::StaticClass(), -760, 380));
	UMaterialExpressionTime* TimeExpr = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -760, 500));

	UMaterialExpressionCustom* HorizonHLSL = Cast<UMaterialExpressionCustom>(
		CreateExpr(UMaterialExpressionCustom::StaticClass(), -220, 0));

	if (CoreCol && MidCol && OuterCol && BoostParam && HorizonFresnel && WorldPos && TimeExpr && HorizonHLSL)
	{
		HorizonHLSL->Description = TEXT("GargantuaEventHorizon");
		HorizonHLSL->OutputType = ECustomMaterialOutputType::CMOT_Float3;
		HorizonHLSL->Code = TEXT(
			"float f = saturate(InFresnel);\n"
			"// Strict pitch-black (#000000) light-trapping horizon core\n"
			"float ringGate = smoothstep(0.035, 0.18, f);\n"
			"float innerBand = pow(f, 2.2);\n"
			"float outerBand = pow(f, 0.65);\n"
			"// Subtle relativistic gravitational lensing shimmer around photon sphere limb\n"
			"float waveA = sin(InWorldPos.x * 0.035 + InTime * 3.8 + InWorldPos.z * 0.028);\n"
			"float waveB = cos(InWorldPos.y * 0.041 - InTime * 4.6 + InWorldPos.x * 0.022);\n"
			"float shimmer = 0.84 + 0.16 * (0.5 * (waveA + waveB));\n"
			"float3 warmMix = lerp(InMidCol, InCoreCol, innerBand);\n"
			"float3 ringCol = lerp(InOuterCol, warmMix, outerBand);\n"
			"// Hot razor photon edge at grazing angle\n"
			"float photonSpike = pow(f, 3.6) * 1.45;\n"
			"return (ringCol * ringGate * shimmer + InCoreCol * photonSpike) * InBoost;\n"
		);

		HorizonHLSL->Inputs.Reset();
		auto AddCustomIn = [HorizonHLSL](const TCHAR* InName, UMaterialExpression* Src)
		{
			FCustomInput In;
			In.InputName = FName(InName);
			In.Input.Connect(0, Src);
			HorizonHLSL->Inputs.Add(In);
		};
		AddCustomIn(TEXT("InFresnel"), HorizonFresnel);
		AddCustomIn(TEXT("InCoreCol"), CoreCol);
		AddCustomIn(TEXT("InMidCol"), MidCol);
		AddCustomIn(TEXT("InOuterCol"), OuterCol);
		AddCustomIn(TEXT("InBoost"), BoostParam);
		AddCustomIn(TEXT("InWorldPos"), WorldPos);
		AddCustomIn(TEXT("InTime"), TimeExpr);
		HorizonHLSL->RebuildOutputs();

		UMaterialEditingLibrary::ConnectMaterialProperty(HorizonHLSL, TEXT(""), MP_EmissiveColor);
	}

	UMaterialEditingLibrary::RecompileMaterial(Mat);
	return Mat;
}

UMaterial* UAetheriaMaterialLibrary::BuildAccretionDiskShaderGraph(UPackage* Package, const FString& Name)
{
	UMaterial* Mat = NewObject<UMaterial>(Package, *Name, RF_Public | RF_Standalone);
	if (!Mat) return nullptr;

	// Interstellar Gargantua Keplerian Accretion Disk:
	// Differential orbital rotation (omega ~ r^-1.5), multi-band spiral plasma filaments,
	// relativistic Doppler beaming, blazing white-gold ISCO inner rim -> amber-orange -> crimson/violet outer wisps.
	Mat->BlendMode = BLEND_Additive;
	Mat->SetShadingModel(MSM_Unlit);
	Mat->TwoSided = true;
	Mat->bUsedWithInstancedStaticMeshes = true;
	Mat->bUsedWithStaticLighting = false;

	auto CreateExpr = [Mat](UClass* ExprClass, int32 X, int32 Y) -> UMaterialExpression*
	{
		return UMaterialEditingLibrary::CreateMaterialExpression(Mat, ExprClass, X, Y);
	};

	UMaterialExpressionTextureCoordinate* UVExpr = Cast<UMaterialExpressionTextureCoordinate>(
		CreateExpr(UMaterialExpressionTextureCoordinate::StaticClass(), -780, -340));
	UMaterialExpressionTime* TimeExpr = Cast<UMaterialExpressionTime>(
		CreateExpr(UMaterialExpressionTime::StaticClass(), -780, -220));
	UMaterialExpressionVectorParameter* InnerCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -780, -80));
	UMaterialExpressionVectorParameter* MidCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -780, 80));
	UMaterialExpressionVectorParameter* OuterCol = Cast<UMaterialExpressionVectorParameter>(
		CreateExpr(UMaterialExpressionVectorParameter::StaticClass(), -780, 240));
	UMaterialExpressionScalarParameter* OmegaParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -780, 380));
	UMaterialExpressionScalarParameter* BoostParam = Cast<UMaterialExpressionScalarParameter>(
		CreateExpr(UMaterialExpressionScalarParameter::StaticClass(), -780, 500));

	if (InnerCol)
	{
		InnerCol->ParameterName = FName("DiskInnerColor");
		InnerCol->DefaultValue = FLinearColor(1.00f, 0.94f, 0.72f, 1.0f); // incandescent white-gold ISCO
	}
	if (MidCol)
	{
		MidCol->ParameterName = FName("DiskMidColor");
		MidCol->DefaultValue = FLinearColor(1.00f, 0.44f, 0.07f, 1.0f); // fiery amber-orange
	}
	if (OuterCol)
	{
		OuterCol->ParameterName = FName("DiskOuterColor");
		OuterCol->DefaultValue = FLinearColor(0.42f, 0.06f, 0.36f, 1.0f); // crimson-violet outer wisps
	}
	if (OmegaParam)
	{
		OmegaParam->ParameterName = FName("KeplerOmega");
		OmegaParam->DefaultValue = 2.6f;
	}
	if (BoostParam)
	{
		BoostParam->ParameterName = FName("EmissiveBoost");
		BoostParam->DefaultValue = 18.0f;
	}

	UMaterialExpressionCustom* DiskHLSL = Cast<UMaterialExpressionCustom>(
		CreateExpr(UMaterialExpressionCustom::StaticClass(), -220, 40));

	if (UVExpr && TimeExpr && InnerCol && MidCol && OuterCol && OmegaParam && BoostParam && DiskHLSL)
	{
		DiskHLSL->Description = TEXT("GargantuaKeplerianAccretionDisk");
		DiskHLSL->OutputType = ECustomMaterialOutputType::CMOT_Float3;
		DiskHLSL->Code = TEXT(
			"float2 p = (InUV - 0.5) * 2.0;\n"
			"float r = length(p);\n"
			"float theta = atan2(p.y, p.x);\n"
			"// Smooth hollow core cutout for the black event horizon sphere & soft outer feather\n"
			"float innerCut = smoothstep(0.23, 0.34, r);\n"
			"float outerFade = 1.0 - smoothstep(0.56, 0.97, r);\n"
			"float diskMask = innerCut * outerFade;\n"
			"if (diskMask <= 0.0005) return float3(0.0, 0.0, 0.0);\n"
			"// True Keplerian differential angular velocity: omega ~ r^-1.5\n"
			"float safeR = max(r, 0.24);\n"
			"float omega = InOmega * pow(safeR, -1.5);\n"
			"float swirlA = theta - InTime * omega + 5.2 / safeR;\n"
			"float swirlB = theta * 2.0 - InTime * omega * 1.35 + 9.4 / safeR;\n"
			"float swirlC = theta * 3.0 - InTime * omega * 0.75 + 14.0 / safeR;\n"
			"// Multi-band turbulent relativistic plasma filaments + concentric shearing lanes\n"
			"float arm1 = 0.5 + 0.5 * sin(swirlA * 2.0 + sin(r * 22.0 - InTime * 3.2) * 0.85);\n"
			"float arm2 = 0.5 + 0.5 * cos(swirlB + r * 34.0 - InTime * 4.5);\n"
			"float arm3 = 0.5 + 0.5 * sin(swirlC - r * 48.0 + InTime * 2.4);\n"
			"float ringLanes = 0.65 + 0.35 * sin(r * 52.0 - InTime * 5.0);\n"
			"float plasma = saturate(arm1 * 0.52 + arm2 * 0.32 + arm3 * 0.26) * ringLanes;\n"
			"plasma = lerp(0.32, 1.42, pow(plasma, 1.25));\n"
			"// Relativistic Doppler beaming (approaching side glows brighter & hotter)\n"
			"float doppler = lerp(0.62, 1.48, 0.5 + 0.5 * cos(theta - 0.65));\n"
			"// Blazing white-gold ISCO inner rim right outside the Schwarzschild shadow\n"
			"float isco = exp(-pow((r - 0.335) / 0.062, 2.0));\n"
			"// Radial blackbody gradient: white-gold ISCO -> fiery amber-orange -> crimson-violet wisps\n"
			"float radialT = saturate((r - 0.30) / 0.62);\n"
			"float3 midToOuter = lerp(InMidCol, InOuterCol, smoothstep(0.18, 0.92, radialT));\n"
			"float3 baseCol = lerp(InInnerCol, midToOuter, smoothstep(0.0, 0.42, radialT));\n"
			"float3 finalCol = (baseCol * plasma * diskMask + InInnerCol * isco * 1.35 * outerFade) * doppler;\n"
			"return finalCol * InBoost;\n"
		);

		DiskHLSL->Inputs.Reset();
		auto AddCustomIn = [DiskHLSL](const TCHAR* InName, UMaterialExpression* Src)
		{
			FCustomInput In;
			In.InputName = FName(InName);
			In.Input.Connect(0, Src);
			DiskHLSL->Inputs.Add(In);
		};
		AddCustomIn(TEXT("InUV"), UVExpr);
		AddCustomIn(TEXT("InTime"), TimeExpr);
		AddCustomIn(TEXT("InInnerCol"), InnerCol);
		AddCustomIn(TEXT("InMidCol"), MidCol);
		AddCustomIn(TEXT("InOuterCol"), OuterCol);
		AddCustomIn(TEXT("InOmega"), OmegaParam);
		AddCustomIn(TEXT("InBoost"), BoostParam);
		DiskHLSL->RebuildOutputs();

		UMaterialEditingLibrary::ConnectMaterialProperty(DiskHLSL, TEXT(""), MP_EmissiveColor);
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
	if (!Params.Contains(TEXT("ShadersOnly"), ESearchCase::IgnoreCase) && !FParse::Param(FCommandLine::Get(), TEXT("ShadersOnly")))
	{
		BatchImportDir(TEXT("RawAssets/Textures"), TEXT("*.png"), TEXT("/Game/Textures"), false, true);
		BatchImportDir(TEXT("RawAssets/Grimoire"), TEXT("*.png"), TEXT("/Game/Textures"), false, false);
		BatchImportDir(TEXT("RawAssets/UI"), TEXT("*.png"), TEXT("/Game/Textures"), false, false);
		BatchImportDir(TEXT("RawAssets/Meshes"), TEXT("*.fbx"), TEXT("/Game/Meshes"), true, false);
		BatchImportDir(TEXT("RawAssets/Audio"), TEXT("*.wav"), TEXT("/Game/Audio"), false, false);
	}

	struct FShaderSpec
	{
		FString AssetName;
		TFunction<UMaterial*(UPackage*, const FString&)> Builder;
	};

	TArray<FShaderSpec> Specs = {
		{ TEXT("M_AetheriaMasterPBR"), &UAetheriaMaterialLibrary::BuildMasterCharacterPBRShaderGraph },
		{ TEXT("M_AetheriaObsidian"), &UAetheriaMaterialLibrary::BuildObsidianShaderGraph },
		{ TEXT("M_AetheriaEmissiveCore"), &UAetheriaMaterialLibrary::BuildEmissiveCoreShaderGraph },
		{ TEXT("M_AetheriaMobRim"), &UAetheriaMaterialLibrary::BuildMobRimOverlayShaderGraph },
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
		static TSet<FString> FailedCookedSkelPaths;
		const FString ObjectPath = Path + TEXT(".") + FPackageName::GetShortName(Path);
		if (FailedCookedSkelPaths.Contains(ObjectPath))
		{
			return nullptr;
		}
		if (USkeletalMesh* Loaded = LoadObject<USkeletalMesh>(nullptr, *ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet))
		{
			return Loaded;
		}
		FailedCookedSkelPaths.Add(ObjectPath);
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
