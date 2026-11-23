# -*- coding: utf-8 -*-
"""Surgical visual-pass patches for Aetheria UE port."""
from pathlib import Path
import sys

ROOT = Path(r"C:\Users\tagir\OneDrive\Desktop\Aetheria")
changed = []
missing = []


def patch(rel, replacements):
    path = ROOT / rel
    data = path.read_bytes()
    if b"\r\n" in data:
        text = data.decode("utf-8").replace("\r\n", "\n")
        nl = "\r\n"
    else:
        text = data.decode("utf-8")
        nl = "\n"
    orig = text
    for i, (old, new) in enumerate(replacements, 1):
        if old not in text:
            missing.append(f"{rel} #{i}")
            print(f"PATTERN MISSING {rel} #{i}")
            print("--- old start ---")
            print(old[:240])
            continue
        text = text.replace(old, new, 1)
        print(f"patched {rel} #{i}")
    if text != orig:
        path.write_bytes(text.replace("\n", nl).encode("utf-8"))
        changed.append(rel)


# ---- camera defaults ----
patch(
    "Source/Aetheria/Public/AetheriaWizardCharacter.h",
    [
        (
            "float DesiredCameraArmLength = 3250.0f;",
            "float DesiredCameraArmLength = 2600.0f;",
        ),
        (
            "bool bIsInLobbyMode = true;",
            "bool bIsInLobbyMode = false;",
        ),
    ],
)

patch(
    "Source/Aetheria/Private/AetheriaWizardCharacter.cpp",
    [
        (
            "\tCameraBoom->TargetArmLength = 3250.0f;\n"
            "\tCameraBoom->SetRelativeRotation(FRotator(-55.0f, -45.0f, 0.0f));\n"
            "\tCameraBoom->bDoCollisionTest = false;\n"
            "\tCameraBoom->bInheritYaw = false;\n"
            "\tCameraBoom->bInheritPitch = false;\n"
            "\tCameraBoom->bInheritRoll = false;\n",
            "\tCameraBoom->TargetArmLength = 2600.0f;\n"
            "\tCameraBoom->SetRelativeRotation(FRotator(-58.0f, -45.0f, 0.0f));\n"
            "\tCameraBoom->bDoCollisionTest = false;\n"
            "\tCameraBoom->bInheritYaw = false;\n"
            "\tCameraBoom->bInheritPitch = false;\n"
            "\tCameraBoom->bInheritRoll = false;\n"
            "\tCameraBoom->bEnableCameraLag = true;\n"
            "\tCameraBoom->CameraLagSpeed = 4.5f;\n"
            "\tCameraBoom->CameraLagMaxDistance = 450.0f;\n"
            "\tCameraBoom->bEnableCameraRotationLag = false;\n"
            "\tCameraBoom->SocketOffset = FVector(0.0f, 0.0f, 110.0f);\n",
        ),
        (
            "\tTopDownCameraComponent->FieldOfView = 50.0f;\n",
            "\tTopDownCameraComponent->FieldOfView = 55.0f;\n",
        ),
        (
            "\t\tFRotator TargetRot(-55.0f, -45.0f, 0.0f);\n"
            "\t\tFVector TargetSocketOffset = FVector::ZeroVector;\n",
            "\t\tFRotator TargetRot(-58.0f, -45.0f, 0.0f);\n"
            "\t\tFVector TargetSocketOffset = FVector(0.0f, 0.0f, 110.0f);\n",
        ),
        (
            "\t\tDesiredCameraArmLength = FMath::Clamp(DesiredCameraArmLength - WheelDelta * 220.0f, 1850.0f, 4200.0f);\n",
            "\t\tDesiredCameraArmLength = FMath::Clamp(DesiredCameraArmLength - WheelDelta * 200.0f, 1600.0f, 3800.0f);\n",
        ),
    ],
)

