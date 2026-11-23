# -*- coding: utf-8 -*-
"""Apply surgical C++ / project patches for the UE port phase."""
from pathlib import Path

ROOT = Path(r"C:\Users\tagir\OneDrive\Desktop\Aetheria")

def read_meta(p: Path):
    raw = p.read_bytes()
    bom = raw.startswith(b"\xef\xbb\xbf")
    text = raw.decode("utf-8-sig").replace("\r\n", "\n")
    crlf = b"\r\n" in raw
    return text, bom, crlf

def write_meta(p: Path, text: str, bom: bool, crlf: bool):
    if crlf:
        text = text.replace("\r\n", "\n").replace("\n", "\r\n")
    data = text.encode("utf-8")
    if bom:
        data = b"\xef\xbb\xbf" + data
    p.write_bytes(data)

def must_replace(text, old, new, label, count=1):
    n = text.count(old)
    if n != count:
        raise SystemExit("PATCH FAIL %s: expected %d, found %d\n%s" % (label, count, n, old[:200]))
    return text.replace(old, new)

# ---- Build.cs ----
p = ROOT / "Source/Aetheria/Aetheria.Build.cs"
t, bom, crlf = read_meta(p)
if "AnimGraphRuntime" not in t:
    t = must_replace(
        t,
        '            "ImageWrapper",\n            "Json"\n        });',
        '            "ImageWrapper",\n            "Json",\n            "AnimGraphRuntime",\n            "NavigationSystem",\n            "AssetRegistry"\n        });',
        "Build.cs modules",
    )
    write_meta(p, t, bom, crlf)
    print("patched Build.cs")
else:
    print("Build.cs already patched")

# ---- uproject ----
p = ROOT / "Aetheria.uproject"
t, bom, crlf = read_meta(p)
if "PythonScriptPlugin" not in t:
    t = must_replace(
        t,
        '''  "Plugins": [
    {
      "Name": "ModelingToolsEditorMode",
      "Enabled": false
    }
  ]''',
        '''  "Plugins": [
    {
      "Name": "ModelingToolsEditorMode",
      "Enabled": false
    },
    {
      "Name": "PythonScriptPlugin",
      "Enabled": true
    },
    {
      "Name": "EditorScriptingUtilities",
      "Enabled": true
    }
  ]''',
        "uproject plugins",
    )
    write_meta(p, t, bom, crlf)
    print("patched uproject")
else:
    print("uproject already patched")

# ---- DefaultGame.ini ----
p = ROOT / "Config/DefaultGame.ini"
t, bom, crlf = read_meta(p)
if "/Game/Characters" not in t:
    t = must_replace(
        t,
        '+DirectoriesToAlwaysCook=(Path="/Game/Audio")',
        '+DirectoriesToAlwaysCook=(Path="/Game/Audio")\n+DirectoriesToAlwaysCook=(Path="/Game/Characters")\n+DirectoriesToAlwaysCook=(Path="/Game/Props")\n+DirectoriesToAlwaysCook=(Path="/Game/Maps")',
        "DefaultGame.ini cook",
    )
    write_meta(p, t, bom, crlf)
    print("patched DefaultGame.ini")
else:
    print("DefaultGame.ini already patched")

# ---- MaterialLibrary.h ----
p = ROOT / "Source/Aetheria/Public/AetheriaMaterialLibrary.h"
t, bom, crlf = read_meta(p)
if "LoadSkeletalMeshFromFolder" not in t:
    t = must_replace(
        t,
        "static UStaticMesh* LoadCustomOrFallbackMesh(const FString& MeshName, const FString& FallbackEnginePath);",
        "static UStaticMesh* LoadCustomOrFallbackMesh(const FString& MeshName, const FString& FallbackEnginePath);\n\tstatic USkeletalMesh* LoadSkeletalMeshFromFolder(const FString& Folder, const FString& PreferredName);",
        "MaterialLibrary.h decl",
    )
    if "class USkeletalMesh;" not in t and '#include "AetheriaMaterialLibrary.generated.h"' in t:
        t = t.replace(
            '#include "AetheriaMaterialLibrary.generated.h"',
            'class USkeletalMesh;\n\n#include "AetheriaMaterialLibrary.generated.h"',
            1,
        )
    write_meta(p, t, bom, crlf)
    print("patched MaterialLibrary.h")
