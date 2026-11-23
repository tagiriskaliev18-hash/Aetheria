"""
Rebuild AAA Diablo IV Slate HUD and Grimoire Texture Kit.

Fixes all pixel geometry, aspect ratios, alpha cutouts, cradle ring holes,
glass rim transparencies, 3D alchemical globe liquids, and 9-slice button borders
observed in live 1920x1080 runtime HUD captures.

Outputs identical assets to:
  - C:\\Games\\AETHERIA\\Aetheria\\RawAssets\\UI\\ (+ Icons\\) and RawAssets\\Grimoire\\
  - c:\\Users\\tagir\\OneDrive\\Desktop\\Aetheria\\RawAssets\\UI\\ (+ Icons\\) and RawAssets\\Grimoire\\
  - c:\\Users\\tagir\\OneDrive\\Desktop\\Aetheria\\UI\\ (+ Icons\\) and Grimoire\\
  - c:\\Users\\tagir\\OneDrive\\Desktop\\Aetheria\\_backup_web_prototype\\public\\ui\\ (if present)
"""

import math
import shutil
from collections import deque
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageEnhance, ImageFilter
from scipy.interpolate import PchipInterpolator
from scipy.ndimage import binary_closing, binary_erosion, binary_fill_holes, label, map_coordinates

ROOT = Path(__file__).resolve().parent.parent
BACKUP_DIR = Path(r"C:\Users\tagir\.gemini\antigravity\brain\e1af9820-2ab0-45d5-8cb4-364efea981db\scratch\backup_20261116\RawAssets")

SYNC_UI_DIRS = [
    Path(r"C:\Games\AETHERIA\Aetheria\RawAssets\UI"),
    ROOT / "RawAssets" / "UI",
    ROOT / "UI",
]
SYNC_GRIMOIRE_DIRS = [
    Path(r"C:\Games\AETHERIA\Aetheria\RawAssets\Grimoire"),
    ROOT / "RawAssets" / "Grimoire",
    ROOT / "Grimoire",
]
WEB_PROTO_DIR = ROOT / "_backup_web_prototype" / "public" / "ui"
if WEB_PROTO_DIR.exists():
    SYNC_UI_DIRS.append(WEB_PROTO_DIR)


def save_image_to_all(img: Image.Image, filename: str, target_dirs: list[Path], is_icon: bool = False) -> None:
    """Save an image identically across all destination directories safely against file locks."""
    for base_dir in target_dirs:
        dest_dir = base_dir / "Icons" if is_icon else base_dir
        dest_dir.mkdir(parents=True, exist_ok=True)
        out_path = dest_dir / filename
        tmp_path = dest_dir / f"_{filename}.tmp.png"
        try:
            img.save(tmp_path, format="PNG", optimize=True)
            if out_path.exists():
                out_path.unlink()
            tmp_path.replace(out_path)
        except Exception:
            img.save(out_path, format="PNG")
            if tmp_path.exists():
                tmp_path.unlink(missing_ok=True)


def _rounded_rect_sdf(h: int, w: int, margin_x: float, margin_y: float, radius: float) -> np.ndarray:
    """Exact signed distance to inner rounded rectangle (negative inside, positive outside)."""
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    cx = (w - 1) * 0.5
    cy = (h - 1) * 0.5
    hx = max(1.0, cx - margin_x - radius)
    hy = max(1.0, cy - margin_y - radius)
    qx = np.abs(xx - cx) - hx
    qy = np.abs(yy - cy) - hy
    outside = np.sqrt(np.maximum(qx, 0.0) ** 2 + np.maximum(qy, 0.0) ** 2)
    inside = np.minimum(np.maximum(qx, qy), 0.0)
    return outside + inside - radius


def make_fbm_noise(h: int, w: int, seed: int = 42, octaves: tuple[int, ...] = (4, 12, 28, 64)) -> np.ndarray:
    """Vectorized multi-octave Perlin/value noise synthesis."""
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w), dtype=np.float32)
    weight = 0.55
    total_weight = 0.0
    for grid in octaves:
        gh = max(2, grid)
        gw = max(2, int(round(grid * (w / max(1, h)))))
        small = rng.uniform(0.0, 1.0, size=(gh, gw)).astype(np.float32)
        im = Image.fromarray(small, mode="F").resize((w, h), Image.Resampling.BICUBIC)
        acc += np.asarray(im, dtype=np.float32) * weight
        total_weight += weight
        weight *= 0.50
    return acc / max(1e-5, total_weight)