# ---- HUD lobby toggles also drive the camera ----
patch(
    "Source/Aetheria/Private/AetheriaSlateHUD.cpp",
    [
        (
            "void AAetheriaHUD::ShowLobby()\n"
            "{\n"
            "\tif (MainOverlay.IsValid()) MainOverlay->SetShowLobby(true);\n"
            "}\n"
            "\n"
            "void AAetheriaHUD::HideLobby()\n"
            "{\n"
            "\tif (MainOverlay.IsValid()) MainOverlay->SetShowLobby(false);\n"
            "}\n",
            "void AAetheriaHUD::ShowLobby()\n"
            "{\n"
            "\tif (MainOverlay.IsValid()) MainOverlay->SetShowLobby(true);\n"
            "\tif (AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(GetOwningPawn()))\n"
            "\t{\n"
            "\t\tW->SetLobbyCameraMode(true);\n"
            "\t}\n"
            "}\n"
            "\n"
            "void AAetheriaHUD::HideLobby()\n"
            "{\n"
            "\tif (MainOverlay.IsValid()) MainOverlay->SetShowLobby(false);\n"
            "\tif (AAetheriaWizardCharacter* W = Cast<AAetheriaWizardCharacter>(GetOwningPawn()))\n"
            "\t{\n"
            "\t\tW->SetLobbyCameraMode(false);\n"
            "\t}\n"
            "}\n",
        ),
    ],
)