else:
    print("MaterialLibrary.h already patched")

# ---- MaterialLibrary.cpp ----
p = ROOT / "Source/Aetheria/Private/AetheriaMaterialLibrary.cpp"
t, bom, crlf = read_meta(p)
if "LoadSkeletalMeshFromFolder" not in t:
    old = (
        "\tconst FString PkgPath = FString::Printf(TEXT(\"/Game/Meshes/%s\"), *MeshName);\n"
        "\tconst FString CookedPath = FString::Printf(TEXT(\"/Game/Meshes/%s.%s\"), *MeshName, *MeshName);\n"
        "\tUStaticMesh* ResultMesh = FPackageName::DoesPackageExist(PkgPath) ? LoadObject<UStaticMesh>(nullptr, *CookedPath) : nullptr;\n"
        "\tif (!ResultMesh && !FallbackEnginePath.IsEmpty())\n"
        "\t{\n"
        "\t\tResultMesh = LoadObject<UStaticMesh>(nullptr, *FallbackEnginePath);\n"
        "\t}"
    )
    new = r'''	auto TryStatic = [](const FString& PkgPath, const FString& ObjectName) -> UStaticMesh*
	{
		if (FPackageName::DoesPackageExist(PkgPath))
		{
			const FString CookedPath = FString::Printf(TEXT("%s.%s"), *PkgPath, *ObjectName);
			return LoadObject<UStaticMesh>(nullptr, *CookedPath);
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
	}'''
    t = must_replace(t, old, new, "LoadCustomOrFallbackMesh search")
    t = t + r'''

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
'''
    write_meta(p, t, bom, crlf)
    print("patched MaterialLibrary.cpp")
else:
    print("MaterialLibrary.cpp already patched")

# ---- Wizard / Mob headers ----
p = ROOT / "Source/Aetheria/Public/AetheriaWizardCharacter.h"
t, bom, crlf = read_meta(p)
if "SetCursorAimPoint" not in t:
    t = must_replace(
        t,
        "	void FaceWorldPoint(const FVector& TargetPoint);",
        "	void FaceWorldPoint(const FVector& TargetPoint);\n	void SetCursorAimPoint(const FVector& WorldPoint);\n	void PlayPresentationAction(uint8 Action);\n	bool bUsingSkeletalPresentation = false;",
        "wizard header",
    )
    write_meta(p, t, bom, crlf)
    print("patched wizard header")
else:
    print("wizard header already patched")

p = ROOT / "Source/Aetheria/Public/AetheriaMobCharacter.h"
t, bom, crlf = read_meta(p)
if "bUsingSkeletalPresentation" not in t:
    t = must_replace(
        t,
        "	void UpdateVisualSilhouette();",
        "	void UpdateVisualSilhouette();\n	void PlayPresentationAction(uint8 Action);\n	bool bUsingSkeletalPresentation = false;",
        "mob header",
    )
    write_meta(p, t, bom, crlf)
    print("patched mob header")
else:
    print("mob header already patched")

print("part A done")

# ---- Wizard cpp ----
p = ROOT / "Source/Aetheria/Private/AetheriaWizardCharacter.cpp"
t, bom, crlf = read_meta(p)
if "AetheriaAnimInstance.h" not in t:
    t = must_replace(
        t,
        '#include "AetheriaMaterialLibrary.h"',
        '#include "AetheriaMaterialLibrary.h"\n#include "AetheriaAnimInstance.h"',
        "wizard include",
    )

if "bOrientRotationToMovement = false" not in t:
    t = must_replace(
        t,
        "	GetCharacterMovement()->bOrientRotationToMovement = true;",
        "	GetCharacterMovement()->bOrientRotationToMovement = false;",
        "wizard orient rotation",
    )

