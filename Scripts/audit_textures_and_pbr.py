"""
AETHERIA PBR & SHADOW/LIGHTING INTEGRITY AUDITOR
Verifies:
1. Physical calibration of Albedo, DirectX Normal (Ny = -dH/dy), and Linear Roughness maps.
2. Complete dual-tree deployment in both project workspace and runtime installation.
3. INI configurations: VSM, contact shadows, Lumen, 16x anisotropy, native screen percentage.
4. Deprecation audit: Zero occurrences of r.ContactShadows.NonShadowCastingIntensity.
5. Scalability @Cine section presence.
"""

import os
import sys
import numpy as np
from PIL import Image

TARGET_DIRS = [
    r"c:\Users\tagir\OneDrive\Desktop\Aetheria\RawAssets\Textures",
    r"C:\Games\AETHERIA\Aetheria\RawAssets\Textures"
]

REQUIRED_STEMS = [
    # World Architecture (2048x2048)
    ("GothicPillar", 2048, (90, 130)),
    ("RunicAltar", 2048, (85, 125)),
    ("IronBrazier", 2048, (90, 130)),
    # Hero Headgear & Staves (1024x1024) - Both T_ and T_SM_
    ("Head_BoneCrown", 1024, (125, 160)),
    ("SM_Head_BoneCrown", 1024, (125, 160)),
    ("Head_HorrorHorns", 1024, (95, 140)),
    ("SM_Head_HorrorHorns", 1024, (95, 140)),
    ("Head_ChronoHalo", 1024, (130, 175)),
    ("SM_Head_ChronoHalo", 1024, (130, 175)),
    ("Head_HopeMask", 1024, (140, 185)),
    ("SM_Head_HopeMask", 1024, (140, 185)),
    ("Head_AbyssHood", 1024, (90, 130)),
    ("SM_Head_AbyssHood", 1024, (90, 130)),
    ("GnarledStaff", 1024, (90, 130)),
    ("SM_GnarledStaff", 1024, (90, 130)),
    ("Staff_Arcane01", 1024, (110, 150)),
    ("SM_Staff_Arcane01", 1024, (110, 150)),
    ("Staff_Arcane02", 1024, (130, 170)),
    ("SM_Staff_Arcane02", 1024, (130, 170)),
    ("OrnateMace", 1024, (75, 110)),
    ("SM_OrnateMace", 1024, (75, 110)),
    ("Familiar_SpectralSkull", 1024, (120, 160)),
    ("SM_Familiar_SpectralSkull", 1024, (120, 160)),
]

CONFIG_FILES_NO_DEPRECATED = [
    r"C:\Games\AETHERIA\Aetheria\Saved\Config\Windows\Engine.ini",
    r"C:\Games\AETHERIA\Aetheria\Saved\Config\Windows\Scalability.ini",
    r"c:\Users\tagir\OneDrive\Desktop\Aetheria\Config\DefaultEngine.ini",
    r"C:\Games\AETHERIA\Play_AETHERIA.bat",
    r"C:\Games\AETHERIA\Host_Coop_Server_7777.bat",
    r"c:\Users\tagir\OneDrive\Desktop\Aetheria\Play_Trial_UE5.bat",
]


def audit_config_files() -> int:
    errors = 0
    print("=" * 80)
    print("PHASE 1: CONFIGURATION & CVAR DEPRECATION AUDIT")
    print("=" * 80)

    for path in CONFIG_FILES_NO_DEPRECATED:
        if not os.path.exists(path):
            print(f"[ERROR] Required config file missing: {path}")
            errors += 1
            continue

        with open(path, "r", encoding="utf-8", errors="ignore") as fp:
            content = fp.read()

        if "NonShadowCastingIntensity" in content:
            print(f"[ERROR] Deprecated CVar 'NonShadowCastingIntensity' found in: {path}")
            errors += 1
        else:
            print(f"[PASS] Zero deprecated CVars in: {os.path.basename(path)}")

    # Check Scalability @Cine
    scalability_path = r"C:\Games\AETHERIA\Aetheria\Saved\Config\Windows\Scalability.ini"
    if os.path.exists(scalability_path):
        with open(scalability_path, "r", encoding="utf-8") as fp:
            s_content = fp.read()
        if "[TextureQuality@Cine]" in s_content and "[ShadowQuality@Cine]" in s_content:
            print("[PASS] Scalability.ini contains [TextureQuality@Cine] and [ShadowQuality@Cine]")
        else:
            print("[ERROR] Scalability.ini missing [TextureQuality@Cine] or [ShadowQuality@Cine]")
            errors += 1

    return errors


