"""
Build AAA Diablo IV Sculpted UI Kit from AI-generated gothic metalwork masters
and normal-lit procedural PBR stone/bronze shaders.

Outputs identically to BOTH:
  - RawAssets/UI/ (and RawAssets/UI/Icons/)   [UE 5.8 Native Slate HUD]
  - _backup_web_prototype/public/ui/ (and public/ui/icons/)  [Three.js Web HUD]
"""

import math
import shutil
from collections import deque
from pathlib import Path

import numpy as np
from PIL import Image, ImageEnhance, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
ART_DIR = Path(r"C:\Users\user\.gemini\antigravity\brain\32b16ce5-cce9-48eb-9f7c-d7bba29e21be")
UI_UE_DIR = ROOT / "RawAssets" / "UI"
UI_WEB_DIR = ROOT / "_backup_web_prototype" / "public" / "ui"

DOCK_MASTER = ART_DIR / "diablo_hud_dock_master_1794828001839.jpg"
GLOBE_MASTER = ART_DIR / "diablo_hud_globe_bezel_clean_1794828040627.jpg"
ICONS_MASTER = ART_DIR / "diablo_hud_icons_grid_1794828074487.jpg"


def _fbm_noise(h: int, w: int, seed: int = 13, octaves=(4, 10, 24, 56)) -> np.ndarray:
    rng = np.random.default_rng(seed)
    acc = np.zeros((h, w), dtype=np.float32)
    weight = 0.55
    norm = 0.0
    for grid in octaves:
        gh = max(2, grid)
        gw = max(2, int(round(grid * (w / max(1, h)))))
        coarse = rng.random((gh, gw), dtype=np.float32)
        im = Image.fromarray((coarse * 255.0).astype(np.uint8), mode="L").resize(
            (w, h), Image.Resampling.BICUBIC
        )
        acc += (np.asarray(im, dtype=np.float32) / 255.0) * weight
        norm += weight
        weight *= 0.52
    return np.clip(acc / max(norm, 1e-5), 0.0, 1.0)


def _rounded_rect_sdf(h: int, w: int, margin_x: float, margin_y: float, radius: float) -> np.ndarray:
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


def _shade_heightfield_pbr(
    height: np.ndarray,
    base_rgb: np.ndarray,
    metallic: np.ndarray,
    roughness: np.ndarray,
    alpha: np.ndarray,
    emissive: np.ndarray | None = None,
    bump_scale: float = 5.2,
    light_dir: tuple[float, float, float] = (-0.28, -0.78, 0.56),
) -> Image.Image:
    gy, gx = np.gradient(height.astype(np.float32))
    nx = -gx * bump_scale
    ny = -gy * bump_scale
    nz = np.ones_like(height, dtype=np.float32)
    inv_len = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz + 1e-8)
    nx *= inv_len
    ny *= inv_len
    nz *= inv_len

    lx, ly, lz = light_dir
    ll = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ll, ly / ll, lz / ll
    ndotl = np.clip(nx * lx + ny * ly + nz * lz, 0.0, 1.0)

    rx, ry, rz = 0.42, -0.58, 0.70
    rl = math.sqrt(rx * rx + ry * ry + rz * rz)
    ndot_fill = np.clip(nx * (rx / rl) + ny * (ry / rl) + nz * (rz / rl), 0.0, 1.0)

    hx, hy, hz = lx, ly, lz + 1.0
    hl = math.sqrt(hx * hx + hy * hy + hz * hz)
    ndoth = np.clip(nx * (hx / hl) + ny * (hy / hl) + nz * (hz / hl), 0.0, 1.0)

    spec_power = np.clip(2.0 / np.maximum(roughness ** 3.2, 0.012), 4.0, 140.0)
    spec = (ndoth ** spec_power) * (0.28 + 1.40 * metallic)

    blur_h = np.asarray(
        Image.fromarray(np.clip(height * 255.0, 0, 255).astype(np.uint8)).filter(
            ImageFilter.GaussianBlur(radius=3.5)
        ),
        dtype=np.float32,
    ) / 255.0
    cavity_ao = np.clip(0.84 + (height - blur_h) * 1.55, 0.36, 1.18)

    diffuse_col = base_rgb * (1.0 - 0.65 * metallic[..., None])
    ambient = 0.32 * cavity_ao[..., None]
    light_col = (
        ambient * diffuse_col
        + ndotl[..., None] * 0.88 * cavity_ao[..., None] * diffuse_col
        + ndot_fill[..., None] * 0.24 * diffuse_col
    )
    spec_tint = (1.0 - metallic[..., None]) * np.array([1.0, 0.96, 0.90], dtype=np.float32) + metallic[..., None] * np.clip(
        base_rgb * 1.38 + 0.14, 0.0, 1.0
    )
    out = light_col + spec[..., None] * spec_tint * 0.74
    if emissive is not None:
        out += emissive

    out_u8 = np.clip(out * 255.0, 0.0, 255.0).astype(np.uint8)
    a_u8 = np.clip(alpha * 255.0, 0.0, 255.0).astype(np.uint8)
    return Image.fromarray(np.dstack([out_u8, a_u8]), mode="RGBA")


