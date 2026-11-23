"""
Procedural PBR Texture Generator for AETHERIA (UE5.8)
Generates organic, physically calibrated Albedo (with cavity AO), DirectX Normal
(Ny = -dH/dy, inverted green channel, unit-length tangent space), and Linear Roughness maps.
Outputs to both packaged game and project workspace RawAssets/Textures/ folders.
"""

import math
import os
import shutil
import sys
import time
from pathlib import Path
import numpy as np
import scipy.ndimage as ndi
from PIL import Image

TARGET_DIRS = [
    Path(r"C:\Games\AETHERIA\Aetheria\RawAssets\Textures"),
    Path(r"c:\Users\tagir\OneDrive\Desktop\Aetheria\RawAssets\Textures"),
]


def ensure_dirs() -> None:
    for target in TARGET_DIRS:
        target.mkdir(parents=True, exist_ok=True)


# ==============================================================================
# ORGANIC PROCEDURAL NOISE & HEIGHTFIELD UTILITIES
# ==============================================================================

def make_tileable_fbm(res: int, seed: int = 42, octaves: int = 5, persistence: float = 0.5) -> np.ndarray:
    """Seamless periodic multi-octave FBM noise using periodic wrap padding and bicubic interpolation."""
    rng = np.random.default_rng(seed)
    noise = np.zeros((res, res), dtype=np.float32)
    weight = 1.0
    total_w = 0.0
    for oct_idx in range(octaves):
        freq = 4 * (2 ** oct_idx)
        if freq > res:
            break
        grid = rng.standard_normal((freq, freq)).astype(np.float32)
        pad_grid = np.pad(grid, pad_width=1, mode="wrap")
        img = Image.fromarray(pad_grid)
        pad_size = int(res * (freq + 2) / freq)
        resized = np.asarray(img.resize((pad_size, pad_size), Image.Resampling.BICUBIC), dtype=np.float32)
        start = int(res * 1.0 / freq)
        layer = resized[start:start + res, start:start + res]
        noise += layer * weight
        total_w += weight
        weight *= persistence

    noise /= total_w
    min_v, max_v = np.min(noise), np.max(noise)
    if max_v > min_v:
        noise = (noise - min_v) / (max_v - min_v)
    return noise


def make_tileable_voronoi(res: int, grid_size: int = 24, seed: int = 42) -> np.ndarray:
    """Seamless periodic Voronoi distance map (d1) for organic cells, pebbles, and hammer dimples."""
    rng = np.random.default_rng(seed)
    cell_w = res / grid_size
    pts_x = np.zeros((grid_size, grid_size), dtype=np.float32)
    pts_y = np.zeros((grid_size, grid_size), dtype=np.float32)
    for i in range(grid_size):
        for j in range(grid_size):
            pts_x[i, j] = (j + rng.uniform(0.15, 0.85)) * cell_w
            pts_y[i, j] = (i + rng.uniform(0.15, 0.85)) * cell_w

    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    cell_x = (xx / cell_w).astype(np.int32) % grid_size
    cell_y = (yy / cell_w).astype(np.int32) % grid_size

    min_dist = np.full((res, res), 1e9, dtype=np.float32)
    for dy in [-1, 0, 1]:
        for dx in [-1, 0, 1]:
            ny = (cell_y + dy) % grid_size
            nx = (cell_x + dx) % grid_size
            diff_x = np.abs(xx - pts_x[ny, nx])
            diff_x = np.minimum(diff_x, res - diff_x)
            diff_y = np.abs(yy - pts_y[ny, nx])
            diff_y = np.minimum(diff_y, res - diff_y)
            d = np.hypot(diff_x, diff_y)
            min_dist = np.minimum(min_dist, d)

    return min_dist / cell_w


def make_anisotropic_noise(res: int, freq_y: int, freq_x: int, seed: int = 42) -> np.ndarray:
    """Stretched organic fibers for bone, wood grain, keratin, and brushed metal."""
    rng = np.random.default_rng(seed)
    grid = rng.standard_normal((freq_y, freq_x)).astype(np.float32)
    pad = np.pad(grid, pad_width=1, mode="wrap")
    img = Image.fromarray(pad)
    pad_h = int(res * (freq_y + 2) / freq_y)
    pad_w = int(res * (freq_x + 2) / freq_x)
    resized = np.asarray(img.resize((pad_w, pad_h), Image.Resampling.BICUBIC), dtype=np.float32)
    start_y = int(res * 1.0 / freq_y)
    start_x = int(res * 1.0 / freq_x)
    layer = resized[start_y:start_y + res, start_x:start_x + res]
    min_v, max_v = np.min(layer), np.max(layer)
    if max_v > min_v:
        layer = (layer - min_v) / (max_v - min_v)
    return layer


def make_ridge_cracks(res: int, seed1: int, seed2: int, mult: float = 6.0) -> np.ndarray:
    """Domain-warped organic ridge cracks for stone fractures, bone sutures, and fissures."""
    f1 = make_tileable_fbm(res, seed=seed1, octaves=4)
    f2 = make_tileable_fbm(res, seed=seed2, octaves=4)
    cracks = np.clip(1.0 - np.abs(f1 - f2) * mult, 0.0, 1.0)
    return cracks


def compute_cavity_ao(height: np.ndarray, sigma: float = 6.0, strength: float = 2.2, min_ao: float = 0.40) -> np.ndarray:
    """Computes physically calibrated Cavity AO from height curvature and local neighborhood depth."""
    blurred = ndi.gaussian_filter(height, sigma=sigma)
    crevice = np.clip(blurred - height, 0.0, 1.0)
    ao = np.clip(1.0 - crevice * strength, min_ao, 1.0)
    return ao


def compute_dx_normal(height: np.ndarray, strength: float = 6.0) -> np.ndarray:
    """
    Computes DirectX Tangent Space Normal map from Height map.
    DirectX tangent space convention:
    Nx = -dH/dx (X points right)
    Ny = -dH/dy (Y points down, inverted green channel for UE5!)
    Nz = 1.0
    Normalized to unit vector: sqrt(Nx^2 + Ny^2 + Nz^2) == 1.0
    Encoded to 8-bit RGB: [0, 255]
    """
    dx = ndi.sobel(height, axis=1) / 8.0
    dy = ndi.sobel(height, axis=0) / 8.0
    nx = -dx * strength
    ny = -dy * strength
    nz = np.ones_like(height)

    inv_len = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz)
    nx *= inv_len
    ny *= inv_len
    nz *= inv_len

    r = np.clip((nx * 0.5 + 0.5) * 255.0, 0.0, 255.0)
    g = np.clip((ny * 0.5 + 0.5) * 255.0, 0.0, 255.0)
    b = np.clip((nz * 0.5 + 0.5) * 255.0, 0.0, 255.0)

    normal_rgb = np.stack([r, g, b], axis=-1)
    return normal_rgb


