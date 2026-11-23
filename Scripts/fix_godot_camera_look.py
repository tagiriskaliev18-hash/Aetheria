# -*- coding: utf-8 -*-
"""Match Godot WizardPlayer camera + Bastion moonlight; fix AnimInstance compile."""
from pathlib import Path

ROOT = Path(r"C:\Users\tagir\OneDrive\Desktop\Aetheria")
missing = []
changed = []


def patch(rel, replacements):
    path = ROOT / rel
    raw = path.read_bytes()
    text = raw.decode("utf-8").replace("\r\n", "\n")
    orig = text
    for i, (old, new) in enumerate(replacements, 1):
        if old not in text:
            missing.append(f"{rel} #{i}")
            print(f"PATTERN MISSING {rel} #{i}")
            print(old[:200])
            continue
        text = text.replace(old, new, 1)
        print(f"patched {rel} #{i}")
    if text != orig:
        nl = "\r\n" if b"\r\n" in raw else "\n"
        path.write_bytes(text.replace("\n", nl).encode("utf-8"))
        changed.append(rel)


# --- AnimInstance compile fixes ---
patch(
    "Source/Aetheria/Private/AetheriaAnimInstance.cpp",
    [
        (
            '#include "Animation/BlendSpace.h"\n',
            '#include "Animation/BlendSpace.h"\n#include "UObject/SavePackage.h"\n',
        ),
        (
            '\t\tFBlendParameter ParamX = BS->GetBlendParameter(0);\n'
            '\t\tParamX.DisplayName = TEXT("Strafe");\n'
            '\t\tParamX.Min = -1.0f;\n'
            '\t\tParamX.Max = 1.0f;\n'
            '\t\tParamX.GridNum = 4;\n'
            '\t\tFBlendParameter ParamY = BS->GetBlendParameter(1);\n'
            '\t\tParamY.DisplayName = TEXT("Forward");\n'
            '\t\tParamY.Min = -1.0f;\n'
            '\t\tParamY.Max = 1.0f;\n'
            '\t\tParamY.GridNum = 4;\n'
            '\t\tBS->UpdateBlendParameter(0, ParamX);\n'
            '\t\tBS->UpdateBlendParameter(1, ParamY);\n',
            '\t\tFBlendParameter& AxisX = const_cast<FBlendParameter&>(BS->GetBlendParameter(0));\n'
            '\t\tAxisX.DisplayName = TEXT("Strafe");\n'
            '\t\tAxisX.Min = -1.0f;\n'
            '\t\tAxisX.Max = 1.0f;\n'
            '\t\tAxisX.GridNum = 4;\n'
            '\t\tFBlendParameter& AxisY = const_cast<FBlendParameter&>(BS->GetBlendParameter(1));\n'
            '\t\tAxisY.DisplayName = TEXT("Forward");\n'
            '\t\tAxisY.Min = -1.0f;\n'
            '\t\tAxisY.Max = 1.0f;\n'
            '\t\tAxisY.GridNum = 4;\n',
        ),
        (
            '\t\t\tBS->PostEditChange();\n',
            '\t\t\tBS->ValidateSampleData();\n'
            '\t\t\tBS->ResampleData();\n'
            '\t\t\tBS->PostEditChange();\n',
        ),
    ],
)

# --- Camera: exact Godot WizardPlayer.gd (metres * 100 = UU) ---
# spring 21.5m, pitch -52, yaw -45, fov 44, zoom 13.5-30m step 1.6m, lerp rate 10
# rig height 1.4m above feet ~ 48 UU above capsule center; no follow-lag
patch(
    "Source/Aetheria/Public/AetheriaWizardCharacter.h",
    [
        (
            "float DesiredCameraArmLength = 2600.0f;",
            "float DesiredCameraArmLength = 2150.0f;",
        ),
    ],
)