def _add_rivets(height: np.ndarray, metallic: np.ndarray, base_rgb: np.ndarray, pts: list[tuple[float, float]], rad: float) -> None:
    h, w = height.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    gold = np.array([0.85, 0.68, 0.38], dtype=np.float32)
    for px, py in pts:
        d = np.sqrt((xx - px) ** 2 + (yy - py) ** 2)
        dome = np.clip(1.0 - (d / rad) ** 2, 0.0, 1.0)
        mask = d <= rad
        height[mask] = np.maximum(height[mask], height[mask] + 0.28 * np.sqrt(dome[mask]))
        metallic[mask] = 0.95
        base_rgb[mask] = gold * (0.85 + 0.26 * dome[mask, None])
        seat = (d > rad) & (d <= rad + 2.4)
        height[seat] -= 0.12


def _flood_mask_dark_bg(rgb: np.ndarray, threshold: int = 24, seeds: list[tuple[int, int]] | None = None) -> np.ndarray:
    """BFS flood-fill connected dark background region so interior dark shadows remain opaque."""
    h, w, _ = rgb.shape
    lum = (rgb[..., 0].astype(np.int32) + rgb[..., 1].astype(np.int32) + rgb[..., 2].astype(np.int32)) // 3
    is_dark = lum <= threshold
    visited = np.zeros((h, w), dtype=bool)
    if seeds is None:
        seeds = [(0, 0), (0, w - 1), (h - 1, 0), (h - 1, w - 1)]
    q = deque()
    for sy, sx in seeds:
        if 0 <= sy < h and 0 <= sx < w and is_dark[sy, sx] and not visited[sy, sx]:
            visited[sy, sx] = True
            q.append((sy, sx))
    while q:
        y, x = q.popleft()
        for ny, nx in ((y - 1, x), (y + 1, x), (y, x - 1), (y, x + 1)):
            if 0 <= ny < h and 0 <= nx < w and not visited[ny, nx] and is_dark[ny, nx]:
                visited[ny, nx] = True
                q.append((ny, nx))
    return visited