# After SetupSculptedMeshes body start — bind skeletal and hide primitives
old_setup = '''void AAetheriaWizardCharacter::SetupSculptedMeshes()
{
	if (UStaticMesh* RobedMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_ArchmageRobed"), TEXT("")))
	{'''
new_setup = '''void AAetheriaWizardCharacter::SetupSculptedMeshes()
{
	bUsingSkeletalPresentation = UAetheriaAnimInstance::BindSkeletalPresentation(
		this,
		TEXT("/Game/Characters/Archmage"),
		TEXT("SKM_Archmage"),
		GetCapsuleComponent() ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 96.0f);

	if (bUsingSkeletalPresentation)
	{
		if (CloakMesh) { CloakMesh->SetVisibility(false); CloakMesh->SetHiddenInGame(true); }
		if (MantleMesh) { MantleMesh->SetVisibility(false); MantleMesh->SetHiddenInGame(true); }
		if (HeadMesh) { HeadMesh->SetVisibility(false); HeadMesh->SetHiddenInGame(true); }
		if (StaffMesh) { StaffMesh->SetVisibility(false); StaffMesh->SetHiddenInGame(true); }
		if (StaffGemMesh) { StaffGemMesh->SetVisibility(false); StaffGemMesh->SetHiddenInGame(true); }
		if (HeadCosmeticMesh) { HeadCosmeticMesh->SetVisibility(false); HeadCosmeticMesh->SetHiddenInGame(true); }
		return;
	}

	if (UStaticMesh* RobedMesh = UAetheriaMaterialLibrary::LoadCustomOrFallbackMesh(TEXT("SM_ArchmageRobed"), TEXT("")))
	{'''
if "BindSkeletalPresentation" not in t:
    t = must_replace(t, old_setup, new_setup, "wizard SetupSculptedMeshes")

# SetupDynamicMaterials should not overwrite skeletal materials
old_dyn = '''	if (ObsidianBase)
	{
		FLinearColor TintColor = AetheriaMindColors::GetPartyColor(PartyColorIndex);
		DynamicCloakMaterial = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, ObsidianBase, AetheriaMindColors::ObsidianVoid, TintColor, 0.35f, 1.0f);

		if (DynamicCloakMaterial)
		{
			CloakMesh->SetMaterial(0, DynamicCloakMaterial);
			MantleMesh->SetMaterial(0, DynamicCloakMaterial);
			HeadMesh->SetMaterial(0, DynamicCloakMaterial);
			StaffMesh->SetMaterial(0, DynamicCloakMaterial);
		}
	}'''
new_dyn = '''	if (ObsidianBase && !bUsingSkeletalPresentation)
	{
		FLinearColor TintColor = AetheriaMindColors::GetPartyColor(PartyColorIndex);
		DynamicCloakMaterial = UAetheriaMaterialLibrary::CreateTintedDynamicMaterial(
			this, ObsidianBase, AetheriaMindColors::ObsidianVoid, TintColor, 0.35f, 1.0f);

		if (DynamicCloakMaterial)
		{
			CloakMesh->SetMaterial(0, DynamicCloakMaterial);
			MantleMesh->SetMaterial(0, DynamicCloakMaterial);
			HeadMesh->SetMaterial(0, DynamicCloakMaterial);
			StaffMesh->SetMaterial(0, DynamicCloakMaterial);
		}
	}'''
if "if (ObsidianBase && !bUsingSkeletalPresentation)" not in t:
    t = must_replace(t, old_dyn, new_dyn, "wizard materials skip skel")

# Keep aiming at cursor; do not clear the yaw target so PC can refresh it every frame
old_clear = '''		if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurRot.Yaw, DesiredAimYaw)) < 0.4f)
		{
			bHasDesiredAimYaw = false;
		}'''