# ---- GameMode: enter Bastion in gameplay camera + lift lighting ----
patch(
    "Source/Aetheria/Private/AetheriaGameMode.cpp",
    [
        (
            '#include "UnrealClient.h"\n',
            '#include "UnrealClient.h"\n'
            '#include "EngineUtils.h"\n'
            '#include "Engine/DirectionalLight.h"\n'
            '#include "Engine/SkyLight.h"\n'
            '#include "Engine/ExponentialHeightFog.h"\n'
            '#include "Engine/PostProcessVolume.h"\n'
            '#include "Components/LightComponent.h"\n'
            '#include "Components/DirectionalLightComponent.h"\n'
            '#include "Components/SkyLightComponent.h"\n'
            '#include "Components/ExponentialHeightFogComponent.h"\n',
        ),
        (
            "\tWarpToLocation(StartLoc, 1);\n",
            "\tWarpToLocation(StartLoc, 1);\n"
            "\n"
            "\t// Bastion playable-map pass: start in the ARPG camera (not the lobby close-up),\n"
            "\t// and lift lighting so the sanctum is moody but readable.\n"
            "\tauto ForceGameplayPresentation = [World]()\n"
            "\t{\n"
            "\t\tif (!World) return;\n"
            "\t\tfor (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)\n"
            "\t\t{\n"
            "\t\t\tAPlayerController* PC = It->Get();\n"
            "\t\t\tif (!PC) continue;\n"
            "\t\t\tif (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(PC->GetHUD()))\n"
            "\t\t\t{\n"
            "\t\t\t\tHud->HideLobby();\n"
            "\t\t\t}\n"
            "\t\t\tif (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(PC->GetPawn()))\n"
            "\t\t\t{\n"
            "\t\t\t\tWiz->SetLobbyCameraMode(false);\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\n"
            "\t\tfor (TActorIterator<ADirectionalLight> It(World); It; ++It)\n"
            "\t\t{\n"
            "\t\t\tif (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))\n"
            "\t\t\t{\n"
            "\t\t\t\tL->SetIntensity(14.0f);\n"
            "\t\t\t\tL->SetLightColor(FLinearColor(1.0f, 0.86f, 0.68f));\n"
            "\t\t\t\tL->SetAtmosphereSunLight(true);\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\t\tfor (TActorIterator<ASkyLight> It(World); It; ++It)\n"
            "\t\t{\n"
            "\t\t\tif (USkyLightComponent* L = It->GetLightComponent())\n"
            "\t\t\t{\n"
            "\t\t\t\tL->SetIntensity(3.2f);\n"
            "\t\t\t\tL->SetLightColor(FLinearColor(0.55f, 0.62f, 0.85f));\n"
            "\t\t\t\tL->bRealTimeCapture = true;\n"
            "\t\t\t\tL->MarkRenderStateDirty();\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\t\tfor (TActorIterator<AExponentialHeightFog> It(World); It; ++It)\n"
            "\t\t{\n"
            "\t\t\tif (UExponentialHeightFogComponent* F = It->GetComponent())\n"
            "\t\t\t{\n"
            "\t\t\t\tF->SetFogDensity(0.0075f);\n"
            "\t\t\t\tF->SetFogHeightFalloff(0.15f);\n"
            "\t\t\t\tF->SetFogInscatteringColor(FLinearColor(0.045f, 0.05f, 0.08f));\n"
            "\t\t\t\tF->SetVolumetricFog(true);\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\t\tfor (TActorIterator<APostProcessVolume> It(World); It; ++It)\n"
            "\t\t{\n"
            "\t\t\tAPostProcessVolume* PP = *It;\n"
            "\t\t\tPP->bUnbound = true;\n"
            "\t\t\tPP->BlendWeight = 1.0f;\n"
            "\t\t\tPP->Priority = 10.0f;\n"
            "\t\t\tFPostProcessSettings& S = PP->Settings;\n"
            "\t\t\tS.bOverride_AutoExposureMethod = true;\n"
            "\t\t\tS.AutoExposureMethod = EAutoExposureMethod::AEM_Manual;\n"
            "\t\t\tS.bOverride_AutoExposureBias = true;\n"
            "\t\t\tS.AutoExposureBias = 1.15f;\n"
            "\t\t\tS.bOverride_BloomIntensity = true;\n"
            "\t\t\tS.BloomIntensity = 0.45f;\n"
            "\t\t\tS.bOverride_VignetteIntensity = true;\n"
            "\t\t\tS.VignetteIntensity = 0.16f;\n"
            "\t\t\tS.bOverride_MotionBlurAmount = true;\n"
            "\t\t\tS.MotionBlurAmount = 0.12f;\n"
            "\t\t\tS.bOverride_ColorContrast = true;\n"
            "\t\t\tS.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.0f);\n"
            "\t\t\tS.bOverride_ColorSaturation = true;\n"
            "\t\t\tS.ColorSaturation = FVector4(1.06f, 1.03f, 1.08f, 1.0f);\n"
            "\t\t\tS.bOverride_AmbientOcclusionIntensity = true;\n"
            "\t\t\tS.AmbientOcclusionIntensity = 0.5f;\n"
            "\t\t}\n"
            "\t};\n"
            "\tForceGameplayPresentation();\n"
            "\tFTimerHandle PresentationHandle;\n"
            "\tWorld->GetTimerManager().SetTimer(PresentationHandle, ForceGameplayPresentation, 0.35f, false);\n",
        ),
        (
            "\tif (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(NewPlayer->GetPawn()))\n"
            "\t{\n"
            "\t\tWiz->PartyColorIndex = Index % 6;\n",
            "\tif (AAetheriaHUD* Hud = Cast<AAetheriaHUD>(NewPlayer->GetHUD()))\n"
            "\t{\n"
            "\t\tHud->HideLobby();\n"
            "\t}\n"
            "\tif (AAetheriaWizardCharacter* Wiz = Cast<AAetheriaWizardCharacter>(NewPlayer->GetPawn()))\n"
            "\t{\n"
            "\t\tWiz->SetLobbyCameraMode(false);\n"
            "\t\tWiz->PartyColorIndex = Index % 6;\n",
        ),
    ],
)