# ==============================================================================
# 1. ACTION DOCK PEDESTAL (1160x268 RGBA)
# ==============================================================================
def rebuild_action_dock_pedestal() -> Image.Image:
    """
    Rebuild exact 1160x268 Action Dock Pedestal.
    Aligns Left cradle to (160, 134) and Right cradle to (1000, 134) with uniform local scaling.
    Repairs missing/floodfilled 5-7 o'clock cradle holes via intact upper ring sampling and
    concentric bronze/iron band synthesis, reinforcing solid 100% alpha across [72..108].
    Carves crisp circular transparent openings cleanly at r <= 72 (with AA ramp at 72..74)
    with ZERO black inner moons.
    Smooths the lower outer boundary where the bottom cradle rings join the horizontal trim (y=206)
    with ZERO jagged semicircular steps.
    Solid carved obsidian backing trough across x in [272, 888], y in [82, 204].
    """
    src_path = BACKUP_DIR / "UI" / "T_UI_ActionDockPedestal.png"
    src_im = Image.open(src_path).convert("RGBA")
    src_arr = np.array(src_im, dtype=np.float32)
    h_src, w_src = src_arr.shape[:2]

    h_dst, w_dst = 268, 1160

    # PCHIP monotonic mapping ensuring cradle centers land exactly at (160, 134) and (1000, 134)
    # with uniform aspect ratio around the cradles
    x_knots_dst = [0.0, 160.0, 320.0, 580.0, 840.0, 1000.0, float(w_dst - 1)]
    x_knots_src = [0.0, 277.0, 545.0, 800.0, 1055.0, 1323.0, float(w_src - 1)]
    pchip_x = PchipInterpolator(x_knots_dst, x_knots_src)
    x_mapped = pchip_x(np.arange(w_dst, dtype=np.float32))

    y_knots_dst = [0.0, 134.0, float(h_dst - 1)]
    y_knots_src = [0.0, 231.3, float(h_src - 1)]
    pchip_y = PchipInterpolator(y_knots_dst, y_knots_src)
    y_mapped = pchip_y(np.arange(h_dst, dtype=np.float32))

    yy_mapped, xx_mapped = np.meshgrid(y_mapped, x_mapped, indexing="ij")
    coords = [yy_mapped, xx_mapped]

    warped = np.zeros((h_dst, w_dst, 4), dtype=np.float32)
    for c in range(4):
        warped[..., c] = map_coordinates(src_arr[..., c], coords, order=1, mode="nearest")

    yy, xx = np.mgrid[0:h_dst, 0:w_dst].astype(np.float32)

    # Reconstruct missing bottom pixels on both left and right cradle rings
    for cx, cy in [(160.0, 134.0), (1000.0, 134.0)]:
        dx = xx - cx
        dy = yy - cy
        r = np.sqrt(dx * dx + dy * dy)

        annulus = (r >= 72.0) & (r <= 108.0)
        bottom_half = annulus & (dy > 0.0)

        # Mirror coordinates from upper ring
        y_mirr = np.clip(np.round(cy - dy).astype(int), 0, h_dst - 1)
        x_mirr = np.clip(np.round(cx + dx).astype(int), 0, w_dst - 1)

        lum = warped[..., :3].max(axis=2)
        mirr_a = warped[y_mirr, x_mirr, 3]
        mirr_lum = warped[y_mirr, x_mirr, :3].max(axis=2)

        # Defective if alpha < 245 or crushed black or white/grey floodfill noise
        defective = bottom_half & ((warped[..., 3] < 245.0) | (lum < 15.0) | (lum > 105.0))
        valid_mirr = defective & (mirr_a >= 240.0) & (mirr_lum >= 18.0) & (mirr_lum <= 105.0)

        warped[valid_mirr, :3] = warped[y_mirr[valid_mirr], x_mirr[valid_mirr], :3]
        warped[valid_mirr, 3] = 255.0

        # For any remaining defective pixels, synthesize clean concentric bronze/iron band
        still_def = defective & (~valid_mirr)
        if np.any(still_def):
            r_sd = r[still_def]
            fill = np.zeros((len(r_sd), 3), dtype=np.float32)
            m_in = r_sd < 84.0
            t_in = np.clip((r_sd[m_in] - 72.0) / 12.0, 0.0, 1.0)
            fill[m_in] = np.array([65.0, 55.0, 42.0]) * (1.0 - t_in[:, None]) + np.array([92.0, 78.0, 58.0]) * t_in[:, None]

            m_mid = (r_sd >= 84.0) & (r_sd < 96.0)
            fill[m_mid] = np.array([56.0, 52.0, 47.0])

            m_out = r_sd >= 96.0
            t_out = np.clip((r_sd[m_out] - 96.0) / 12.0, 0.0, 1.0)
            fill[m_out] = np.array([72.0, 62.0, 48.0]) * (1.0 - t_out[:, None]) + np.array([55.0, 52.0, 48.0]) * t_out[:, None]

            warped[still_def, :3] = fill
            warped[still_def, 3] = 255.0

        # Solid 100% alpha across core cradle ring annulus [72..108]
        warped[annulus, 3] = 255.0

        # Outer bevel rim in [108..112]
        outer_rim = (r > 108.0) & (r <= 112.0)
        bevel_alpha = np.clip((112.0 - r) / 4.0, 0.0, 1.0) * 255.0
        warped[outer_rim, 3] = np.maximum(warped[outer_rim, 3], bevel_alpha[outer_rim])
        outer_def = outer_rim & ((warped[..., 3] < 50.0) | (warped[..., :3].max(axis=2) < 15.0) | (warped[..., :3].max(axis=2) > 105.0))
        warped[outer_def, :3] = np.array([52.0, 48.0, 44.0])

        # Beyond 112 at bottom: transparent
        far_bot = (r > 112.0) & (dy > 0.0) & (dx < 50.0 if cx < 500 else dx > -50.0)
        warped[far_bot, 3] = 0.0
        warped[far_bot, :3] = 0.0

        # Crisp circular AA transparent opening for r <= 72px (AA ramp at 72..74)
        inner_aa = np.clip((r - 72.0) / 2.0, 0.0, 1.0)
        inner_hole = r < 74.0
        warped[inner_hole, 3] = np.minimum(warped[inner_hole, 3], inner_aa[inner_hole] * 255.0)
        warped[r <= 72.0, :3] = 0.0

    # Solid carved obsidian backing trough across x in [272, 888], y in [82, 204]
    trough_mask = (xx >= 272.0) & (xx <= 888.0) & (yy >= 82.0) & (yy <= 204.0)
    fill_trough = trough_mask & (warped[..., 3] < 255.0)
    trough_base_rgb = np.array([12.0, 11.0, 16.0], dtype=np.float32)
    for c in range(3):
        warped[fill_trough, c] = trough_base_rgb[c]
    warped[trough_mask, 3] = 255.0

    # Smooth bottom silhouette boundary across the entire dock
    # Naturally joins circular cradles to horizontal bar y=206 with zero step
    y_bot = np.full(w_dst, 206.0, dtype=np.float32)
    left_arc = (xx[0] < 243.2) & (xx[0] >= 50.0)
    dx_l = xx[0][left_arc] - 160.0
    y_bot[left_arc] = 134.0 + np.sqrt(np.maximum(0.0, 110.0**2 - dx_l**2))

    right_arc = (xx[0] > 916.8) & (xx[0] <= 1110.0)
    dx_r = xx[0][right_arc] - 1000.0
    y_bot[right_arc] = 134.0 + np.sqrt(np.maximum(0.0, 110.0**2 - dx_r**2))

    for x_idx in range(50, 1110):
        yb = y_bot[x_idx]
        y_mask = (yy[:, x_idx] >= yb - 1.0) & (yy[:, x_idx] <= yb + 2.5)
        aa_factor = np.clip((yb + 2.0 - yy[y_mask, x_idx]) / 2.5, 0.0, 1.0)
        warped[y_mask, x_idx, 3] = np.minimum(warped[y_mask, x_idx, 3], aa_factor * 255.0)
        warped[yy[:, x_idx] > yb + 2.5, x_idx, 3] = 0.0
        warped[yy[:, x_idx] > yb + 2.5, x_idx, :3] = 0.0

    # Ensure zero white halos outside silhouette: clear RGB where alpha == 0
    zero_alpha = warped[..., 3] < 1.0
    warped[zero_alpha, :3] = 0.0

    clamped = np.clip(warped, 0, 255).astype(np.uint8)
    return Image.fromarray(clamped, mode="RGBA")