new_clear = '''		if (FMath::Abs(FMath::FindDeltaAngleDegrees(CurRot.Yaw, DesiredAimYaw)) < 0.4f)
		{
			// Keep tracking the cursor; PlayerController refreshes DesiredAimYaw each tick.
		}'''
if "Keep tracking the cursor" not in t:
    t = must_replace(t, old_clear, new_clear, "wizard keep aim yaw")

# Append new methods if missing
if "void AAetheriaWizardCharacter::SetCursorAimPoint" not in t:
    t += r'''

void AAetheriaWizardCharacter::SetCursorAimPoint(const FVector& WorldPoint)
{
	FaceWorldPoint(WorldPoint);
}

void AAetheriaWizardCharacter::PlayPresentationAction(uint8 Action)
{
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Skel->GetAnimInstance()))
		{
			Anim->PlayAction(static_cast<EAetheriaAnimAction>(Action));
		}
	}
}
'''

# Hook dash / cast / hit / death
if "PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::Dash))" not in t:
    # ServerDash after DashAnimTimer = 0.24f first occurrence in ServerDash_Implementation
    t = must_replace(
        t,
        "void AAetheriaWizardCharacter::ServerDash_Implementation(FVector AimPoint)\n{\n	if (!bIsAlive || DashCooldownRemaining > 0.0f) return;\n\n	DashCooldownRemaining = 2.4f;\n	DashInvulnRemaining = 0.28f;\n	DashAnimTimer = 0.24f;",
        "void AAetheriaWizardCharacter::ServerDash_Implementation(FVector AimPoint)\n{\n	if (!bIsAlive || DashCooldownRemaining > 0.0f) return;\n\n	DashCooldownRemaining = 2.4f;\n	DashInvulnRemaining = 0.28f;\n	DashAnimTimer = 0.24f;\n	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::Dash));",
        "wizard dash montage",
    )

# CastAnimTimer = 0.46f appears multiple times. Hook the assignment sites by adding play after FaceWorldPoint + CastAnimTimer pairs if not already.
# Safer: when CastAnimTimer is set to 0.46f after FaceWorldPoint, also play. There are several. Use replace-all for a unique two-line pattern.
pair_old = "	FaceWorldPoint(AimPoint);\n	CastAnimTimer = 0.46f;"
pair_new = "	FaceWorldPoint(AimPoint);\n	CastAnimTimer = 0.46f;\n	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::CastPrimary));"
if "PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::CastPrimary))" not in t:
    n = t.count(pair_old)
    t = t.replace(pair_old, pair_new)
    print("wizard cast hooks:", n)

# Hit flash
old_hit = '''	if (HP < LastSeenWizardHP && bIsAlive)
	{
		HitFlashTimer = 0.18f;
		HitRecoilTimer = 0.16f;
	}'''
new_hit = '''	if (HP < LastSeenWizardHP && bIsAlive)
	{
		HitFlashTimer = 0.18f;
		HitRecoilTimer = 0.16f;
		PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::HitReact));
	}'''
if "EAetheriaAnimAction::HitReact" not in t:
    t = must_replace(t, old_hit, new_hit, "wizard hit react")

write_meta(p, t, bom, crlf)
print("patched wizard cpp")

# Death: search ApplyDamage or Die-like. If wizard has a death function we hook it later if compile allows.
# Look for bIsAlive = false
print("wizard death sites leftover handled in ApplyDamage if needed")

# ---- PlayerController ----
p = ROOT / "Source/Aetheria/Private/AetheriaPlayerController.cpp"
t, bom, crlf = read_meta(p)
if "SetCursorAimPoint" not in t:
    t = must_replace(
        t,
        "	UpdateGroundAimRay();\n	UpdateMovementInput(DeltaTime);",
        "	UpdateGroundAimRay();\n	if (AAetheriaWizardCharacter* AimWiz = Cast<AAetheriaWizardCharacter>(GetPawn()))\n	{\n		AimWiz->SetCursorAimPoint(CachedGroundAim);\n	}\n	UpdateMovementInput(DeltaTime);",
        "PC cursor facing",
    )
    write_meta(p, t, bom, crlf)
    print("patched player controller")