# ---- textures / mesh load ----
patch(
    "Source/Aetheria/Private/AetheriaMaterialLibrary.cpp",
    [
        (
            "\tUTexture2D* Tex = FPackageName::DoesPackageExist(PkgPath) ? LoadObject<UTexture2D>(nullptr, *CookedPath) : nullptr;\n"
            "\n"
            "\tif (!Tex && !RawRelativePngPath.IsEmpty())\n",
            "\tUTexture2D* Tex = FPackageName::DoesPackageExist(PkgPath)\n"
            "\t\t? LoadObject<UTexture2D>(nullptr, *CookedPath, nullptr, LOAD_NoWarn | LOAD_Quiet)\n"
            "\t\t: nullptr;\n"
            "\n"
            "\tauto TryLoadTexPath = [&Tex](const TCHAR* ObjectPath)\n"
            "\t{\n"
            "\t\tif (!Tex && ObjectPath && *ObjectPath)\n"
            "\t\t{\n"
            "\t\t\tTex = LoadObject<UTexture2D>(nullptr, ObjectPath, nullptr, LOAD_NoWarn | LOAD_Quiet);\n"
            "\t\t}\n"
            "\t};\n"
            "\n"
            "\tif (AssetName == TEXT(\"T_GothicStone_Albedo\"))\n"
            "\t{\n"
            "\t\tTryLoadTexPath(TEXT(\"/Game/Props/SM_PropPillar/prop_pillar/Textures/cathedral_brick_albedo.cathedral_brick_albedo\"));\n"
            "\t}\n"
            "\telse if (AssetName == TEXT(\"T_GothicStone_Normal\"))\n"
            "\t{\n"
            "\t\tTryLoadTexPath(TEXT(\"/Game/Props/SM_PropPillar/prop_pillar/Textures/cathedral_brick_normal.cathedral_brick_normal\"));\n"
            "\t}\n"
            "\telse if (AssetName == TEXT(\"T_GothicStone_Roughness\"))\n"
            "\t{\n"
            "\t\tTryLoadTexPath(TEXT(\"/Game/Props/SM_PropPillar/prop_pillar/Textures/cathedral_brick_roughness.cathedral_brick_roughness\"));\n"
            "\t}\n"
            "\telse if (AssetName == TEXT(\"T_SanctumFlagstone_Albedo\"))\n"
            "\t{\n"
            "\t\tTryLoadTexPath(TEXT(\"/Game/Props/SM_PropPillar/prop_pillar/Textures/stone_floor_albedo.stone_floor_albedo\"));\n"
            "\t}\n"
            "\telse if (AssetName == TEXT(\"T_SanctumFlagstone_Normal\"))\n"
            "\t{\n"
            "\t\tTryLoadTexPath(TEXT(\"/Game/Props/SM_PropPillar/prop_pillar/Textures/stone_floor_normal.stone_floor_normal\"));\n"
            "\t}\n"
            "\telse if (AssetName == TEXT(\"T_SanctumFlagstone_Roughness\"))\n"
            "\t{\n"
            "\t\tTryLoadTexPath(TEXT(\"/Game/Props/SM_PropPillar/prop_pillar/Textures/stone_floor_roughness.stone_floor_roughness\"));\n"
            "\t}\n"
            "\n"
            "\tif (!Tex && !RawRelativePngPath.IsEmpty())\n",
        ),
        (
            "\t\t\t\tTex->UpdateResource();\n"
            "\t\t\t\t}\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\t}\n"
            "\n"
            "\tif (Tex)\n",
            "\t\t\t\tTex->UpdateResource();\n"
            "\t\t\t\t}\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\t}\n"
            "\n"
            "\tif (!Tex)\n"
            "\t{\n"
            "\t\tFString GodotRel;\n"
            "\t\tif (AssetName == TEXT(\"T_GothicStone_Albedo\")) GodotRel = TEXT(\"GodotAetheria/assets/textures/pbr/cathedral_brick_albedo.png\");\n"
            "\t\telse if (AssetName == TEXT(\"T_GothicStone_Normal\")) GodotRel = TEXT(\"GodotAetheria/assets/textures/pbr/cathedral_brick_normal.png\");\n"
            "\t\telse if (AssetName == TEXT(\"T_GothicStone_Roughness\")) GodotRel = TEXT(\"GodotAetheria/assets/textures/pbr/cathedral_brick_roughness.png\");\n"
            "\t\telse if (AssetName == TEXT(\"T_SanctumFlagstone_Albedo\")) GodotRel = TEXT(\"GodotAetheria/assets/textures/pbr/stone_floor_albedo.png\");\n"
            "\t\telse if (AssetName == TEXT(\"T_SanctumFlagstone_Normal\")) GodotRel = TEXT(\"GodotAetheria/assets/textures/pbr/stone_floor_normal.png\");\n"
            "\t\telse if (AssetName == TEXT(\"T_SanctumFlagstone_Roughness\")) GodotRel = TEXT(\"GodotAetheria/assets/textures/pbr/stone_floor_roughness.png\");\n"
            "\t\tif (!GodotRel.IsEmpty())\n"
            "\t\t{\n"
            "\t\t\tconst FString GodotFull = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir() / GodotRel);\n"
            "\t\t\tif (FPaths::FileExists(GodotFull))\n"
            "\t\t\t{\n"
            "\t\t\t\tTex = FImageUtils::ImportFileAsTexture2D(GodotFull);\n"
            "\t\t\t\tif (Tex)\n"
            "\t\t\t\t{\n"
            "\t\t\t\t\tTex->AddToRoot();\n"
            "\t\t\t\t\tif (bNormalMap)\n"
            "\t\t\t\t\t{\n"
            "\t\t\t\t\t\tTex->CompressionSettings = TC_Normalmap;\n"
            "\t\t\t\t\t\tTex->SRGB = false;\n"
            "\t\t\t\t\t\tTex->UpdateResource();\n"
            "\t\t\t\t\t}\n"
            "\t\t\t\t}\n"
            "\t\t\t}\n"
            "\t\t}\n"
            "\t}\n"
            "\n"
            "\tif (Tex)\n",
        ),
        (
            "\t\t\treturn LoadObject<UStaticMesh>(nullptr, *CookedPath);\n",
            "\t\t\treturn LoadObject<UStaticMesh>(nullptr, *CookedPath, nullptr, LOAD_NoWarn | LOAD_Quiet);\n",
        ),
    ],
)