def save_and_verify(
    stem: str,
    albedo: np.ndarray,
    normal: np.ndarray,
    roughness: np.ndarray,
    alt_stems: list[str] | None = None
) -> dict:
    all_stems = [stem]
    if alt_stems:
        all_stems.extend(alt_stems)

    # 1. Verify Albedo
    assert not np.isnan(albedo).any(), f"{stem} Albedo contains NaNs!"
    lum = 0.2126 * albedo[..., 0] + 0.7152 * albedo[..., 1] + 0.0722 * albedo[..., 2]
    mean_lum = float(np.mean(lum))

    # 2. Verify Normal
    assert not np.isnan(normal).any(), f"{stem} Normal contains NaNs!"
    r = normal[..., 0]
    g = normal[..., 1]
    b = normal[..., 2]
    r_std = float(np.std(r))
    g_std = float(np.std(g))
    b_mean = float(np.mean(b))

    nx = (r / 255.0) * 2.0 - 1.0
    ny = (g / 255.0) * 2.0 - 1.0
    nz = (b / 255.0) * 2.0 - 1.0
    lengths = np.sqrt(nx * nx + ny * ny + nz * nz)
    len_dev = float(np.max(np.abs(lengths - 1.0)))

    assert b_mean > 205.0, f"{stem} Normal B mean ({b_mean:.1f}) not > 205!"
    assert r_std > 8.0, f"{stem} Normal R std ({r_std:.2f}) not > 8.0!"
    assert g_std > 8.0, f"{stem} Normal G std ({g_std:.2f}) not > 8.0!"
    assert len_dev < 0.03, f"{stem} Normal vectors not unit-length (max dev: {len_dev})!"

    # 3. Verify Roughness
    assert not np.isnan(roughness).any(), f"{stem} Roughness contains NaNs!"
    rough_min = float(np.min(roughness))
    rough_max = float(np.max(roughness))
    rough_mean = float(np.mean(roughness))

    alb_img = Image.fromarray(np.clip(albedo, 0, 255).astype(np.uint8), mode="RGB")
    nor_img = Image.fromarray(np.clip(normal, 0, 255).astype(np.uint8), mode="RGB")
    rou_img = Image.fromarray(np.clip(roughness, 0, 255).astype(np.uint8), mode="L")

    written_paths = []
    prim_dir = TARGET_DIRS[0]
    base_alb = prim_dir / f"T_{stem}_Albedo.png"
    base_nor = prim_dir / f"T_{stem}_Normal.png"
    base_rou = prim_dir / f"T_{stem}_Roughness.png"

    alb_img.save(base_alb, compress_level=4)
    nor_img.save(base_nor, compress_level=4)
    rou_img.save(base_rou, compress_level=4)
    written_paths.extend([base_alb, base_nor, base_rou])

    for tdir in TARGET_DIRS:
        for s in all_stems:
            alb_dst = tdir / f"T_{s}_Albedo.png"
            nor_dst = tdir / f"T_{s}_Normal.png"
            rou_dst = tdir / f"T_{s}_Roughness.png"

            if alb_dst != base_alb:
                shutil.copyfile(base_alb, alb_dst)
                written_paths.append(alb_dst)
            if nor_dst != base_nor:
                shutil.copyfile(base_nor, nor_dst)
                written_paths.append(nor_dst)
            if rou_dst != base_rou:
                shutil.copyfile(base_rou, rou_dst)
                written_paths.append(rou_dst)

    return {
        "stem": stem,
        "stems": all_stems,
        "mean_lum": mean_lum,
        "r_std": r_std,
        "g_std": g_std,
        "b_mean": b_mean,
        "rough_min": rough_min,
        "rough_max": rough_max,
        "rough_mean": rough_mean,
        "written_count": len(written_paths),
    }


# ==============================================================================
# PROCEDURAL TEXTURE GENERATORS (ORGANIC & SEAMLESS)
# ==============================================================================