def process_action_dock_master() -> None:
    """Crop sculpted Diablo IV gargoyle dock and matte outer background & inner globe openings cleanly."""
    with Image.open(DOCK_MASTER) as im:
        rgb = np.asarray(im.convert("RGB"))
    h, w, _ = rgb.shape
    lum = rgb.max(axis=2)
    ys, xs = np.where(lum > 22)
    y0, y1 = max(0, int(ys.min()) - 6), min(h, int(ys.max()) + 7)
    x0, x1 = max(0, int(xs.min()) - 6), min(w, int(xs.max()) + 7)
    crop = rgb[y0:y1, x0:x1]
    ch, cw, _ = crop.shape

    # Conservative outer flood-fill (lum <= 11) ONLY from corners so dark cast-iron armor never leaks!
    bg_seeds = [(0, 0), (0, cw - 1), (ch - 1, 0), (ch - 1, cw - 1)]
    outer_bg = _flood_mask_dark_bg(crop, threshold=11, seeds=bg_seeds)

    # Exact circular globe openings measured on the left/right gargoyle cradles
    yy, xx = np.mgrid[0:ch, 0:cw].astype(np.float32)
    cx_l, cy_lr = 0.1751 * cw, 0.5291 * ch
    cx_r = (1.0 - 0.1751) * cw
    r_hole = 0.258 * ch
    dist_l = np.sqrt((xx - cx_l) ** 2 + (yy - cy_lr) ** 2)
    dist_r = np.sqrt((xx - cx_r) ** 2 + (yy - cy_lr) ** 2)

    alpha = np.where(outer_bg, 0.0, 1.0).astype(np.float32)
    # Enforce solid structural masonry around both left/right globe rings
    ring_band_l = (dist_l >= 0.260 * ch) & (dist_l <= 0.448 * ch) & (crop.max(axis=2) > 9)
    ring_band_r = (dist_r >= 0.260 * ch) & (dist_r <= 0.448 * ch) & (crop.max(axis=2) > 9)
    alpha[ring_band_l | ring_band_r] = 1.0
    # Protect ring bevels strictly outside 0.99*r_hole, carve crisp AA holes inside r_hole
    aa_l = np.clip((dist_l - (r_hole - 1.8)) / 2.4, 0.0, 1.0)
    aa_r = np.clip((dist_r - (r_hole - 1.8)) / 2.4, 0.0, 1.0)
    alpha = np.minimum(alpha, np.minimum(aa_l, aa_r))

    # Crisp 1px outer anti-aliasing along silhouette only
    alpha_blur = np.asarray(
        Image.fromarray(np.clip(alpha * 255.0, 0, 255).astype(np.uint8), mode="L").filter(ImageFilter.GaussianBlur(radius=0.7)),
        dtype=np.float32,
    ) / 255.0
    alpha = np.where((alpha > 0.92) | (alpha < 0.05), alpha, alpha_blur)

    # Also enforce obsidian opaque trough behind central skill bar (clean dark interior backing)
    in_center_tray = (xx > 0.265 * cw) & (xx < 0.735 * cw) & (yy > 0.34 * ch) & (yy < 0.67 * ch)
    alpha[in_center_tray] = 1.0

    rgba = np.dstack([crop, (alpha * 255.0).astype(np.uint8)])
    dock_rgba = Image.fromarray(rgba, mode="RGBA")
    target_w = 1600
    target_h = max(200, int(round(target_w * (ch / cw))))
    dock_resized = dock_rgba.resize((target_w, target_h), Image.Resampling.LANCZOS)
    dock_resized.save(UI_UE_DIR / "T_UI_ActionDockPedestal.png", "PNG")
    print(f"Built T_UI_ActionDockPedestal.png ({target_w}x{target_h})")