else:
    print("PC already patched")

# ---- GameMode smoke ----
p = ROOT / "Source/Aetheria/Private/AetheriaGameMode.cpp"
t, bom, crlf = read_meta(p)
if "GrokSmoke" not in t:
    t = must_replace(
        t,
        '#include "Misc/Paths.h"',
        '#include "Misc/Paths.h"\n#include "Misc/CommandLine.h"\n#include "Hal/FileManager.h"\n#include "UnrealClient.h"',
        "gamemode includes",
    )
    # Fix typo Hal -> HAL if I wrote Hal. Unreal uses HAL/FileManager.h
    t = t.replace('#include "Hal/FileManager.h"', '#include "HAL/FileManager.h"')
    old_bp = '''	World->GetTimerManager().SetTimer(
		SimulationTimerHandle,
		this,
		&AAetheriaGameMode::OnSimulationTick,
		0.05f,
		true);
}'''
    new_bp = '''	World->GetTimerManager().SetTimer(
		SimulationTimerHandle,
		this,
		&AAetheriaGameMode::OnSimulationTick,
		0.05f,
		true);

	if (FParse::Param(FCommandLine::Get(), TEXT("GrokSmoke")))
	{
		const FString ShotDir = FPaths::ProjectSavedDir() / TEXT("GrokShots");
		IFileManager::Get().MakeDirectory(*ShotDir, true);
		FTimerHandle ShotHandle;
		FTimerHandle QuitHandle;
		World->GetTimerManager().SetTimer(ShotHandle, [ShotDir]()
		{
			const FString Path = ShotDir / FString::Printf(TEXT("bastion_%s.png"), *FDateTime::Now().ToString(TEXT("%Y%m%d_%H%M%S")));
			FScreenshotRequest::RequestScreenshot(Path, false, false);
		}, 18.0f, false);
		World->GetTimerManager().SetTimer(QuitHandle, []()
		{
			FGenericPlatformMisc::RequestExit(false);
		}, 60.0f, false);
	}
}'''
    t = must_replace(t, old_bp, new_bp, "gamemode groksmoke")
    write_meta(p, t, bom, crlf)
    print("patched game mode")
else:
    print("game mode already patched")

print("part B headers/wizard/pc/gm done")

# PlatformMisc include for RequestExit
p = ROOT / "Source/Aetheria/Private/AetheriaGameMode.cpp"
t, bom, crlf = read_meta(p)
if "PlatformMisc.h" not in t:
    t = must_replace(
        t,
        '#include "HAL/FileManager.h"',
        '#include "HAL/FileManager.h"\n#include "HAL/PlatformMisc.h"',
        "gamemode platformmisc",
    )
    write_meta(p, t, bom, crlf)
    print("added PlatformMisc include")

# Wizard death hook
p = ROOT / "Source/Aetheria/Private/AetheriaWizardCharacter.cpp"
t, bom, crlf = read_meta(p)
if "EAetheriaAnimAction::Death" not in t:
    # Try common patterns
    hooked = False
    for old in (
        "	bIsAlive = false;\n	DownedAnimAlpha = 1.0f;",
        "	bIsAlive = false;",
    ):
        if old in t:
            t = t.replace(old, old + "\n	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::Death));", 1)
            hooked = True
            break
    if hooked:
        write_meta(p, t, bom, crlf)
        print("hooked wizard death")
    else:
        print("WARN: no wizard death hook site; will rely on HP<=0 in Tick if added later")