# ==============================================================================
# 2. GLOBE GLASS RIM (512x512 RGBA)
# ==============================================================================
def rebuild_globe_glass_rim() -> Image.Image:
    """
    Rebuild 512x512 Globe Glass Rim.
    Repairs 7 o'clock claw notch by enforcing 4-way rotational symmetry on dragon-claw bezel.
    Inside rn < 0.585: eliminates opaque white crescent, sets pure transparent dark glass
    with contact vignette [0.47..0.585], top-left catchlight arc (10:30, alpha<=0.24),
    and soft bottom-right caustic (4:30, alpha<=0.12).
    """
    src_path = BACKUP_DIR / "UI" / "T_UI_GlobeGlassRim.png"
    src_arr = np.array(Image.open(src_path).convert("RGBA"), dtype=np.float32)
    h, w = src_arr.shape[:2]

    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    c = (w - 1) * 0.5
    dx = (xx - c) / c
    dy = (yy - c) / c
    rn = np.sqrt(dx * dx + dy * dy)
    theta = np.arctan2(dy, dx)

    # 4-way rotational symmetry on bezel band rn in [0.585, 0.96]
    x_rot90 = np.clip(np.round(c - dy * c).astype(int), 0, w - 1)
    y_rot90 = np.clip(np.round(c + dx * c).astype(int), 0, h - 1)
    x_rot180 = np.clip(np.round(c - dx * c).astype(int), 0, w - 1)
    y_rot180 = np.clip(np.round(c - dy * c).astype(int), 0, h - 1)
    x_rot270 = np.clip(np.round(c + dy * c).astype(int), 0, w - 1)
    y_rot270 = np.clip(np.round(c - dx * c).astype(int), 0, h - 1)

    bezel_core = (rn >= 0.585) & (rn <= 0.96)
    alphas = [
        src_arr[..., 3],
        src_arr[y_rot90, x_rot90, 3],
        src_arr[y_rot180, x_rot180, 3],
        src_arr[y_rot270, x_rot270, 3],
    ]
    rgbs = [
        src_arr[..., :3],
        src_arr[y_rot90, x_rot90, :3],
        src_arr[y_rot180, x_rot180, :3],
        src_arr[y_rot270, x_rot270, :3],
    ]

    repaired = src_arr.copy()
    cur_a = src_arr[..., 3]
    # Repair any low alpha defect in the bezel band (rn in [0.585, 0.96]) by borrowing intact metal from rotated quadrants
    for i in [1, 2, 3]:
        fix_mask = (rn >= 0.585) & (cur_a < 245.0) & (alphas[i] >= 245.0)
        repaired[fix_mask, :3] = rgbs[i][fix_mask]
        repaired[fix_mask, 3] = alphas[i][fix_mask]
        cur_a = repaired[..., 3]

    # Enforce solid 100% alpha strictly across the core circular bronze ring band
    repaired[(rn >= 0.595) & (rn <= 0.845), 3] = 255.0

    # Ensure zero white halos outside: clear RGB where alpha < 1
    repaired[repaired[..., 3] < 1.0, :3] = 0.0

    # Choke outer perimeter edge by ~1px (rn > 0.982)
    outer_choke = rn > 0.982
    choke_factor = np.clip((0.992 - rn[outer_choke]) / 0.010, 0.0, 1.0)
    repaired[outer_choke, 3] *= choke_factor

    # Glass liquid window inside rn < 0.585
    inner_mask = rn < 0.585
    # Reset interior to dark transparent glass
    repaired[inner_mask, :3] = [4.0, 3.0, 5.0]
    repaired[inner_mask, 3] = 0.0

    # Contact vignette for rn in [0.47, 0.585]: alpha ramping 0 -> 0.72 * 255
    vig_mask = (rn >= 0.47) & (rn < 0.585)
    t_vig = (rn[vig_mask] - 0.47) / (0.585 - 0.47)
    repaired[vig_mask, :3] = [4.0, 3.0, 5.0]
    repaired[vig_mask, 3] = t_vig * 0.72 * 255.0

    # Curved top-left catchlight arc around 10:30 (theta ~ -2.356 rad)
    ang_1030 = -2.356
    ang_diff = np.abs(np.arctan2(np.sin(theta - ang_1030), np.cos(theta - ang_1030)))
    catch_r = np.exp(-((rn - 0.525) / 0.024) ** 2)
    catch_a = np.exp(-(ang_diff / 0.26) ** 2)
    catch_intensity = catch_r * catch_a
    catch_mask = inner_mask & (catch_intensity > 0.015)
    # Soft specular catchlight with warm off-white RGB (alpha <= 0.20 * 255)
    catch_alpha = np.clip(catch_intensity * 0.20 * 255.0, 0.0, 51.0)

    # Soft caustic glint at 4:30 (theta ~ 0.785 rad)
    ang_430 = 0.785
    ang_diff_430 = np.abs(np.arctan2(np.sin(theta - ang_430), np.cos(theta - ang_430)))
    caust_r = np.exp(-((rn - 0.50) / 0.035) ** 2)
    caust_a = np.exp(-(ang_diff_430 / 0.32) ** 2)
    caust_intensity = caust_r * caust_a
    caust_mask = inner_mask & (caust_intensity > 0.015)
    # Strictly alpha <= 0.10 * 255 (~25.5) for soft caustics
    caust_alpha = np.clip(caust_intensity * 0.10 * 255.0, 0.0, 25.5)

    # Warm off-white highlight
    warm_white_hl = np.array([250.0, 244.0, 235.0], dtype=np.float32)
    for c_idx in range(3):
        repaired[catch_mask, c_idx] = warm_white_hl[c_idx]
    repaired[catch_mask, 3] = catch_alpha[catch_mask]

    caust_hl = np.array([195.0, 218.0, 245.0], dtype=np.float32)
    for c_idx in range(3):
        repaired[caust_mask & (~catch_mask), c_idx] = caust_hl[c_idx]
    repaired[caust_mask & (~catch_mask), 3] = caust_alpha[caust_mask & (~catch_mask)]

    # Strictly zero RGB where alpha < 8
    low_a = repaired[..., 3] < 8.0
    repaired[low_a, :3] = 0.0
    repaired[low_a, 3] = 0.0

    # Choke semi-transparent outer edges so brightNearTransparentEdge == 0
    semi_outer = (repaired[..., 3] >= 8.0) & (repaired[..., 3] < 40.0) & (rn > 0.60)
    repaired[semi_outer, :3] *= (repaired[semi_outer, 3:] / 40.0)

    return Image.fromarray(np.clip(repaired, 0, 255).astype(np.uint8), mode="RGBA")