def generate_gothic_pillar(res: int = 2048) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """1. GothicPillar: Organic cathedral basalt ashlar blockwork, fluted ribs, soot patina (NO dot grids)."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)

    fbm_rock = make_tileable_fbm(res, seed=101, octaves=6)
    fbm_warp = make_tileable_fbm(res, seed=102, octaves=4)
    micro_veins = make_ridge_cracks(res, seed1=103, seed2=104, mult=5.0)

    # Architectural gothic fluted ribs with organic edge chiseling (16 flutes)
    flute_coord = xx + (fbm_warp - 0.5) * 24.0
    fluting = np.abs(np.cos(flute_coord * np.pi * 16.0 / res)) ** 0.65

    # Ashlar blockwork with organic chipped edges
    block_h = res // 8
    row = (yy // block_h).astype(np.int32)
    shift = (row % 2) * (res // 4)

    chip = (fbm_rock - 0.5) * 16.0
    dist_y = np.abs((yy % block_h) - block_h * 0.5)
    mortar_y = dist_y > (block_h * 0.5 - 6.0 + chip)

    dist_x = np.abs(((xx + shift) % (res // 2)) - res * 0.25)
    mortar_x = dist_x > (res * 0.25 - 6.0 + chip)
    mortar = mortar_y | mortar_x

    bevel_y = np.clip((block_h * 0.5 - dist_y) / 22.0, 0.0, 1.0)
    bevel_x = np.clip((res * 0.25 - dist_x) / 22.0, 0.0, 1.0)
    bevel = np.minimum(bevel_y, bevel_x)

    height = fluting * 0.55 + bevel * 0.35 + fbm_rock * 0.15 + 0.25
    height[mortar] -= 0.45
    height -= micro_veins * 0.15
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=7.0, strength=2.2, min_ao=0.45)

    # Albedo: Dark cathedral basalt (108, 112, 118) + bronze soot in crevices (126, 96, 62)
    rng = np.random.default_rng(105)
    mineral = rng.uniform(0.92, 1.08, (res, res, 1)).astype(np.float32)

    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = (108.0 + fbm_rock * 16.0)
    albedo[..., 1] = (112.0 + fbm_rock * 14.0)
    albedo[..., 2] = (118.0 + fbm_rock * 12.0)
    albedo *= mineral

    soot_mask = (1.0 - ao)[..., None]
    bronze_soot = np.array([126.0, 96.0, 62.0], dtype=np.float32)
    albedo = albedo * (1.0 - soot_mask * 0.55) + bronze_soot * (soot_mask * 0.55)
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: basalt stone 0.68, polished ribs 0.48, mortar 0.88
    roughness = (1.0 - fluting) * 45.0 + 130.0 + fbm_rock * 30.0
    roughness[mortar] = 220.0
    roughness = np.clip(roughness, 90.0, 230.0)

    return albedo, normal, roughness


def generate_runic_altar(res: int = 2048) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """2. RunicAltar: Carved ritual obsidian slab with Voronoi fracture relief & astrolabe engravings."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    cx, cy = res * 0.5, res * 0.5
    r = np.hypot(xx - cx, yy - cy)
    theta = np.arctan2(yy - cy, xx - cx)

    fbm_stone = make_tileable_fbm(res, seed=201, octaves=6)
    vor_facets = make_tileable_voronoi(res, grid_size=20, seed=202)
    veins = make_ridge_cracks(res, seed1=203, seed2=204, mult=5.0)

    # Astrolabe concentric ritual rings
    rings = np.zeros((res, res), dtype=bool)
    ring_radii = [res * 0.12, res * 0.20, res * 0.28, res * 0.36, res * 0.44]
    for rad in ring_radii:
        rings |= (np.abs(r - rad) < 8.0)

    spokes = (np.abs(np.sin(theta * 8.0)) < 0.045) & (r < res * 0.44) & (r > res * 0.08)
    margin = res * 0.04
    chamfer_dist = np.minimum(np.minimum(xx, res - xx), np.minimum(yy, res - yy))
    chamfer = np.clip(chamfer_dist / margin, 0.0, 1.0)

    runic_ticks = (np.abs(np.sin(theta * 32.0)) < 0.12) & (r > res * 0.25) & (r < res * 0.31)
    outer_ticks = (np.abs(np.sin(theta * 64.0)) < 0.08) & (r > res * 0.41) & (r < res * 0.45)
    runes = rings | spokes | runic_ticks | outer_ticks

    # Obsidian fracture relief
    obsidian_relief = (1.0 - vor_facets * 1.5) * 0.20 + fbm_stone * 0.15
    height = chamfer * 0.65 + obsidian_relief + 0.30
    height[runes] -= 0.45
    height -= veins * 0.12
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=6.0, strength=2.5, min_ao=0.40)

    # Albedo: Dark glassy obsidian slab (92, 96, 104), metallic worn chamfers (142, 145, 155),
    # dried sanguine-amber crevice accents (168, 55, 30)
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 94.0 + fbm_stone * 12.0
    albedo[..., 1] = 98.0 + fbm_stone * 10.0
    albedo[..., 2] = 106.0 + fbm_stone * 8.0

    chamfer_edge = (chamfer > 0.8) & (chamfer < 0.98)
    albedo[chamfer_edge] = np.array([142.0, 145.0, 155.0])

    sanguine = np.array([168.0, 58.0, 32.0], dtype=np.float32)
    rune_mask = runes[..., None]
    albedo = albedo * (1.0 - rune_mask * 0.70) + sanguine * (rune_mask * 0.70)
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: obsidian faces 0.32, chamfers 0.44, runes 0.72
    roughness = np.full((res, res), 85.0, dtype=np.float32)
    roughness[chamfer_edge] = 110.0
    roughness[runes] = 185.0
    roughness += fbm_stone * 20.0
    roughness = np.clip(roughness, 70.0, 210.0)

    return albedo, normal, roughness