def process_globe_bezel_master() -> None:
    """Extract ornate circular bronze dragon-claw bezel with clean transparent inner window & glass catchlight."""
    with Image.open(GLOBE_MASTER) as im:
        rgb = np.asarray(im.convert("RGB"))
    h, w, _ = rgb.shape

    lum = rgb.max(axis=2)
    ys, xs = np.where(lum > 20)
    pad = max(4, min(int(ys.min()), int(xs.min()), h - 1 - int(ys.max()), w - 1 - int(xs.max())) - 4)
    crop = rgb[pad : h - pad, pad : w - pad]
    crop = Image.fromarray(crop).resize((512, 512), Image.Resampling.LANCZOS)
    arr_u8 = np.asarray(crop, dtype=np.uint8)
    arr = arr_u8.astype(np.float32) / 255.0

    yy, xx = np.mgrid[0:512, 0:512].astype(np.float32)
    c = 255.5
    dx = (xx - c) / c
    dy = (yy - c) / c
    rn = np.sqrt(dx * dx + dy * dy)

    # Very conservative outer flood-fill (lum <= 10) so dark metal filigree grooves never puncture!
    outer_bg = _flood_mask_dark_bg(arr_u8, threshold=10, seeds=[(0, 0), (0, 511), (511, 0), (511, 511)])

    brightness = arr.max(axis=2)
    alpha = np.where(outer_bg, 0.0, 1.0).astype(np.float32)
    # Strict solid protection over the entire metal ring band (0.58 <= rn <= 0.85)
    alpha[(rn >= 0.585) & (rn <= 0.84) & (brightness > 0.04)] = 1.0

    # Inner aperture (rn < 0.585): keep crisp top-left glass specular crescent translucent, black interior transparent
    t_inner = np.clip((rn - 0.555) / 0.032, 0.0, 1.0)
    spec_glass = np.clip((brightness - 0.16) / 0.50, 0.0, 0.45)
    alpha = np.where(rn < 0.585, np.maximum(spec_glass, t_inner), alpha)
    alpha[rn < 0.545] = np.clip((brightness[rn < 0.545] - 0.14) * 0.58, 0.0, 0.46)

    alpha_blur = np.asarray(
        Image.fromarray(np.clip(alpha * 255.0, 0, 255).astype(np.uint8), mode="L").filter(ImageFilter.GaussianBlur(radius=0.65)),
        dtype=np.float32,
    ) / 255.0
    alpha = np.where((alpha > 0.90) | (alpha < 0.03), alpha, alpha_blur)

    out = np.dstack([arr_u8, np.clip(alpha * 255.0, 0, 255).astype(np.uint8)])
    Image.fromarray(out, mode="RGBA").save(UI_UE_DIR / "T_UI_GlobeGlassRim.png", "PNG")
    print("Built T_UI_GlobeGlassRim.png (512x512)")


def build_globe_liquids() -> None:
    size = 512
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = (size - 1) * 0.5
    dx = (xx - c) / c
    dy = (yy - c) / c
    r = np.sqrt(dx * dx + dy * dy)
    swirl = _fbm_noise(size, size, seed=717, octaves=(4, 10, 24, 52, 96))
    swirl2 = _fbm_noise(size, size, seed=828, octaves=(6, 16, 40, 88))
    caustics = _fbm_noise(size, size, seed=939, octaves=(8, 22, 54))

    r_liq = 0.88
    alpha_back = np.clip((r_liq - r) * 36.0, 0.0, 0.98)
    nz = np.sqrt(np.maximum(0.0, 1.0 - np.clip(r / r_liq, 0.0, 1.0) ** 2))
    back_rgb = np.dstack([0.02 + 0.028 * nz, 0.022 + 0.028 * nz, 0.032 + 0.04 * nz]).astype(np.float32)
    Image.fromarray(
        np.dstack([np.clip(back_rgb * 255, 0, 255).astype(np.uint8), (alpha_back * 255).astype(np.uint8)]),
        mode="RGBA",
    ).save(UI_UE_DIR / "T_UI_GlobeBack.png", "PNG")

    for kind, deep_c, mid_c, bright_c in [
        ("HP", np.array([0.20, 0.015, 0.02]), np.array([0.74, 0.06, 0.07]), np.array([1.0, 0.34, 0.22])),
        ("MP", np.array([0.02, 0.08, 0.24]), np.array([0.07, 0.36, 0.86]), np.array([0.30, 0.82, 1.0])),
    ]:
        sphere_z = np.sqrt(np.maximum(0.0, 1.0 - np.clip(r / r_liq, 0.0, 1.0) ** 2))
        light_spot = np.clip(1.0 - np.sqrt((dx + 0.25) ** 2 + (dy + 0.28) ** 2) * 0.90, 0.0, 1.0) ** 1.45
        vein = np.clip((swirl * 0.55 + swirl2 * 0.45 - 0.32) * 2.1, 0.0, 1.0)
        filament = np.exp(-((caustics - 0.52) * 9.5) ** 2) * sphere_z
        col = (
            deep_c * (1.0 - sphere_z[..., None]) * 1.35
            + mid_c * (sphere_z[..., None] * (0.58 + 0.52 * vein[..., None]))
            + bright_c * (light_spot[..., None] * 0.70 + (vein[..., None] ** 2) * 0.26 + filament[..., None] * 0.42)
        )
        alpha_f = np.clip((r_liq - r) * 36.0, 0.0, 0.99)
        Image.fromarray(
            np.dstack([np.clip(col * 255, 0, 255).astype(np.uint8), (alpha_f * 255).astype(np.uint8)]),
            mode="RGBA",
        ).save(UI_UE_DIR / f"T_UI_GlobeFill_{kind}.png", "PNG")
    print("Built 512x512 T_UI_GlobeBack & T_UI_GlobeFill_HP/MP")