# header method
patch(
    "Source/Aetheria/Public/AetheriaMaterialLibrary.h",
    [
        (
            "\tstatic UMaterialInterface* GetGothicStoneMaterial(UObject* Outer = nullptr);\n",
            "\tstatic UMaterialInterface* GetGothicStoneMaterial(UObject* Outer = nullptr);\n"
            "\n"
            "\t/** Build and save M_AetheriaGothicStone / M_AetheriaSanctumFloor if they are missing. */\n"
            "\tUFUNCTION(BlueprintCallable, Category = \"Aetheria|Materials\")\n"
            "\tstatic int32 EnsureBastionEnvironmentMaterials();\n",
        ),
    ],
)

# Append EnsureBastionEnvironmentMaterials after GetGothicStoneMaterial function
patch(
    "Source/Aetheria/Private/AetheriaMaterialLibrary.cpp",
    [
        (
            "\tif (!Mat) Mat = GetObsidianMaterial(Outer);\n"
            "\tCached = Mat;\n"
            "\treturn Mat;\n"
            "}\n"
            "\n"
            "UMaterialInterface* UAetheriaMaterialLibrary::GetDarkIronMaterial(UObject* Outer)\n",
            "\tif (!Mat) Mat = GetObsidianMaterial(Outer);\n"
            "\tCached = Mat;\n"
            "\treturn Mat;\n"
            "}\n"
            "\n"
            "int32 UAetheriaMaterialLibrary::EnsureBastionEnvironmentMaterials()\n"
            "{\n"
            "\tint32 Saved = 0;\n"
            "#if WITH_EDITOR\n"
            "\tauto SaveMat = [&Saved](UMaterialInterface* Mat)\n"
            "\t{\n"
            "\t\tif (!Mat) return;\n"
            "\t\tUPackage* Pkg = Mat->GetOutermost();\n"
            "\t\tif (!Pkg) return;\n"
            "\t\tMat->MarkPackageDirty();\n"
            "\t\tconst FString Filename = FPackageName::LongPackageNameToFilename(Pkg->GetName(), FPackageName::GetAssetPackageExtension());\n"
            "\t\tFSavePackageArgs SaveArgs;\n"
            "\t\tSaveArgs.TopLevelFlags = RF_Public | RF_Standalone;\n"
            "\t\tif (UPackage::SavePackage(Pkg, Mat, *Filename, SaveArgs))\n"
            "\t\t{\n"
            "\t\t\t++Saved;\n"
            "\t\t\tUE_LOG(LogTemp, Display, TEXT(\"[Aetheria] Saved material %s\"), *Pkg->GetName());\n"
            "\t\t}\n"
            "\t};\n"
            "\tSaveMat(GetGothicStoneMaterial());\n"
            "\tSaveMat(GetBiomeFloorMaterial(0));\n"
            "#endif\n"
            "\treturn Saved;\n"
            "}\n"
            "\n"
            "UMaterialInterface* UAetheriaMaterialLibrary::GetDarkIronMaterial(UObject* Outer)\n",
        ),
    ],
)