# ==============================================================================
# 3. GLOBE LIQUIDS & BACK (512x512 RGBA)
# ==============================================================================
def rebuild_globe_liquids() -> tuple[Image.Image, Image.Image, Image.Image]:
    """
    Rebuild 512x512 GlobeBack, GlobeFill_HP, and GlobeFill_MP with 3D volumetric sphere lighting.
    GlobeBack: deep smoked-obsidian cavity (RGB 5,3,4 -> 1,1,2), ZERO crosshairs or radar rings!
    GlobeFill_HP & MP: rich arterial blood marbling & swirling sapphire lightning, outer perimeter
    fading cleanly in r=0.960..0.975 to alpha=0 with RGB darkened smoothly (ZERO bright grey hairline).
    """
    size = 512
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = (size - 1) * 0.5
    dx = (xx - c) / c
    dy = (yy - c) / c
    r = np.sqrt(dx * dx + dy * dy)

    # 1. GlobeBack: pure deep smoked-obsidian cavity, ZERO crosshairs or radar rings!
    # Depth glow at center (5, 3, 4) fading smoothly to (1, 1, 2) at perimeter (r=0.96..0.975)
    back_rgb = np.zeros((size, size, 3), dtype=np.float32)
    depth_t = np.clip(1.0 - (r / 0.96) ** 1.8, 0.0, 1.0)
    center_glow = np.array([5.0, 3.0, 4.0], dtype=np.float32)
    perim_col = np.array([1.0, 1.0, 2.0], dtype=np.float32)
    back_rgb = perim_col * (1.0 - depth_t[..., None]) + center_glow * depth_t[..., None]

    # Soft inner circular edge: alpha 255 out to r=0.96, choking to 0 at r=0.975
    back_alpha = np.clip((0.975 - r) / (0.975 - 0.960), 0.0, 1.0) * 255.0
    back_alpha[r <= 0.960] = 255.0

    # Black RGB strictly where alpha < 1
    back_rgb[back_alpha < 1.0] = 0.0
    back_rgba = np.dstack([np.clip(back_rgb, 0, 255), back_alpha]).astype(np.uint8)
    img_back = Image.fromarray(back_rgba, mode="RGBA")

    # Liquid Alpha for HP/MP: solid 255 inside r <= 0.960, choke outward to 0 at r=0.975
    alpha_liq = np.clip((0.975 - r) / (0.975 - 0.960), 0.0, 1.0) * 255.0
    alpha_liq[r <= 0.960] = 255.0

    # Smooth darkening toward dark obsidian edge at r in [0.90..0.975] so ZERO bright grey hairline
    edge_darken = 1.0 - np.clip((r - 0.90) / (0.975 - 0.90), 0.0, 1.0)[..., None] * 0.85

    # 2. HP Globe: Expand authentic high-contrast cellular filaments from backup
    src_hp_path = BACKUP_DIR / "UI" / "T_UI_GlobeFill_HP.png"
    src_hp_im = Image.open(src_hp_path).convert("RGBA")
    src_hp_arr = np.array(src_hp_im, dtype=np.float32)

    scale_hp = 0.845 / 0.975
    src_hx = c + dx * scale_hp * c
    src_hy = c + dy * scale_hp * c

    hp_sampled = np.zeros((size, size, 3), dtype=np.float32)
    for i in range(3):
        hp_sampled[..., i] = map_coordinates(src_hp_arr[..., i], [src_hy, src_hx], order=3, mode="reflect")

    lum_hp = hp_sampled[..., 0] * 0.7 + hp_sampled[..., 1] * 0.2 + hp_sampled[..., 2] * 0.1
    lum_hp_norm = np.clip((lum_hp - 50.0) / (240.0 - 50.0), 0.0, 1.0)

    venous = np.array([36.0, 3.0, 6.0], dtype=np.float32)
    arterial = np.array([195.0, 16.0, 22.0], dtype=np.float32)
    caustic = np.array([255.0, 82.0, 58.0], dtype=np.float32)

    f_curve_hp = lum_hp_norm ** 1.30
    hp_rgb = np.zeros_like(hp_sampled)
    for i in range(3):
        low_mid = venous[i] * (1.0 - f_curve_hp * 1.35) + arterial[i] * (f_curve_hp * 1.35)
        mid_high = arterial[i] * (1.0 - (f_curve_hp - 0.72) / 0.28) + caustic[i] * ((f_curve_hp - 0.72) / 0.28)
        hp_rgb[..., i] = np.where(f_curve_hp < 0.72, low_mid, mid_high)

    # Spherical vignette (deep venous rim at r > 0.65)
    t_rim = np.clip((r - 0.65) / (0.975 - 0.65), 0.0, 1.0)
    spherical_vignette = 1.0 - (t_rim ** 1.8) * 0.62
    hp_rgb *= spherical_vignette[..., None]

    # Subsurface light bloom from top-left (-0.22, -0.28)
    subsurface = np.clip(1.0 - np.sqrt((dx + 0.22) ** 2 + (dy + 0.28) ** 2) * 1.0, 0.0, 1.0) ** 2.0
    hp_rgb += caustic * (subsurface[..., None] * 0.26)

    # Darken outer edge toward dark obsidian edge
    hp_rgb *= edge_darken
    hp_rgb[alpha_liq < 1.0] = 0.0
    hp_rgba = np.dstack([np.clip(hp_rgb, 0, 255), alpha_liq]).astype(np.uint8)
    img_hp = Image.fromarray(hp_rgba, mode="RGBA")

    # 3. MP Globe: Expand authentic swirling arcane eddies from backup
    src_mp_path = BACKUP_DIR / "UI" / "T_UI_GlobeFill_MP.png"
    src_mp_im = Image.open(src_mp_path).convert("RGBA")
    src_mp_arr = np.array(src_mp_im, dtype=np.float32)

    scale_mp = 0.845 / 0.975
    src_mx = c + dx * scale_mp * c
    src_my = c + dy * scale_mp * c

    mp_sampled = np.zeros((size, size, 3), dtype=np.float32)
    for i in range(3):
        mp_sampled[..., i] = map_coordinates(src_mp_arr[..., i], [src_my, src_mx], order=3, mode="reflect")

    lum_mp = mp_sampled[..., 0] * 0.1 + mp_sampled[..., 1] * 0.3 + mp_sampled[..., 2] * 0.6
    lum_mp_norm = np.clip((lum_mp - 35.0) / (230.0 - 35.0), 0.0, 1.0)

    deep_violet = np.array([6.0, 8.0, 38.0], dtype=np.float32)
    royal_cobalt = np.array([18.0, 92.0, 225.0], dtype=np.float32)
    arcane_cyan = np.array([115.0, 238.0, 255.0], dtype=np.float32)

    f_curve_mp = lum_mp_norm ** 1.30
    mp_rgb = np.zeros_like(mp_sampled)
    for i in range(3):
        low_mid = deep_violet[i] * (1.0 - f_curve_mp * 1.35) + royal_cobalt[i] * (f_curve_mp * 1.35)
        mid_high = royal_cobalt[i] * (1.0 - (f_curve_mp - 0.72) / 0.28) + arcane_cyan[i] * ((f_curve_mp - 0.72) / 0.28)
        mp_rgb[..., i] = np.where(f_curve_mp < 0.72, low_mid, mid_high)

    mp_rgb *= spherical_vignette[..., None]
    mp_rgb += arcane_cyan * (subsurface[..., None] * 0.26)

    # Darken outer edge toward dark obsidian edge
    mp_rgb *= edge_darken
    mp_rgb[alpha_liq < 1.0] = 0.0
    mp_rgba = np.dstack([np.clip(mp_rgb, 0, 255), alpha_liq]).astype(np.uint8)
    img_mp = Image.fromarray(mp_rgba, mode="RGBA")

    return img_back, img_hp, img_mp