patch(
    "Source/Aetheria/Private/AetheriaWizardCharacter.cpp",
    [
        (
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
            "\t// Godot SpringArm3D: 21.5 m, rotation (-52, -45, 0), collision_mask 0.\n"
            "\tCameraBoom->TargetArmLength = 2150.0f;\n"
            "\tCameraBoom->SetRelativeRotation(FRotator(-52.0f, -45.0f, 0.0f));\n"
            "\tCameraBoom->bDoCollisionTest = false;\n"
            "\tCameraBoom->bInheritYaw = false;\n"
            "\tCameraBoom->bInheritPitch = false;\n"
            "\tCameraBoom->bInheritRoll = false;\n"
            "\tCameraBoom->bEnableCameraLag = false;\n"
            "\tCameraBoom->bEnableCameraRotationLag = false;\n"
            "\t// CameraRig sits 1.4 m above Godot feet (~48 UU above UE capsule center).\n"
            "\tCameraBoom->SocketOffset = FVector(0.0f, 0.0f, 48.0f);\n",
        ),
        (
            "\tTopDownCameraComponent->FieldOfView = 55.0f;\n",
            "\tTopDownCameraComponent->FieldOfView = 44.0f;\n",
        ),
        (
            "\t\tfloat TargetArmLength = DesiredCameraArmLength;\n"
            "\t\tFRotator TargetRot(-58.0f, -45.0f, 0.0f);\n"
            "\t\tFVector TargetSocketOffset = FVector(0.0f, 0.0f, 110.0f);\n",
            "\t\t// Godot: spring_length = lerp(..., desired - trauma*0.45 m, delta*10)\n"
            "\t\tconst float TraumaPunchUU = CameraShakeTrauma * 45.0f;\n"
            "\t\tfloat TargetArmLength = DesiredCameraArmLength - TraumaPunchUU;\n"
            "\t\tFRotator TargetRot(-52.0f, -45.0f, 0.0f);\n"
            "\t\tFVector TargetSocketOffset = FVector(0.0f, 0.0f, 48.0f);\n",
        ),
        (
            "\t\telse if (CameraShakeTrauma > 0.001f)\n"
            "\t\t{\n"
            "\t\t\tconst float TNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;\n"
            "\t\t\tconst float Mag = CameraShakeTrauma * CameraShakeTrauma * 24.0f;\n"
            "\t\t\tTargetSocketOffset = FVector(\n"
            "\t\t\t\tFMath::Sin(TNow * 53.0f) * Mag,\n"
            "\t\t\t\tFMath::Cos(TNow * 47.0f) * Mag,\n"
            "\t\t\t\tFMath::Sin(TNow * 61.0f) * Mag * 0.45f);\n"
            "\t\t}\n"
            "\n"
            "\t\tconst float InterpSpeed = (CameraShakeTrauma > 0.01f && !bIsInEditorMode && !bIsInLobbyMode) ? 18.0f : 7.5f;\n"
            "\t\tCameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, InterpSpeed);\n"
            "\t\tCameraBoom->SetRelativeRotation(FMath::RInterpTo(CameraBoom->GetRelativeRotation(), TargetRot, DeltaSeconds, InterpSpeed));\n"
            "\t\tCameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocketOffset, DeltaSeconds, InterpSpeed);\n",
            "\t\telse if (CameraShakeTrauma > 0.001f)\n"
            "\t\t{\n"
            "\t\t\t// Godot Camera3D h/v_offset: trauma^2 * 0.38 m\n"
            "\t\t\tconst float TNow = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;\n"
            "\t\t\tconst float Mag = CameraShakeTrauma * CameraShakeTrauma * 38.0f;\n"
            "\t\t\tTargetSocketOffset = FVector(\n"
            "\t\t\t\t48.0f + FMath::Sin(TNow * 52.0f) * Mag,\n"
            "\t\t\t\tFMath::Cos(TNow * 47.0f) * Mag,\n"
            "\t\t\t\t0.0f);\n"
            "\t\t\tCameraShakeTrauma = FMath::Max(0.0f, CameraShakeTrauma - DeltaSeconds * 2.8f);\n"
            "\t\t}\n"
            "\n"
            "\t\tconst float InterpSpeed = 10.0f; // Godot lerpf rate delta*10\n"
            "\t\tCameraBoom->TargetArmLength = FMath::FInterpTo(CameraBoom->TargetArmLength, TargetArmLength, DeltaSeconds, InterpSpeed);\n"
            "\t\tCameraBoom->SetRelativeRotation(FMath::RInterpTo(CameraBoom->GetRelativeRotation(), TargetRot, DeltaSeconds, InterpSpeed));\n"
            "\t\tCameraBoom->SocketOffset = FMath::VInterpTo(CameraBoom->SocketOffset, TargetSocketOffset, DeltaSeconds, InterpSpeed);\n",
        ),
        (
            "\t\tDesiredCameraArmLength = FMath::Clamp(DesiredCameraArmLength - WheelDelta * 200.0f, 1600.0f, 3800.0f);\n",
            "\t\t// Godot: wheel step 1.6 m, clamp 13.5-30.0 m\n"
            "\t\tDesiredCameraArmLength = FMath::Clamp(DesiredCameraArmLength - WheelDelta * 160.0f, 1350.0f, 3000.0f);\n",
        ),
    ],
)