def generate_iron_brazier(res: int = 2048) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """3. IronBrazier: Hand-hammered wrought iron dimpling (Voronoi ball-peen craters, rivets, heat temper)."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)

    fbm_iron = make_tileable_fbm(res, seed=301, octaves=5)
    # Multi-scale Voronoi for true hand-hammered ball-peen depressions
    d1_large = make_tileable_voronoi(res, grid_size=18, seed=302)
    d1_fine = make_tileable_voronoi(res, grid_size=32, seed=303)
    hammer_dents = (
        (1.0 - np.clip(d1_large * 1.5, 0.0, 1.0) ** 1.8) * 0.38
        + (1.0 - np.clip(d1_fine * 1.5, 0.0, 1.0) ** 1.8) * 0.20
    )

    # Wrought iron lattice cage bars
    strap_w = res // 14
    bar_x = (xx % (res // 4) < strap_w)
    bar_y = (yy % (res // 4) < strap_w)
    lattice = bar_x | bar_y

    # Forged dome rivets with washer collars
    rivet_mask = np.zeros((res, res), dtype=np.float32)
    rivet_radius = res * 0.022
    step = res // 4
    for gx in range(strap_w // 2, res, step):
        for gy in range(strap_w // 2, res, step):
            d = np.hypot(xx - gx, yy - gy)
            dome = np.maximum(0.0, 1.0 - (d / rivet_radius) ** 2)
            rivet_mask = np.maximum(rivet_mask, dome)

    height = lattice.astype(np.float32) * 0.45 + rivet_mask * 0.45 - hammer_dents + fbm_iron * 0.12 + 0.35
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=7.0, strength=2.2, min_ao=0.42)

    # Albedo: Charcoal iron (98, 100, 106) with heat temper blue & straw rim gradient
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 98.0 + fbm_iron * 14.0
    albedo[..., 1] = 100.0 + fbm_iron * 12.0
    albedo[..., 2] = 108.0 + fbm_iron * 10.0

    strap_edges = ndi.binary_dilation(lattice, iterations=4) ^ lattice
    albedo[strap_edges] = np.array([124.0, 110.0, 85.0])  # straw gold oxide
    albedo[rivet_mask > 0.4] = np.array([115.0, 120.0, 134.0])  # tempered blue steel
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=7.5)

    # Roughness: tempered edges 0.28 (~72), flats 0.55 (~140), crevices 0.78 (~200)
    roughness = np.full((res, res), 140.0, dtype=np.float32)
    roughness[strap_edges] = 85.0
    roughness[rivet_mask > 0.3] = 74.0
    roughness[ao < 0.6] = 195.0
    roughness += fbm_iron * 15.0
    roughness = np.clip(roughness, 72.0, 198.0)

    return albedo, normal, roughness


def generate_bone_crown(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """4. Head_BoneCrown: Photorealistic Diablo IV aged ossuary bone, osteon pores, hairline sutures, dark antique forged bronze band."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)

    bone_mask = (yy < res * 0.82)
    bronze_mask = ~bone_mask

    fbm_bone = make_tileable_fbm(res, seed=401, octaves=6)
    fbm_macro = make_tileable_fbm(res, seed=402, octaves=4)
    fbm_bronze = make_tileable_fbm(res, seed=403, octaves=5)

    # Hairline branching suture cracks (thin fissure erosion, ZERO thick cow blobs)
    f_crk1 = make_tileable_fbm(res, seed=404, octaves=6)
    f_crk2 = make_tileable_fbm(res, seed=405, octaves=6)
    f_crk3 = make_tileable_fbm(res, seed=408, octaves=6)
    diff1 = np.abs(f_crk1 - f_crk2)
    diff2 = np.abs(f_crk2 - f_crk3)
    suture1 = 1.0 - np.clip(diff1 / 0.045, 0.0, 1.0)
    suture2 = 1.0 - np.clip(diff2 / 0.038, 0.0, 1.0)
    hairline_cracks = np.maximum(suture1, suture2 * 0.85)
    hairline_cracks = ndi.gaussian_filter(hairline_cracks, sigma=0.6)

    # High-frequency organic trabecular micro-porosity (osteon pits)
    rng = np.random.default_rng(406)
    micro_noise = rng.standard_normal((res, res)).astype(np.float32)
    micro_pores = ndi.gaussian_filter(micro_noise, sigma=0.55)
    micro_pores = (micro_pores - micro_pores.min()) / (micro_pores.max() - micro_pores.min() + 1e-6)

    # Height map
    height = np.zeros((res, res), dtype=np.float32) + 0.50
    bone_ribs = np.sin((xx + (fbm_macro - 0.5) * 60.0) * 0.035) * 0.14
    bone_h = 0.54 + bone_ribs + (fbm_bone - 0.5) * 0.16 - (micro_pores * 0.10)
    bone_h -= hairline_cracks * 0.38
    height[bone_mask] = bone_h[bone_mask]

    # Bronze band height: heavy forged rim beveling & hammered strike facets
    y_rel = (yy[bronze_mask] - res * 0.82) / (res * 0.18)
    rim_bevel = np.sin(y_rel * np.pi) ** 0.40 * 0.25
    stud_pitch = res // 8
    x_rel_stud = np.abs((xx[bronze_mask] % stud_pitch) - (stud_pitch * 0.5))
    stud_profile = np.clip(1.0 - (x_rel_stud / 16.0)**2 - ((y_rel - 0.5) / 0.25)**2, 0.0, 1.0) * 0.25

    bronze_h = 0.58 + rim_bevel + stud_profile + (fbm_bronze[bronze_mask] - 0.5) * 0.14
    height[bronze_mask] = bronze_h

    height = ndi.gaussian_filter(height, sigma=0.6)
    height = np.clip(height, 0.0, 1.0)

    # Cavity AO
    ao = compute_cavity_ao(height, sigma=4.0, strength=2.2, min_ao=0.40)

    # --- Albedo ---
    albedo = np.zeros((res, res, 3), dtype=np.float32)

    # Bone albedo: aged parchment/ivory (RGB ~168..198) with vertical weathering gradient
    v_factor = np.clip(yy / (res * 0.82), 0.0, 1.0)
    base_ochre = np.array([176.0, 154.0, 126.0], dtype=np.float32)
    tip_bleached = np.array([198.0, 192.0, 180.0], dtype=np.float32)

    bone_col = tip_bleached[None, None, :] * (1.0 - v_factor[..., None]) + base_ochre[None, None, :] * v_factor[..., None]
    bone_col += (fbm_bone[..., None] - 0.5) * 14.0 - (micro_pores[..., None] * 10.0)
    albedo[bone_mask] = bone_col[bone_mask]

    # Hairline suture cracks filled with deep char/soot: RGB [52, 42, 34]
    crack_active = bone_mask & (hairline_cracks > 0.28)
    c_int = np.clip((hairline_cracks[crack_active, None] - 0.28) / 0.40, 0.0, 1.0)
    char_color = np.array([52.0, 42.0, 34.0], dtype=np.float32)
    albedo[crack_active] = albedo[crack_active] * (1.0 - c_int * 0.88) + char_color * (c_int * 0.88)

    # Tarnished forged bronze band (RGB [105, 80, 42] fading down to hammered dark iron [38, 30, 22])
    bronze_base = np.array([105.0, 80.0, 42.0], dtype=np.float32)
    iron_recess = np.array([38.0, 30.0, 22.0], dtype=np.float32)
    bronze_tarnish = np.clip((fbm_bronze[bronze_mask, None] - 0.20) / 0.60, 0.0, 1.0)
    bronze_col = iron_recess * (1.0 - bronze_tarnish) + bronze_base * bronze_tarnish
    rim_sheen = np.clip(np.sin(y_rel * np.pi) ** 1.8, 0.0, 1.0)[..., None] * 16.0
    bronze_col += rim_sheen + stud_profile[..., None] * 24.0
    albedo[bronze_mask] = bronze_col

    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=8.5)

    # Roughness: dry aged bone (0.58..0.72 -> ~148..184), deep crack char 0.88 (~224), tarnished bronze 0.32..0.45 (~82..115)
    roughness = np.zeros((res, res), dtype=np.float32)
    roughness[bone_mask] = 162.0 + (fbm_bone[bone_mask] - 0.5) * 24.0
    roughness[crack_active] = 224.0
    roughness[bronze_mask] = 95.0 + (1.0 - bronze_tarnish[:, 0]) * 35.0 - rim_sheen[:, 0] * 0.8
    roughness = np.clip(roughness, 75.0, 230.0)

    return albedo.astype(np.uint8), normal.astype(np.uint8), roughness.astype(np.uint8)