# ==============================================================================
# 4. SKILL SOCKETS (256x256 RGBA)
# ==============================================================================
def rebuild_skill_sockets() -> tuple[Image.Image, Image.Image]:
    """
    Rebuild 256x256 Skill Sockets (Normal & Armed).
    Bezel metalwork strictly within outer 26px border (leaving [26..230] window).
    Inside [26..230]: alpha = 0 in center [40..216] with dark (2,2,4) recessed AO shadow ramp.
    ZERO white inner bevel glare or center bleed!
    Normal: clean brushed dark bronze bevels + hairline highlight + 4 corner rivets + 4 cardinal gothic prongs.
    Armed: refined glowing molten-amber engraved lines (RGB 255, 196, 68) along metal ridge, ZERO dotted beads.
    """
    size = 256
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    d_edge = np.minimum(np.minimum(xx, 255.0 - xx), np.minimum(yy, 255.0 - yy))

    grain = make_fbm_noise(size, size, seed=808, octaves=(6, 18, 48))

    outputs = []
    for armed in (False, True):
        rgba = np.zeros((size, size, 4), dtype=np.float32)

        # 1. Bezel metalwork in outer d_edge in [0..26]
        in_bezel = d_edge < 26.0

        # Base metal colors: Darker for Normal socket, warm bronze for Armed
        bronze_base = np.array([155.0, 122.0, 70.0], dtype=np.float32) if armed else np.array([76.0, 62.0, 42.0], dtype=np.float32)
        iron_base = np.array([32.0, 32.0, 38.0], dtype=np.float32)

        # Outer rounded corner mask via exact SDF
        sdf = _rounded_rect_sdf(size, size, margin_x=2.0, margin_y=2.0, radius=8.0)
        outer_alpha = np.clip(-sdf / 1.5, 0.0, 1.0) * 255.0

        # Outer lip: 0..4.5px dark iron
        m_lip = in_bezel & (d_edge < 4.5)
        rgba[m_lip, :3] = iron_base
        rgba[m_lip, 3] = outer_alpha[m_lip]

        # Main forged bronze band: 4.5..21px
        m_bronze = in_bezel & (d_edge >= 4.5) & (d_edge < 21.0)
        tb = (d_edge[m_bronze] - 4.5) / 16.5
        ridge_curve = np.sin(tb * math.pi)
        rgba[m_bronze, :3] = (
            bronze_base * (0.80 + 0.35 * ridge_curve[..., None] + 0.12 * (grain[m_bronze, None] - 0.5))
        )
        rgba[m_bronze, 3] = outer_alpha[m_bronze]

        # Inner metal ridge: 20..26px (pure dark wrought iron [12, 12, 15] for Normal, warm bronze for Armed)
        m_inner_ridge = in_bezel & (d_edge >= 20.0) & (d_edge < 26.0)
        if armed:
            rgba[m_inner_ridge, :3] = np.array([28.0, 24.0, 20.0], dtype=np.float32)
        else:
            rgba[m_inner_ridge, :3] = np.array([12.0, 12.0, 15.0], dtype=np.float32)
        rgba[m_inner_ridge, 3] = 255.0

        # Hairline bevel highlight along metal ridge at d_edge ~ 14px (top & left edges)
        ridge_hl = in_bezel & (np.abs(d_edge - 14.0) < 1.2) & ((xx < 128.0) | (yy < 128.0))
        rgba[ridge_hl, :3] = np.clip(rgba[ridge_hl, :3] + (55.0 if armed else 26.0), 0, 255)

        # Armed: Refined glowing molten-amber engraved line along [10..20]
        if armed:
            rune_band = in_bezel & (d_edge >= 10.0) & (d_edge <= 20.0)
            groove_dist = np.abs(d_edge[rune_band] - 15.0)
            core_glow = np.exp(-((groove_dist / 1.6) ** 2))
            bloom_glow = np.exp(-((groove_dist / 4.2) ** 2))

            amber_core = np.array([255.0, 215.0, 95.0], dtype=np.float32)
            amber_bloom = np.array([225.0, 120.0, 20.0], dtype=np.float32)

            rgba[rune_band, :3] = (
                rgba[rune_band, :3] * (1.0 - bloom_glow[..., None] * 0.75)
                + amber_bloom * (bloom_glow[..., None] * 0.75)
                + amber_core * (core_glow[..., None] * 0.65)
            )

        # 4 Cardinal Gothic Prong Notches at top, bottom, left, right midpoints (x=127.5 or y=127.5)
        mid = 127.5
        dx_top = np.abs(xx - mid)
        prong_top = (yy >= 4.0) & (yy <= 28.0) & (dx_top <= (28.0 - yy) * 0.45 + 1.5)
        dx_bot = np.abs(xx - mid)
        prong_bot = (yy >= 227.0) & (yy <= 251.0) & (dx_bot <= (yy - 227.0) * 0.45 + 1.5)
        dy_left = np.abs(yy - mid)
        prong_left = (xx >= 4.0) & (xx <= 28.0) & (dy_left <= (28.0 - xx) * 0.45 + 1.5)
        dy_right = np.abs(yy - mid)
        prong_right = (xx >= 227.0) & (xx <= 251.0) & (dy_right <= (xx - 227.0) * 0.45 + 1.5)

        all_prongs = prong_top | prong_bot | prong_left | prong_right
        prong_col = np.array([175.0, 140.0, 82.0]) if armed else np.array([92.0, 74.0, 48.0])
        spine_col = np.array([235.0, 200.0, 125.0]) if armed else np.array([128.0, 104.0, 66.0])
        rgba[all_prongs, :3] = prong_col + (grain[all_prongs, None] - 0.5) * 8.0
        rgba[all_prongs, 3] = 255.0

        spine_top = (yy >= 5.0) & (yy <= 27.0) & (dx_top < 1.2)
        spine_bot = (yy >= 228.0) & (yy <= 250.0) & (dx_bot < 1.2)
        spine_left = (xx >= 5.0) & (xx <= 27.0) & (dy_left < 1.2)
        spine_right = (xx >= 228.0) & (xx <= 250.0) & (dy_right < 1.2)
        rgba[spine_top | spine_bot | spine_left | spine_right, :3] = spine_col

        # 4 Corner Iron Rivets at (16, 16), (239, 16), (16, 239), (239, 239)
        rivet_rim_tint = np.array([120.0, 105.0, 75.0]) if armed else np.array([86.0, 72.0, 52.0])
        for rx, ry in [(16.0, 16.0), (239.0, 16.0), (16.0, 239.0), (239.0, 239.0)]:
            rdist = np.sqrt((xx - rx) ** 2 + (yy - ry) ** 2)
            rivet_mask = rdist < 4.8
            if np.any(rivet_mask):
                rnz = np.sqrt(np.maximum(0.0, 1.0 - (rdist[rivet_mask] / 4.8) ** 2))
                rgba[rivet_mask, :3] = (
                    iron_base * 1.8 * rnz[..., None]
                    + rivet_rim_tint * (1.0 - rnz[..., None])
                )
                rgba[rivet_mask, 3] = 255.0

        # 2. Inside [26..230] window:
        # Recessed ambient occlusion shadow ramp: px in [26.0..38.0], alpha 0.78 -> 0, RGB = [1, 1, 2]
        in_ao_ramp = (d_edge >= 26.0) & (d_edge < 38.0)
        t_ao = (38.0 - d_edge[in_ao_ramp]) / 12.0
        rgba[in_ao_ramp, :3] = [1.0, 1.0, 2.0]
        rgba[in_ao_ramp, 3] = t_ao * 0.78 * 255.0

        # Center [38.0..218.0]: strictly [0, 0, 0, 0]!
        in_transparent_center = d_edge >= 38.0
        rgba[in_transparent_center, :3] = [0.0, 0.0, 0.0]
        rgba[in_transparent_center, 3] = 0.0

        outputs.append(Image.fromarray(np.clip(rgba, 0, 255).astype(np.uint8), mode="RGBA"))

    return outputs[0], outputs[1]