# --- Lighting: Godot DungeonGenerator biome 0 moonlight ---
# moon_color (0.58, 0.70, 0.94) lerped 0.24 toward (0.58, 0.72, 0.98)
# energy 1.65, rotation (-56, 26, 0)
# fog (0.05, 0.06, 0.11), volumetric density 0.038
# exposure 1.08, contrast 1.15, saturation 1.12, glow 0.90 / bloom 0.26
patch(
    "Source/Aetheria/Private/AetheriaGameMode.cpp",
    [
        (
            '#include "Engine/PostProcessVolume.h"\n',
            '#include "Engine/PostProcessVolume.h"\n'
            '#include "Engine/SkyAtmosphere.h"\n'
            '#include "Components/SkyAtmosphereComponent.h"\n',
        ),
        (
            '\t\t\tif (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))\n'
            '\t\t\t{\n'
            '\t\t\t\tL->SetIntensity(14.0f);\n'
            '\t\t\t\tL->SetLightColor(FLinearColor(1.0f, 0.86f, 0.68f));\n'
            '\t\t\t\tL->SetAtmosphereSunLight(true);\n'
            '\t\t\t}\n',
            '\t\t\tif (UDirectionalLightComponent* L = Cast<UDirectionalLightComponent>(It->GetLightComponent()))\n'
            '\t\t\t{\n'
            '\t\t\t\t// Godot MoonDirectionalLight: energy 1.65, cool moon, rot (-56, 26, 0)\n'
            '\t\t\t\tIt->SetActorRotation(FRotator(-56.0f, 26.0f, 0.0f));\n'
            '\t\t\t\tL->SetIntensity(6.0f);\n'
            '\t\t\t\tL->SetLightColor(FLinearColor(0.58f, 0.705f, 0.95f));\n'
            '\t\t\t\tL->SetAtmosphereSunLight(true);\n'
            '\t\t\t\tL->SetVolumetricScatteringIntensity(2.4f);\n'
            '\t\t\t}\n',
        ),
        (
            '\t\t\t\tL->SetIntensity(3.2f);\n'
            '\t\t\t\tL->SetLightColor(FLinearColor(0.55f, 0.62f, 0.85f));\n',
            '\t\t\t\t// Godot ambient: moon darkened 0.72 * energy 0.22\n'
            '\t\t\t\tL->SetIntensity(1.15f);\n'
            '\t\t\t\tL->SetLightColor(FLinearColor(0.162f, 0.196f, 0.263f));\n',
        ),
        (
            '\t\t\t\tF->SetFogDensity(0.0075f);\n'
            '\t\t\t\tF->SetFogHeightFalloff(0.15f);\n'
            '\t\t\t\tF->SetFogInscatteringColor(FLinearColor(0.045f, 0.05f, 0.08f));\n'
            '\t\t\t\tF->SetVolumetricFog(true);\n',
            '\t\t\t\t// Godot volumetric fog density 0.038, albedo = fog_col lightened 0.24\n'
            '\t\t\t\tF->SetFogDensity(0.022f);\n'
            '\t\t\t\tF->SetFogHeightFalloff(0.12f);\n'
            '\t\t\t\tF->SetFogInscatteringColor(FLinearColor(0.278f, 0.286f, 0.328f));\n'
            '\t\t\t\tF->SetVolumetricFog(true);\n'
            '\t\t\t\tF->SetVolumetricFogScatteringDistribution(0.72f);\n',
        ),
        (
            '\t\t\tS.AutoExposureBias = 1.15f;\n'
            '\t\t\tS.bOverride_BloomIntensity = true;\n'
            '\t\t\tS.BloomIntensity = 0.45f;\n'
            '\t\t\tS.bOverride_VignetteIntensity = true;\n'
            '\t\t\tS.VignetteIntensity = 0.16f;\n'
            '\t\t\tS.bOverride_MotionBlurAmount = true;\n'
            '\t\t\tS.MotionBlurAmount = 0.12f;\n'
            '\t\t\tS.bOverride_ColorContrast = true;\n'
            '\t\t\tS.ColorContrast = FVector4(1.08f, 1.08f, 1.08f, 1.0f);\n'
            '\t\t\tS.bOverride_ColorSaturation = true;\n'
            '\t\t\tS.ColorSaturation = FVector4(1.06f, 1.03f, 1.08f, 1.0f);\n'
            '\t\t\tS.bOverride_AmbientOcclusionIntensity = true;\n'
            '\t\t\tS.AmbientOcclusionIntensity = 0.5f;\n',
            '\t\t\tS.AutoExposureBias = 0.55f; // Godot tonemap_exposure 1.08, manual, readable but moody\n'
            '\t\t\tS.bOverride_BloomIntensity = true;\n'
            '\t\t\tS.BloomIntensity = 0.85f;\n'
            '\t\t\tS.bOverride_BloomThreshold = true;\n'
            '\t\t\tS.BloomThreshold = 0.85f;\n'
            '\t\t\tS.bOverride_VignetteIntensity = true;\n'
            '\t\t\tS.VignetteIntensity = 0.22f;\n'
            '\t\t\tS.bOverride_MotionBlurAmount = true;\n'
            '\t\t\tS.MotionBlurAmount = 0.10f;\n'
            '\t\t\tS.bOverride_ColorContrast = true;\n'
            '\t\t\tS.ColorContrast = FVector4(1.15f, 1.15f, 1.15f, 1.0f);\n'
            '\t\t\tS.bOverride_ColorSaturation = true;\n'
            '\t\t\tS.ColorSaturation = FVector4(1.12f, 1.12f, 1.12f, 1.0f);\n'
            '\t\t\tS.bOverride_AmbientOcclusionIntensity = true;\n'
            '\t\t\tS.AmbientOcclusionIntensity = 0.85f;\n'
            '\t\t\tS.bOverride_AmbientOcclusionRadius = true;\n'
            '\t\t\tS.AmbientOcclusionRadius = 180.0f;\n',
        ),
    ],
)