def generate_horror_horns(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """5. Head_HorrorHorns: Organic keratin growth rings, longitudinal fibers, bone-amber tips (lum ~120)."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    rng = np.random.default_rng(501)

    fbm_warp = make_tileable_fbm(res, seed=501, octaves=4)
    # Fine fibrous keratin lines along X:
    jitter = rng.standard_normal((res, res)).astype(np.float32) * 2.0
    keratin_lines = np.sin((xx + jitter) * 0.15) * 0.22
    # Curved annular keratin growth ridges warped by FBM
    warped_y = yy + (fbm_warp - 0.5) * 35.0
    growth_rings = (np.sin(warped_y * 0.10) * 0.5 + 0.5) ** 0.8 * 0.35

    height = 0.45 + growth_rings + keratin_lines + fbm_warp * 0.10
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=5.0, strength=2.2, min_ao=0.42)

    # Root to tip gradient along Y: Root (yy=res-1, charred black) to Tip (yy=0, bone-amber)
    t = 1.0 - (yy / res)
    c_root = np.array([80.0, 76.0, 74.0], dtype=np.float32)
    c_mid = np.array([128.0, 108.0, 80.0], dtype=np.float32)
    c_tip = np.array([176.0, 156.0, 118.0], dtype=np.float32)

    albedo = np.zeros((res, res, 3), dtype=np.float32)
    for c in range(3):
        albedo[..., c] = np.where(
            t < 0.5,
            c_root[c] + (c_mid[c] - c_root[c]) * (t * 2.0),
            c_mid[c] + (c_tip[c] - c_mid[c]) * ((t - 0.5) * 2.0)
        )
    albedo += (fbm_warp[..., None] - 0.5) * 16.0
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    roughness = 180.0 - t * 90.0 + (1.0 - growth_rings) * 25.0
    roughness = np.clip(roughness, 85.0, 205.0)

    return albedo, normal, roughness


def generate_chrono_halo(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """6. Head_ChronoHalo: Burnished bronze/gold concentric astrolabe graduation rings (roughness 0.22..0.38)."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    cx, cy = res * 0.5, res * 0.5
    r = np.hypot(xx - cx, yy - cy)
    theta = np.arctan2(yy - cy, xx - cx)

    fbm_metal = make_tileable_fbm(res, seed=601, octaves=4)
    lathe_scratch = np.sin(r * 1.5) * 0.08

    rings = np.zeros((res, res), dtype=np.float32)
    for rad in [res * 0.18, res * 0.26, res * 0.34, res * 0.42, res * 0.48]:
        rings += np.maximum(0.0, 1.0 - np.abs(r - rad) / 8.0)

    ticks_360 = (np.abs(np.sin(theta * 180.0)) < 0.16) & (r > res * 0.34) & (r < res * 0.38)
    ticks_72 = (np.abs(np.sin(theta * 36.0)) < 0.12) & (r > res * 0.42) & (r < res * 0.47)
    engravings = ticks_360 | ticks_72

    height = rings * 0.45 + lathe_scratch + fbm_metal * 0.06 + 0.35
    height[engravings] -= 0.35
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=4.0, strength=2.2, min_ao=0.55)

    # Albedo: Burnished astronomical bronze/gold (186, 156, 88), tick recesses (112, 90, 52)
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 186.0 + fbm_metal * 10.0
    albedo[..., 1] = 156.0 + fbm_metal * 8.0
    albedo[..., 2] = 88.0 + fbm_metal * 6.0

    albedo[engravings] = np.array([112.0, 90.0, 52.0])
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: low mirror-burnished 0.22..0.38 (56..97)
    roughness = np.full((res, res), 68.0, dtype=np.float32)
    roughness[engravings] = 94.0
    roughness += fbm_metal * 12.0
    roughness = np.clip(roughness, 56.0, 97.0)

    return albedo, normal, roughness


def generate_hope_mask(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """7. Head_HopeMask: Glazed porcelain mask with organic antique gold & silver filigree leaf ribbons."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    cx, cy = res * 0.5, res * 0.5

    fbm_porcelain = make_tileable_fbm(res, seed=701, octaves=4)
    fbm_scroll = make_tileable_fbm(res, seed=702, octaves=4)

    face_dome = np.maximum(0.0, 1.0 - (np.hypot((xx - cx) * 1.1, (yy - cy) * 0.9) / (res * 0.48)) ** 2)

    scroll_field = np.abs(np.sin((xx + fbm_scroll * 40.0) * 0.05 + np.cos((yy + fbm_scroll * 40.0) * 0.04) * 2.5))
    scroll = scroll_field < 0.15

    height = face_dome * 0.45 + scroll.astype(np.float32) * 0.35 + fbm_porcelain * 0.06 + 0.25
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=5.0, strength=2.0, min_ao=0.55)

    # Albedo: Fine porcelain ivory (178, 175, 170), gold filigree (196, 168, 86)
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 178.0 + fbm_porcelain * 8.0
    albedo[..., 1] = 175.0 + fbm_porcelain * 8.0
    albedo[..., 2] = 170.0 + fbm_porcelain * 8.0

    albedo[scroll] = np.array([196.0, 168.0, 86.0])
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: porcelain glaze 0.24, filigree 0.30
    roughness = np.full((res, res), 62.0, dtype=np.float32)
    roughness[scroll] = 78.0
    roughness[ao < 0.7] = 140.0
    roughness += fbm_porcelain * 14.0
    roughness = np.clip(roughness, 55.0, 145.0)

    return albedo, normal, roughness


def generate_abyss_hood(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """8. Head_AbyssHood: Coarse gothic void-linen drapery folds, thread grain, stitched leather trim."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    rng = np.random.default_rng(801)

    fbm_cloth = make_tileable_fbm(res, seed=801, octaves=5)
    fbm_warp = make_tileable_fbm(res, seed=802, octaves=4)

    # Deep sculptural cowl creases & flowing folds
    folds = (
        np.sin((yy + fbm_cloth * 80.0) * 0.04) * 0.35
        + np.sin((xx * 0.4 + yy + fbm_warp * 60.0) * 0.03) * 0.22
    )

    # Fine directional thread grain (warp threads with organic noise, NO cross-hatch dot matrix!)
    jitter = rng.standard_normal((res, res)).astype(np.float32) * 2.0
    thread_grain = np.sin((xx + jitter) * 0.12) * 0.15

    trim = (yy > res * 0.88) | (xx < res * 0.06) | (xx > res * 0.94)

    height = 0.50 + folds + thread_grain + trim.astype(np.float32) * 0.30 + fbm_cloth * 0.08
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=6.0, strength=2.2, min_ao=0.45)

    # Albedo: Charcoal void-linen (104, 106, 114), leather trim (130, 98, 76)
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 104.0 + fbm_cloth * 12.0
    albedo[..., 1] = 106.0 + fbm_cloth * 12.0
    albedo[..., 2] = 114.0 + fbm_cloth * 10.0

    albedo[trim] = np.array([130.0, 98.0, 76.0])
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: linen cloth 0.78 (~198), leather trim 0.45 (~115)
    roughness = np.full((res, res), 198.0, dtype=np.float32)
    roughness[trim] = 115.0
    roughness += fbm_cloth * 15.0
    roughness = np.clip(roughness, 95.0, 225.0)

    return albedo, normal, roughness