# ==============================================================================
# 5. BUTTONS (384x88 RGBA) & PANEL OBSIDIAN (512x512 RGBA)
# ==============================================================================
def rebuild_button_triplet(is_crimson: bool) -> tuple[Image.Image, Image.Image, Image.Image]:
    """
    Rebuild 384x88 buttons for Slate Box FMargin(0.14, 0.24).
    Dark iron outer bevel edge (d_edge < 2.0).
    Crisp 1px muted antique-bronze trim (d_edge in [2.0..3.5], RGB 95, 74, 42).
    Inner shadow groove (d_edge in [3.5..5.0]).
    Dark sculpted stone (RGB 18..36) / dark crimson (RGB 40..88) interior with smooth vertical lighting.
    Hover subtly warmer/brighter without neon glare, Pressed inverted lighting.
    col_lum_std = 0.00 (ZERO vertical striping, perfectly seamless 9-slice stretching).
    """
    w, h = 384, 88
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    d_edge = np.minimum(np.minimum(xx, float(w - 1) - xx), np.minimum(yy, float(h - 1) - yy))
    v_norm = yy / float(h - 1)

    buttons = []
    for state in ("Normal", "Hovered", "Pressed"):
        rgba = np.zeros((h, w, 4), dtype=np.float32)

        sdf = _rounded_rect_sdf(h, w, margin_x=1.5, margin_y=1.5, radius=4.5)
        outer_alpha = np.clip(-sdf / 1.5, 0.0, 1.0) * 255.0

        if is_crimson:
            top_col = np.array([88.0, 16.0, 22.0], dtype=np.float32)
            bot_col = np.array([40.0, 6.0, 10.0], dtype=np.float32)
            trim_col = np.array([95.0, 74.0, 42.0], dtype=np.float32)
            if state == "Hovered":
                top_col = np.array([112.0, 22.0, 28.0], dtype=np.float32)
                bot_col = np.array([52.0, 8.0, 14.0], dtype=np.float32)
                trim_col = np.array([115.0, 92.0, 54.0], dtype=np.float32)
            elif state == "Pressed":
                top_col = np.array([36.0, 5.0, 8.0], dtype=np.float32)
                bot_col = np.array([65.0, 12.0, 16.0], dtype=np.float32)
                trim_col = np.array([80.0, 62.0, 35.0], dtype=np.float32)
        else:
            top_col = np.array([36.0, 36.0, 42.0], dtype=np.float32)
            bot_col = np.array([18.0, 18.0, 22.0], dtype=np.float32)
            trim_col = np.array([95.0, 74.0, 42.0], dtype=np.float32)
            if state == "Hovered":
                top_col = np.array([48.0, 48.0, 56.0], dtype=np.float32)
                bot_col = np.array([24.0, 24.0, 30.0], dtype=np.float32)
                trim_col = np.array([115.0, 92.0, 54.0], dtype=np.float32)
            elif state == "Pressed":
                top_col = np.array([18.0, 18.0, 22.0], dtype=np.float32)
                bot_col = np.array([32.0, 32.0, 38.0], dtype=np.float32)
                trim_col = np.array([80.0, 62.0, 35.0], dtype=np.float32)

        base_fill = top_col * (1.0 - v_norm[..., None]) + bot_col * v_norm[..., None]
        rgba[..., :3] = base_fill
        rgba[..., 3] = outer_alpha

        # 0..2.0px: dark iron outer bevel edge
        m_iron = d_edge < 2.0
        rgba[m_iron, :3] = np.array([20.0, 20.0, 24.0])

        # 2.0..3.5px: crisp 1px muted antique-bronze trim (RGB 95, 74, 42)
        m_trim = (d_edge >= 2.0) & (d_edge < 3.5)
        is_top_left = (xx < yy * (w / h)) | (xx < w * 0.5)
        chisel = np.where(is_top_left, 1.15, 0.85)
        rgba[m_trim, :3] = trim_col * chisel[m_trim, None]

        # 3.5..5.0px: inner shadow groove
        m_groove = (d_edge >= 3.5) & (d_edge < 5.0)
        rgba[m_groove, :3] *= 0.55

        rgba[rgba[..., 3] < 1.0, :3] = 0.0
        buttons.append(Image.fromarray(np.clip(rgba, 0, 255).astype(np.uint8), mode="RGBA"))

    return buttons[0], buttons[1], buttons[2]


def rebuild_panel_obsidian() -> Image.Image:
    """
    Rebuild 512x512 T_UI_PanelObsidian.png for Slate Box FMargin(0.11f) (~56px).
    Dark forged gunmetal-iron outer frame (RGB 18..28) in d_edge < 16.
    Sharp hairline (1-2px) muted antique-bronze pinstripe (RGB 88, 68, 38) in d_edge [16..18]
    with subtle top-left chisel highlight (RGB 105, 84, 48), NO wide mustard band!
    Inner shadow groove in d_edge [18..22].
    Deep dark basalt slate center (RGB 13..18) with subtle carved micro-grain.
    Corner brackets and rivets strictly in outer 24px so 9-slice stretch is pristine.
    """
    size = 512
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    d_edge = np.minimum(np.minimum(xx, float(size - 1) - xx), np.minimum(yy, float(size - 1) - yy))

    grain = make_fbm_noise(size, size, seed=333, octaves=(6, 16, 44, 96))

    rgba = np.zeros((size, size, 4), dtype=np.float32)

    # Outer rounded rectangle mask via exact SDF
    sdf = _rounded_rect_sdf(size, size, margin_x=3.0, margin_y=3.0, radius=10.0)
    outer_alpha = np.clip(-sdf / 1.5, 0.0, 1.0) * 255.0

    # Clean dark basalt slate center: RGB ~ (14, 14, 17) with micro-grain
    obsidian_base = np.array([14.0, 14.0, 17.0], dtype=np.float32)
    rgba[..., :3] = obsidian_base + (grain[..., None] - 0.5) * 3.0
    rgba[..., 3] = outer_alpha

    # Outer frame in d_edge < 16px: dark forged gunmetal iron (RGB 18..24)
    m_frame = d_edge < 16.0
    iron_frame = np.array([20.0, 20.0, 24.0], dtype=np.float32)
    is_top_left = (xx + yy) < size
    bevel_factor = np.where(is_top_left, 1.15, 0.85)
    rgba[m_frame, :3] = iron_frame * bevel_factor[m_frame, None]

    # Hairline 1-2px muted antique-bronze pinstripe at d_edge in [16..18] (RGB 88, 68, 38)
    m_pinstripe = (d_edge >= 16.0) & (d_edge < 18.0)
    pinstripe_col = np.array([88.0, 68.0, 38.0], dtype=np.float32)
    pinstripe_hl = np.where(is_top_left, 1.20, 0.80)
    rgba[m_pinstripe, :3] = pinstripe_col * pinstripe_hl[m_pinstripe, None]

    # Inner shadow groove at d_edge in [18..22]
    m_groove = (d_edge >= 18.0) & (d_edge < 22.0)
    t_groove = (22.0 - d_edge[m_groove]) / 4.0
    rgba[m_groove, :3] *= 1.0 - t_groove[..., None] * 0.45

    # 4 Corner Iron Clasps with Muted Rivets strictly in corners (d_edge < 24 and corner dist)
    for cx, cy in [(17.0, 17.0), (float(size - 18), 17.0), (17.0, float(size - 18)), (float(size - 18), float(size - 18))]:
        cdist = np.sqrt((xx - cx) ** 2 + (yy - cy) ** 2)
        c_plate = (np.abs(xx - cx) <= 7.0) & (np.abs(yy - cy) <= 7.0) & (d_edge < 24.0)
        rgba[c_plate, :3] = np.array([28.0, 28.0, 34.0])
        c_rivet = cdist < 3.2
        if np.any(c_rivet):
            rgba[c_rivet, :3] = np.array([72.0, 58.0, 35.0])

    rgba[rgba[..., 3] < 1.0, :3] = 0.0
    return Image.fromarray(np.clip(rgba, 0, 255).astype(np.uint8), mode="RGBA")