# Skip ApplyCustomization if skeletal? Keep cosmetics hidden — already hidden staff/head at bind time.
# If ApplyCustomization later shows HeadCosmeticMesh, it's ok if we hide again after it.
p = ROOT / "Source/Aetheria/Private/AetheriaWizardCharacter.cpp"
t, bom, crlf = read_meta(p)
if "BindSkeletalPresentation" in t and "if (bUsingSkeletalPresentation) return;" not in t.split("ApplyCustomizationVisuals")[1][:800] if "ApplyCustomizationVisuals" in t else True:
    # After ApplyCustomizationVisuals opening, re-hide primitive body if skeletal
    old = "void AAetheriaWizardCharacter::ApplyCustomizationVisuals()\n{"
    new = '''void AAetheriaWizardCharacter::ApplyCustomizationVisuals()
{
	if (bUsingSkeletalPresentation)
	{
		if (CloakMesh) { CloakMesh->SetVisibility(false); CloakMesh->SetHiddenInGame(true); }
		if (MantleMesh) { MantleMesh->SetVisibility(false); MantleMesh->SetHiddenInGame(true); }
		if (HeadMesh) { HeadMesh->SetVisibility(false); HeadMesh->SetHiddenInGame(true); }
		if (StaffMesh) { StaffMesh->SetVisibility(false); StaffMesh->SetHiddenInGame(true); }
		return;
	}'''
    if old in t and "if (bUsingSkeletalPresentation)\n	{\n		if (CloakMesh) { CloakMesh->SetVisibility(false); CloakMesh->SetHiddenInGame(true); }\n		if (MantleMesh)" in t:
        # already in SetupSculpted only
        if old + "\n	if (bUsingSkeletalPresentation)\n	{\n		if (CloakMesh)" not in t:
            t = must_replace(t, old, new, "ApplyCustomization skip skel")
            write_meta(p, t, bom, crlf)
            print("patched ApplyCustomizationVisuals")
        else:
            print("ApplyCustomization already guarded")
    else:
        if old in t:
            t = must_replace(t, old, new, "ApplyCustomization skip skel")
            write_meta(p, t, bom, crlf)
            print("patched ApplyCustomizationVisuals")

# ---- Mob cpp ----
p = ROOT / "Source/Aetheria/Private/AetheriaMobCharacter.cpp"
t, bom, crlf = read_meta(p)
if "AetheriaAnimInstance.h" not in t:
    # add include near top
    if '#include "AetheriaMaterialLibrary.h"' in t:
        t = must_replace(t, '#include "AetheriaMaterialLibrary.h"', '#include "AetheriaMaterialLibrary.h"\n#include "AetheriaAnimInstance.h"', "mob include")
    elif '#include "AetheriaMobCharacter.h"' in t:
        t = must_replace(t, '#include "AetheriaMobCharacter.h"', '#include "AetheriaMobCharacter.h"\n#include "AetheriaAnimInstance.h"', "mob include fallback")

# After successful CustomMesh apply (BodyMeshComponent->SetStaticMesh), we still try skeletal and prefer it.
# Insert at start of UpdateVisualSilhouette a skeletal attempt, or after kind switch for fallback mesh names.