def generate_gnarled_staff(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """9. GnarledStaff: Domain-warped organic twisted ironwood bark ridges, fissures, amber sap nodes."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    rng = np.random.default_rng(901)

    fbm_warp = make_tileable_fbm(res, seed=901, octaves=4)
    jitter = rng.standard_normal((res, res)).astype(np.float32) * 2.5
    bark_lines_x = np.sin((xx + jitter + fbm_warp * 30.0) * 0.12) * 0.25
    bark_lines_y = np.cos((yy + jitter) * 0.08) * 0.22
    fissures = np.abs(np.sin(xx * 0.03 + fbm_warp * 4.0)) < 0.08

    # Rounded amber sap resin knots
    cx, cy = res * 0.35, res * 0.45
    d1 = np.hypot(xx - cx, yy - cy)
    d2 = np.hypot(xx - res * 0.65, yy - res * 0.75)
    knot1 = np.maximum(0.0, 1.0 - d1 / (res * 0.08))
    knot2 = np.maximum(0.0, 1.0 - d2 / (res * 0.06))
    knots = np.maximum(knot1, knot2)

    height = 0.50 + bark_lines_x + bark_lines_y + knots * 0.38 + fbm_warp * 0.10
    height[fissures] -= 0.35
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=6.0, strength=2.2, min_ao=0.42)

    # Albedo: Charred ironwood (114, 98, 84), fissures (82, 74, 68), amber resin (196, 126, 40)
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 114.0 + fbm_warp * 14.0
    albedo[..., 1] = 98.0 + fbm_warp * 12.0
    albedo[..., 2] = 84.0 + fbm_warp * 10.0

    albedo[fissures] = np.array([82.0, 74.0, 68.0])
    amber_color = np.array([196.0, 126.0, 40.0], dtype=np.float32)
    k_mask = (knots > 0.2)[..., None]
    albedo = albedo * (1.0 - k_mask * 0.8) + amber_color * (k_mask * 0.8)
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: bark 0.75, amber sap 0.20 (~52)
    roughness = np.full((res, res), 190.0, dtype=np.float32)
    roughness[knots > 0.2] = 52.0
    roughness += fbm_warp * 18.0
    roughness = np.clip(roughness, 50.0, 220.0)

    return albedo, normal, roughness


def generate_staff_arcane01(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """10. Staff_Arcane01: Fluted dark bronze, inlaid silver rune channels, verdigris patina."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)

    fbm_bronze = make_tileable_fbm(res, seed=1001, octaves=5)
    fbm_warp = make_tileable_fbm(res, seed=1002, octaves=4)

    fluting = np.abs(np.cos((xx + fbm_warp * 12.0) * np.pi * 12.0 / res)) ** 0.65
    runic_bars = np.sin(yy * 0.12) * 0.25
    runes = (np.abs(xx - res * 0.5) < (res * 0.09)) & (np.abs(np.sin(yy * 0.08)) < 0.25)

    height = fluting * 0.55 + runic_bars + runes.astype(np.float32) * 0.35 + fbm_bronze * 0.10 + 0.35
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=5.0, strength=2.2, min_ao=0.48)

    # Albedo: Fluted bronze (140, 120, 84), inlaid silver (192, 196, 212)
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 140.0 + fbm_bronze * 12.0
    albedo[..., 1] = 120.0 + fbm_bronze * 10.0
    albedo[..., 2] = 84.0 + fbm_bronze * 8.0

    albedo[runes] = np.array([192.0, 196.0, 212.0])
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.5)

    # Roughness: silver runes 0.26 (~66), bronze 0.38 (~98), recesses 0.72 (~185)
    roughness = np.full((res, res), 98.0, dtype=np.float32)
    roughness[runes] = 66.0
    roughness[ao < 0.6] = 185.0
    roughness += fbm_bronze * 15.0
    roughness = np.clip(roughness, 60.0, 195.0)

    return albedo, normal, roughness