# ==============================================================================
# 6. GRIMOIRE BOOK SPREAD (1440x920 RGBA) & SPELL PLATES (512x512)
# ==============================================================================
def rebuild_grimoire_spread() -> Image.Image:
    """
    Rebuild exact 1440x920 RGBA Grimoire Book Spread from sculpted backup art.
    Preserves 100% of the sculpted iron corner skulls, embossed stitched leather,
    aged foxed vellum, and deckled page edges.
    Gentle threshold lum <= 16 for outer silhouette with binary_closing & binary_fill_holes.
    Core leather cover forced solid across book_mask[54:864, 24:1398] = True (unbroken leather bindings).
    Middle-right tabletop sliver in y in [160..760], x >= 1404 excluded where lum <= 20.
    Inset-resizes book+skulls (1440x920 -> 1404x892, LANCZOS) and centers onto fresh 1440x920 canvas at (18, 14),
    applying a clean 9px soft obsidian drop shadow AFTER centering so all 4 sculpted corner skulls sit 100%
    unclipped inside the canvas with >= 14px clean shadow margin on every side!
    """
    src_path = BACKUP_DIR / "Grimoire" / "T_GrimoireBookSpread.png"
    im = Image.open(src_path).convert("RGB")
    arr = np.array(im, dtype=np.uint8)
    h, w = arr.shape[:2]
    yy, xx = np.mgrid[0:h, 0:w].astype(np.int32)
    lum = arr.max(axis=2)

    # Flood-fill BFS from outer boundary where lum <= 16
    visited = np.zeros((h, w), dtype=bool)
    q = deque()
    for x in range(w):
        if lum[0, x] <= 16:
            visited[0, x] = True
            q.append((0, x))
        if lum[h - 1, x] <= 16:
            visited[h - 1, x] = True
            q.append((h - 1, x))
    for y in range(h):
        # Only seed left edge where it is truly background outside the book
        if (y < 54 or y > 864) and lum[y, 0] <= 16 and not visited[y, 0]:
            visited[y, 0] = True
            q.append((y, 0))
        if lum[y, w - 1] <= 16 and not visited[y, w - 1]:
            visited[y, w - 1] = True
            q.append((y, w - 1))

    # Seed middle-right artifact table sliver in y in [115..805], x >= 1398
    for y in range(115, 805):
        for x in range(1398, w):
            if not visited[y, x]:
                visited[y, x] = True
                q.append((y, x))

    while q:
        y, x = q.popleft()
        for dy, dx in [(-1, 0), (1, 0), (0, -1), (0, 1)]:
            ny, nx = y + dy, x + dx
            if 0 <= ny < h and 0 <= nx < w and not visited[ny, nx]:
                if lum[ny, nx] <= 16:
                    visited[ny, nx] = True
                    q.append((ny, nx))

    book_mask = ~visited

    # 1. Closing (iterations=3) & fill internal holes
    book_mask = binary_closing(book_mask, iterations=3)
    book_mask = binary_fill_holes(book_mask)

    # 2. FORCE core leather cover solid across book_mask[54:864, 24:1398] = True
    # (unbroken, solid leather binding with zero moth-eaten cutouts)
    book_mask[54:864, 24:1398] = True

    # 3. Central spine protection at x in [650..720], y in [20..65] where lum > 18
    spine_area = lum[20:65, 650:720] > 18
    book_mask[20:65, 650:720] = spine_area

    # 4. Set book_mask[115:805, 1398:] = False BEFORE main_component extraction
    # so ONLY intact corner skull brackets remain with zero dangling tabletop shreds
    book_mask[115:805, 1398:] = False

    # 5. Keep ONLY the largest connected component (the main book body)
    lbl, num = label(book_mask)
    counts = np.bincount(lbl.ravel())
    counts[0] = 0  # Ignore background
    main_component = counts.argmax()
    book_mask = (lbl == main_component)

    # 6. Clamp edge RGB within 2px of book silhouette to dark oxblood leather so no pale tabletop fringe
    inner_edge = book_mask & (~binary_erosion(book_mask, iterations=2))
    arr_clamped = arr.copy()
    arr_clamped[inner_edge] = np.minimum(arr_clamped[inner_edge], np.array([32, 26, 24], dtype=np.uint8))

    # 7. Extract book RGBA on 1440x920
    book_rgba = np.zeros((h, w, 4), dtype=np.uint8)
    book_rgba[book_mask, :3] = arr_clamped[book_mask]
    book_rgba[book_mask, 3] = 255
    book_im = Image.fromarray(book_rgba, mode="RGBA")

    # 8. Inset-resize book+skulls (1440x920 -> 1404x892, LANCZOS) and center onto fresh 1440x920 canvas at (18, 14)
    target_w, target_h = 1404, 892
    offset_x, offset_y = 18, 14
    book_resized = book_im.resize((target_w, target_h), Image.Resampling.LANCZOS)
    canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    canvas.paste(book_resized, (offset_x, offset_y), book_resized)

    canvas_arr = np.array(canvas, dtype=np.float32)
    book_alpha = canvas_arr[..., 3]

    # 9. Apply clean 9px soft obsidian drop shadow AFTER centering
    mask_l = Image.fromarray((book_alpha > 12.0).astype(np.uint8) * 255, mode="L")
    shadow_im = mask_l.filter(ImageFilter.GaussianBlur(radius=9.0))
    shadow_arr = np.array(shadow_im, dtype=np.float32) / 255.0

    final_alpha = book_alpha.copy()
    outside = book_alpha < 12.0
    final_alpha[outside] = np.clip(shadow_arr[outside] * 0.76 * 255.0, 0.0, 255.0)

    # Margins strictly 0
    final_alpha[:10, :] = 0.0
    final_alpha[908:, :] = 0.0

    final_rgb = canvas_arr[..., :3].copy()
    final_rgb[outside] = [4.0, 3.0, 5.0]

    # Multiply any semi-transparent border pixel RGB (0 < alpha < 235) by 0.55
    # to eliminate any 1px pale tabletop halo around top metal brackets
    semi_trans = (book_alpha > 0.0) & (book_alpha < 235.0)
    final_rgb[semi_trans] *= 0.55

    final_rgb[final_alpha < 1.0] = [0.0, 0.0, 0.0]

    out_rgba = np.dstack([np.clip(final_rgb, 0, 255), np.clip(final_alpha, 0, 255)]).astype(np.uint8)
    return Image.fromarray(out_rgba, mode="RGBA")


def rebuild_spell_plates() -> list[tuple[str, Image.Image]]:
    """
    Copy and enhance all 10 SpellPlate_01..10.png from backup.
    Applies subtle dark antique vignette & 2px brass inner engraved frame (no white border pixels).
    """
    plates = []
    src_dir = BACKUP_DIR / "Grimoire"
    for i in range(1, 11):
        fname = f"SpellPlate_{i:02d}.png"
        src_path = src_dir / fname
        img = Image.open(src_path).convert("RGBA")
        arr = np.array(img, dtype=np.float32)

        h, w = arr.shape[:2]
        yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
        cx, cy = (w - 1) * 0.5, (h - 1) * 0.5
        r_norm = np.sqrt(((xx - cx) / cx) ** 2 + ((yy - cy) / cy) ** 2)

        # Subtle antique vignette
        vign = np.clip(1.0 - (r_norm ** 2.2) * 0.28, 0.65, 1.0)
        arr[..., :3] *= vign[..., None]

        # 2px brass inner engraved frame around outer 4..6px
        d_edge = np.minimum(np.minimum(xx, float(w - 1) - xx), np.minimum(yy, float(h - 1) - yy))
        frame_mask = (d_edge >= 4.0) & (d_edge <= 6.5)
        arr[frame_mask, :3] = np.array([140.0, 110.0, 60.0])

        # Outer 0..4px clean dark antique border (zero white fringe)
        outer_mask = d_edge < 4.0
        arr[outer_mask, :3] = np.array([12.0, 10.0, 12.0])
        arr[..., 3] = 255.0

        plates.append((fname, Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode="RGBA")))

    return plates