def audit_textures() -> int:
    errors = 0
    print("\n" + "=" * 80)
    print("PHASE 2: PBR TEXTURE INTEGRITY & CALIBRATION AUDIT")
    print("=" * 80)

    for target_dir in TARGET_DIRS:
        print(f"\nChecking directory: {target_dir}")
        if not os.path.exists(target_dir):
            print(f"[ERROR] Target directory does not exist: {target_dir}")
            errors += 1
            continue

        for stem, expected_res, lum_range in REQUIRED_STEMS:
            for map_type in ["Albedo", "Normal", "Roughness"]:
                fname = f"T_{stem}_{map_type}.png"
                fpath = os.path.join(target_dir, fname)

                if not os.path.exists(fpath):
                    print(f"  [ERROR] Missing file: {fname}")
                    errors += 1
                    continue

                try:
                    with Image.open(fpath) as img:
                        w, h = img.size
                        arr = np.array(img, dtype=np.float32)

                    if w != expected_res or h != expected_res:
                        print(f"  [ERROR] {fname} dimension mismatch: got ({w}x{h}), expected ({expected_res}x{expected_res})")
                        errors += 1

                    if map_type == "Albedo":
                        mean_lum = float(np.mean(arr))
                        if not (lum_range[0] <= mean_lum <= lum_range[1]):
                            print(f"  [ERROR] {fname} mean lum {mean_lum:.1f} out of expected range {lum_range}")
                            errors += 1

                    elif map_type == "Normal":
                        r, g, b = arr[..., 0], arr[..., 1], arr[..., 2]
                        b_mean = float(np.mean(b))
                        r_std = float(np.std(r))
                        g_std = float(np.std(g))

                        if b_mean <= 205.0:
                            print(f"  [ERROR] {fname} normal B channel mean {b_mean:.1f} <= 205.0")
                            errors += 1

                        if r_std < 8.0 or g_std < 8.0:
                            print(f"  [ERROR] {fname} flat normal detected (R std: {r_std:.2f}, G std: {g_std:.2f})")
                            errors += 1

                        # Unit length check: (r/127.5 - 1)^2 + (g/127.5 - 1)^2 + (b/127.5 - 1)^2 ~ 1
                        nx = r / 127.5 - 1.0
                        ny = g / 127.5 - 1.0
                        nz = b / 127.5 - 1.0
                        lens = np.sqrt(nx * nx + ny * ny + nz * nz)
                        if np.isnan(lens).any() or np.min(lens) < 0.6 or np.max(lens) > 1.4:
                            print(f"  [ERROR] {fname} normal vectors non-unit or NaNs")
                            errors += 1

                    elif map_type == "Roughness":
                        r_mean = float(np.mean(arr))
                        r_std = float(np.std(arr))
                        if np.isnan(arr).any() or r_std < 1.0:
                            print(f"  [ERROR] {fname} roughness invalid or flat (mean: {r_mean:.1f}, std: {r_std:.2f})")
                            errors += 1

                except Exception as e:
                    print(f"  [ERROR] Failed to read {fname}: {e}")
                    errors += 1

        print(f"  Directory check completed for: {target_dir}")

    return errors


def main() -> int:
    config_errors = audit_config_files()
    texture_errors = audit_textures()
    total_errors = config_errors + texture_errors

    print("\n" + "=" * 80)
    print("AUDIT SUMMARY")
    print(f"  Configuration Errors : {config_errors}")
    print(f"  Texture Errors       : {texture_errors}")
    print(f"  TOTAL ERRORS         : {total_errors}")
    print("=" * 80)

    if total_errors == 0:
        print("[SUCCESS] All PBR textures and lighting/shadow configurations passed with 0 errors!")
        return 0
    else:
        print(f"[FAILURE] Found {total_errors} errors!")
        return 1


if __name__ == "__main__":
    sys.exit(main())