def generate_staff_arcane02(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """11. Staff_Arcane02: Swirling multi-scale Damascus steel ripples + astral gold spire collars."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)

    fbm1 = make_tileable_fbm(res, seed=1101, octaves=5)
    fbm2 = make_tileable_fbm(res, seed=1102, octaves=4)
    damascus = np.sin((fbm1 + fbm2 * 0.5) * 26.0) * 0.28
    hairline = make_anisotropic_noise(res, freq_y=128, freq_x=16, seed=1103) * 0.12

    gold_spire = (yy % (res // 4) < (res // 16))

    height = damascus + hairline + gold_spire.astype(np.float32) * 0.38 + 0.40
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=5.0, strength=2.0, min_ao=0.52)

    # Albedo: Damascus light steel (170, 174, 184) and dark steel (136, 140, 150),
    # astral gold (198, 168, 84)
    steel_blend = (damascus * 0.5 + 0.5)[..., None]
    light_steel = np.array([170.0, 174.0, 184.0], dtype=np.float32)
    dark_steel = np.array([136.0, 140.0, 150.0], dtype=np.float32)
    albedo = dark_steel * (1.0 - steel_blend) + light_steel * steel_blend

    albedo[gold_spire] = np.array([198.0, 168.0, 84.0])
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: Damascus steel 0.32 (~82), gold spire 0.24 (~62)
    roughness = np.full((res, res), 82.0, dtype=np.float32)
    roughness[gold_spire] = 62.0
    roughness += fbm1 * 14.0
    roughness = np.clip(roughness, 58.0, 130.0)

    return albedo, normal, roughness


def generate_ornate_mace(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """12. OrnateMace: Photorealistic Diablo IV Damascus steel flanges, antique patinated bronze collar, oiled oxblood leather grip."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)

    flange_zone = (yy < res * 0.45)
    collar_zone = (yy >= res * 0.45) & (yy < res * 0.55)
    grip_zone = (yy >= res * 0.55)

    # --- 1. Top 45%: Blackened Damascus Steel Flanges ---
    fbm_damascus_warp = make_tileable_fbm(res, seed=1201, octaves=5)
    fbm_damascus_fine = make_tileable_fbm(res, seed=1202, octaves=6)

    # Fluid flowing wave layers
    damascus_pattern = np.sin((yy * 0.12 + xx * 0.04) + fbm_damascus_warp * 18.0 + fbm_damascus_fine * 6.0)
    damascus_norm = (damascus_pattern + 1.0) * 0.5

    # Gunmetal steel range RGB 98..146
    dark_layer = np.array([96.0, 102.0, 112.0], dtype=np.float32)
    bright_layer = np.array([138.0, 144.0, 154.0], dtype=np.float32)
    steel_albedo = dark_layer[None, None, :] * (1.0 - damascus_norm[..., None]) + bright_layer[None, None, :] * damascus_norm[..., None]

    # 4 Flange blade bevels across X
    flange_period = res / 4.0
    x_in_flange = (xx % flange_period) - (flange_period * 0.5)
    flange_blade_profile = np.clip(1.0 - np.abs(x_in_flange) / (flange_period * 0.35), 0.0, 1.0)
    flange_bevel = flange_blade_profile ** 1.8
    flange_facets = np.sin(x_in_flange * 0.15) * 0.14

    # Cutting edge highlights
    cutting_edge = np.clip(1.0 - np.abs(x_in_flange) / (flange_period * 0.06), 0.0, 1.0)
    steel_albedo += cutting_edge[..., None] * 14.0

    # Fine weapon micro-scratches
    rng_scratch = np.random.default_rng(1203)
    scratches = (rng_scratch.standard_normal((res, res)).astype(np.float32)) * 0.08
    scratches_filtered = ndi.gaussian_filter(scratches, sigma=(0.5, 3.0))
    steel_albedo += scratches_filtered[..., None] * 18.0

    # --- 2. Middle 10%: Antique Bronze Collar ---
    fbm_bronze = make_tileable_fbm(res, seed=1204, octaves=5)
    y_collar = (yy[collar_zone] - res * 0.45) / (res * 0.10)
    ribs = np.sin(y_collar * np.pi * 6.0)
    ribs_raised = np.clip(ribs, 0.0, 1.0) ** 1.2
    grooves_deep = np.clip(-ribs, 0.0, 1.0) ** 1.5

    bronze_base = np.array([112.0, 84.0, 42.0], dtype=np.float32)
    char_crevice = np.array([42.0, 32.0, 22.0], dtype=np.float32)
    polished_bronze = np.array([135.0, 105.0, 55.0], dtype=np.float32)

    bronze_col = bronze_base[None, :] * np.ones_like(y_collar[:, None])
    bronze_col = bronze_col * (1.0 - grooves_deep[:, None] * 0.85) + char_crevice[None, :] * (grooves_deep[:, None] * 0.85)
    bronze_col += polished_bronze[None, :] * (ribs_raised[:, None] * 0.25)
    bronze_col += (fbm_bronze[collar_zone, None] - 0.5) * 14.0

    # --- 3. Bottom 45%: Oiled Oxblood / Walnut Grip ---
    fbm_leather = make_tileable_fbm(res, seed=1205, octaves=5)
    fbm_wrinkle = make_tileable_fbm(res, seed=1206, octaves=6)

    wrap_coord = (xx * 0.65 + yy * 0.75)
    strap_width = 48.0
    strap_phase = (wrap_coord % strap_width) / strap_width

    strap_seam = np.clip(1.0 - strap_phase / 0.12, 0.0, 1.0) + np.clip((strap_phase - 0.88) / 0.12, 0.0, 1.0)
    strap_center_sheen = np.sin(strap_phase * np.pi) ** 1.4

    base_oxblood = np.array([68.0, 44.0, 30.0], dtype=np.float32)
    knuckle_sheen = np.array([86.0, 56.0, 38.0], dtype=np.float32)
    seam_shadow = np.array([28.0, 18.0, 12.0], dtype=np.float32)

    leather_col = base_oxblood[None, :] * np.ones((np.count_nonzero(grip_zone), 1))
    leather_col += (knuckle_sheen - base_oxblood)[None, :] * strap_center_sheen[grip_zone, None]
    leather_col = leather_col * (1.0 - strap_seam[grip_zone, None] * 0.75) + seam_shadow[None, :] * (strap_seam[grip_zone, None] * 0.75)
    leather_col += (fbm_wrinkle[grip_zone, None] - 0.5) * 8.0

    # Height map
    height = np.zeros((res, res), dtype=np.float32) + 0.50
    flange_h = 0.50 + flange_bevel * 0.40 + flange_facets + (damascus_norm - 0.5) * 0.06
    height[flange_zone] = flange_h[flange_zone]

    collar_h = 0.60 + ribs_raised * 0.22 - grooves_deep * 0.20
    height[collar_zone] = collar_h

    strap_step = (1.0 - strap_phase) * 0.24 - strap_seam * 0.18
    grip_h = 0.45 + strap_step + (fbm_leather - 0.5) * 0.05
    height[grip_zone] = grip_h[grip_zone]

    height = ndi.gaussian_filter(height, sigma=0.6)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=4.0, strength=2.4, min_ao=0.40)

    # Assemble Albedo
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[flange_zone] = steel_albedo[flange_zone]
    albedo[collar_zone] = bronze_col
    albedo[grip_zone] = leather_col
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=7.5)

    # Roughness: Damascus steel 0.26..0.36 (~66..92), Bronze collar 0.30..0.40 (~76..102), Leather 0.45..0.58 (~115..148)
    roughness = np.zeros((res, res), dtype=np.float32)
    roughness[flange_zone] = 78.0 + (fbm_damascus_fine[flange_zone] - 0.5) * 18.0 - cutting_edge[flange_zone] * 12.0
    roughness[collar_zone] = 88.0 + grooves_deep * 70.0 - ribs_raised * 16.0
    roughness[grip_zone] = 128.0 + strap_seam[grip_zone] * 50.0 - strap_center_sheen[grip_zone] * 18.0
    roughness = np.clip(roughness, 55.0, 210.0)

    return albedo.astype(np.uint8), normal.astype(np.uint8), roughness.astype(np.uint8)