# ---- AnimInstance blend fill + smoother locomotion ----
patch(
    "Source/Aetheria/Public/AetheriaAnimInstance.h",
    [
        (
            "\tstatic bool BindSkeletalPresentation(\n"
            "\t\tACharacter* Character,\n"
            "\t\tconst FString& CharacterFolder,\n"
            "\t\tconst FString& PreferredMeshName,\n"
            "\t\tfloat CapsuleHalfHeight);\n"
            "\n"
            "private:\n",
            "\tstatic bool BindSkeletalPresentation(\n"
            "\t\tACharacter* Character,\n"
            "\t\tconst FString& CharacterFolder,\n"
            "\t\tconst FString& PreferredMeshName,\n"
            "\t\tfloat CapsuleHalfHeight);\n"
            "\n"
            "\t/** Editor-only: fill BS_Locomotion 2D blendspaces (Idle/Fwd/Back/Strafe). */\n"
            "\tUFUNCTION(BlueprintCallable, Category = \"Aetheria|Anim\")\n"
            "\tstatic int32 FillLocomotionBlendSpaces();\n"
            "\n"
            "private:\n",
        ),
        (
            "\tTWeakObjectPtr<UAnimSequence> CurrentLocomotion;\n"
            "};\n",
            "\tTWeakObjectPtr<UAnimSequence> CurrentLocomotion;\n"
            "\tfloat LastLocomotionDirection = 0.0f;\n"
            "};\n",
        ),
    ],
)

# Anim cpp includes + functions - read first lines to patch includes and TickLocomotion
anim_cpp = (ROOT / "Source/Aetheria/Private/AetheriaAnimInstance.cpp").read_text(encoding="utf-8")
if "FillLocomotionBlendSpaces" not in anim_cpp:
    print("will patch anim cpp includes + body via replacements")

patch(
    "Source/Aetheria/Private/AetheriaAnimInstance.cpp",
    [
        (
            "PlaySlotAnimationAsDynamicMontage(Desired, TEXT(\"DefaultSlot\"), 0.20f, 0.20f, 1.0f, -1);",
            "const float DirDelta = FMath::Abs(FMath::FindDeltaAngleDegrees(LastLocomotionDirection, Direction));\n"
            "\tLastLocomotionDirection = Direction;\n"
            "\tconst float BlendIn = (DirDelta > 35.0f) ? 0.35f : 0.18f;\n"
            "\tPlaySlotAnimationAsDynamicMontage(Desired, TEXT(\"DefaultSlot\"), BlendIn, 0.22f, 1.0f, -1);",
        ),
    ],
)

print("CHANGED:", ", ".join(changed) if changed else "(none)")
print("MISSING:", ", ".join(missing) if missing else "(none)")
sys.exit(1 if missing else 0)