def build_sculpted_panels_and_sockets() -> None:
    # 1. Deeply Chiseled Cracked Basalt Obsidian Panel (512x512 9-slice)
    size = 512
    sdf = _rounded_rect_sdf(size, size, margin_x=5.0, margin_y=5.0, radius=14.0)
    grain = _fbm_noise(size, size, seed=101, octaves=(5, 14, 34, 72))
    fine = _fbm_noise(size, size, seed=202, octaves=(24, 64, 140))
    vein = _fbm_noise(size, size, seed=313, octaves=(6, 16, 40))
    cracks = np.exp(-((vein - 0.50) * 18.0) ** 2)

    alpha = np.clip(-sdf * 0.85, 0.0, 0.97)
    alpha[sdf > 0.0] = 0.0
    d_in = np.maximum(-sdf, 0.0)

    height = np.zeros((size, size), dtype=np.float32)
    metallic = np.zeros((size, size), dtype=np.float32)
    roughness = np.full((size, size), 0.72, dtype=np.float32)
    base_rgb = np.zeros((size, size, 3), dtype=np.float32)

    obsidian = np.array([0.052, 0.054, 0.068], dtype=np.float32)
    bronze = np.array([0.78, 0.61, 0.34], dtype=np.float32)
    iron = np.array([0.16, 0.17, 0.21], dtype=np.float32)

    height[:] = 0.22 + (grain - 0.5) * 0.07 + (fine - 0.5) * 0.03 - cracks * 0.055
    base_rgb[:] = obsidian * (0.72 + 0.48 * grain[..., None]) * (0.86 + 0.22 * fine[..., None]) * (1.0 - 0.35 * cracks[..., None])

    m_iron = (d_in >= 0.0) & (d_in < 6.0)
    height[m_iron] = 0.56 + 0.18 * np.sin(np.clip(d_in[m_iron] / 6.0, 0.0, 1.0) * math.pi)
    base_rgb[m_iron] = iron
    metallic[m_iron] = 0.82
    roughness[m_iron] = 0.34

    m_bronze = (d_in >= 6.0) & (d_in < 19.0)
    height[m_bronze] = 0.70 + 0.32 * np.sin(np.clip((d_in[m_bronze] - 6.0) / 13.0, 0.0, 1.0) * math.pi)
    base_rgb[m_bronze] = bronze * (0.82 + 0.28 * fine[m_bronze, None])
    metallic[m_bronze] = 0.93
    roughness[m_bronze] = 0.24

    m_moat = (d_in >= 19.0) & (d_in < 25.0)
    height[m_moat] = 0.10
    base_rgb[m_moat] = obsidian * 0.45
    metallic[m_moat] = 0.25

    m_inner = (d_in >= 25.0) & (d_in < 29.0)
    height[m_inner] = 0.50
    base_rgb[m_inner] = bronze * 0.82
    metallic[m_inner] = 0.90
    roughness[m_inner] = 0.28

    m_shadow = (d_in >= 29.0) & (d_in < 56.0)
    ts = (d_in[m_shadow] - 29.0) / 27.0
    base_rgb[m_shadow] *= (0.42 + 0.58 * ts)[..., None]

    # Corner L-brackets
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    corner_box = ((xx < 54) | (xx > size - 55)) & ((yy < 54) | (yy > size - 55)) & (d_in >= 6.0) & (d_in < 36.0)
    height[corner_box] = np.maximum(height[corner_box], 0.62)
    base_rgb[corner_box] = iron * 1.15
    metallic[corner_box] = 0.88
    roughness[corner_box] = 0.30

    _add_rivets(
        height,
        metallic,
        base_rgb,
        [(36.0, 36.0), (size - 37.0, 36.0), (36.0, size - 37.0), (size - 37.0, size - 37.0)],
        rad=7.2,
    )
    _shade_heightfield_pbr(height, base_rgb, metallic, roughness, alpha, bump_scale=5.6).save(
        UI_UE_DIR / "T_UI_PanelObsidian.png", "PNG"
    )

    # 2. Ornate Gothic Skill Sockets (256x256 Normal + Armed)
    s = 256
    sdf_o = _rounded_rect_sdf(s, s, margin_x=4.0, margin_y=4.0, radius=10.0)
    sdf_i = _rounded_rect_sdf(s, s, margin_x=24.0, margin_y=24.0, radius=5.0)
    s_grain = _fbm_noise(s, s, seed=505, octaves=(6, 18, 48))
    yy, xx = np.mgrid[0:s, 0:s].astype(np.float32)

    for armed in (False, True):
        sh = np.zeros((s, s), dtype=np.float32)
        sm = np.zeros((s, s), dtype=np.float32)
        sr = np.full((s, s), 0.28, dtype=np.float32)
        scol = np.zeros((s, s, 3), dtype=np.float32)
        semi = np.zeros((s, s, 3), dtype=np.float32)

        in_bezel = (sdf_o <= 0.0) & (sdf_i >= -9.0)
        d_out = np.maximum(-sdf_o, 0.0)
        d_in_open = -sdf_i

        b_col = np.array([0.82, 0.64, 0.34] if not armed else [0.98, 0.82, 0.44], dtype=np.float32)
        d_iron = np.array([0.13, 0.13, 0.17], dtype=np.float32)

        sh[in_bezel] = 0.64 + 0.28 * np.sin(np.clip(d_out[in_bezel] / 20.0, 0.0, 1.0) * math.pi)
        scol[in_bezel] = b_col * (0.84 + 0.24 * s_grain[in_bezel, None])
        sm[in_bezel] = 0.94
        sr[in_bezel] = 0.23

        lip = (d_out >= 0.0) & (d_out < 5.0)
        scol[lip] = d_iron
        sm[lip] = 0.84

        # Cardinal notch teeth jutting softly toward the icon edge
        mid_tooth = (((np.abs(xx - 127.5) < 10.0) & ((yy < 28.0) | (yy > s - 29.0))) | ((np.abs(yy - 127.5) < 10.0) & ((xx < 28.0) | (xx > s - 29.0)))) & (sdf_o <= 0.0)
        sh[mid_tooth] = 0.88
        scol[mid_tooth] = b_col * 1.08
        sm[mid_tooth] = 0.96

        in_win = sdf_i < 0.0
        sa = np.zeros((s, s), dtype=np.float32)
        sa[(sdf_o <= 0.0) & (~in_win)] = 1.0
        sa[mid_tooth] = 1.0
        w_fade = np.clip(1.0 - (d_in_open / 16.0), 0.0, 1.0)
        sa[in_win] = np.maximum(sa[in_win], (w_fade[in_win] ** 1.8) * 0.86)
        scol[in_win & (~mid_tooth)] = d_iron * 0.30

        if armed:
            glow_band = np.exp(-((np.abs(sdf_i) - 3.5) / 7.5) ** 2)
            semi += glow_band[..., None] * np.array([1.0, 0.62, 0.14], dtype=np.float32) * 0.48

        _add_rivets(sh, sm, scol, [(17.0, 17.0), (s - 18.0, 17.0), (17.0, s - 18.0), (s - 18.0, s - 18.0)], rad=5.2)
        suffix = "Armed" if armed else "Normal"
        _shade_heightfield_pbr(sh, scol, sm, sr, sa, emissive=semi, bump_scale=5.4).save(
            UI_UE_DIR / f"T_UI_SkillSocket_{suffix}.png", "PNG"
        )
    print("Built sculpted T_UI_PanelObsidian & T_UI_SkillSocket_*")