# Hide default bright sky atmosphere so the horizon matches Godot BG_COLOR fog*0.45
gm = ROOT / "Source/Aetheria/Private/AetheriaGameMode.cpp"
raw = gm.read_bytes()
t = raw.decode("utf-8").replace("\r\n", "\n")
needle = "\t\tfor (TActorIterator<APostProcessVolume> It(World); It; ++It)\n"
sky = (
    "\t\tfor (TActorIterator<ASkyAtmosphere> It(World); It; ++It)\n"
    "\t\t{\n"
    "\t\t\tIt->SetActorHiddenInGame(true);\n"
    "\t\t\tIt->SetActorEnableCollision(false);\n"
    "\t\t\tif (USkyAtmosphereComponent* C = It->GetComponent())\n"
    "\t\t\t{\n"
    "\t\t\t\tC->SetVisibility(false);\n"
    "\t\t\t}\n"
    "\t\t}\n"
    "\t\tfor (TActorIterator<APostProcessVolume> It(World); It; ++It)\n"
)
if "TActorIterator<ASkyAtmosphere>" not in t:
    if needle not in t:
        missing.append("GameMode SkyAtmosphere insert")
        print("PATTERN MISSING SkyAtmosphere insert")
    else:
        t = t.replace(needle, sky, 1)
        gm.write_bytes(t.replace("\n", "\r\n" if b"\r\n" in raw else "\n").encode("utf-8"))
        print("inserted SkyAtmosphere hide")
        if "Source/Aetheria/Private/AetheriaGameMode.cpp" not in changed:
            changed.append("Source/Aetheria/Private/AetheriaGameMode.cpp")
else:
    print("SkyAtmosphere hide already present")

print("CHANGED:", ", ".join(changed) if changed else "(none)")
print("MISSING:", ", ".join(missing) if missing else "(none)")
raise SystemExit(1 if missing else 0)