if "BindSkeletalPresentation" not in t:
    old = "void AAetheriaMobCharacter::UpdateVisualSilhouette()\n{"
    # Need to see if the function starts this way
    if old not in t:
        raise SystemExit("cannot find UpdateVisualSilhouette start")
    new = r'''void AAetheriaMobCharacter::UpdateVisualSilhouette()
{
	FString SkelFolder;
	FString SkelName;
	switch (MobKind)
	{
	case EAetheriaMobKind::Boss:
		SkelFolder = TEXT("/Game/Characters/BossArchon");
		SkelName = TEXT("SKM_BossArchon");
		break;
	case EAetheriaMobKind::Swarm:
		SkelFolder = TEXT("/Game/Characters/SwarmReaper");
		SkelName = TEXT("SKM_SwarmReaper");
		break;
	case EAetheriaMobKind::Ranged:
		SkelFolder = TEXT("/Game/Characters/RangedLich");
		SkelName = TEXT("SKM_RangedLich");
		break;
	case EAetheriaMobKind::Bruiser:
		SkelFolder = TEXT("/Game/Characters/BruiserJuggernaut");
		SkelName = TEXT("SKM_BruiserJuggernaut");
		break;
	default:
		break;
	}

	if (!SkelFolder.IsEmpty())
	{
		const float HalfH = GetCapsuleComponent() ? GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 96.0f;
		bUsingSkeletalPresentation = UAetheriaAnimInstance::BindSkeletalPresentation(this, SkelFolder, SkelName, HalfH);
		if (bUsingSkeletalPresentation)
		{
			if (BodyMeshComponent) { BodyMeshComponent->SetVisibility(false); BodyMeshComponent->SetHiddenInGame(true); }
			if (HeadMeshComponent) { HeadMeshComponent->SetVisibility(false); HeadMeshComponent->SetHiddenInGame(true); }
			if (ShoulderLeftMesh) { ShoulderLeftMesh->SetVisibility(false); ShoulderLeftMesh->SetHiddenInGame(true); }
			if (ShoulderRightMesh) { ShoulderRightMesh->SetVisibility(false); ShoulderRightMesh->SetHiddenInGame(true); }
			if (CoreMesh) { CoreMesh->SetVisibility(false); CoreMesh->SetHiddenInGame(true); }
			if (FinMesh1) { FinMesh1->SetVisibility(false); FinMesh1->SetHiddenInGame(true); }
			if (FinMesh2) { FinMesh2->SetVisibility(false); FinMesh2->SetHiddenInGame(true); }
			if (FinMesh3) { FinMesh3->SetVisibility(false); FinMesh3->SetHiddenInGame(true); }
			if (FinMesh4) { FinMesh4->SetVisibility(false); FinMesh4->SetHiddenInGame(true); }
			return;
		}
	}

'''
    t = must_replace(t, old, new, "mob silhouette skeletal bind")

if "void AAetheriaMobCharacter::PlayPresentationAction" not in t:
    t += r'''

void AAetheriaMobCharacter::PlayPresentationAction(uint8 Action)
{
	if (USkeletalMeshComponent* Skel = GetMesh())
	{
		if (UAetheriaAnimInstance* Anim = Cast<UAetheriaAnimInstance>(Skel->GetAnimInstance()))
		{
			Anim->PlayAction(static_cast<EAetheriaAnimAction>(Action));
		}
	}
}
'''

# Hook attacks / death
if "EAetheriaAnimAction::AttackMelee" not in t:
    if "void AAetheriaMobCharacter::ExecuteMeleeAttack" in t:
        # insert after opening brace of the function
        t = t.replace(
            "void AAetheriaMobCharacter::ExecuteMeleeAttack(AAetheriaWizardCharacter* TargetWizard)\n{",
            "void AAetheriaMobCharacter::ExecuteMeleeAttack(AAetheriaWizardCharacter* TargetWizard)\n{\n	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::AttackMelee));",
            1,
        )
        print("hooked melee")
    if "void AAetheriaMobCharacter::ExecuteRangedAttack" in t:
        t = t.replace(
            "void AAetheriaMobCharacter::ExecuteRangedAttack(AAetheriaWizardCharacter* TargetWizard)\n{",
            "void AAetheriaMobCharacter::ExecuteRangedAttack(AAetheriaWizardCharacter* TargetWizard)\n{\n	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::CastPrimary));",
            1,
        )
        print("hooked ranged")
    if "void AAetheriaMobCharacter::ExecuteBossSlam" in t:
        t = t.replace(
            "void AAetheriaMobCharacter::ExecuteBossSlam()\n{",
            "void AAetheriaMobCharacter::ExecuteBossSlam()\n{\n	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::AttackSlam));",
            1,
        )
        print("hooked slam")
    if "void AAetheriaMobCharacter::Die()\n{" in t:
        t = t.replace(
            "void AAetheriaMobCharacter::Die()\n{",
            "void AAetheriaMobCharacter::Die()\n{\n	PlayPresentationAction(static_cast<uint8>(EAetheriaAnimAction::Death));",
            1,
        )
        print("hooked mob death")

write_meta(p, t, bom, crlf)
print("patched mob cpp")
print("ALL PATCHES APPLIED")