# ==============================================================================
# 7. ICONS & PORTRAITS & CREST (256x256 RGBA)
# ==============================================================================
def rebuild_icons_and_crests() -> list[tuple[str, Image.Image]]:
    """
    Clean white corner fringe on UI/Icons/T_UI_Crest_Skull.png.
    Boost contrast and apply subtle dark-fantasy vignette on icons and portraits.
    """
    src_icons_dir = BACKUP_DIR / "UI" / "Icons"
    results = []

    for fpath in sorted(src_icons_dir.glob("T_UI_*.png")):
        fname = fpath.name
        img = Image.open(fpath).convert("RGBA")
        arr = np.array(img, dtype=np.float32)
        h, w = arr.shape[:2]
        yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
        cx, cy = (w - 1) * 0.5, (h - 1) * 0.5

        if fname == "T_UI_Crest_Skull.png":
            # Circular medallion mask at r_norm in [0.94..0.96]
            rn = np.sqrt(((xx - cx) / cx) ** 2 + ((yy - cy) / cy) ** 2)
            alpha_circ = np.clip((0.96 - rn) / 0.02, 0.0, 1.0) * 255.0
            arr[..., 3] = np.minimum(arr[..., 3], alpha_circ)
            # Eliminate any RGB where alpha == 0
            arr[arr[..., 3] < 1.0, :3] = 0.0
        else:
            # Icons and portraits: subtle edge vignette and crisp contrast curve
            rn = np.sqrt(((xx - cx) / cx) ** 2 + ((yy - cy) / cy) ** 2)
            vign = np.clip(1.0 - (rn ** 2.4) * 0.22, 0.72, 1.0)
            arr[..., :3] *= vign[..., None]

            # Contrast S-curve
            norm_rgb = arr[..., :3] / 255.0
            boosted = (norm_rgb - 0.5) * 1.14 + 0.5
            arr[..., :3] = np.clip(boosted * 255.0, 0, 255)
            arr[..., 3] = 255.0

            # Soft 6px dark inner vignette along outermost border (-> RGB * 0.65) for spell icons
            # so icons sit deeply recessed inside iron sockets without milky perimeter glare
            if fname.startswith("T_UI_Icon_"):
                d_edge = np.minimum(np.minimum(xx, float(w - 1) - xx), np.minimum(yy, float(h - 1) - yy))
                edge_vign = np.clip(d_edge / 6.0, 0.0, 1.0)
                edge_mult = 0.65 + 0.35 * (3.0 * edge_vign ** 2 - 2.0 * edge_vign ** 3)
                arr[..., :3] *= edge_mult[..., None]

        results.append((fname, Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode="RGBA")))

    return results


# ==============================================================================
# MAIN EXECUTION & VERIFICATION
# ==============================================================================
def main() -> int:
    print("=" * 70)
    print("REBUILDING AAA DIABLO IV HUD & GRIMOIRE TEXTURE KIT")
    print("=" * 70)

    # 1. Action Dock Pedestal
    print("\n[1/7] Building T_UI_ActionDockPedestal.png (1160x268)...")
    dock_img = rebuild_action_dock_pedestal()
    assert dock_img.size == (1160, 268), f"Bad size: {dock_img.size}"
    save_image_to_all(dock_img, "T_UI_ActionDockPedestal.png", SYNC_UI_DIRS)
    print(f"  -> Saved 1160x268 RGBA to {len(SYNC_UI_DIRS)} locations")

    # 2. Globe Glass Rim
    print("\n[2/7] Building T_UI_GlobeGlassRim.png (512x512)...")
    rim_img = rebuild_globe_glass_rim()
    assert rim_img.size == (512, 512), f"Bad size: {rim_img.size}"
    save_image_to_all(rim_img, "T_UI_GlobeGlassRim.png", SYNC_UI_DIRS)
    print(f"  -> Saved 512x512 RGBA to {len(SYNC_UI_DIRS)} locations")

    # 3. Globe Liquids & Back
    print("\n[3/7] Building T_UI_GlobeBack & T_UI_GlobeFill_HP/MP (512x512, r=0.985)...")
    img_back, img_hp, img_mp = rebuild_globe_liquids()
    save_image_to_all(img_back, "T_UI_GlobeBack.png", SYNC_UI_DIRS)
    save_image_to_all(img_hp, "T_UI_GlobeFill_HP.png", SYNC_UI_DIRS)
    save_image_to_all(img_mp, "T_UI_GlobeFill_MP.png", SYNC_UI_DIRS)
    print(f"  -> Saved 3 globe textures to {len(SYNC_UI_DIRS)} locations")

    # 4. Skill Sockets
    print("\n[4/7] Building T_UI_SkillSocket_Normal & Armed (256x256)...")
    sock_norm, sock_armed = rebuild_skill_sockets()
    save_image_to_all(sock_norm, "T_UI_SkillSocket_Normal.png", SYNC_UI_DIRS)
    save_image_to_all(sock_armed, "T_UI_SkillSocket_Armed.png", SYNC_UI_DIRS)
    print(f"  -> Saved 2 socket textures to {len(SYNC_UI_DIRS)} locations")

    # 5. Buttons & Obsidian Panel
    print("\n[5/7] Building T_UI_BtnStone & BtnCrimson (384x88) + PanelObsidian (512x512)...")
    stone_norm, stone_hov, stone_prs = rebuild_button_triplet(is_crimson=False)
    crim_norm, crim_hov, crim_prs = rebuild_button_triplet(is_crimson=True)
    panel_img = rebuild_panel_obsidian()

    save_image_to_all(stone_norm, "T_UI_BtnStone_Normal.png", SYNC_UI_DIRS)
    save_image_to_all(stone_hov, "T_UI_BtnStone_Hovered.png", SYNC_UI_DIRS)
    save_image_to_all(stone_prs, "T_UI_BtnStone_Pressed.png", SYNC_UI_DIRS)

    save_image_to_all(crim_norm, "T_UI_BtnCrimson_Normal.png", SYNC_UI_DIRS)
    save_image_to_all(crim_hov, "T_UI_BtnCrimson_Hovered.png", SYNC_UI_DIRS)
    save_image_to_all(crim_prs, "T_UI_BtnCrimson_Pressed.png", SYNC_UI_DIRS)

    save_image_to_all(panel_img, "T_UI_PanelObsidian.png", SYNC_UI_DIRS)
    print(f"  -> Saved 7 button/panel textures to {len(SYNC_UI_DIRS)} locations")

    # 6. Grimoire Book Spread & Spell Plates
    print("\n[6/7] Building T_GrimoireBookSpread.png (1440x920) & 10 Spell Plates...")
    spread_img = rebuild_grimoire_spread()
    assert spread_img.size == (1440, 920), f"Bad size: {spread_img.size}"
    save_image_to_all(spread_img, "T_GrimoireBookSpread.png", SYNC_GRIMOIRE_DIRS)

    plates = rebuild_spell_plates()
    for fname, p_img in plates:
        save_image_to_all(p_img, fname, SYNC_GRIMOIRE_DIRS)
    print(f"  -> Saved Grimoire Book Spread + 10 Spell Plates to {len(SYNC_GRIMOIRE_DIRS)} locations")

    # 7. Icons & Crests
    print("\n[7/7] Slicing & enhancing 9 icons, crest, and portraits...")
    icons = rebuild_icons_and_crests()
    for fname, icon_img in icons:
        save_image_to_all(icon_img, fname, SYNC_UI_DIRS, is_icon=True)
    print(f"  -> Saved {len(icons)} icons & crests to {len(SYNC_UI_DIRS)} locations")

    print("\n" + "=" * 70)
    print("ALL ASSETS SUCCESSFULLY REBUILT AND SYNCED!")
    print("=" * 70)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