def slice_icons_3x3() -> None:
    icons_ue = UI_UE_DIR / "Icons"
    icons_ue.mkdir(parents=True, exist_ok=True)
    with Image.open(ICONS_MASTER) as im:
        rgb = np.asarray(im.convert("RGB"))
    h, w, _ = rgb.shape

    names = [
        "T_UI_Icon_Ignis.png",
        "T_UI_Icon_Tempest.png",
        "T_UI_Icon_Graviton.png",
        "T_UI_Icon_Invoke.png",
        "T_UI_Icon_Dash.png",
        "T_UI_Crest_Skull.png",
        "T_UI_Portrait_Hero0.png",
        "T_UI_Portrait_Hero1.png",
        "T_UI_Portrait_Hero2.png",
    ]

    # Locate the non-white row and col spans by thresholding pixel lightness below 238
    is_art = rgb.min(axis=2) < 235
    row_proj = is_art.mean(axis=1) > 0.15
    col_proj = is_art.mean(axis=0) > 0.15

    def find_3_segments(proj: np.ndarray) -> list[tuple[int, int]]:
        segs = []
        in_seg = False
        start = 0
        for i, val in enumerate(proj):
            if val and not in_seg:
                start = i
                in_seg = True
            elif not val and in_seg:
                if i - start > 50:
                    segs.append((start, i))
                in_seg = False
        if in_seg and len(proj) - start > 50:
            segs.append((start, len(proj)))
        if len(segs) != 3:
            third = len(proj) // 3
            return [(10, third - 10), (third + 10, 2 * third - 10), (2 * third + 10, len(proj) - 10)]
        return segs

    r_segs = find_3_segments(row_proj)
    c_segs = find_3_segments(col_proj)

    idx = 0
    for r0, r1 in r_segs:
        for c0, c1 in c_segs:
            fname = names[idx]
            idx += 1
            # Inset 14px inside detected cell border so 100% of white grid border and black frame edge are trimmed
            inset = 14
            cell = rgb[r0 + inset : r1 - inset, c0 + inset : c1 - inset]
            cell_im = Image.fromarray(cell).resize((256, 256), Image.Resampling.LANCZOS)

            if fname == "T_UI_Crest_Skull.png":
                # Clean circular gothic bronze & obsidian medallion mask (zero interior floodfill holes)
                arr = np.asarray(cell_im, dtype=np.float32) / 255.0
                yy_i, xx_i = np.mgrid[0:256, 0:256].astype(np.float32)
                rn_i = np.sqrt(((xx_i - 127.5) / 127.5) ** 2 + ((yy_i - 127.5) / 127.5) ** 2)
                ring_m = np.exp(-((rn_i - 0.90) / 0.028) ** 2)
                arr = arr * (1.0 - 0.35 * ring_m[..., None]) + np.array([0.78, 0.61, 0.33], dtype=np.float32) * (ring_m[..., None] * 0.55)
                a_circ = np.clip((0.955 - rn_i) * 32.0, 0.0, 1.0)
                rgba = np.dstack([
                    np.clip(arr * 255.0, 0, 255).astype(np.uint8),
                    np.clip(a_circ * 255.0, 0, 255).astype(np.uint8),
                ])
                Image.fromarray(rgba, mode="RGBA").save(icons_ue / fname, "PNG")
            else:
                cell_im.convert("RGBA").save(icons_ue / fname, "PNG")
    print(f"Sliced 9 Diablo IV HUD icons & portraits into {icons_ue}")


def mirror_to_web_public() -> None:
    UI_WEB_DIR.mkdir(parents=True, exist_ok=True)
    web_icons = UI_WEB_DIR / "icons"
    web_icons.mkdir(parents=True, exist_ok=True)

    for png in UI_UE_DIR.glob("T_UI_*.png"):
        shutil.copy2(png, UI_WEB_DIR / png.name)
    for png in (UI_UE_DIR / "Icons").glob("T_UI_*.png"):
        shutil.copy2(png, web_icons / png.name)
    print(f"Mirrored all T_UI_*.png textures & icons to {UI_WEB_DIR}")


def main() -> int:
    UI_UE_DIR.mkdir(parents=True, exist_ok=True)
    process_action_dock_master()
    process_globe_bezel_master()
    build_globe_liquids()
    build_sculpted_panels_and_sockets()
    slice_icons_3x3()
    mirror_to_web_public()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