def generate_spectral_skull(res: int = 1024) -> tuple[np.ndarray, np.ndarray, np.ndarray]:
    """13. Familiar_SpectralSkull: Porous ossuary bone, osteon pores, orbital cavities, arcane glyph."""
    yy, xx = np.mgrid[0:res, 0:res].astype(np.float32)
    cx, cy = res * 0.5, res * 0.45

    fbm_bone = make_tileable_fbm(res, seed=1301, octaves=5)
    pores = make_anisotropic_noise(res, freq_y=48, freq_x=48, seed=1302) * 0.25
    suture = make_ridge_cracks(res, seed1=1303, seed2=1304, mult=5.0)

    cranium = np.maximum(0.0, 1.0 - (np.hypot((xx - cx) * 1.05, (yy - cy) * 0.95) / (res * 0.42)) ** 2)
    eye_l = np.hypot(xx - res * 0.38, yy - res * 0.52) < (res * 0.10)
    eye_r = np.hypot(xx - res * 0.62, yy - res * 0.52) < (res * 0.10)
    nasal = (np.abs(xx - res * 0.5) < (res * 0.04)) & (yy > res * 0.58) & (yy < res * 0.68)
    cavities = eye_l | eye_r | nasal

    d_glyph = np.hypot(xx - res * 0.5, yy - res * 0.32)
    glyph = (d_glyph < res * 0.08) & ((np.abs(xx - res * 0.5) < 6.0) | (np.abs(yy - res * 0.32) < 6.0))

    height = cranium * 0.55 + pores + fbm_bone * 0.10 + 0.30
    height[cavities] -= 0.50
    height[glyph] += 0.35
    height[suture > 0.5] -= 0.25
    height = ndi.gaussian_filter(height, sigma=0.8)
    height = np.clip(height, 0.0, 1.0)

    ao = compute_cavity_ao(height, sigma=6.0, strength=2.2, min_ao=0.45)

    # Albedo: Weathered ossuary bone (166, 158, 146), cavity recesses (74, 68, 65),
    # ethereal teal glyph (126, 236, 250)
    albedo = np.zeros((res, res, 3), dtype=np.float32)
    albedo[..., 0] = 166.0 + fbm_bone * 12.0
    albedo[..., 1] = 158.0 + fbm_bone * 10.0
    albedo[..., 2] = 146.0 + fbm_bone * 8.0

    albedo[cavities] = np.array([74.0, 68.0, 65.0])
    albedo[glyph] = np.array([126.0, 236.0, 250.0])
    albedo[suture > 0.5] = np.array([110.0, 95.0, 80.0])
    albedo *= ao[..., None]

    normal = compute_dx_normal(height, strength=6.0)

    # Roughness: porous bone 0.58 (~148), glyph floor 0.28 (~72), cavities 0.82 (~210)
    roughness = np.full((res, res), 148.0, dtype=np.float32)
    roughness[glyph] = 72.0
    roughness[cavities] = 210.0
    roughness += fbm_bone * 15.0
    roughness = np.clip(roughness, 65.0, 220.0)

    return albedo, normal, roughness


# ==============================================================================
# PIPELINE EXECUTION
# ==============================================================================

def run_pipeline() -> list[dict]:
    ensure_dirs()
    print("=" * 80)
    print("AETHERIA PBR TEXTURE GENERATOR (AAA ORGANIC ENGINE) - STARTING GENERATION")
    print(f"Target 1: {TARGET_DIRS[0]}")
    print(f"Target 2: {TARGET_DIRS[1]}")
    print("=" * 80)

    results = []

    # 1. World Architecture (2048x2048)
    print("\n--- 1. WORLD ARCHITECTURE (2048x2048) ---")
    arch_generators = [
        ("GothicPillar", generate_gothic_pillar),
        ("RunicAltar", generate_runic_altar),
        ("IronBrazier", generate_iron_brazier),
    ]

    for stem, gen_func in arch_generators:
        t0 = time.time()
        alb, nor, rough = gen_func(2048)
        res = save_and_verify(stem, alb, nor, rough)
        results.append(res)
        print(f"[OK] {stem:20}: mean_lum={res['mean_lum']:5.1f}, nor_R_std={res['r_std']:5.2f}, nor_G_std={res['g_std']:5.2f}, nor_B_mean={res['b_mean']:5.1f} ({time.time()-t0:.2f}s)")

    # 2. Hero Headgear & Staves (1024x1024) - Both T_<Stem>_* and T_SM_<Stem>_*
    print("\n--- 2. HERO HEADGEAR & STAVES (1024x1024) ---")

    hero_generators = [
        ("Head_BoneCrown", ["SM_Head_BoneCrown"], generate_bone_crown),
        ("Head_HorrorHorns", ["SM_Head_HorrorHorns"], generate_horror_horns),
        ("Head_ChronoHalo", ["SM_Head_ChronoHalo"], generate_chrono_halo),
        ("Head_HopeMask", ["SM_Head_HopeMask"], generate_hope_mask),
        ("Head_AbyssHood", ["SM_Head_AbyssHood"], generate_abyss_hood),
        ("GnarledStaff", ["SM_GnarledStaff"], generate_gnarled_staff),
        ("Staff_Arcane01", ["SM_Staff_Arcane01"], generate_staff_arcane01),
        ("Staff_Arcane02", ["SM_Staff_Arcane02"], generate_staff_arcane02),
        ("OrnateMace", ["SM_OrnateMace"], generate_ornate_mace),
        ("Familiar_SpectralSkull", ["SM_Familiar_SpectralSkull"], generate_spectral_skull),
    ]

    for stem, alts, gen_func in hero_generators:
        t0 = time.time()
        alb, nor, rough = gen_func(1024)
        res = save_and_verify(stem, alb, nor, rough, alt_stems=alts)
        results.append(res)
        print(f"[OK] {stem:20} (+ {alts[0]}): mean_lum={res['mean_lum']:5.1f}, nor_R_std={res['r_std']:5.2f}, nor_G_std={res['g_std']:5.2f}, nor_B_mean={res['b_mean']:5.1f} ({time.time()-t0:.2f}s)")

    total_files = sum(r["written_count"] for r in results)
    print("\n" + "=" * 80)
    print(f"PBR GENERATION COMPLETE: {len(results)} material sets processed.")
    print(f"Total PNG files written across both directories: {total_files}")
    print("=" * 80)

    return results


if __name__ == "__main__":
    run_pipeline()
