#!/usr/bin/env python3
"""Build 5 AAA Grimdark Diablo IV Rigged 3D Characters + 15 Dense Biomechanical
Animations in Blender 4.2.3 CLI and export identical verified .glb assets to:
  1) c:\\Users\\tagir\\OneDrive\\Desktop\\Aetheria\\GodotAetheria\\assets\\models\\
  2) C:\\Games\\AETHERIA\\AetheriaData\\assets\\models\\

Features:
  - Self-contained 2048x2048 procedural grimdark PBR texture synthesizer
    (hammered dark steel, weathered crimson/indigo woven wool, antique gilded
    brass, carved grim bone, obsidian carapace, glowing rune filigree) using
    pure NumPy + lossless PNG (Sub-filtered zlib) encoding.
  - 7.5-8-head anatomical proportions & sculpted BMesh geometry (4k-12k verts)
    with shoulder-conforming multi-tiered pauldrons, contoured gothic cuirasses,
    anatomical skulls, exposed 6-pair ribcages, horned helms, split pleated
    battle coats, flowing capes, and ornate Diablo IV weapons.
  - Uniform 24-bone armature:
    * Coordinate conventions: Face points along -Y in Blender (+Z Up, -Y Forward, +X Left).
    * When exported with standard glTF Y-up, Blender -Y maps to glTF +Z Forward (+Y Up, +Z Forward, +X Left).
    * Guaranteed consistent across Blender viewport, glTF 2.0 binary, and Godot 4.x scene importers.
    * Min Z = 0.0 rest pose with top-4 normalized anatomical capsule skinning.
  - 15 dense 30 FPS AAA dark-fantasy biomechanical animations (Diablo IV / No Rest for the Wicked grade):
    * 5 core gait phases: Contact (heel strike), Down (compression & Trendelenburg hip tilt),
      Passing (mid-stance knee tuck), Up (extension push-off & apex rise), Follow-Through (deceleration & reach).
    * Dynamic 3D pelvis kinematics: vertical bobbing (2 full dips/peaks per stride), forward lean pitch,
      Trendelenburg lateral roll, and yaw counter to thoracic spine.
    * Torso counter-rotation: thoracic spine and chest counter-twist against pelvis yaw (>20° total twist).
    * Head gaze stabilization: cancels torso pitch/yaw/roll so character eyes stay locked forward on target.
    * Staff/weapon secondary drag: inertial phase lag (~3 frames) with 3D elliptical follow-through loop.
    * Aerodynamic cape physics: multi-segment trailing C-curve with progressive vertical wave propagation
      and dynamic lateral S-curve sway across Cape_1 -> Cape_2 -> Cape_3.
    * Binary GLB JSON post-processing guaranteeing exact canonical clip names matching Godot expectations.
"""

from __future__ import annotations

import json
import math
import shutil
import struct
import zlib
from pathlib import Path
from typing import Callable, Dict, List, Optional, Tuple

import bmesh
import bpy
import numpy as np
from mathutils import Euler, Matrix, Vector

SCRIPT_DIR = Path(__file__).resolve().parent
TEX_CACHE_DIR = SCRIPT_DIR / "_cache" / "d4_tex_2k_v2"

OUT_DIRS = [
    Path(r"c:\Users\tagir\OneDrive\Desktop\Aetheria\GodotAetheria\assets\models"),
    Path(r"C:\Games\AETHERIA\AetheriaData\assets\models"),
]

CANONICAL_ANIMS = [
    "Idle",
    "Run_Fwd",
    "Run_Back",
    "Run_StrafeL",
    "Run_StrafeR",
    "Cast_Primary",
    "Cast_Heavy",
    "Dash",
    "Attack_Melee",
    "Attack_Slam",
    "Hit_React",
    "Downed_Loop",
    "Revive_Channel",
    "Revive_Rise",
    "Death",
]


# ---------------------------------------------------------------------------
# Fast Sub-Filtered Lossless PNG Writer + 2048x2048 Grimdark PBR Texture Synth
# ---------------------------------------------------------------------------

def _png_chunk(tag: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(tag + payload) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + tag + payload + struct.pack(">I", crc)


def _write_png_rgb(path: Path, rgb_u8: np.ndarray) -> None:
    """Write an (H, W, 3) uint8 RGB array using PNG Sub filter (filter=1) for compact lossless size."""
    h, w, c = rgb_u8.shape
    assert c == 3
    flat = rgb_u8.reshape(h, w * 3)
    sub = np.empty_like(flat)
    sub[:, :3] = flat[:, :3]
    sub[:, 3:] = (flat[:, 3:].astype(np.int16) - flat[:, :-3].astype(np.int16)) & 0xFF
    raw_rows = np.hstack([np.ones((h, 1), dtype=np.uint8), sub])
    compressed = zlib.compress(raw_rows.tobytes(), level=6)
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    png_bytes = (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", ihdr)
        + _png_chunk(b"IDAT", compressed)
        + _png_chunk(b"IEND", b"")
    )
    path.write_bytes(png_bytes)


def _tileable_fbm(
    size: int,
    octaves: Tuple[int, ...],
    weights: Tuple[float, ...],
    seed: int,
    aniso_y: float = 1.0,
) -> np.ndarray:
    """Vectorized tileable 2D value-noise FBM in NumPy."""
    rng = np.random.default_rng(seed)
    acc = np.zeros((size, size), dtype=np.float32)
    wsum = 0.0
    for g, w in zip(octaves, weights):
        gy = max(2, int(round(g * aniso_y)))
        gx = max(2, g)
        base = rng.random((gy, gx), dtype=np.float32)
        cy = np.linspace(0, gy, size, endpoint=False, dtype=np.float32)
        cx = np.linspace(0, gx, size, endpoint=False, dtype=np.float32)
        iy = np.floor(cy).astype(np.int32)
        ix = np.floor(cx).astype(np.int32)
        fy = cy - iy
        fx = cx - ix
        fy = fy * fy * (3.0 - 2.0 * fy)
        fx = fx * fx * (3.0 - 2.0 * fx)
        y0, y1 = iy % gy, (iy + 1) % gy
        x0, x1 = ix % gx, (ix + 1) % gx
        row = base[y0, :] * (1.0 - fy[:, None]) + base[y1, :] * fy[:, None]
        acc += w * (row[:, x0] * (1.0 - fx[None, :]) + row[:, x1] * fx[None, :])
        wsum += w
    return np.clip(acc / max(wsum, 1e-6), 0.0, 1.0)


def _tileable_hammer_cells(size: int, grid_n: int, seed: int) -> np.ndarray:
    """Smooth rounded hammer-peen / dimple map via 3x3 tiled Worley distance."""
    rng = np.random.default_rng(seed)
    pts_y = rng.uniform(0.15, 0.85, (grid_n, grid_n)).astype(np.float32)
    pts_x = rng.uniform(0.15, 0.85, (grid_n, grid_n)).astype(np.float32)

    coords = np.linspace(0.0, float(grid_n), size, endpoint=False, dtype=np.float32)
    cy = np.floor(coords).astype(np.int32)
    cx = cy
    fy = coords - cy
    fx = fy

    min_d2 = np.full((size, size), 10.0, dtype=np.float32)
    for dy in (-1, 0, 1):
        iy = (cy + dy) % grid_n
        oy = dy - fy
        for dx in (-1, 0, 1):
            ix = (cx + dx) % grid_n
            ox = dx - fx
            py = pts_y[iy][:, ix] + oy[:, None]
            px = pts_x[iy][:, ix] + ox[None, :]
            min_d2 = np.minimum(min_d2, px * px + py * py)
    return np.clip(np.sqrt(min_d2), 0.0, 1.2)


def _normal_from_height(h: np.ndarray, strength: float = 3.2) -> np.ndarray:
    dx = (np.roll(h, -1, axis=1) - np.roll(h, 1, axis=1)) * strength
    dy = (np.roll(h, -1, axis=0) - np.roll(h, 1, axis=0)) * strength
    nx, ny, nz = -dx, dy, np.ones_like(h, dtype=np.float32)
    inv = 1.0 / (np.sqrt(nx * nx + ny * ny + nz * nz) + 1e-8)
    rgb = np.stack([(nx * inv) * 0.5 + 0.5, (ny * inv) * 0.5 + 0.5, (nz * inv) * 0.5 + 0.5], axis=-1)
    return np.clip(rgb * 255.0, 0, 255).astype(np.uint8)


def _save_pbr_set(
    prefix: str,
    albedo: np.ndarray,
    height: np.ndarray,
    ao: np.ndarray,
    roughness: np.ndarray,
    metallic: np.ndarray,
    normal_strength: float,
    emissive: Optional[np.ndarray] = None,
) -> None:
    TEX_CACHE_DIR.mkdir(parents=True, exist_ok=True)
    _write_png_rgb(TEX_CACHE_DIR / f"{prefix}_albedo.png", np.clip(albedo * 255.0, 0, 255).astype(np.uint8))
    orm = np.stack([np.clip(ao, 0.0, 1.0), np.clip(roughness, 0.04, 0.99), np.clip(metallic, 0.0, 1.0)], axis=-1)
    _write_png_rgb(TEX_CACHE_DIR / f"{prefix}_orm.png", np.clip(orm * 255.0, 0, 255).astype(np.uint8))
    _write_png_rgb(TEX_CACHE_DIR / f"{prefix}_normal.png", _normal_from_height(height, strength=normal_strength))
    if emissive is not None:
        _write_png_rgb(TEX_CACHE_DIR / f"{prefix}_emissive.png", np.clip(emissive * 255.0, 0, 255).astype(np.uint8))


def ensure_d4_2k_textures() -> Path:
    """Generate 2048x2048 Diablo IV PBR texture sets once and cache on disk."""
    prefixes = (
        "d4_hammered_steel",
        "d4_woven_wool",
        "d4_gilded_brass",
        "d4_grim_bone",
        "d4_obsidian_carapace",
        "d4_rune_filigree",
    )
    expected = [TEX_CACHE_DIR / f"{p}_{k}.png" for p in prefixes for k in ("albedo", "orm", "normal")]
    expected.append(TEX_CACHE_DIR / "d4_rune_filigree_emissive.png")
    if all(p.exists() and p.stat().st_size > 80_000 for p in expected):
        print(f"[TEX] Using cached 2048x2048 PBR maps in {TEX_CACHE_DIR}")
        return TEX_CACHE_DIR

    S = 2048
    print(f"[TEX] Synthesizing 6x 2048x2048 Diablo IV PBR texture sets into {TEX_CACHE_DIR}...")
    yy, xx = np.mgrid[0:S, 0:S].astype(np.float32) / float(S)

    macro = _tileable_fbm(S, (6, 12, 24), (0.50, 0.32, 0.18), seed=101)
    mid = _tileable_fbm(S, (32, 64, 128), (0.45, 0.35, 0.20), seed=202)
    micro = _tileable_fbm(S, (192, 384), (0.60, 0.40), seed=303)

    # 1. Hammered Dark Steel
    dimple_coarse = _tileable_hammer_cells(S, 30, seed=401)
    dimple_fine = _tileable_hammer_cells(S, 60, seed=402)
    hammer = 0.65 * (1.0 - dimple_coarse) + 0.35 * (1.0 - dimple_fine)
    brush = _tileable_fbm(S, (16, 128, 256), (0.20, 0.45, 0.35), seed=403, aniso_y=0.22)
    scratch = np.clip((_tileable_fbm(S, (12, 96), (0.35, 0.65), seed=404, aniso_y=4.0) - 0.72) * 3.8, 0.0, 1.0)
    h_steel = 0.42 + 0.26 * hammer + 0.16 * macro + 0.10 * brush - 0.14 * scratch + 0.06 * micro
    alb_s = 0.46 + 0.22 * macro + 0.14 * hammer + 0.08 * brush + 0.10 * scratch
    alb_steel = np.stack([alb_s * 0.96, alb_s * 0.98, np.clip(alb_s * 1.05, 0.0, 1.0)], axis=-1)
    ao_steel = np.clip(0.74 + 0.26 * hammer - 0.18 * (1.0 - macro), 0.55, 1.0)
    rgh_steel = np.clip(0.24 + 0.24 * (1.0 - hammer) + 0.12 * (1.0 - macro) - 0.10 * scratch + 0.08 * micro, 0.15, 0.54)
    met_steel = np.clip(0.88 + 0.08 * hammer - 0.10 * (1.0 - macro), 0.76, 0.97)
    _save_pbr_set("d4_hammered_steel", alb_steel, h_steel, ao_steel, rgh_steel, met_steel, normal_strength=3.2)

    # 2. Weathered Woven Wool / Heavy Twill
    twill = 0.5 + 0.5 * np.sin((xx * 256.0 + yy * 256.0) * 2.0 * math.pi)
    warp = 0.5 + 0.5 * np.sin(xx * 512.0 * 2.0 * math.pi)
    weft = 0.5 + 0.5 * np.sin(yy * 512.0 * 2.0 * math.pi)
    weave = 0.55 * twill + 0.45 * (warp * 0.5 + weft * 0.5)
    slub = _tileable_fbm(S, (24, 160), (0.35, 0.65), seed=501, aniso_y=0.35)
    soot = np.clip(1.0 - macro * 0.50 - mid * 0.22, 0.45, 1.0)
    h_wool = 0.35 + 0.28 * weave + 0.20 * macro + 0.12 * slub + 0.05 * micro
    alb_w = np.clip((0.58 + 0.20 * macro + 0.14 * weave + 0.08 * slub) * soot, 0.18, 0.95)
    alb_wool = np.stack([alb_w, alb_w * 0.98, alb_w * 0.97], axis=-1)
    ao_wool = np.clip(0.68 + 0.32 * weave * macro, 0.55, 1.0)
    rgh_wool = np.clip(0.76 + 0.15 * (1.0 - weave) + 0.07 * micro, 0.66, 0.96)
    met_wool = np.zeros((S, S), dtype=np.float32)
    _save_pbr_set("d4_woven_wool", alb_wool, h_wool, ao_wool, rgh_wool, met_wool, normal_strength=2.6)

    # 3. Antique Gilded Brass
    filigree_wave = 0.5 + 0.5 * np.sin(xx * 32.0 * math.pi) * np.cos(yy * 32.0 * math.pi)
    tarnish = np.clip((1.0 - macro) * 0.65 + (1.0 - hammer) * 0.35, 0.0, 1.0)
    h_brass = 0.45 + 0.25 * hammer + 0.15 * filigree_wave + 0.10 * brush + 0.05 * micro
    alb_brass = np.stack([
        np.clip(0.86 - 0.34 * tarnish + 0.10 * brush, 0.22, 1.0),
        np.clip(0.68 - 0.30 * tarnish + 0.08 * brush, 0.18, 0.95),
        np.clip(0.32 - 0.14 * tarnish + 0.05 * micro, 0.08, 0.65),
    ], axis=-1)
    ao_brass = np.clip(0.68 + 0.32 * (1.0 - tarnish * 0.7), 0.55, 1.0)
    rgh_brass = np.clip(0.18 + 0.26 * tarnish + 0.08 * micro, 0.14, 0.50)
    met_brass = np.clip(0.92 - 0.16 * tarnish, 0.72, 0.98)
    _save_pbr_set("d4_gilded_brass", alb_brass, h_brass, ao_brass, rgh_brass, met_brass, normal_strength=2.8)

    # 4. Carved Grim Bone
    stria = _tileable_fbm(S, (8, 48, 192), (0.25, 0.40, 0.35), seed=601, aniso_y=3.2)
    pores = np.clip((micro - 0.64) * 3.4, 0.0, 1.0)
    crack_field = np.abs(_tileable_fbm(S, (10, 28, 72), (0.4, 0.4, 0.2), seed=602) - 0.5) * 2.0
    cracks = np.clip((0.075 - crack_field) * 14.0, 0.0, 1.0)
    h_bone = 0.58 + 0.20 * macro + 0.16 * stria - 0.24 * pores - 0.30 * cracks
    alb_bone = np.stack([
        np.clip(0.78 + 0.14 * macro - 0.22 * pores - 0.35 * cracks, 0.18, 0.96),
        np.clip(0.72 + 0.13 * macro - 0.24 * pores - 0.34 * cracks, 0.15, 0.90),
        np.clip(0.60 + 0.12 * macro - 0.24 * pores - 0.30 * cracks, 0.12, 0.80),
    ], axis=-1)
    ao_bone = np.clip(0.92 - 0.35 * pores - 0.45 * cracks - 0.15 * (1.0 - macro), 0.40, 1.0)
    rgh_bone = np.clip(0.38 + 0.34 * pores + 0.22 * cracks + 0.14 * (1.0 - macro), 0.32, 0.92)
    met_bone = np.zeros((S, S), dtype=np.float32)
    _save_pbr_set("d4_grim_bone", alb_bone, h_bone, ao_bone, rgh_bone, met_bone, normal_strength=3.6)

    # 5. Demonic Obsidian Carapace (chiseled dark volcanic glass facets & forged scales, zero zebra striping)
    ridge = np.abs(macro - 0.5) * 2.0
    scale_cells = _tileable_hammer_cells(S, 24, seed=701)
    facet = np.clip(1.0 - scale_cells, 0.0, 1.0)
    veins = np.clip((0.06 - np.abs(mid - 0.5) * 2.0) * 12.0, 0.0, 1.0)
    h_car = 0.42 + 0.24 * ridge + 0.24 * facet - 0.18 * veins + 0.10 * micro
    alb_c = np.clip(0.38 + 0.20 * ridge + 0.18 * facet - 0.14 * veins + 0.06 * micro, 0.12, 0.88)
    alb_car = np.stack([alb_c * 0.93, alb_c * 0.91, np.clip(alb_c * 1.08, 0.0, 1.0)], axis=-1)
    ao_car = np.clip(0.70 + 0.28 * facet - 0.22 * veins, 0.46, 1.0)
    rgh_car = np.clip(0.18 + 0.24 * (1.0 - facet) + 0.16 * veins + 0.08 * micro, 0.14, 0.58)
    met_car = np.clip(0.72 + 0.18 * facet - 0.12 * veins, 0.52, 0.92)
    _save_pbr_set("d4_obsidian_carapace", alb_car, h_car, ao_car, rgh_car, met_car, normal_strength=3.4)

    # 6. Delicate Carved Horadric Rune Filigree (fine intricate etched sigils + subtle glowing inlays)
    tx = (xx * 6.0) % 1.0 - 0.5
    ty = (yy * 6.0) % 1.0 - 0.5
    rad = np.sqrt(tx * tx + ty * ty)
    ang = np.arctan2(ty, tx)
    ring1 = np.exp(-((rad - 0.36) / 0.009) ** 2)
    ring2 = np.exp(-((rad - 0.20) / 0.008) ** 2)
    diamond = np.exp(-((np.abs(tx) + np.abs(ty) - 0.44) / 0.009) ** 2)
    spokes = np.exp(-((np.abs(np.sin(ang * 4.0)) * rad) / 0.010) ** 2) * (rad < 0.36) * (rad > 0.12)
    border = np.exp(-((np.maximum(np.abs(tx), np.abs(ty)) - 0.48) / 0.009) ** 2)
    rune_mask = np.clip(np.maximum.reduce([ring1, ring2, diamond, spokes * 0.75, border * 0.65]), 0.0, 1.0)

    h_rune = 0.60 + 0.16 * hammer + 0.12 * macro - 0.36 * rune_mask
    alb_r = 0.34 + 0.16 * macro + 0.10 * hammer + 0.38 * rune_mask
    alb_rune = np.stack([alb_r * 0.98, alb_r * 0.94, alb_r * 0.90], axis=-1)
    ao_rune = np.clip(0.84 - 0.28 * rune_mask - 0.15 * (1.0 - hammer), 0.45, 1.0)
    rgh_rune = np.clip(0.30 + 0.18 * (1.0 - hammer) - 0.14 * rune_mask, 0.12, 0.54)
    met_rune = np.clip(0.88 - 0.38 * rune_mask, 0.40, 0.95)
    emi_rune = np.stack([rune_mask, rune_mask, rune_mask], axis=-1)
    _save_pbr_set("d4_rune_filigree", alb_rune, h_rune, ao_rune, rgh_rune, met_rune, normal_strength=3.4, emissive=emi_rune)

    print("[TEX] 2048x2048 PBR texture synthesis complete.")
    return TEX_CACHE_DIR


# ---------------------------------------------------------------------------
# Material Builder (Blender 4.2 Principled BSDF -> Native glTF 2.0 PBR)
# ---------------------------------------------------------------------------

def _load_tex(prefix: str, suffix: str, colorspace: str) -> Optional[bpy.types.Image]:
    path = TEX_CACHE_DIR / f"{prefix}_{suffix}.png"
    if not path.exists():
        return None
    img = bpy.data.images.load(str(path), check_existing=True)
    img.colorspace_settings.name = colorspace
    return img


def make_d4_pbr_material(
    name: str,
    tex_prefix: Optional[str],
    base_tint: Tuple[float, float, float],
    metallic_val: float = 0.0,
    roughness_val: float = 0.5,
    normal_strength: float = 1.0,
    coat_weight: float = 0.0,
    sheen_weight: float = 0.0,
    emission_color: Optional[Tuple[float, float, float]] = None,
    emission_strength: float = 0.0,
    emissive_from_tex: bool = False,
) -> bpy.types.Material:
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()

    out = nt.nodes.new("ShaderNodeOutputMaterial")
    out.location = (520, 0)
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.location = (200, 0)
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    if "Coat Weight" in bsdf.inputs:
        bsdf.inputs["Coat Weight"].default_value = coat_weight
    if "Sheen Weight" in bsdf.inputs:
        bsdf.inputs["Sheen Weight"].default_value = sheen_weight

    if tex_prefix:
        alb_img = _load_tex(tex_prefix, "albedo", "sRGB")
        if alb_img:
            t_alb = nt.nodes.new("ShaderNodeTexImage")
            t_alb.image = alb_img
            t_alb.location = (-320, 260)
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type = "RGBA"
            mix.blend_type = "MULTIPLY"
            mix.inputs["Factor"].default_value = 1.0
            mix.inputs["B"].default_value = (*base_tint, 1.0)
            mix.location = (-60, 260)
            nt.links.new(t_alb.outputs["Color"], mix.inputs["A"])
            nt.links.new(mix.outputs["Result"], bsdf.inputs["Base Color"])
        else:
            bsdf.inputs["Base Color"].default_value = (*base_tint, 1.0)

        orm_img = _load_tex(tex_prefix, "orm", "Non-Color")
        if orm_img:
            t_orm = nt.nodes.new("ShaderNodeTexImage")
            t_orm.image = orm_img
            t_orm.location = (-320, 0)
            sep = nt.nodes.new("ShaderNodeSeparateColor")
            sep.location = (-60, 0)
            nt.links.new(t_orm.outputs["Color"], sep.inputs["Color"])
            nt.links.new(sep.outputs["Green"], bsdf.inputs["Roughness"])
            nt.links.new(sep.outputs["Blue"], bsdf.inputs["Metallic"])
        else:
            bsdf.inputs["Roughness"].default_value = roughness_val
            bsdf.inputs["Metallic"].default_value = metallic_val

        nrm_img = _load_tex(tex_prefix, "normal", "Non-Color")
        if nrm_img:
            t_nrm = nt.nodes.new("ShaderNodeTexImage")
            t_nrm.image = nrm_img
            t_nrm.location = (-320, -260)
            nmap = nt.nodes.new("ShaderNodeNormalMap")
            nmap.space = "TANGENT"
            nmap.inputs["Strength"].default_value = normal_strength
            nmap.location = (-60, -260)
            nt.links.new(t_nrm.outputs["Color"], nmap.inputs["Color"])
            nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])

        if emissive_from_tex and emission_color and emission_strength > 0.0:
            emi_img = _load_tex(tex_prefix, "emissive", "sRGB")
            if emi_img:
                t_emi = nt.nodes.new("ShaderNodeTexImage")
                t_emi.image = emi_img
                t_emi.location = (-320, -520)
                mix_e = nt.nodes.new("ShaderNodeMix")
                mix_e.data_type = "RGBA"
                mix_e.blend_type = "MULTIPLY"
                mix_e.inputs["Factor"].default_value = 1.0
                mix_e.inputs["B"].default_value = (*emission_color, 1.0)
                mix_e.location = (-60, -520)
                nt.links.new(t_emi.outputs["Color"], mix_e.inputs["A"])
                nt.links.new(mix_e.outputs["Result"], bsdf.inputs["Emission Color"])
                bsdf.inputs["Emission Strength"].default_value = emission_strength
    else:
        bsdf.inputs["Base Color"].default_value = (*base_tint, 1.0)
        bsdf.inputs["Metallic"].default_value = metallic_val
        bsdf.inputs["Roughness"].default_value = roughness_val
        if emission_color and emission_strength > 0.0:
            bsdf.inputs["Emission Color"].default_value = (*emission_color, 1.0)
            bsdf.inputs["Emission Strength"].default_value = emission_strength

    return mat


# ---------------------------------------------------------------------------
# High-Craft BMesh Accumulator & Anatomical Sculpting Primitives
# ---------------------------------------------------------------------------

class CharacterMeshAccumulator:
    """Collects sculpted BMesh sub-parts with exact material slots, UVs, and skinning tags."""

    def __init__(self, materials: List[bpy.types.Material]):
        self.materials = materials
        self.verts: List[Vector] = []
        self.faces: List[Tuple[int, ...]] = []
        self.face_mats: List[int] = []
        self.face_smooth: List[bool] = []
        self.loop_uvs: List[Tuple[float, float]] = []
        self.vert_tags: List[str] = []

    def add_bmesh(
        self,
        bm: bmesh.types.BMesh,
        mat_idx: int,
        tag: str,
        uv_scale: float = 1.5,
        split_sharp_deg: Optional[float] = 44.0,
        smooth: bool = True,
    ) -> None:
        tiny = [f for f in bm.faces if f.calc_area() < 1e-10]
        if tiny:
            bmesh.ops.delete(bm, geom=tiny, context="FACES")

        bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
        if split_sharp_deg is not None and split_sharp_deg > 0.0:
            sharp_edges = [
                e for e in bm.edges
                if len(e.link_faces) == 2 and e.calc_face_angle(0.0) > math.radians(split_sharp_deg)
            ]
            if sharp_edges:
                bmesh.ops.split_edges(bm, edges=sharp_edges)

        uv_layer = bm.loops.layers.uv.verify()
        bm.normal_update()
        for face in bm.faces:
            n = face.normal
            ax, ay, az = abs(n.x), abs(n.y), abs(n.z)
            for loop in face.loops:
                if loop[uv_layer].uv.length_squared < 1e-10:
                    co = loop.vert.co
                    if az >= ax and az >= ay:
                        loop[uv_layer].uv = (co.x * uv_scale, co.y * uv_scale)
                    elif ax >= ay and ax >= az:
                        loop[uv_layer].uv = (co.y * uv_scale, co.z * uv_scale)
                    else:
                        loop[uv_layer].uv = (co.x * uv_scale, co.z * uv_scale)

        v_offset = len(self.verts)
        bm.verts.ensure_lookup_table()
        for v in bm.verts:
            self.verts.append(v.co.copy())
            self.vert_tags.append(tag)

        for f in bm.faces:
            self.faces.append(tuple(v_offset + l.vert.index for l in f.loops))
            self.face_mats.append(mat_idx)
            self.face_smooth.append(smooth)
            for l in f.loops:
                uv = l[uv_layer].uv
                self.loop_uvs.append((uv.x, uv.y))

        bm.free()

    def build_object(self, name: str) -> Tuple[bpy.types.Object, List[str]]:
        mesh = bpy.data.meshes.new(name)
        mesh.from_pydata([tuple(v) for v in self.verts], [], self.faces)
        for mat in self.materials:
            mesh.materials.append(mat)

        uv_layer = mesh.uv_layers.new(name="UVMap")
        for poly_idx, poly in enumerate(mesh.polygons):
            poly.material_index = self.face_mats[poly_idx]
            poly.use_smooth = self.face_smooth[poly_idx]
        for loop_idx, uv in enumerate(self.loop_uvs):
            uv_layer.data[loop_idx].uv = uv

        mesh.validate(clean_customdata=False)
        mesh.update()
        obj = bpy.data.objects.new(name, mesh)
        bpy.context.scene.collection.objects.link(obj)
        return obj, self.vert_tags


def bm_merge(bms: List[bmesh.types.BMesh]) -> bmesh.types.BMesh:
    out = bmesh.new()
    for sub in bms:
        m = bpy.data.meshes.new("_tmp_m")
        sub.to_mesh(m)
        sub.free()
        out.from_mesh(m)
        bpy.data.meshes.remove(m)
    return out


def bm_sculpt_loft(
    sections: List[Tuple[Vector, float, float]],
    segments: int = 20,
    fold_amp: float = 0.0,
    fold_freq: int = 8,
    profile_fn: Optional[Callable[[float, float], Tuple[float, float]]] = None,
    cap_start: bool = True,
    cap_end: bool = True,
    assign_uvs: bool = True,
    uv_tile_u: float = 1.0,
    uv_tile_v: float = 1.0,
) -> bmesh.types.BMesh:
    """Loft elliptical / custom cross-sections with Parallel Transport frames & clean UVs."""
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()
    rings: List[List[bmesh.types.BMVert]] = []
    n_sec = len(sections)

    fwd0 = (sections[1][0] - sections[0][0]) if n_sec > 1 else Vector((0, 0, 1))
    if fwd0.length < 1e-6:
        fwd0 = Vector((0, 0, 1))
    fwd0.normalize()
    x_init = Vector((1, 0, 0))
    if abs(fwd0.dot(x_init)) > 0.92:
        x_init = Vector((0, 1, 0))
    side_x = (x_init - fwd0 * fwd0.dot(x_init)).normalized()
    prev_fwd = fwd0

    for si, (center, rx, ry) in enumerate(sections):
        if si == 0:
            fwd = fwd0
        elif si < n_sec - 1:
            fwd = sections[si + 1][0] - sections[si - 1][0]
            fwd = prev_fwd if fwd.length < 1e-6 else fwd.normalized()
        else:
            fwd = center - sections[si - 1][0]
            fwd = prev_fwd if fwd.length < 1e-6 else fwd.normalized()

        rot_q = prev_fwd.rotation_difference(fwd)
        side_x = (rot_q @ side_x).normalized()
        side_x = (side_x - fwd * fwd.dot(side_x)).normalized()
        side_y = fwd.cross(side_x).normalized()
        prev_fwd = fwd

        ring: List[bmesh.types.BMVert] = []
        t_along = si / max(n_sec - 1, 1)
        for i in range(segments):
            ang = 2.0 * math.pi * i / segments
            f_mod = 1.0 + fold_amp * t_along * math.cos(fold_freq * ang)
            mx, my = profile_fn(t_along, ang) if profile_fn else (1.0, 1.0)
            px = math.cos(ang) * max(rx, 0.0015) * f_mod * mx
            py = math.sin(ang) * max(ry, 0.0015) * f_mod * my
            pos = center + side_x * px + side_y * py
            ring.append(bm.verts.new(pos))
        rings.append(ring)

    for si in range(n_sec - 1):
        r0, r1 = rings[si], rings[si + 1]
        v0 = (si / max(n_sec - 1, 1)) * uv_tile_v
        v1 = ((si + 1) / max(n_sec - 1, 1)) * uv_tile_v
        for i in range(segments):
            j = (i + 1) % segments
            f = bm.faces.new((r0[i], r0[j], r1[j], r1[i]))
            if assign_uvs:
                u0 = (i / segments) * uv_tile_u
                u1 = ((i + 1) / segments) * uv_tile_u
                for lp, uv in zip(f.loops, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]):
                    lp[uv_layer].uv = uv

    if cap_start:
        bm.faces.new(list(reversed(rings[0])))
    if cap_end:
        bm.faces.new(rings[-1])
    return bm


def bm_curved_plate(
    center: Vector,
    rx: float,
    ry: float,
    height: float,
    arc_deg: float,
    thickness: float,
    center_yaw_deg: float = 0.0,
    tilt_x_deg: float = 0.0,
    tilt_y_deg: float = 0.0,
    taper_top: float = 0.92,
    taper_bot: float = 0.88,
    ridge_amp: float = 0.14,
    segs_u: int = 12,
    segs_v: int = 7,
) -> bmesh.types.BMesh:
    """Sculpted thick beveled curved armor plate with explicit Z-yaw THEN local tilt."""
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()
    half_a = math.radians(arc_deg * 0.5)
    outer_grid, inner_grid = [], []

    for r in range(segs_v):
        tv = r / (segs_v - 1)
        z_local = (tv - 0.5) * height
        w_taper = taper_bot * (1.0 - tv) + taper_top * tv
        edge_bevel = math.sin(tv * math.pi) ** 0.35
        row_o, row_i = [], []
        for c in range(segs_u):
            tu = c / (segs_u - 1) - 0.5
            ang = -math.pi * 0.5 + tu * 2.0 * half_a  # centered at -Y (front)
            u_env = math.cos(tu * math.pi) ** 0.35
            ridge = ridge_amp * max(0.0, 1.0 - abs(tu) * 3.6) * edge_bevel
            rad_x = rx * w_taper * (1.0 + ridge * 0.35)
            rad_y = ry * w_taper * (1.0 + ridge)
            th_eff = max(0.002, thickness * (0.45 + 0.55 * u_env * edge_bevel))
            ox = rad_x * math.cos(ang)
            oy = rad_y * math.sin(ang)
            ix = max(0.002, rad_x - th_eff) * math.cos(ang)
            iy = max(0.002, rad_y - th_eff) * math.sin(ang)
            row_o.append(bm.verts.new((ox, oy, z_local)))
            row_i.append(bm.verts.new((ix, iy, z_local)))
        outer_grid.append(row_o)
        inner_grid.append(row_i)

    def _set_quad_uv(f, u0, u1, v0, v1):
        for lp, uv in zip(f.loops, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]):
            lp[uv_layer].uv = uv

    for r in range(segs_v - 1):
        v0, v1 = r / (segs_v - 1), (r + 1) / (segs_v - 1)
        for c in range(segs_u - 1):
            u0, u1 = c / (segs_u - 1), (c + 1) / (segs_u - 1)
            f1 = bm.faces.new((outer_grid[r][c], outer_grid[r][c + 1], outer_grid[r + 1][c + 1], outer_grid[r + 1][c]))
            f2 = bm.faces.new((inner_grid[r + 1][c], inner_grid[r + 1][c + 1], inner_grid[r][c + 1], inner_grid[r][c]))
            _set_quad_uv(f1, u0, u1, v0, v1)
            _set_quad_uv(f2, u0, u1, v0, v1)
        bm.faces.new((inner_grid[r][0], outer_grid[r][0], outer_grid[r + 1][0], inner_grid[r + 1][0]))
        bm.faces.new((outer_grid[r][-1], inner_grid[r][-1], inner_grid[r + 1][-1], outer_grid[r + 1][-1]))

    for c in range(segs_u - 1):
        bm.faces.new((inner_grid[0][c], inner_grid[0][c + 1], outer_grid[0][c + 1], outer_grid[0][c]))
        bm.faces.new((outer_grid[-1][c], outer_grid[-1][c + 1], inner_grid[-1][c + 1], inner_grid[-1][c]))

    rot_m = Euler((math.radians(tilt_x_deg), math.radians(tilt_y_deg), math.radians(center_yaw_deg)), "ZYX").to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=Matrix.Translation(center) @ rot_m, verts=bm.verts)
    return bm


def bm_icosphere(
    center: Tuple[float, float, float],
    radii: Tuple[float, float, float],
    subdiv: int = 2,
    rot_euler: Tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bmesh.types.BMesh:
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    bmesh.ops.scale(bm, vec=radii, verts=bm.verts)
    xform = Matrix.Translation(center) @ Euler(rot_euler, "XYZ").to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=xform, verts=bm.verts)
    return bm


def bm_bevel_diamond(
    center: Tuple[float, float, float],
    size: Tuple[float, float, float],
    bevel: float = 0.008,
    rot_euler: Tuple[float, float, float] = (0.0, 0.0, 0.0),
) -> bmesh.types.BMesh:
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    bmesh.ops.scale(bm, vec=size, verts=bm.verts)
    if bevel > 0.0:
        bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(bevel, min(size) * 0.35), segments=2, affect="EDGES")
    xform = Matrix.Translation(center) @ Euler(rot_euler, "XYZ").to_matrix().to_4x4()
    bmesh.ops.transform(bm, matrix=xform, verts=bm.verts)
    return bm


def bm_horn_arc(base: Vector, pts: List[Tuple[Vector, float]], segments: int = 12) -> bmesh.types.BMesh:
    sections = [(base + p, r, r * 0.86) for p, r in pts]
    return bm_sculpt_loft(sections, segments=segments, cap_start=True, cap_end=True)


def bm_rivet_row(points: List[Vector], radius: float) -> bmesh.types.BMesh:
    return bm_merge([bm_icosphere(tuple(p), (radius, radius, radius), subdiv=1) for p in points])


def bm_shoulder_dome_pauldron(
    shoulder_pos: Vector,
    side: float,
    rx: float,
    ry: float,
    rz: float,
    tiers: int = 3,
    spiked: bool = False,
    wing_blades: bool = False,
) -> Tuple[bmesh.types.BMesh, bmesh.types.BMesh]:
    """Build multi-tiered gothic pauldrons arching cleanly OVER top & outer deltoid."""
    plates, trims = [], []
    segs_u, segs_v = 11, 6
    th = max(0.004, 0.11 * rx)

    for ti in range(tiers):
        frac = ti / max(tiers - 1, 1)
        a_start = 0.02 + frac * 0.44
        a_end = 0.68 + frac * 0.72
        sc_rad = 1.06 - frac * 0.08
        bm = bmesh.new()
        uv_layer = bm.loops.layers.uv.verify()
        o_grid, i_grid = [], []
        for r in range(segs_v):
            tv = r / (segs_v - 1)
            va = a_start + (a_end - a_start) * tv  # 0=top(+Z), pi/2=outer(±X)
            edge_env = math.sin(tv * math.pi) ** 0.32
            ro, ri = [], []
            for c in range(segs_u):
                tu = c / (segs_u - 1) - 0.5
                ua = tu * 2.15  # front (-Y) to back (+Y)
                flute = 1.0 + 0.14 * max(0.0, 1.0 - abs(tu) * 3.4) * edge_env
                sx = math.sin(va) * math.cos(ua * 0.45)
                sy = math.sin(ua) * (0.45 + 0.55 * math.sin(va))
                sz = math.cos(va)
                px = side * sx * rx * sc_rad * flute
                py = sy * ry * sc_rad * flute
                pz = sz * rz * sc_rad * flute - frac * 0.22 * rz
                th_eff = th * (0.45 + 0.55 * edge_env)
                ro.append(bm.verts.new(shoulder_pos + Vector((px, py, pz))))
                ri.append(bm.verts.new(shoulder_pos + Vector((px - side * th_eff * sx, py - th_eff * sy * 0.6, pz - th_eff * sz))))
            o_grid.append(ro)
            i_grid.append(ri)

        for r in range(segs_v - 1):
            v0, v1 = r / (segs_v - 1), (r + 1) / (segs_v - 1)
            for c in range(segs_u - 1):
                u0, u1 = c / (segs_u - 1), (c + 1) / (segs_u - 1)
                f1 = bm.faces.new((o_grid[r][c], o_grid[r][c + 1], o_grid[r + 1][c + 1], o_grid[r + 1][c]))
                f2 = bm.faces.new((i_grid[r + 1][c], i_grid[r + 1][c + 1], i_grid[r][c + 1], i_grid[r][c]))
                for f in (f1, f2):
                    for lp, uv in zip(f.loops, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]):
                        lp[uv_layer].uv = uv
            bm.faces.new((i_grid[r][0], o_grid[r][0], o_grid[r + 1][0], i_grid[r + 1][0]))
            bm.faces.new((o_grid[r][-1], i_grid[r][-1], i_grid[r + 1][-1], o_grid[r + 1][-1]))
        for c in range(segs_u - 1):
            bm.faces.new((i_grid[0][c], i_grid[0][c + 1], o_grid[0][c + 1], o_grid[0][c]))
            bm.faces.new((o_grid[-1][c], o_grid[-1][c + 1], i_grid[-1][c + 1], i_grid[-1][c]))
        plates.append(bm)

        # Brass/bone rivets hugging the lower outer crest of each lame
        va_riv = a_end - 0.10
        riv_pts = []
        for tu in (-0.32, 0.0, 0.32):
            ua = tu * 2.15
            sx = math.sin(va_riv) * math.cos(ua * 0.45)
            sy = math.sin(ua) * (0.45 + 0.55 * math.sin(va_riv))
            sz = math.cos(va_riv)
            riv_pts.append(shoulder_pos + Vector((
                side * sx * rx * (sc_rad + 0.06),
                sy * ry * (sc_rad + 0.06),
                sz * rz * (sc_rad + 0.06) - frac * 0.22 * rz,
            )))
        trims.append(bm_rivet_row(riv_pts, radius=0.075 * rx))

    if spiked:
        for ay, ax in ((0.35, 0.0), (0.78, 0.38), (0.78, -0.38)):
            sb = shoulder_pos + Vector((side * math.sin(ay) * rx * 0.85, ax * ry * 0.72, math.cos(ay) * rz * 0.90))
            st = sb + Vector((side * math.sin(ay) * rx * 1.10, ax * ry * 0.35, math.cos(ay) * rz * 1.15))
            sm = sb.lerp(st, 0.48) + Vector((0, 0, 0.06 * rz))
            plates.append(bm_sculpt_loft([(sb, 0.13 * rx, 0.13 * rx), (sm, 0.08 * rx, 0.08 * rx), (st, 0.002, 0.002)], segments=8))

    if wing_blades:
        # Sweeping arched dark-iron wing-blades rising majestically from Archon shoulder cap
        for bi, (out_m, up_m, back_m) in enumerate([(1.75, 2.15, 0.45), (2.25, 1.45, 0.75), (1.95, 0.70, 0.95)]):
            wb = shoulder_pos + Vector((side * rx * 0.55, back_m * ry * 0.35, rz * (0.75 - bi * 0.20)))
            wm = wb + Vector((side * rx * out_m * 0.58, back_m * ry * 0.55, rz * up_m * 0.58))
            wt = wb + Vector((side * rx * out_m, back_m * ry * 1.05, rz * up_m))
            plates.append(bm_sculpt_loft(
                [(wb, 0.14 * rx, 0.05 * rx), (wm, 0.18 * rx, 0.032 * rx), (wt, 0.003, 0.003)],
                segments=6,
                profile_fn=lambda t, a: (1.0 + 0.32 * abs(math.cos(a)), 0.75 + 0.25 * abs(math.sin(a))),
            ))

    return bm_merge(plates), bm_merge(trims)


def bm_anatomical_skull(center: Vector, head_r: float, jaw_open: float = 0.10) -> bmesh.types.BMesh:
    """Sculpted cranium, brow ridge, zygomatic cheekbones, nasal recess, and toothed jaw."""
    parts = []
    cran = bm_icosphere(
        tuple(center + Vector((0, 0.08 * head_r, 0.22 * head_r))),
        (head_r * 0.76, head_r * 0.94, head_r * 0.82),
        subdiv=2,
    )
    for v in cran.verts:
        if v.co.y < center.y - 0.35 * head_r and abs(v.co.z - (center.z + 0.14 * head_r)) < 0.28 * head_r:
            if 0.12 * head_r < abs(v.co.x - center.x) < 0.52 * head_r:
                v.co.y += 0.30 * head_r
        if v.co.y < center.y - 0.35 * head_r and abs(v.co.z - (center.z + 0.36 * head_r)) < 0.12 * head_r:
            v.co.y -= 0.15 * head_r
    parts.append(cran)

    for sgn in (-1.0, 1.0):
        parts.append(bm_icosphere(
            (center.x + sgn * head_r * 0.45, center.y - head_r * 0.56, center.z - head_r * 0.06),
            (head_r * 0.23, head_r * 0.33, head_r * 0.17),
            subdiv=1,
            rot_euler=(0.2, 0.0, sgn * -0.35),
        ))

    parts.append(bm_sculpt_loft([
        (center + Vector((0, -head_r * 0.42, -head_r * 0.04)), head_r * 0.44, head_r * 0.38),
        (center + Vector((0, -head_r * 0.50, -head_r * 0.30)), head_r * 0.36, head_r * 0.30),
    ], segments=12))

    jaw_drop = jaw_open * head_r
    jaw_pts = [
        (center + Vector((-head_r * 0.48, -head_r * 0.10, -head_r * 0.12)), head_r * 0.11, head_r * 0.15),
        (center + Vector((-head_r * 0.34, -head_r * 0.52, -head_r * 0.54 - jaw_drop)), head_r * 0.11, head_r * 0.14),
        (center + Vector((0.0, -head_r * 0.64, -head_r * 0.60 - jaw_drop)), head_r * 0.13, head_r * 0.16),
        (center + Vector((head_r * 0.34, -head_r * 0.52, -head_r * 0.54 - jaw_drop)), head_r * 0.11, head_r * 0.14),
        (center + Vector((head_r * 0.48, -head_r * 0.10, -head_r * 0.12)), head_r * 0.11, head_r * 0.15),
    ]
    parts.append(bm_sculpt_loft(jaw_pts, segments=8))
    return bm_merge(parts)


def bm_ribcage_and_spine(
    chest_h: Vector,
    chest_t: Vector,
    spine_h: Vector,
    hunch: float,
    W: float,
    H: float,
    pairs: int = 6,
    parted_center: bool = False,
) -> bmesh.types.BMesh:
    """6-pair sculptural anatomical ribcage spanning full thorax (solar plexus to clavicles)."""
    parts = []
    z_bot = spine_h.z + 0.025 * H
    z_top = chest_t.z - 0.022 * H
    for ri in range(pairs):
        t = ri / max(pairs - 1, 1)
        rz_back = z_bot + t * (z_top - z_bot)
        rz_front = rz_back - 0.036 * H
        p_mid = spine_h.lerp(chest_t, t)
        ry = p_mid.y
        rw = (0.072 + math.sin(t * math.pi * 0.85) * 0.044) * W
        rd = (0.056 + math.sin(t * math.pi * 0.80) * 0.026) * W
        if parted_center and 1 <= ri <= 4:
            for sgn in (-1.0, 1.0):
                pts = []
                for k in range(7):
                    u = k / 6.0  # 0 = back spine (+Y), 1 = parted opening (-Y)
                    ang = math.pi * (1.0 - u * 0.72)
                    px = sgn * rw * math.sin(ang)
                    py = ry - rd * math.cos(ang)
                    pz = rz_back * (1.0 - u) + rz_front * u - 0.010 * H * math.sin(u * math.pi)
                    pts.append((Vector((px, py, pz)), 0.0072 * H, 0.0054 * H))
                parts.append(bm_sculpt_loft(pts, segments=8))
        else:
            rib_pts = []
            for k in range(13):
                u = k / 12.0
                ang = -0.92 * math.pi + 1.84 * math.pi * u
                front_frac = 0.5 + 0.5 * math.cos(ang)
                pz = rz_back * (1.0 - front_frac) + rz_front * front_frac
                rib_pts.append((
                    Vector((rw * math.sin(ang), ry - rd * math.cos(ang), pz)),
                    0.0074 * H,
                    0.0054 * H,
                ))
            parts.append(bm_sculpt_loft(rib_pts, segments=8))

    if not parted_center:
        parts.append(bm_sculpt_loft([
            (Vector((0, spine_h.y - 0.058 * W, z_bot - 0.018 * H)), 0.015 * W, 0.010 * W),
            (Vector((0, chest_t.y - 0.064 * W, z_top - 0.025 * H)), 0.020 * W, 0.012 * W),
        ], segments=8))

    for vi in range(8):
        vt = vi / 7.0
        p_sp = spine_h.lerp(chest_t, vt)
        vz = p_sp.z
        vy = p_sp.y + 0.064 * W
        parts.append(
            bm_sculpt_loft([
                (Vector((0, vy, vz)), 0.010 * H, 0.010 * H),
                (Vector((0, vy + 0.038 * H, vz - 0.006 * H)), 0.002 * H, 0.002 * H),
            ], segments=6)
        )
    return bm_merge(parts)


def bm_flowing_cape(
    top_center: Vector,
    top_w: float,
    bot_w: float,
    length: float,
    back_sweep: float,
    thickness: float,
    rows: int = 18,
    cols: int = 17,
    tattered: bool = False,
) -> bmesh.types.BMesh:
    """High-res double-sided draped cape with S-folds, flared hem, and clean UVs."""
    bm = bmesh.new()
    uv_layer = bm.loops.layers.uv.verify()
    front_grid, back_grid = [], []

    for r in range(rows):
        tv = r / (rows - 1)
        z = top_center.z - tv * length
        y_base = top_center.y + tv * back_sweep + 0.026 * length * math.sin(tv * math.pi)
        w = top_w * (1.0 - tv) + bot_w * tv
        row_f, row_b = [], []
        for c in range(cols):
            tu = c / (cols - 1) - 0.5
            x = top_center.x + tu * w * 2.0
            fold = (0.010 + 0.030 * tv) * math.cos(tu * math.pi * 6.0) + 0.009 * tv * math.sin(tu * math.pi * 11.0)
            tat_z = 0.0
            if tattered and tv > 0.72:
                jag = abs(math.sin(tu * 19.0 + 0.4)) ** 1.4
                tat_z = -0.105 * length * ((tv - 0.72) / 0.28) * jag
            zf = z + tat_z
            row_f.append(bm.verts.new((x, y_base + fold, zf)))
            row_b.append(bm.verts.new((x, y_base + fold + thickness, zf)))
        front_grid.append(row_f)
        back_grid.append(row_b)

    def _uv_quad(f, u0, u1, v0, v1):
        for lp, uv in zip(f.loops, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]):
            lp[uv_layer].uv = uv

    for r in range(rows - 1):
        v0, v1 = r / (rows - 1), (r + 1) / (rows - 1)
        for c in range(cols - 1):
            u0, u1 = c / (cols - 1), (c + 1) / (cols - 1)
            f1 = bm.faces.new((front_grid[r][c], front_grid[r][c + 1], front_grid[r + 1][c + 1], front_grid[r + 1][c]))
            f2 = bm.faces.new((back_grid[r + 1][c], back_grid[r + 1][c + 1], back_grid[r][c + 1], back_grid[r][c]))
            _uv_quad(f1, u0, u1, v0, v1)
            _uv_quad(f2, u0, u1, v0, v1)
    for c in range(cols - 1):
        bm.faces.new((back_grid[0][c], back_grid[0][c + 1], front_grid[0][c + 1], front_grid[0][c]))
        bm.faces.new((front_grid[-1][c], front_grid[-1][c + 1], back_grid[-1][c + 1], back_grid[-1][c]))
    for r in range(rows - 1):
        bm.faces.new((back_grid[r][0], front_grid[r][0], front_grid[r + 1][0], back_grid[r + 1][0]))
        bm.faces.new((front_grid[r][-1], back_grid[r][-1], back_grid[r + 1][-1], front_grid[r + 1][-1]))
    return bm


def bm_split_pleated_coat(
    hips_h: Vector,
    W: float,
    H: float,
    coat_len: float,
    front_split_deg: float = 56.0,
) -> Tuple[bmesh.types.BMesh, bmesh.types.BMesh]:
    """Build split pleated battle coat (cloth main + delicate hem trim border)."""
    rows, cols = 10, 25
    start_a = -math.pi * 0.5 + math.radians(front_split_deg * 0.5)
    end_a = math.pi * 1.5 - math.radians(front_split_deg * 0.5)

    def _build_panel(z_top: float, z_bot: float, r_top_x: float, r_top_y: float, r_bot_x: float, r_bot_y: float, r_rows: int) -> bmesh.types.BMesh:
        bm = bmesh.new()
        uv_layer = bm.loops.layers.uv.verify()
        th = 0.008 * H
        o_grid, i_grid = [], []
        for r in range(r_rows):
            tv = r / (r_rows - 1)
            z = z_top * (1.0 - tv) + z_bot * tv
            rx = r_top_x * (1.0 - tv) + r_bot_x * tv
            ry = r_top_y * (1.0 - tv) + r_bot_y * tv
            ro, ri = [], []
            for c in range(cols):
                tu = c / (cols - 1)
                a = start_a + (end_a - start_a) * tu
                pleat = 1.0 + (0.04 + 0.07 * tv) * math.cos(tu * math.pi * 14.0)
                ox = rx * pleat * math.cos(a)
                oy = hips_h.y + ry * pleat * math.sin(a)
                ix = max(0.004, rx * pleat - th) * math.cos(a)
                iy = hips_h.y + max(0.004, ry * pleat - th) * math.sin(a)
                ro.append(bm.verts.new((ox, oy, z)))
                ri.append(bm.verts.new((ix, iy, z)))
            o_grid.append(ro)
            i_grid.append(ri)

        for r in range(r_rows - 1):
            v0, v1 = r / (r_rows - 1), (r + 1) / (r_rows - 1)
            for c in range(cols - 1):
                u0, u1 = c / (cols - 1), (c + 1) / (cols - 1)
                f1 = bm.faces.new((o_grid[r][c], o_grid[r][c + 1], o_grid[r + 1][c + 1], o_grid[r + 1][c]))
                f2 = bm.faces.new((i_grid[r + 1][c], i_grid[r + 1][c + 1], i_grid[r][c + 1], i_grid[r][c]))
                for f in (f1, f2):
                    for lp, uv in zip(f.loops, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]):
                        lp[uv_layer].uv = uv
            bm.faces.new((i_grid[r][0], o_grid[r][0], o_grid[r + 1][0], i_grid[r + 1][0]))
            bm.faces.new((o_grid[r][-1], i_grid[r][-1], i_grid[r + 1][-1], o_grid[r + 1][-1]))
        for c in range(cols - 1):
            bm.faces.new((i_grid[0][c], i_grid[0][c + 1], o_grid[0][c + 1], o_grid[0][c]))
            bm.faces.new((o_grid[-1][c], o_grid[-1][c + 1], i_grid[-1][c + 1], i_grid[-1][c]))
        return bm

    z0 = hips_h.z + 0.032 * H
    z_split = hips_h.z - coat_len * 0.90
    z_hem = hips_h.z - coat_len
    main_bm = _build_panel(z0, z_split, 0.112 * W, 0.084 * W, 0.212 * W, 0.168 * W, rows)
    trim_bm = _build_panel(z_split, z_hem, 0.212 * W, 0.168 * W, 0.222 * W, 0.176 * W, 4)
    return main_bm, trim_bm


def bm_archmage_cowl_and_crown(
    head_h: Vector,
    head_t: Vector,
    head_r: float,
    H: float,
) -> Tuple[bmesh.types.BMesh, bmesh.types.BMesh, bmesh.types.BMesh]:
    """Build pointed gothic Horadric cowl with drooping front brow peak + circlet crown."""
    hc = head_h.lerp(head_t, 0.5)
    hood_specs = [
        (hc.y + 0.015 * H, head_h.z - 0.028 * H, head_r * 1.48, head_r * 1.42, 0.34, 0.00),
        (hc.y + 0.004 * H, hc.z - 0.014 * H, head_r * 1.38, head_r * 1.38, 0.54, 0.00),
        (hc.y - 0.016 * H, hc.z + 0.028 * H, head_r * 1.28, head_r * 1.38, 0.62, -0.006 * H),
        (hc.y - 0.038 * H, head_t.z + 0.014 * H, head_r * 1.15, head_r * 1.36, 0.40, -0.018 * H),
        (hc.y + 0.042 * H, head_t.z + 0.040 * H, head_r * 0.62, head_r * 0.85, 0.08, 0.00),
        (hc.y + 0.125 * H, head_t.z + 0.020 * H, head_r * 0.24, head_r * 0.32, 0.02, 0.00),
        (hc.y + 0.195 * H, head_t.z - 0.022 * H, head_r * 0.05, head_r * 0.07, 0.00, 0.00),
    ]
    n_arc = 21
    th = 0.011 * H
    bm_hood = bmesh.new()
    uv_layer = bm_hood.loops.layers.uv.verify()
    o_rings, i_rings = [], []
    for cy, cz, rw, rd, open_a, peak_dz in hood_specs:
        ro, ri = [], []
        sa = -math.pi * 0.5 + open_a
        ea = math.pi * 1.5 - open_a
        for i in range(n_arc):
            tu = i / (n_arc - 1)
            a = sa + (ea - sa) * tu
            # Pointed gothic brow dip toward front edges
            brow_dip = peak_dz * (abs(tu - 0.5) * 2.0) ** 2
            ox = rw * math.cos(a)
            oy = cy + rd * math.sin(a)
            ix = max(0.002, rw - th) * math.cos(a)
            iy = cy + max(0.002, rd - th) * math.sin(a)
            ro.append(bm_hood.verts.new((ox, oy, cz + brow_dip)))
            ri.append(bm_hood.verts.new((ix, iy, cz + brow_dip)))
        o_rings.append(ro)
        i_rings.append(ri)

    for r in range(len(o_rings) - 1):
        v0, v1 = r / (len(o_rings) - 1), (r + 1) / (len(o_rings) - 1)
        for c in range(n_arc - 1):
            u0, u1 = c / (n_arc - 1), (c + 1) / (n_arc - 1)
            f1 = bm_hood.faces.new((o_rings[r][c], o_rings[r][c + 1], o_rings[r + 1][c + 1], o_rings[r + 1][c]))
            f2 = bm_hood.faces.new((i_rings[r + 1][c], i_rings[r + 1][c + 1], i_rings[r][c + 1], i_rings[r][c]))
            for f in (f1, f2):
                for lp, uv in zip(f.loops, [(u0, v0), (u1, v0), (u1, v1), (u0, v1)]):
                    lp[uv_layer].uv = uv
        bm_hood.faces.new((i_rings[r][0], o_rings[r][0], o_rings[r + 1][0], i_rings[r + 1][0]))
        bm_hood.faces.new((o_rings[r][-1], i_rings[r][-1], i_rings[r + 1][-1], o_rings[r + 1][-1]))
    for c in range(n_arc - 1):
        bm_hood.faces.new((i_rings[0][c], i_rings[0][c + 1], o_rings[0][c + 1], o_rings[0][c]))
        bm_hood.faces.new((o_rings[-1][c], o_rings[-1][c + 1], i_rings[-1][c + 1], i_rings[-1][c]))

    crown_z = hc.z + head_r * 0.44
    circlet = bm_curved_plate(
        center=Vector((0, hc.y - head_r * 0.06, crown_z)),
        rx=head_r * 1.15, ry=head_r * 1.22, height=0.022 * H,
        arc_deg=200.0, thickness=0.007 * H, taper_top=1.05, taper_bot=0.95, ridge_amp=0.14, segs_u=16, segs_v=5,
    )
    crest = bm_sculpt_loft([
        (Vector((0, hc.y - head_r * 1.24, crown_z - 0.005 * H)), 0.014 * H, 0.006 * H),
        (Vector((0, hc.y - head_r * 1.27, crown_z + 0.014 * H)), 0.017 * H, 0.007 * H),
        (Vector((0, hc.y - head_r * 1.21, crown_z + 0.044 * H)), 0.0025 * H, 0.0025 * H),
    ], segments=8)
    gem = bm_icosphere((0, hc.y - head_r * 1.28, crown_z + 0.010 * H), (0.008 * H, 0.006 * H, 0.012 * H), subdiv=1)
    return bm_hood, bm_merge([circlet, crest]), gem


# ---------------------------------------------------------------------------
# Uniform 24-Bone Skeleton & Top-4 Deterministic Capsule Skinning
# ---------------------------------------------------------------------------

def build_armature_and_spec(spec: dict) -> Tuple[bpy.types.Object, Dict[str, Tuple[Vector, Vector]]]:
    H = spec["height"]
    W = spec["width_scale"] * H
    hunch = spec.get("hunch_y", 0.0) * H

    hip_z = 0.52 * H
    spine_z = 0.63 * H
    chest_z = 0.77 * H
    neck_z = 0.855 * H
    head_z = 0.905 * H

    hip_y = 0.01 * H
    chest_y = hunch * 0.62
    neck_y = hunch
    head_y = hunch * 1.15

    sh_x = 0.168 * W
    sh_z = 0.805 * H
    elb_x = 0.258 * W
    elb_y = chest_y + 0.02 * H
    elb_z = 0.65 * H
    wri_x = 0.328 * W
    wri_y = chest_y - 0.042 * H
    wri_z = 0.51 * H

    hip_w = 0.084 * W
    knee_z = 0.28 * H
    ank_z = 0.056 * H

    cape_y = neck_y + 0.076 * W
    bones_def: List[Tuple[str, Optional[str], Vector, Vector, bool]] = [
        ("Root", None, Vector((0, 0, 0)), Vector((0, 0, 0.16 * H)), False),
        ("Hips", "Root", Vector((0, hip_y, hip_z)), Vector((0, hip_y, spine_z)), False),
        ("Spine", "Hips", Vector((0, hip_y, spine_z)), Vector((0, chest_y, chest_z)), True),
        ("Chest", "Spine", Vector((0, chest_y, chest_z)), Vector((0, neck_y, neck_z)), True),
        ("Neck", "Chest", Vector((0, neck_y, neck_z)), Vector((0, head_y, head_z)), True),
        ("Head", "Neck", Vector((0, head_y, head_z)), Vector((0, head_y, H * 0.985)), True),

        ("Shoulder_L", "Chest", Vector((0.042 * W, chest_y, sh_z - 0.01 * H)), Vector((sh_x, chest_y, sh_z)), False),
        ("UpperArm_L", "Shoulder_L", Vector((sh_x, chest_y, sh_z)), Vector((elb_x, elb_y, elb_z)), True),
        ("Forearm_L", "UpperArm_L", Vector((elb_x, elb_y, elb_z)), Vector((wri_x, wri_y, wri_z)), True),
        ("Hand_L", "Forearm_L", Vector((wri_x, wri_y, wri_z)), Vector((wri_x + 0.03 * W, wri_y - 0.02 * H, wri_z - 0.055 * H)), True),

        ("Shoulder_R", "Chest", Vector((-0.042 * W, chest_y, sh_z - 0.01 * H)), Vector((-sh_x, chest_y, sh_z)), False),
        ("UpperArm_R", "Shoulder_R", Vector((-sh_x, chest_y, sh_z)), Vector((-elb_x, elb_y, elb_z)), True),
        ("Forearm_R", "UpperArm_R", Vector((-elb_x, elb_y, elb_z)), Vector((-wri_x, wri_y, wri_z)), True),
        ("Hand_R", "Forearm_R", Vector((-wri_x, wri_y, wri_z)), Vector((-wri_x - 0.03 * W, wri_y - 0.02 * H, wri_z - 0.055 * H)), True),

        ("Weapon_R", "Hand_R", Vector((-wri_x - 0.02 * W, wri_y - 0.025 * H, wri_z - 0.03 * H)),
         Vector((-wri_x - 0.02 * W, wri_y - 0.22 * H, wri_z + 0.12 * H)), False),

        ("Thigh_L", "Hips", Vector((hip_w, hip_y, hip_z)), Vector((hip_w * 1.04, 0.0, knee_z)), False),
        ("Calf_L", "Thigh_L", Vector((hip_w * 1.04, 0.0, knee_z)), Vector((hip_w * 1.04, 0.01 * H, ank_z)), True),
        ("Foot_L", "Calf_L", Vector((hip_w * 1.04, 0.01 * H, ank_z)), Vector((hip_w * 1.04, -0.095 * H, 0.014 * H)), True),

        ("Thigh_R", "Hips", Vector((-hip_w, hip_y, hip_z)), Vector((-hip_w * 1.04, 0.0, knee_z)), False),
        ("Calf_R", "Thigh_R", Vector((-hip_w * 1.04, 0.0, knee_z)), Vector((-hip_w * 1.04, 0.01 * H, ank_z)), True),
        ("Foot_R", "Calf_R", Vector((-hip_w * 1.04, 0.01 * H, ank_z)), Vector((-hip_w * 1.04, -0.095 * H, 0.014 * H)), True),

        ("Cape_1", "Chest", Vector((0, cape_y, sh_z)), Vector((0, cape_y + 0.035 * H, spine_z)), False),
        ("Cape_2", "Cape_1", Vector((0, cape_y + 0.035 * H, spine_z)), Vector((0, cape_y + 0.068 * H, knee_z + 0.08 * H)), True),
        ("Cape_3", "Cape_2", Vector((0, cape_y + 0.068 * H, knee_z + 0.08 * H)), Vector((0, cape_y + 0.098 * H, 0.09 * H)), True),
    ]

    arm_data = bpy.data.armatures.new("Armature")
    arm_obj = bpy.data.objects.new("Armature", arm_data)
    bpy.context.scene.collection.objects.link(arm_obj)
    bpy.context.view_layer.objects.active = arm_obj
    bpy.ops.object.mode_set(mode="EDIT")

    bone_segments: Dict[str, Tuple[Vector, Vector]] = {}
    eb_map: Dict[str, bpy.types.EditBone] = {}
    for bname, parent_name, head, tail, connect in bones_def:
        eb = arm_data.edit_bones.new(bname)
        eb.head = head
        eb.tail = tail
        eb.use_deform = True
        if parent_name:
            eb.parent = eb_map[parent_name]
            eb.use_connect = connect
        eb_map[bname] = eb
        bone_segments[bname] = (head, tail)

    bpy.ops.object.mode_set(mode="OBJECT")
    return arm_obj, bone_segments


def _point_to_segment_dist(p: Vector, a: Vector, b: Vector) -> float:
    ab = b - a
    l2 = ab.length_squared
    if l2 < 1e-10:
        return (p - a).length
    t = max(0.0, min(1.0, (p - a).dot(ab) / l2))
    return (p - (a + ab * t)).length


def bind_and_weight_mesh(
    mesh_obj: bpy.types.Object,
    arm_obj: bpy.types.Object,
    bone_segments: Dict[str, Tuple[Vector, Vector]],
    vert_tags: List[str],
    height: float,
) -> None:
    """Deterministic top-4 anatomical capsule & region skinning."""
    vgs = {bn: mesh_obj.vertex_groups.new(name=bn) for bn in bone_segments}
    hip_z = bone_segments["Hips"][0].z
    cape_top_z = bone_segments["Cape_1"][0].z
    cape_bot_z = bone_segments["Cape_3"][1].z

    def commit(v_idx: int, raw: Dict[str, float]) -> None:
        pos = [(bn, w) for bn, w in raw.items() if w > 0.0]
        if not pos:
            vgs["Hips"].add([v_idx], 1.0, "REPLACE")
            return
        top4 = sorted(pos, key=lambda x: x[1], reverse=True)[:4]
        tot = sum(w for _, w in top4)
        if tot <= 1e-12:
            vgs[top4[0][0]].add([v_idx], 1.0, "REPLACE")
            return
        for bn, w in top4:
            wn = w / tot
            if wn > 0.001:
                vgs[bn].add([v_idx], wn, "REPLACE")

    def seg_weights(v_idx: int, p: Vector, cands: List[str], sig: float) -> None:
        raw = {bn: math.exp(-(_point_to_segment_dist(p, *bone_segments[bn]) ** 2) / (2.0 * sig * sig)) + 1e-6 for bn in cands}
        commit(v_idx, raw)

    for v in mesh_obj.data.vertices:
        idx = v.index
        p = v.co
        tag = vert_tags[idx]

        if tag in ("Weapon_R", "Hand_L", "Hand_R", "Head", "Forearm_L"):
            vgs[tag].add([idx], 1.0, "REPLACE")
        elif tag == "ChestRigid":
            vgs["Chest"].add([idx], 1.0, "REPLACE")
        elif tag == "Pauldron_L":
            commit(idx, {"UpperArm_L": 0.65, "Shoulder_L": 0.25, "Chest": 0.10})
        elif tag == "Pauldron_R":
            commit(idx, {"UpperArm_R": 0.65, "Shoulder_R": 0.25, "Chest": 0.10})
        elif tag == "Cape":
            t = max(0.0, min(1.0, (cape_top_z - p.z) / max(cape_top_z - cape_bot_z, 1e-4)))
            if t < 0.14:
                wc = max(0.0, (0.14 - t) / 0.14) * 0.45
                commit(idx, {"Chest": wc, "Cape_1": 1.0 - wc})
            elif t < 0.48:
                u = (t - 0.14) / 0.34
                commit(idx, {"Cape_1": 1.0 - u, "Cape_2": u})
            else:
                u = (t - 0.48) / 0.52
                commit(idx, {"Cape_2": 1.0 - u, "Cape_3": u})
        elif tag == "Skirt":
            if p.z >= hip_z - 0.025 * height:
                vgs["Hips"].add([idx], 1.0, "REPLACE")
            else:
                blend_hip = max(0.0, min(1.0, (p.z - (hip_z - 0.18 * height)) / (0.155 * height)))
                side_blend = max(-1.0, min(1.0, p.x / (0.065 * height)))
                w_l = 0.5 + 0.5 * side_blend
                w_r = 1.0 - w_l
                knee_z = bone_segments["Calf_L"][0].z
                w_th = max(0.0, min(1.0, (p.z - (knee_z - 0.05 * height)) / (0.13 * height)))
                w_ca = 1.0 - w_th
                rem = 1.0 - blend_hip
                commit(idx, {
                    "Hips": blend_hip,
                    "Thigh_L": rem * w_l * w_th,
                    "Calf_L": rem * w_l * w_ca,
                    "Thigh_R": rem * w_r * w_th,
                    "Calf_R": rem * w_r * w_ca,
                })
        elif tag == "Arm_L":
            seg_weights(idx, p, ["Chest", "Shoulder_L", "UpperArm_L", "Forearm_L", "Hand_L"], sig=0.042 * height)
        elif tag == "Arm_R":
            seg_weights(idx, p, ["Chest", "Shoulder_R", "UpperArm_R", "Forearm_R", "Hand_R"], sig=0.042 * height)
        elif tag == "Leg_L":
            seg_weights(idx, p, ["Hips", "Thigh_L", "Calf_L", "Foot_L"], sig=0.044 * height)
        elif tag == "Leg_R":
            seg_weights(idx, p, ["Hips", "Thigh_R", "Calf_R", "Foot_R"], sig=0.044 * height)
        else:
            seg_weights(idx, p, ["Hips", "Spine", "Chest", "Neck", "Head"], sig=0.052 * height)

    mod = mesh_obj.modifiers.new("Armature", "ARMATURE")
    mod.object = arm_obj
    mesh_obj.parent = arm_obj


# ---------------------------------------------------------------------------
# Grimdark High-Detail Character Sculptor (5 Distinct Diablo IV Archetypes)
# ---------------------------------------------------------------------------

def build_character_geometry(spec: dict, bone_segments: Dict[str, Tuple[Vector, Vector]]) -> CharacterMeshAccumulator:
    H = spec["height"]
    W = spec["width_scale"] * H
    arch = spec["archetype"]
    hunch = spec.get("hunch_y", 0.0) * H

    # Materials:
    # 0 = Armor (steel / carapace), 1 = Cloth (woven wool), 2 = Accent (brass / bone),
    # 3 = Glow Core/Eyes (emissive), 4 = Weapon Steel, 5 = Rune Filigree (emissive etched plate)
    mats = [
        make_d4_pbr_material(f"{arch}_Armor", spec["armor_tex"], spec["armor_tint"], coat_weight=spec.get("coat", 0.1)),
        make_d4_pbr_material(f"{arch}_Cloth", "d4_woven_wool", spec["cloth_tint"], sheen_weight=0.22),
        make_d4_pbr_material(f"{arch}_Accent", spec["accent_tex"], spec["accent_tint"]),
        make_d4_pbr_material(f"{arch}_Glow", None, spec["glow_color"], roughness_val=0.12, emission_color=spec["glow_color"], emission_strength=spec["glow_strength"]),
        make_d4_pbr_material(f"{arch}_Weapon", "d4_hammered_steel", spec["weapon_tint"], coat_weight=0.15),
        make_d4_pbr_material(f"{arch}_RuneFiligree", "d4_rune_filigree", spec["rune_base_tint"], emission_color=spec["glow_color"], emission_strength=spec["rune_strength"], emissive_from_tex=True),
    ]
    acc = CharacterMeshAccumulator(mats)

    def bpos(name: str) -> Tuple[Vector, Vector]:
        return bone_segments[name]

    hips_h, _ = bpos("Hips")
    spine_h, spine_t = bpos("Spine")
    chest_h, chest_t = bpos("Chest")
    neck_h, neck_t = bpos("Neck")
    head_h, head_t = bpos("Head")

    # ---- 1. Sculpted Anatomical 7.8-Head Torso, Contoured Cuirass, Straps & Belt ----
    tb = 1.24 if arch in ("juggernaut", "archon") else (0.40 if arch in ("reaper", "lich") else 1.00)
    torso_secs = [
        (hips_h - Vector((0, 0, 0.045 * H)), 0.096 * W * tb, 0.070 * W * tb),
        (hips_h, 0.094 * W * tb, 0.068 * W * tb),
        (hips_h.lerp(spine_t, 0.40), 0.082 * W * tb, 0.060 * W * tb),
        (spine_t, 0.096 * W * tb, 0.068 * W * tb),
        (chest_h.lerp(chest_t, 0.25), 0.136 * W * tb, 0.086 * W * tb),
        (chest_h.lerp(chest_t, 0.52), 0.140 * W * tb, 0.088 * W * tb),
        (chest_h.lerp(chest_t, 0.78), 0.104 * W * tb, 0.068 * W * tb),
        (neck_h, 0.046 * W, 0.044 * W),
        (neck_t, 0.038 * W, 0.038 * W),
    ]
    torso_mat = 0 if arch in ("juggernaut", "archon") else 1
    acc.add_bmesh(bm_sculpt_loft(torso_secs, segments=24, uv_tile_u=2.0, uv_tile_v=2.0), torso_mat, "Torso", split_sharp_deg=None)

    chest_mid = chest_h.lerp(chest_t, 0.36)

    if arch == "archmage":
        # Tight anatomical lofted cuirass conforming smoothly along trapezius slope
        cuirass_secs = [
            (spine_t, 0.100 * W, 0.072 * W),
            (chest_h.lerp(chest_t, 0.25), 0.142 * W, 0.092 * W),
            (chest_h.lerp(chest_t, 0.55), 0.144 * W, 0.094 * W),
            (chest_h.lerp(chest_t, 0.80), 0.105 * W, 0.072 * W),
            (neck_h, 0.056 * W, 0.052 * W),
        ]
        acc.add_bmesh(
            bm_sculpt_loft(
                cuirass_secs, segments=22,
                profile_fn=lambda t, a: (1.0, 1.0 + 0.14 * max(0.0, -math.sin(a)) * math.sin(t * math.pi)),
            ),
            0, "Torso", split_sharp_deg=44.0,
        )
        # Central sculpted Horadric rune filigree chest crest
        chest_crest = bm_bevel_diamond(
            (0, chest_mid.y - 0.104 * W, chest_mid.z + 0.008 * H),
            (0.050 * W, 0.014 * W, 0.076 * H),
            bevel=0.005 * H,
            rot_euler=(-0.10, 0, 0),
        )
        acc.add_bmesh(chest_crest, 5, "Torso", split_sharp_deg=36.0)
        # Leather/brass cross-straps hugging chest contour
        for sgn in (-1.0, 1.0):
            strap = bm_sculpt_loft([
                (Vector((sgn * 0.088 * W, chest_t.y - 0.048 * W, chest_t.z - 0.014 * H)), 0.013 * W, 0.007 * W),
                (Vector((0.0, chest_mid.y - 0.102 * W, chest_mid.z - 0.014 * H)), 0.014 * W, 0.007 * W),
                (Vector((-sgn * 0.080 * W, spine_t.y - 0.070 * W, spine_t.z)), 0.013 * W, 0.007 * W),
            ], segments=8)
            acc.add_bmesh(strap, 2, "Torso")
        # Sleek sloped collar gorget hugging trapezius-to-neck transition
        gorget = bm_sculpt_loft([
            (chest_h.lerp(chest_t, 0.78), 0.108 * W, 0.076 * W),
            (neck_h + Vector((0, 0, 0.008 * H)), 0.054 * W, 0.050 * W),
        ], segments=18)
        acc.add_bmesh(gorget, 2, "Torso", split_sharp_deg=42.0)

    elif arch in ("reaper", "lich"):
        # 6-pair exposed anatomical ribcage spanning solar plexus to clavicles!
        ribs = bm_ribcage_and_spine(
            chest_h, chest_t, spine_h, hunch, W, H,
            pairs=6, parted_center=(arch == "lich"),
        )
        acc.add_bmesh(ribs, 2, "Torso", split_sharp_deg=48.0)
        if arch == "lich":
            # Blazing emerald/cyan soul-phylactery core centered inside parted ribcage!
            lich_core_p = spine_h.lerp(chest_t, 0.56)
            core_p = Vector((0, lich_core_p.y - 0.054 * W, lich_core_p.z))
            acc.add_bmesh(bm_icosphere(tuple(core_p), (0.028 * H, 0.022 * H, 0.040 * H), subdiv=2), 3, "ChestRigid")
            for sgn in (-1.0, 1.0):
                claw_b = bm_horn_arc(core_p + Vector((sgn * 0.032 * H, 0.006 * H, -0.028 * H)), [
                    (Vector((0, 0, 0)), 0.007 * H),
                    (Vector((sgn * 0.006 * H, -0.010 * H, 0.028 * H)), 0.006 * H),
                    (Vector((-sgn * 0.008 * H, -0.006 * H, 0.056 * H)), 0.002 * H),
                ], segments=6)
                acc.add_bmesh(claw_b, 0, "ChestRigid", split_sharp_deg=38.0)
            mantle = bm_curved_plate(
                center=chest_h.lerp(chest_t, 0.55) + Vector((0, 0.012 * H, 0.0)),
                rx=0.140 * W, ry=0.088 * W, height=0.072 * H,
                arc_deg=205.0, thickness=0.008 * H, center_yaw_deg=180.0, taper_top=0.60, taper_bot=1.10, segs_u=16, segs_v=6,
            )
            acc.add_bmesh(mantle, 1, "Torso", split_sharp_deg=None)

    else:  # juggernaut & archon contoured gothic breastplates (smooth trapezius shoulder slope!)
        cuirass_secs = [
            (spine_t, 0.102 * W * tb, 0.074 * W * tb),
            (chest_h.lerp(chest_t, 0.25), 0.144 * W * tb, 0.092 * W * tb),
            (chest_h.lerp(chest_t, 0.55), 0.148 * W * tb, 0.094 * W * tb),
            (chest_h.lerp(chest_t, 0.80), 0.106 * W * tb, 0.072 * W * tb),
            (neck_h, 0.056 * W, 0.052 * W),
        ]
        acc.add_bmesh(
            bm_sculpt_loft(
                cuirass_secs, segments=24,
                profile_fn=lambda t, a: (1.0, 1.0 + 0.16 * max(0.0, -math.sin(a)) * math.sin(t * math.pi)),
            ),
            0, "Torso", split_sharp_deg=44.0,
        )
        # Sleek contoured neck collar gorget hugging trapezius-to-neck slope (ZERO flat shelf!)
        gorget = bm_sculpt_loft([
            (chest_h.lerp(chest_t, 0.78), 0.112 * W * tb * 0.90, 0.076 * W * tb * 0.90),
            (neck_h + Vector((0, 0, 0.008 * H)), 0.056 * W, 0.052 * W),
        ], segments=20)
        acc.add_bmesh(gorget, 2 if arch == "archon" else 0, "Torso", split_sharp_deg=42.0)

        if arch == "archon":
            # Blazing Chrono-Rift Core protruding cleanly from sternum + gilded bone-gold pectoral ribs
            core_pos = Vector((0, chest_mid.y - 0.106 * W * tb, chest_mid.z + 0.006 * H))
            acc.add_bmesh(bm_icosphere(tuple(core_pos), (0.036 * H, 0.022 * H, 0.044 * H), subdiv=2), 3, "ChestRigid")
            bezel = bm_bevel_diamond(tuple(core_pos + Vector((0, 0.012 * H, 0))), (0.090 * H, 0.022 * H, 0.090 * H), bevel=0.010 * H, rot_euler=(0, math.pi * 0.25, 0))
            acc.add_bmesh(bezel, 5, "ChestRigid", split_sharp_deg=35.0)
            for sgn in (-1.0, 1.0):
                rib_arc = bm_sculpt_loft([
                    (Vector((sgn * 0.125 * W * tb, chest_t.y - 0.038 * W * tb, chest_t.z - 0.028 * H)), 0.015 * W, 0.009 * W),
                    (Vector((sgn * 0.055 * W * tb, chest_mid.y - 0.104 * W * tb, chest_mid.z + 0.006 * H)), 0.017 * W, 0.010 * W),
                    (Vector((sgn * 0.095 * W * tb, spine_t.y - 0.076 * W * tb, spine_t.z)), 0.013 * W, 0.009 * W),
                ], segments=8)
                acc.add_bmesh(rib_arc, 2, "Torso", split_sharp_deg=40.0)

    # Sculpted armored belt & waist medallion (proportional to waist width!)
    waist_tb = 0.72 if arch in ("reaper", "lich") else tb
    belt = bm_curved_plate(
        center=hips_h + Vector((0, 0, 0.032 * H)),
        rx=0.108 * W * waist_tb, ry=0.080 * W * waist_tb, height=0.044 * H,
        arc_deg=350.0, thickness=0.009 * H, taper_top=0.96, taper_bot=1.02, ridge_amp=0.10, segs_u=22, segs_v=5,
    )
    acc.add_bmesh(belt, 2 if arch in ("archmage", "archon") else 0, "Torso", split_sharp_deg=42.0)
    med_pos = Vector((0, hips_h.y - 0.086 * W * waist_tb, hips_h.z + 0.032 * H))
    acc.add_bmesh(
        bm_bevel_diamond(tuple(med_pos), (0.040 * W * waist_tb, 0.018 * W * waist_tb, 0.040 * W * waist_tb), bevel=0.007 * H, rot_euler=(0, math.pi * 0.25, 0)),
        5 if arch in ("archmage", "archon") else 2, "Torso", split_sharp_deg=36.0,
    )
    if arch in ("archmage", "archon"):
        acc.add_bmesh(bm_icosphere(tuple(med_pos + Vector((0, -0.009 * W * waist_tb, 0))), (0.013 * W * waist_tb, 0.008 * W * waist_tb, 0.015 * W * waist_tb), subdiv=1), 3, "Torso")

    # ---- 2. Split Pleated Battle Coat / Draped Robes / Pointed Gothic Iron Tassets ----
    if arch == "archmage":
        coat_main, coat_trim = bm_split_pleated_coat(hips_h, W, H, coat_len=0.38 * H, front_split_deg=58.0)
        acc.add_bmesh(coat_main, 1, "Skirt", split_sharp_deg=None)
        acc.add_bmesh(coat_trim, 5, "Skirt", split_sharp_deg=38.0)
    elif arch == "lich":
        skirt_secs = [
            (hips_h + Vector((0, 0, 0.035 * H)), 0.092 * W, 0.070 * W),
            (hips_h - Vector((0, 0, 0.15 * H)), 0.142 * W, 0.110 * W),
            (hips_h - Vector((0, 0, 0.31 * H)), 0.190 * W, 0.150 * W),
            (hips_h - Vector((0, 0, 0.44 * H)), 0.228 * W, 0.184 * W),
        ]
        acc.add_bmesh(bm_sculpt_loft(skirt_secs, segments=26, fold_amp=0.10, fold_freq=11, uv_tile_u=2.0, uv_tile_v=2.0), 1, "Skirt", split_sharp_deg=None)
    elif arch == "reaper":
        for yaw_d in (-130.0, -65.0, 65.0, 130.0, 180.0):
            flap = bm_curved_plate(
                center=hips_h + Vector((0, 0, -0.095 * H)),
                rx=0.088 * W, ry=0.072 * W, height=0.22 * H,
                arc_deg=44.0, thickness=0.006 * H, center_yaw_deg=yaw_d, taper_top=0.95, taper_bot=0.45, segs_u=8, segs_v=7,
            )
            acc.add_bmesh(flap, 1, "Skirt", split_sharp_deg=None)
    else:  # juggernaut & archon pointed beveled gothic tassets
        t_mat = 2 if arch == "archon" else 0
        for yaw_d in (-72.0, -28.0, 28.0, 72.0, 145.0, -145.0):
            tas = bm_curved_plate(
                center=hips_h + Vector((0, 0, -0.074 * H)),
                rx=0.124 * W * tb * 0.90, ry=0.094 * W * tb * 0.90, height=0.175 * H,
                arc_deg=40.0, thickness=0.010 * H, center_yaw_deg=yaw_d,
                taper_top=1.04, taper_bot=0.52, ridge_amp=0.24, segs_u=8, segs_v=6,
            )
            acc.add_bmesh(tas, t_mat if abs(yaw_d) < 90 else 0, "Skirt", split_sharp_deg=40.0)

    # ---- 3. Contoured Legs, Thigh Cuisses, Knee Poleyns, Greaves & Sabatons ----
    for side_tag, sgn in (("Leg_L", 1.0), ("Leg_R", -1.0)):
        th_h, th_t = bpos("Thigh_L" if sgn > 0 else "Thigh_R")
        ca_h, ca_t = bpos("Calf_L" if sgn > 0 else "Calf_R")
        ft_h, _ = bpos("Foot_L" if sgn > 0 else "Foot_R")

        leg_b = 1.24 if arch in ("juggernaut", "archon") else (0.64 if arch == "reaper" else 0.94)
        leg_secs = [
            (th_h + Vector((0, 0, 0.03 * H)), 0.068 * W * leg_b, 0.070 * W * leg_b),
            (th_h.lerp(th_t, 0.38), 0.058 * W * leg_b, 0.062 * W * leg_b),
            (th_h.lerp(th_t, 0.78), 0.046 * W * leg_b, 0.049 * W * leg_b),
            (th_t, 0.042 * W * leg_b, 0.045 * W * leg_b),
            (ca_h.lerp(ca_t, 0.32), 0.050 * W * leg_b, 0.055 * W * leg_b),
            (ca_h.lerp(ca_t, 0.72), 0.038 * W * leg_b, 0.040 * W * leg_b),
            (ca_t, 0.034 * W * leg_b, 0.036 * W * leg_b),
        ]
        leg_mat = 2 if arch == "reaper" else (1 if arch in ("archmage", "lich") else 0)
        acc.add_bmesh(bm_sculpt_loft(leg_secs, segments=16), leg_mat, side_tag, split_sharp_deg=None)

        if arch != "reaper":
            cuisse = bm_curved_plate(
                center=th_h.lerp(th_t, 0.52),
                rx=0.056 * W * leg_b, ry=0.062 * W * leg_b, height=0.145 * H,
                arc_deg=140.0, thickness=0.008 * H, center_yaw_deg=sgn * 10.0, taper_top=1.08, taper_bot=0.84, ridge_amp=0.16, segs_u=10, segs_v=5,
            )
            acc.add_bmesh(cuisse, 0, side_tag, split_sharp_deg=42.0)

        greave = bm_curved_plate(
            center=ca_h.lerp(ca_t, 0.48),
            rx=0.048 * W * leg_b, ry=0.054 * W * leg_b, height=0.175 * H,
            arc_deg=155.0, thickness=0.008 * H, taper_top=1.10, taper_bot=0.78, ridge_amp=0.20, segs_u=10, segs_v=6,
        )
        acc.add_bmesh(greave, 0, side_tag, split_sharp_deg=42.0)

        poleyn = bm_curved_plate(
            center=th_t + Vector((0, -0.006 * H, 0)),
            rx=0.048 * W * leg_b, ry=0.056 * W * leg_b, height=0.068 * H,
            arc_deg=150.0, thickness=0.009 * H, taper_top=0.92, taper_bot=0.88, ridge_amp=0.26, segs_u=10, segs_v=5,
        )
        acc.add_bmesh(poleyn, 2 if arch in ("archmage", "archon") else 0, side_tag, split_sharp_deg=40.0)

        foot_secs = [
            (Vector((ft_h.x, ft_h.y + 0.025 * H, 0.036 * H)), 0.039 * W * leg_b, 0.036 * H),
            (Vector((ft_h.x, ft_h.y - 0.020 * H, 0.030 * H)), 0.043 * W * leg_b, 0.030 * H),
            (Vector((ft_h.x, ft_h.y - 0.065 * H, 0.024 * H)), 0.041 * W * leg_b, 0.024 * H),
            (Vector((ft_h.x, ft_h.y - 0.105 * H, 0.017 * H)), 0.028 * W * leg_b, 0.017 * H),
            (Vector((ft_h.x, ft_h.y - 0.130 * H, 0.009 * H)), 0.012 * W * leg_b, 0.009 * H),
        ]
        acc.add_bmesh(bm_sculpt_loft(foot_secs, segments=14), 0, side_tag, split_sharp_deg=40.0)

    # ---- 4. Sculpted Arms, Vambraces, Gauntlets & Shoulder-Conforming Pauldrons ----
    for sgn in (1.0, -1.0):
        suf = "L" if sgn > 0 else "R"
        arm_tag = f"Arm_{suf}"
        ua_h, ua_t = bpos(f"UpperArm_{suf}")
        fa_h, fa_t = bpos(f"Forearm_{suf}")
        ha_h, ha_t = bpos(f"Hand_{suf}")

        arm_b = 1.26 if arch in ("juggernaut", "archon") else (0.62 if arch == "reaper" else 0.94)
        arm_secs = [
            (ua_h, 0.046 * W * arm_b, 0.046 * W * arm_b),
            (ua_h.lerp(ua_t, 0.35), 0.042 * W * arm_b, 0.042 * W * arm_b),
            (ua_h.lerp(ua_t, 0.72), 0.036 * W * arm_b, 0.036 * W * arm_b),
            (ua_t, 0.034 * W * arm_b, 0.034 * W * arm_b),
            (fa_h.lerp(fa_t, 0.38), 0.039 * W * arm_b, 0.039 * W * arm_b),
            (fa_t, 0.029 * W * arm_b, 0.029 * W * arm_b),
        ]
        arm_mat = 2 if arch == "reaper" else (0 if arch in ("juggernaut", "archon") else 1)
        acc.add_bmesh(bm_sculpt_loft(arm_secs, segments=14), arm_mat, arm_tag, split_sharp_deg=None)

        vamb = bm_sculpt_loft([
            (fa_h.lerp(fa_t, 0.18), 0.045 * W * arm_b, 0.045 * W * arm_b),
            (fa_h.lerp(fa_t, 0.55), 0.042 * W * arm_b, 0.042 * W * arm_b),
            (fa_h.lerp(fa_t, 0.94), 0.034 * W * arm_b, 0.034 * W * arm_b),
        ], segments=14, profile_fn=lambda t, a: (1.0 + 0.08 * abs(math.cos(a * 2)), 1.0))
        acc.add_bmesh(vamb, 2 if arch == "reaper" else 0, arm_tag, split_sharp_deg=42.0)

        if arch == "reaper":
            for bi in (0.35, 0.65):
                sp_b = fa_h.lerp(fa_t, bi) + Vector((sgn * 0.032 * W * arm_b, 0.015 * H, 0.01 * H))
                sp_t = sp_b + Vector((sgn * 0.035 * H, 0.025 * H, 0.035 * H))
                acc.add_bmesh(bm_sculpt_loft([(sp_b, 0.008 * H, 0.008 * H), (sp_t, 0.0015 * H, 0.0015 * H)], segments=6), 2, arm_tag)

        palm = bm_sculpt_loft([
            (ha_h, 0.028 * W * arm_b, 0.022 * W * arm_b),
            (ha_h.lerp(ha_t, 0.55), 0.033 * W * arm_b, 0.024 * W * arm_b),
            (ha_t, 0.026 * W * arm_b, 0.018 * W * arm_b),
        ], segments=10)
        acc.add_bmesh(palm, 0, f"Hand_{suf}", split_sharp_deg=40.0)
        knuckle_p = ha_h.lerp(ha_t, 0.58) + Vector((sgn * 0.014 * W * arm_b, -0.010 * H, 0))
        acc.add_bmesh(bm_bevel_diamond(tuple(knuckle_p), (0.018 * W * arm_b, 0.028 * W * arm_b, 0.020 * H), bevel=0.004 * H), 2 if arch in ("archmage", "archon") else 0, f"Hand_{suf}", split_sharp_deg=38.0)

        # Clavicle/deltoid shoulder bridge connecting torso to upper arm cleanly
        sh_bridge = bm_sculpt_loft([
            (chest_h.lerp(chest_t, 0.50), 0.036 * W, 0.036 * W),
            (ua_h, 0.044 * W * arm_b, 0.044 * W * arm_b),
        ], segments=10)
        acc.add_bmesh(sh_bridge, 2 if arch == "reaper" else (1 if arch == "lich" else 0), arm_tag, split_sharp_deg=None)

        # Shoulder-hugging multi-tiered pauldrons (enveloping deltoid cap heroically!)
        p_rx = 0.125 * W * arm_b
        p_ry = 0.108 * W * arm_b
        p_rz = 0.105 * H
        p_plates, p_trims = bm_shoulder_dome_pauldron(
            ua_h + Vector((-sgn * 0.024 * W, 0, -0.010 * H)),
            sgn, p_rx, p_ry, p_rz,
            tiers=3 if arch != "archon" else 4,
            spiked=(arch in ("juggernaut", "archon", "reaper")),
            wing_blades=(arch == "archon"),
        )
        acc.add_bmesh(p_plates, 0, f"Pauldron_{suf}", split_sharp_deg=42.0)
        acc.add_bmesh(p_trims, 2, f"Pauldron_{suf}", split_sharp_deg=None)

    if arch == "reaper":
        _, hl_t = bpos("Hand_L")
        for ci in (-1, 0, 1):
            c_base = hl_t + Vector((ci * 0.012 * H, -0.008 * H, 0))
            claw = bm_horn_arc(c_base, [
                (Vector((0, 0, 0)), 0.008 * H),
                (Vector((ci * 0.012 * H, -0.055 * H, -0.045 * H)), 0.005 * H),
                (Vector((ci * 0.016 * H, -0.115 * H, -0.105 * H)), 0.0012 * H),
            ], segments=6)
            acc.add_bmesh(claw, 4, "Hand_L", split_sharp_deg=38.0)
    elif arch == "juggernaut":
        # Spiked Gothic Shield-Bracer curving around outer Forearm_L (+X face)
        fa_l_h, fa_l_t = bpos("Forearm_L")
        sh_c = fa_l_h.lerp(fa_l_t, 0.56) + Vector((0.032 * W, 0, 0))
        shield_plate = bm_curved_plate(
            center=sh_c, rx=0.052 * H, ry=0.088 * H, height=0.22 * H,
            arc_deg=120.0, thickness=0.014 * H, center_yaw_deg=90.0, tilt_x_deg=-18.0,
            taper_top=0.84, taper_bot=0.72, ridge_amp=0.28, segs_u=10, segs_v=7,
        )
        acc.add_bmesh(shield_plate, 0, "Forearm_L", split_sharp_deg=38.0)
        boss_s = bm_sculpt_loft([
            (sh_c + Vector((0.045 * H, 0, 0)), 0.025 * H, 0.025 * H),
            (sh_c + Vector((0.105 * H, 0, 0)), 0.002 * H, 0.002 * H),
        ], segments=8)
        acc.add_bmesh(boss_s, 2, "Forearm_L", split_sharp_deg=35.0)

    # ---- 5. Sculpted Head, Hood, Crown, Skulls, Helms & Glowing Eyes ----
    hc = head_h.lerp(head_t, 0.5)
    head_r = 0.064 * H if arch != "archon" else 0.054 * H
    if arch == "archmage":
        acc.add_bmesh(bm_icosphere(tuple(hc), (head_r * 0.76, head_r * 0.86, head_r * 0.92), subdiv=2), 0, "Head")
        mask = bm_curved_plate(
            center=hc + Vector((0, -head_r * 0.14, -head_r * 0.34)),
            rx=head_r * 0.84, ry=head_r * 0.88, height=head_r * 0.85,
            arc_deg=180.0, thickness=0.006 * H, taper_top=1.05, taper_bot=0.78, ridge_amp=0.18, segs_u=12, segs_v=5,
        )
        acc.add_bmesh(mask, 1, "Head", split_sharp_deg=None)
        hood_bm, crown_bm, gem_bm = bm_archmage_cowl_and_crown(head_h, head_t, head_r, H)
        acc.add_bmesh(hood_bm, 1, "Head", split_sharp_deg=None)
        acc.add_bmesh(crown_bm, 2, "Head", split_sharp_deg=38.0)
        acc.add_bmesh(gem_bm, 3, "Head", split_sharp_deg=25.0)
    elif arch in ("reaper", "lich"):
        acc.add_bmesh(bm_anatomical_skull(hc, head_r, jaw_open=(0.14 if arch == "reaper" else 0.08)), 2, "Head", split_sharp_deg=44.0)
        if arch == "lich":
            band = bm_curved_plate(
                center=hc + Vector((0, 0.04 * head_r, head_r * 0.56)),
                rx=head_r * 0.88, ry=head_r * 1.02, height=0.026 * H,
                arc_deg=330.0, thickness=0.008 * H, ridge_amp=0.14, segs_u=18, segs_v=4,
            )
            acc.add_bmesh(band, 0, "Head", split_sharp_deg=38.0)
            spire_specs = [
                (-0.75, 0.082 * H, 0.010 * H),
                (-0.38, 0.105 * H, 0.011 * H),
                (0.0,   0.138 * H, 0.014 * H),
                (0.38,  0.105 * H, 0.011 * H),
                (0.75,  0.082 * H, 0.010 * H),
            ]
            for ang_s, sp_h, sp_r in spire_specs:
                sb = hc + Vector((math.sin(ang_s) * head_r * 0.82, -math.cos(ang_s) * head_r * 0.86, head_r * 0.64))
                sm = sb + Vector((math.sin(ang_s) * 0.014 * H, -0.008 * H, sp_h * 0.52))
                st = sb + Vector((math.sin(ang_s) * 0.024 * H, -0.012 * H, sp_h))
                acc.add_bmesh(bm_sculpt_loft([(sb, sp_r, sp_r), (sm, sp_r * 0.68, sp_r * 0.68), (st, 0.002, 0.002)], segments=6), 0, "Head", split_sharp_deg=35.0)
            acc.add_bmesh(bm_icosphere((0, hc.y - head_r * 0.96, hc.z + head_r * 0.72), (0.011 * H, 0.010 * H, 0.018 * H), subdiv=1), 3, "Head")
        else:
            # Reaper sinister swept-back curved demon skull horns (back +Y, outward ±X, curving forward-up)
            for sgn in (-1.0, 1.0):
                horn = bm_horn_arc(hc + Vector((sgn * head_r * 0.48, head_r * 0.15, head_r * 0.42)), [
                    (Vector((0, 0, 0)), 0.018 * H),
                    (Vector((sgn * 0.065 * H, 0.055 * H, 0.038 * H)), 0.014 * H),
                    (Vector((sgn * 0.105 * H, 0.125 * H, 0.072 * H)), 0.008 * H),
                    (Vector((sgn * 0.082 * H, 0.155 * H, 0.135 * H)), 0.002 * H),
                ], segments=10)
                acc.add_bmesh(horn, 2, "Head")
    elif arch == "juggernaut":
        helm = bm_sculpt_loft([
            (head_h - Vector((0, 0, 0.018 * H)), head_r * 0.96, head_r * 1.02),
            (hc, head_r * 1.00, head_r * 1.06),
            (hc + Vector((0, 0, head_r * 0.45)), head_r * 0.92, head_r * 0.98),
            (head_t + Vector((0, 0, 0.015 * H)), head_r * 0.42, head_r * 0.50),
        ], segments=18)
        acc.add_bmesh(helm, 0, "Head", split_sharp_deg=42.0)
        brow = bm_curved_plate(
            center=hc + Vector((0, -0.006 * H, head_r * 0.34)),
            rx=head_r * 1.06, ry=head_r * 1.12, height=head_r * 0.36,
            arc_deg=175.0, thickness=0.008 * H, ridge_amp=0.22, segs_u=12, segs_v=4,
        )
        chin = bm_curved_plate(
            center=hc + Vector((0, -0.006 * H, -head_r * 0.32)),
            rx=head_r * 1.04, ry=head_r * 1.10, height=head_r * 0.44,
            arc_deg=165.0, thickness=0.008 * H, taper_top=1.0, taper_bot=0.82, ridge_amp=0.24, segs_u=12, segs_v=5,
        )
        acc.add_bmesh(bm_merge([brow, chin]), 2, "Head", split_sharp_deg=38.0)
        for sgn in (-1.0, 1.0):
            horn = bm_horn_arc(hc + Vector((sgn * head_r * 0.80, 0, head_r * 0.32)), [
                (Vector((0, 0, 0)), 0.020 * H),
                (Vector((sgn * 0.055 * H, -0.025 * H, 0.040 * H)), 0.014 * H),
                (Vector((sgn * 0.078 * H, -0.065 * H, 0.095 * H)), 0.008 * H),
                (Vector((sgn * 0.062 * H, -0.088 * H, 0.135 * H)), 0.002 * H),
            ], segments=10)
            acc.add_bmesh(horn, 2, "Head")
    else:  # archon 4-horned gothic dreadlord helm
        helm = bm_sculpt_loft([
            (head_h - Vector((0, 0, 0.018 * H)), head_r * 0.98, head_r * 1.04),
            (hc, head_r * 1.02, head_r * 1.10),
            (head_t + Vector((0, 0, 0.018 * H)), head_r * 0.72, head_r * 0.82),
            (head_t + Vector((0, 0.012 * H, 0.048 * H)), head_r * 0.22, head_r * 0.28),
        ], segments=18)
        acc.add_bmesh(helm, 0, "Head", split_sharp_deg=40.0)
        crown_plate = bm_curved_plate(
            center=hc + Vector((0, -0.006 * H, head_r * 0.36)),
            rx=head_r * 1.12, ry=head_r * 1.18, height=head_r * 0.45,
            arc_deg=180.0, thickness=0.009 * H, ridge_amp=0.26, segs_u=14, segs_v=5,
        )
        acc.add_bmesh(crown_plate, 2, "Head", split_sharp_deg=38.0)
        for sgn in (-1.0, 1.0):
            big_horn = bm_horn_arc(hc + Vector((sgn * head_r * 0.72, 0, head_r * 0.52)), [
                (Vector((0, 0, 0)), 0.024 * H),
                (Vector((sgn * 0.070 * H, 0.020 * H, 0.050 * H)), 0.019 * H),
                (Vector((sgn * 0.125 * H, 0.010 * H, 0.125 * H)), 0.012 * H),
                (Vector((sgn * 0.088 * H, -0.048 * H, 0.195 * H)), 0.0025 * H),
            ], segments=12)
            acc.add_bmesh(big_horn, 2, "Head")
            cheek_horn = bm_horn_arc(hc + Vector((sgn * head_r * 0.80, 0, -head_r * 0.08)), [
                (Vector((0, 0, 0)), 0.013 * H),
                (Vector((sgn * 0.055 * H, -0.025 * H, 0.018 * H)), 0.009 * H),
                (Vector((sgn * 0.092 * H, -0.055 * H, 0.052 * H)), 0.002 * H),
            ], segments=8)
            acc.add_bmesh(cheek_horn, 2, "Head")

    # Glowing eyes facing -Y
    eye_y = hc.y - head_r * 0.88
    eye_z = hc.z + head_r * 0.12
    for sgn in (-1.0, 1.0):
        acc.add_bmesh(
            bm_icosphere((sgn * head_r * 0.30, eye_y, eye_z), (head_r * 0.14, head_r * 0.10, head_r * 0.085), subdiv=1, rot_euler=(0, sgn * 0.18, 0)),
            3, "Head", split_sharp_deg=None,
        )

    # ---- 6. Articulated 3-Bone Flowing Cape / Tattered Shroud (Cape_1..3) ----
    c1_h, _ = bpos("Cape_1")
    _, c3_t = bpos("Cape_3")
    cape_len = c1_h.z - c3_t.z
    cape_tattered = arch in ("reaper", "lich", "juggernaut", "archon")
    cape_w_top = 0.135 * W if arch == "reaper" else 0.155 * W
    cape_w_bot = 0.215 * W if arch == "reaper" else 0.285 * W
    cape = bm_flowing_cape(
        top_center=c1_h + Vector((0, 0.016 * H, 0.012 * H)),
        top_w=cape_w_top,
        bot_w=cape_w_bot,
        length=cape_len,
        back_sweep=0.098 * H,
        thickness=0.010 * H,
        rows=18,
        cols=17,
        tattered=cape_tattered,
    )
    acc.add_bmesh(cape, 1, "Cape", split_sharp_deg=None)

    if arch == "archon":
        for sgn in (-1.0, 1.0):
            cloak_plate = bm_curved_plate(
                center=c1_h + Vector((sgn * 0.075 * W, 0.032 * H, -0.075 * H)),
                rx=0.092 * W, ry=0.072 * W, height=0.140 * H,
                arc_deg=110.0, thickness=0.010 * H, center_yaw_deg=180.0 - sgn * 28.0,
                tilt_x_deg=-14.0, taper_top=0.82, taper_bot=1.18, ridge_amp=0.22, segs_u=8, segs_v=6,
            )
            acc.add_bmesh(cloak_plate, 0, "Cape", split_sharp_deg=38.0)

    # ---- 7. Iconic High-Detail Weapons (Weapon_R) ----
    wh, _ = bpos("Weapon_R")
    grip = wh
    if arch == "archmage":
        staff_bot = grip + Vector((0, 0.04 * H, -0.40 * H))
        staff_top = grip + Vector((0, -0.08 * H, 0.52 * H))
        shaft = bm_sculpt_loft([
            (staff_bot, 0.010 * H, 0.010 * H),
            (staff_bot + Vector((0, -0.005 * H, 0.04 * H)), 0.018 * H, 0.018 * H),
            (grip, 0.015 * H, 0.015 * H),
            (staff_top - Vector((0, -0.01 * H, 0.06 * H)), 0.022 * H, 0.022 * H),
            (staff_top, 0.026 * H, 0.026 * H),
        ], segments=14)
        acc.add_bmesh(shaft, 4, "Weapon_R", split_sharp_deg=42.0)

        for dx, dy in [(-0.048 * H, -0.012 * H), (0.048 * H, -0.012 * H), (0.0, 0.045 * H)]:
            prong = bm_horn_arc(staff_top, [
                (Vector((0, 0, 0)), 0.014 * H),
                (Vector((dx, dy, 0.055 * H)), 0.011 * H),
                (Vector((dx * 0.65, dy * 0.65, 0.135 * H)), 0.005 * H),
            ], segments=8)
            acc.add_bmesh(prong, 2, "Weapon_R", split_sharp_deg=38.0)

        core_pos = staff_top + Vector((0, -0.006 * H, 0.082 * H))
        for tilt_a in ((0.62, 0.35, 0.0), (-0.62, -0.35, 0.4)):
            ring_pts = []
            r_ast = 0.052 * H
            rot_m = Euler(tilt_a, "XYZ").to_matrix()
            for k in range(17):
                a = 2.0 * math.pi * k / 16.0
                ring_pts.append((core_pos + rot_m @ Vector((r_ast * math.cos(a), r_ast * math.sin(a), 0)), 0.0045 * H, 0.0045 * H))
            acc.add_bmesh(bm_sculpt_loft(ring_pts, segments=6, cap_start=False, cap_end=False), 2, "Weapon_R", split_sharp_deg=40.0)

        crystal = bm_bevel_diamond(tuple(core_pos), (0.036 * H, 0.036 * H, 0.068 * H), bevel=0.006 * H, rot_euler=(0.2, 0.3, math.pi * 0.25))
        acc.add_bmesh(crystal, 3, "Weapon_R", split_sharp_deg=24.0)

    elif arch == "lich":
        staff_bot = grip + Vector((0, 0.05 * H, -0.44 * H))
        staff_top = grip + Vector((0, -0.09 * H, 0.56 * H))
        shaft_pts = []
        for k in range(9):
            t = k / 8.0
            p = staff_bot.lerp(staff_top, t) + Vector((0.012 * H * math.sin(t * 7.0), 0.010 * H * math.cos(t * 5.0), 0))
            rad = (0.011 + 0.010 * (t ** 1.4)) * H
            shaft_pts.append((p, rad, rad * 0.92))
        acc.add_bmesh(bm_sculpt_loft(shaft_pts, segments=12), 4, "Weapon_R", split_sharp_deg=45.0)
        for k in range(4):
            ang = k * math.pi * 0.5 + 0.4
            claw = bm_horn_arc(staff_top, [
                (Vector((0, 0, 0)), 0.013 * H),
                (Vector((math.cos(ang) * 0.050 * H, math.sin(ang) * 0.050 * H, 0.048 * H)), 0.009 * H),
                (Vector((math.cos(ang) * 0.030 * H, math.sin(ang) * 0.030 * H, 0.118 * H)), 0.003 * H),
            ], segments=6)
            acc.add_bmesh(claw, 2, "Weapon_R", split_sharp_deg=38.0)
        orb_p = staff_top + Vector((0, -0.005 * H, 0.078 * H))
        acc.add_bmesh(bm_icosphere(tuple(orb_p), (0.034 * H, 0.034 * H, 0.040 * H), subdiv=2), 3, "Weapon_R", split_sharp_deg=None)

    elif arch == "reaper":
        # Curved Serrated Obsidian-Iron Scythe sweeping outward (-X) and forward (-Y)
        haft_bot = grip + Vector((0, 0.16 * H, -0.22 * H))
        haft_top = grip + Vector((0, -0.18 * H, 0.24 * H))
        acc.add_bmesh(bm_sculpt_loft([(haft_bot, 0.014 * H, 0.014 * H), (grip, 0.017 * H, 0.017 * H), (haft_top, 0.020 * H, 0.020 * H)], segments=10), 2, "Weapon_R")
        blade_pts = [
            (Vector((0, 0, 0)), 0.030 * H, 0.010 * H),
            (Vector((-0.11 * H, -0.09 * H, 0.04 * H)), 0.042 * H, 0.012 * H),
            (Vector((-0.24 * H, -0.17 * H, -0.02 * H)), 0.034 * H, 0.010 * H),
            (Vector((-0.34 * H, -0.22 * H, -0.14 * H)), 0.020 * H, 0.007 * H),
            (Vector((-0.37 * H, -0.24 * H, -0.28 * H)), 0.003 * H, 0.003 * H),
        ]
        b_secs = [(haft_top + p, rx, ry) for p, rx, ry in blade_pts]
        acc.add_bmesh(bm_sculpt_loft(b_secs, segments=6), 4, "Weapon_R", split_sharp_deg=28.0)
        for ti in range(1, 7):
            tt = ti / 7.5
            tp = haft_top + blade_pts[1][0].lerp(blade_pts[3][0], tt)
            tooth = bm_sculpt_loft([
                (tp + Vector((0.008 * H, 0.006 * H, -0.012 * H)), 0.008 * H, 0.005 * H),
                (tp + Vector((0.026 * H, 0.018 * H, -0.052 * H)), 0.0015 * H, 0.0015 * H),
            ], segments=4)
            acc.add_bmesh(tooth, 4, "Weapon_R", split_sharp_deg=25.0)

    elif arch == "juggernaut":
        # Colossal 2-Handed Bearded Executioner Axe with broad crescent blade clearly facing outward
        haft_bot = grip + Vector((0, 0.14 * H, -0.26 * H))
        haft_top = grip + Vector((0, -0.34 * H, 0.44 * H))
        haft_dir = (haft_top - haft_bot).normalized()
        acc.add_bmesh(bm_sculpt_loft([(haft_bot, 0.018 * H, 0.018 * H), (grip, 0.020 * H, 0.020 * H), (haft_top, 0.024 * H, 0.024 * H)], segments=12), 4, "Weapon_R", split_sharp_deg=42.0)
        axe_c = haft_top - haft_dir * (0.09 * H)
        blade_bm = bmesh.new()
        uv_layer = blade_bm.loops.layers.uv.verify()
        su, sv = 10, 8
        g_pos, g_neg = [], []
        axe_fwd = Vector((-0.86, -0.44, -0.24)).normalized()  # broad outward (-X) + forward (-Y) silhouette!
        axe_side = haft_dir.cross(axe_fwd).normalized()
        for r in range(sv):
            tv = r / (sv - 1) - 0.5
            h_span = (0.34 * H) * (tv - 0.12 if tv < 0 else tv)
            reach_scale = 1.0 - 0.25 * (tv * 2.0) ** 2 + (0.28 if tv < -0.1 else 0.0)
            row_p, row_n = [], []
            for c in range(su):
                tu = c / (su - 1)
                reach = (0.02 + 0.22 * tu * reach_scale) * H
                curve_z = (0.06 * math.sin(tu * math.pi) - 0.08 * tu * (1.0 - tv * 1.5)) * H
                th_b = 0.020 * H * (1.0 - 0.85 * (tu ** 1.3))
                base_p = axe_c + haft_dir * h_span + axe_fwd * reach + haft_dir * curve_z
                row_p.append(blade_bm.verts.new(base_p + axe_side * th_b))
                row_n.append(blade_bm.verts.new(base_p - axe_side * th_b))
            g_pos.append(row_p)
            g_neg.append(row_n)
        for r in range(sv - 1):
            for c in range(su - 1):
                f1 = blade_bm.faces.new((g_pos[r][c], g_pos[r][c + 1], g_pos[r + 1][c + 1], g_pos[r + 1][c]))
                f2 = blade_bm.faces.new((g_neg[r + 1][c], g_neg[r + 1][c + 1], g_neg[r][c + 1], g_neg[r][c]))
                for f in (f1, f2):
                    for lp, uv in zip(f.loops, [(c / (su - 1), r / (sv - 1)), ((c + 1) / (su - 1), r / (sv - 1)), ((c + 1) / (su - 1), (r + 1) / (sv - 1)), (c / (su - 1), (r + 1) / (sv - 1))]):
                        lp[uv_layer].uv = uv
            blade_bm.faces.new((g_neg[r][0], g_pos[r][0], g_pos[r + 1][0], g_neg[r + 1][0]))
            blade_bm.faces.new((g_pos[r][-1], g_neg[r][-1], g_neg[r + 1][-1], g_pos[r + 1][-1]))
        for c in range(su - 1):
            blade_bm.faces.new((g_neg[0][c], g_neg[0][c + 1], g_pos[0][c + 1], g_pos[0][c]))
            blade_bm.faces.new((g_pos[-1][c], g_pos[-1][c + 1], g_neg[-1][c + 1], g_neg[-1][c]))
        acc.add_bmesh(blade_bm, 4, "Weapon_R", split_sharp_deg=26.0)
        # Beveled dark brass socket collar + glowing molten rune boss on axe head
        sock = bm_sculpt_loft([
            (axe_c - haft_dir * (0.06 * H), 0.030 * H, 0.028 * H),
            (axe_c + haft_dir * (0.06 * H), 0.032 * H, 0.030 * H),
        ], segments=10)
        acc.add_bmesh(sock, 2, "Weapon_R", split_sharp_deg=35.0)
        acc.add_bmesh(bm_icosphere(tuple(axe_c + axe_fwd * (0.048 * H)), (0.022 * H, 0.022 * H, 0.026 * H), subdiv=1), 3, "Weapon_R", split_sharp_deg=None)
        rear_sp = bm_sculpt_loft([(axe_c, 0.024 * H, 0.018 * H), (axe_c - axe_fwd * (0.14 * H), 0.002 * H, 0.002 * H)], segments=6)
        top_sp = bm_sculpt_loft([(haft_top, 0.020 * H, 0.020 * H), (haft_top + haft_dir * (0.13 * H), 0.002 * H, 0.002 * H)], segments=6)
        acc.add_bmesh(bm_merge([rear_sp, top_sp]), 4, "Weapon_R", split_sharp_deg=32.0)

    else:  # archon colossal Runic Greatsword (sleek razor-beveled gothic greatsword)
        pommel = grip + Vector((0, 0.14 * H, -0.12 * H))
        guard = grip + Vector((0, -0.10 * H, 0.08 * H))
        tip = grip + Vector((0, -0.58 * H, 0.50 * H))
        hilt = bm_sculpt_loft([(pommel, 0.022 * H, 0.022 * H), (grip, 0.016 * H, 0.016 * H), (guard, 0.020 * H, 0.020 * H)], segments=12)
        acc.add_bmesh(hilt, 2, "Weapon_R", split_sharp_deg=40.0)
        for sgn in (-1.0, 1.0):
            wing_g = bm_horn_arc(guard, [
                (Vector((0, 0, 0)), 0.018 * H),
                (Vector((sgn * 0.075 * H, -0.01 * H, 0.018 * H)), 0.015 * H),
                (Vector((sgn * 0.14 * H, -0.035 * H, 0.045 * H)), 0.004 * H),
            ], segments=8)
            acc.add_bmesh(wing_g, 2, "Weapon_R", split_sharp_deg=32.0)
        blade = bm_sculpt_loft([
            (guard, 0.040 * H, 0.012 * H),
            (guard.lerp(tip, 0.25), 0.037 * H, 0.011 * H),
            (guard.lerp(tip, 0.68), 0.030 * H, 0.009 * H),
            (guard.lerp(tip, 0.90), 0.020 * H, 0.006 * H),
            (tip, 0.002 * H, 0.002 * H),
        ], segments=6)
        acc.add_bmesh(blade, 4, "Weapon_R", split_sharp_deg=24.0)
        fuller = bm_sculpt_loft([
            (guard.lerp(tip, 0.06), 0.011 * H, 0.013 * H),
            (guard.lerp(tip, 0.74), 0.007 * H, 0.010 * H),
        ], segments=4)
        acc.add_bmesh(fuller, 3, "Weapon_R", split_sharp_deg=24.0)

    return acc


# ---------------------------------------------------------------------------
# Dense Biomechanical Animation Engine (15 Clips, 17-23 Keyframes, Smooth Bezier)
# ---------------------------------------------------------------------------

def reset_pose(arm_obj: bpy.types.Object) -> None:
    for pb in arm_obj.pose.bones:
        pb.rotation_mode = "QUATERNION"
        pb.location = (0.0, 0.0, 0.0)
        pb.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
        pb.scale = (1.0, 1.0, 1.0)


def set_bone_rot(arm_obj: bpy.types.Object, bone_name: str, rx_deg: float, ry_deg: float, rz_deg: float) -> None:
    if bone_name not in arm_obj.pose.bones:
        return
    pb = arm_obj.pose.bones[bone_name]
    pb.rotation_mode = "QUATERNION"
    r_world = Euler((math.radians(rx_deg), math.radians(ry_deg), math.radians(rz_deg)), "XYZ").to_matrix()
    r_rest = pb.bone.matrix_local.to_3x3()
    pb.rotation_quaternion = (r_rest.inverted() @ r_world @ r_rest).to_quaternion()


def set_bone_loc(arm_obj: bpy.types.Object, bone_name: str, delta_char_space: Vector) -> None:
    if bone_name not in arm_obj.pose.bones:
        return
    pb = arm_obj.pose.bones[bone_name]
    pb.location = pb.bone.matrix_local.to_3x3().inverted() @ delta_char_space


def key_all_bones(arm_obj: bpy.types.Object, frame: int) -> None:
    for pb in arm_obj.pose.bones:
        pb.keyframe_insert(data_path="location", frame=frame)
        pb.keyframe_insert(data_path="rotation_quaternion", frame=frame)


def apply_smooth_beziers(act: bpy.types.Action) -> None:
    for fcurve in act.fcurves:
        for kp in fcurve.keyframe_points:
            kp.interpolation = "BEZIER"
            kp.handle_left_type = "AUTO_CLAMPED"
            kp.handle_right_type = "AUTO_CLAMPED"


def _smoothstep(t: float) -> float:
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def _eval_pwl(t: float, pts: List[Tuple[float, float]]) -> float:
    if t <= pts[0][0]:
        return pts[0][1]
    if t >= pts[-1][0]:
        return pts[-1][1]
    for i in range(len(pts) - 1):
        t0, v0 = pts[i]
        t1, v1 = pts[i + 1]
        if t <= t1:
            u = _smoothstep((t - t0) / max(t1 - t0, 1e-6))
            return v0 + (v1 - v0) * u
    return pts[-1][1]


def _eval_cyclic_pwl(p: float, pts: List[Tuple[float, float]]) -> float:
    """Evaluate periodic piecewise smooth curve wrapping continuously across [0, 1)."""
    p = p % 1.0
    return _eval_pwl(p, pts)


def _dense_frames(end_frame: int, target_keys: int) -> List[int]:
    raw = [int(round(1 + (end_frame - 1) * i / (target_keys - 1))) for i in range(target_keys)]
    out: List[int] = []
    for f in raw:
        if not out or f > out[-1]:
            out.append(f)
    if out[-1] != end_frame:
        out[-1] = end_frame
    return out


def build_all_animations(arm_obj: bpy.types.Object, spec: dict) -> List[str]:
    H = spec["height"]
    arch = spec["archetype"]
    bpy.context.scene.render.fps = 30
    arm_obj.animation_data_create()

    hunch_s = 16.0 if arch == "reaper" else (6.0 if arch in ("juggernaut", "archon") else 2.5)
    hunch_c = 18.0 if arch == "reaper" else (7.0 if arch in ("juggernaut", "archon") else 3.5)
    head_c = -24.0 if arch == "reaper" else -4.5
    heavy_m = 0.85 if arch in ("juggernaut", "archon") else (1.12 if arch == "reaper" else 1.0)

    def apply_base_guard(breath: float = 0.0) -> None:
        set_bone_rot(arm_obj, "Spine", hunch_s + breath * 1.4, 0, 0)
        set_bone_rot(arm_obj, "Chest", hunch_c + breath * 2.4, 0, 0)
        set_bone_rot(arm_obj, "Head", head_c - breath * 1.2, 0, 0)
        set_bone_rot(arm_obj, "Shoulder_L", 0, -breath * 1.5, 0)
        set_bone_rot(arm_obj, "Shoulder_R", 0, breath * 1.5, 0)
        if arch in ("archmage", "lich"):
            set_bone_rot(arm_obj, "UpperArm_R", -28 - breath * 2.2, -8, -10)
            set_bone_rot(arm_obj, "Forearm_R", -38 + breath * 2.0, 0, 12)
            set_bone_rot(arm_obj, "Weapon_R", 18, 0, 0)
            set_bone_rot(arm_obj, "UpperArm_L", -18 + breath * 1.8, 10, 12)
            set_bone_rot(arm_obj, "Forearm_L", -35 - breath * 1.8, 0, -14)
        elif arch == "reaper":
            set_bone_rot(arm_obj, "UpperArm_R", -36 - breath * 2.0, -12, -14)
            set_bone_rot(arm_obj, "Forearm_R", -46, 0, 10)
            set_bone_rot(arm_obj, "Weapon_R", 8, 0, 0)
            set_bone_rot(arm_obj, "UpperArm_L", -36 + breath * 2.0, 12, 14)
            set_bone_rot(arm_obj, "Forearm_L", -46, 0, -10)
        else:
            set_bone_rot(arm_obj, "UpperArm_R", -22 - breath * 2.0, -14, -8)
            set_bone_rot(arm_obj, "Forearm_R", -32, 0, 14)
            set_bone_rot(arm_obj, "Weapon_R", 10, 0, -10)
            set_bone_rot(arm_obj, "UpperArm_L", -16 + breath * 1.8, 12, 8)
            set_bone_rot(arm_obj, "Forearm_L", -28, 0, -12)

    created: List[str] = []

    def record_anim(name: str, frame_len: int, key_count: int, eval_fn: Callable[[float], None]) -> None:
        end_fr = 1 + frame_len
        act = bpy.data.actions.new(name=name)
        arm_obj.animation_data.action = act
        for fr in _dense_frames(end_fr, key_count):
            t = (fr - 1) / float(frame_len)
            reset_pose(arm_obj)
            eval_fn(t)
            key_all_bones(arm_obj, fr)
        apply_smooth_beziers(act)
        tr = arm_obj.animation_data.nla_tracks.new()
        tr.name = name
        st = tr.strips.new(name, 1, act)
        st.action_frame_start = 1
        st.action_frame_end = end_fr
        arm_obj.animation_data.action = None
        created.append(name)

    def set_cape_advanced(
        base_lift_world: float,
        wave_amp_x: float,
        ph_x: float,
        wave_amp_lat: float,
        ph_lat: float,
        chest_lean_x: float,
        torso_yaw: float = 0.0,
    ) -> None:
        """Aerodynamic multi-segment trailing cloth wave simulation.
        Creates an organic trailing parabolic C-curve with progressive phase lag,
        amplitude amplification toward the hem, and dynamic S-wave lateral sway.
        """
        c1_base = max(-45.0, min(55.0, base_lift_world * 0.70 - chest_lean_x))
        c2_base = base_lift_world * 0.28 + 6.0
        c3_base = base_lift_world * 0.20 + 8.0

        c1_x = c1_base + wave_amp_x * 0.45 * math.sin(ph_x)
        c2_x = c2_base + wave_amp_x * 0.85 * math.sin(ph_x - 0.75)
        c3_x = c3_base + wave_amp_x * 1.30 * math.sin(ph_x - 1.55)

        c1_z = -torso_yaw * 0.35 + wave_amp_lat * 0.50 * math.sin(ph_lat)
        c2_z = -torso_yaw * 0.65 + wave_amp_lat * 0.90 * math.sin(ph_lat - 0.70)
        c3_z = -torso_yaw * 0.95 + wave_amp_lat * 1.40 * math.sin(ph_lat - 1.45)

        c1_y = wave_amp_lat * 0.35 * math.cos(ph_lat)
        c2_y = wave_amp_lat * 0.70 * math.cos(ph_lat - 0.70)
        c3_y = wave_amp_lat * 1.10 * math.cos(ph_lat - 1.45)

        set_bone_rot(arm_obj, "Cape_1", c1_x, c1_y, c1_z)
        set_bone_rot(arm_obj, "Cape_2", c2_x, c2_y, c2_z)
        set_bone_rot(arm_obj, "Cape_3", c3_x, c3_y, c3_z)

    # 1. Idle (90 frames, 3.0s loop)
    def eval_idle(t: float) -> None:
        ph = 2.0 * math.pi * t
        b = 0.70 * math.sin(ph) - 0.25 * math.cos(2.0 * ph) + 0.15
        sway = math.sin(ph * 0.5)
        sway_y = math.cos(ph * 0.5)

        hip_dz = 0.006 * H * b
        hip_dx = 0.010 * H * sway
        hip_rx = 1.4 * b
        hip_ry = 2.4 * sway
        hip_yaw = 1.8 * sway_y

        set_bone_loc(arm_obj, "Hips", Vector((hip_dx, 0.0, hip_dz)))
        set_bone_rot(arm_obj, "Hips", hip_rx, hip_ry, hip_yaw)

        sp_rx = hunch_s + b * 1.6
        ch_rx = hunch_c + b * 2.6
        set_bone_rot(arm_obj, "Spine", sp_rx, -hip_ry * 0.40, -hip_yaw * 0.40)
        set_bone_rot(arm_obj, "Chest", ch_rx, -hip_ry * 0.50, -hip_yaw * 0.60)

        head_pitch = head_c - b * 0.8
        head_yaw = 3.2 * math.sin(ph * 0.5 + 0.8)
        set_bone_rot(arm_obj, "Head", head_pitch, -hip_ry * 0.35, head_yaw)

        set_bone_rot(arm_obj, "Thigh_L", -4.0 - 2.5 * b, -3.5 + hip_ry * 0.5, 0)
        set_bone_rot(arm_obj, "Calf_L", 8.0 + 3.2 * b, 0, 0)
        set_bone_rot(arm_obj, "Foot_L", -3.0 - 1.2 * b, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", -2.0 + 2.5 * b, 3.5 + hip_ry * 0.5, 0)
        set_bone_rot(arm_obj, "Calf_R", 8.0 - 2.8 * b, 0, 0)
        set_bone_rot(arm_obj, "Foot_R", -3.0 + 1.2 * b, 0, 0)

        apply_base_guard(breath=b)
        if arch in ("archmage", "lich"):
            set_bone_rot(arm_obj, "Weapon_R", 18.0 + 2.5 * b, 1.2 * sway, 0.8 * sway_y)
            set_bone_rot(arm_obj, "Hand_L", 4.0 * b, 0, -2.0 * sway)

        lean = hip_rx + sp_rx + ch_rx
        set_cape_advanced(
            base_lift_world=11.0,
            wave_amp_x=2.8,
            ph_x=ph,
            wave_amp_lat=2.2,
            ph_lat=ph * 0.5,
            chest_lean_x=lean,
            torso_yaw=hip_yaw,
        )
    record_anim("Idle", 90, 23, eval_idle)

    # 2. Run_Fwd (24 frames, 0.8s loop, 25 dense keys)
    thigh_run_curve: List[Tuple[float, float]] = [
        (0.00, -34.0), # Phase 1: Contact (Heel Strike reach)
        (0.08, -30.0), # Stance absorption
        (0.15, -20.0), # Phase 2: Down (Compression / shock absorption)
        (0.26, -5.0),  # Phase 3: Passing (Mid-Stance pass-through)
        (0.36, 12.0),  # Drive extension
        (0.44, 28.0),  # Phase 4: Up (Push-Off full trailing extension)
        (0.52, 22.0),  # Toe-off release
        (0.64, 4.0),   # Phase 5: High knee tuck acceleration
        (0.76, -18.0), # Forward swing whip
        (0.88, -32.0), # Deceleration & reach for contact
        (1.00, -34.0), # Contact wrap
    ]

    calf_run_curve: List[Tuple[float, float]] = [
        (0.00, 14.0),  # Phase 1: Contact (slight knee unlock)
        (0.08, 28.0),  # Impact absorption
        (0.15, 46.0),  # Phase 2: Down (deep knee compression shock absorber)
        (0.26, 30.0),  # Passing extension
        (0.36, 16.0),  # Drive extension
        (0.44, 12.0),  # Phase 4: Up (push-off straightened leg)
        (0.52, 38.0),  # Lift-off rapid knee bend
        (0.64, 82.0),  # Phase 5: High knee tuck (heel to glute pendulum shortening)
        (0.76, 64.0),  # Forward swing-through
        (0.88, 32.0),  # Pendulum unfolding forward
        (1.00, 14.0),  # Contact wrap
    ]

    foot_run_curve: List[Tuple[float, float]] = [
        (0.00, -18.0), # Phase 1: Contact (dorsiflexion - heel hits first)
        (0.07, -6.0),  # Rolling onto heel
        (0.15, 2.0),   # Phase 2: Down (foot flat on ground absorbing load)
        (0.26, 8.0),   # Passing (roll toward ball of foot)
        (0.36, 18.0),  # Heel rise
        (0.44, 34.0),  # Phase 4: Up (powerful toe push-off / plantarflexion)
        (0.52, 22.0),  # Leaving ground
        (0.64, 8.0),   # Phase 5: Swing neutral
        (0.76, -2.0),  # Forward flight
        (0.88, -12.0), # Dorsiflexion preparing for next heel strike
        (1.00, -18.0), # Contact wrap
    ]

    pelvis_dz_curve: List[Tuple[float, float]] = [
        (0.00, -0.012),
        (0.07, -0.032),
        (0.15, -0.046),
        (0.24, -0.020),
        (0.34, 0.008),
        (0.44, 0.020),
        (0.50, -0.012),
        (0.57, -0.032),
        (0.65, -0.046),
        (0.74, -0.020),
        (0.84, 0.008),
        (0.94, 0.020),
        (1.00, -0.012),
    ]

    def eval_run_fwd(t: float) -> None:
        ph = 2.0 * math.pi * t
        p_l = t % 1.0
        p_r = (t + 0.5) % 1.0

        hip_dz = _eval_cyclic_pwl(t, pelvis_dz_curve) * H * (1.15 if arch == "juggernaut" else (0.75 if arch == "lich" else 1.0))
        hip_dx = 0.014 * H * math.sin(ph - 0.25)
        hip_dy = -0.008 * H * math.cos(2.0 * ph)
        set_bone_loc(arm_obj, "Hips", Vector((hip_dx, hip_dy, hip_dz)))

        hip_rx = 10.0 + 3.5 * math.cos(4.0 * math.pi * (t - 0.15))
        hip_ry = -5.4 * math.sin(ph - 0.20)
        hip_yaw = -11.5 * math.cos(ph)
        set_bone_rot(arm_obj, "Hips", hip_rx, hip_ry, hip_yaw)

        spine_rx = hunch_s + 4.0 + 2.0 * math.cos(4.0 * math.pi * (t - 0.15))
        spine_ry = -hip_ry * 0.45
        spine_rz = -hip_yaw * 0.45
        set_bone_rot(arm_obj, "Spine", spine_rx, spine_ry, spine_rz)

        chest_rx = hunch_c + 6.0 + 2.5 * math.cos(4.0 * math.pi * (t - 0.15))
        chest_ry = -hip_ry * 0.50
        chest_rz = -hip_yaw * 0.85
        set_bone_rot(arm_obj, "Chest", chest_rx, chest_ry, chest_rz)

        tot_torso_yaw = hip_yaw + spine_rz + chest_rz
        tot_torso_roll = hip_ry + spine_ry + chest_ry
        tot_pitch_delta = (hip_rx - 10.0) + (chest_rx - hunch_c - 6.0)
        head_rx = head_c - 8.0 - tot_pitch_delta * 0.70
        head_ry = -tot_torso_roll * 0.92
        head_rz = -tot_torso_yaw * 0.94
        set_bone_rot(arm_obj, "Head", head_rx, head_ry, head_rz)

        th_l = _eval_cyclic_pwl(p_l, thigh_run_curve) * heavy_m
        ca_l = _eval_cyclic_pwl(p_l, calf_run_curve)
        ft_l = _eval_cyclic_pwl(p_l, foot_run_curve)

        th_r = _eval_cyclic_pwl(p_r, thigh_run_curve) * heavy_m
        ca_r = _eval_cyclic_pwl(p_r, calf_run_curve)
        ft_r = _eval_cyclic_pwl(p_r, foot_run_curve)

        set_bone_rot(arm_obj, "Thigh_L", th_l, -3.0, 0)
        set_bone_rot(arm_obj, "Calf_L", ca_l, 0, 0)
        set_bone_rot(arm_obj, "Foot_L", ft_l, 0, 0)

        set_bone_rot(arm_obj, "Thigh_R", th_r, 3.0, 0)
        set_bone_rot(arm_obj, "Calf_R", ca_r, 0, 0)
        set_bone_rot(arm_obj, "Foot_R", ft_r, 0, 0)

        uar_l_rx = -16.0 + 36.0 * math.cos(ph)
        far_l_rx = -62.0 - 16.0 * math.sin(ph)
        set_bone_rot(arm_obj, "UpperArm_L", uar_l_rx, 12.0, 10.0)
        set_bone_rot(arm_obj, "Forearm_L", far_l_rx, 0, -10.0)
        set_bone_rot(arm_obj, "Hand_L", -10.0 - 12.0 * math.sin(ph - 0.50), 0, 0)

        uar_r_rx = -18.0 - 16.0 * math.cos(ph)
        far_r_rx = -46.0 + 10.0 * math.sin(ph)
        set_bone_rot(arm_obj, "UpperArm_R", uar_r_rx, -14.0, -10.0)
        set_bone_rot(arm_obj, "Forearm_R", far_r_rx, 0, 12.0)
        set_bone_rot(arm_obj, "Hand_R", 4.0 + 6.0 * math.cos(ph - 0.60), 0, 0)

        drag_ph = ph - 0.75
        wep_rx = 26.0 + 14.0 * math.cos(drag_ph)
        wep_ry = -5.0 * math.sin(drag_ph)
        wep_rz = 7.0 * math.sin(drag_ph)
        set_bone_rot(arm_obj, "Weapon_R", wep_rx, wep_ry, wep_rz)

        total_chest_lean = hip_rx + spine_rx + chest_rx
        set_cape_advanced(
            base_lift_world=38.0,
            wave_amp_x=9.0,
            ph_x=2.0 * ph - 0.40,
            wave_amp_lat=10.0,
            ph_lat=ph - 0.50,
            chest_lean_x=total_chest_lean,
            torso_yaw=chest_rz,
        )
    record_anim("Run_Fwd", 24, 25, eval_run_fwd)

    # 3. Run_Back (24 frames, 0.8s loop, 21 dense keys)
    thigh_back_curve: List[Tuple[float, float]] = [
        (0.00, 28.0),
        (0.12, 20.0),
        (0.24, 4.0),
        (0.38, -18.0),
        (0.50, -28.0),
        (0.62, -12.0),
        (0.74, 8.0),
        (0.88, 24.0),
        (1.00, 28.0),
    ]
    calf_back_curve: List[Tuple[float, float]] = [
        (0.00, 16.0),
        (0.12, 42.0),
        (0.24, 26.0),
        (0.38, 14.0),
        (0.50, 12.0),
        (0.62, 54.0),
        (0.74, 62.0),
        (0.88, 30.0),
        (1.00, 16.0),
    ]
    foot_back_curve: List[Tuple[float, float]] = [
        (0.00, 22.0),
        (0.12, 6.0),
        (0.24, -4.0),
        (0.38, -16.0),
        (0.50, -18.0),
        (0.62, 4.0),
        (0.74, 14.0),
        (0.88, 20.0),
        (1.00, 22.0),
    ]

    def eval_run_back(t: float) -> None:
        ph = 2.0 * math.pi * t
        p_l = t % 1.0
        p_r = (t + 0.5) % 1.0

        hip_dz = (-0.022 + 0.026 * math.sin(2.0 * ph)) * H
        hip_dx = 0.010 * H * math.sin(ph)
        yaw = 8.5 * math.cos(ph)
        set_bone_loc(arm_obj, "Hips", Vector((hip_dx, 0.0, hip_dz)))
        set_bone_rot(arm_obj, "Hips", -5.0, -4.0 * math.sin(ph), yaw)

        set_bone_rot(arm_obj, "Spine", hunch_s - 2.0, 0, -yaw * 0.45)
        set_bone_rot(arm_obj, "Chest", hunch_c - 4.0, 0, -yaw * 0.70)
        set_bone_rot(arm_obj, "Head", head_c + 6.0, 0, -(yaw - yaw * 0.70) * 0.90)

        th_l = _eval_cyclic_pwl(p_l, thigh_back_curve)
        ca_l = _eval_cyclic_pwl(p_l, calf_back_curve)
        ft_l = _eval_cyclic_pwl(p_l, foot_back_curve)

        th_r = _eval_cyclic_pwl(p_r, thigh_back_curve)
        ca_r = _eval_cyclic_pwl(p_r, calf_back_curve)
        ft_r = _eval_cyclic_pwl(p_r, foot_back_curve)

        set_bone_rot(arm_obj, "Thigh_L", th_l, -3.0, 0)
        set_bone_rot(arm_obj, "Calf_L", ca_l, 0, 0)
        set_bone_rot(arm_obj, "Foot_L", ft_l, 0, 0)

        set_bone_rot(arm_obj, "Thigh_R", th_r, 3.0, 0)
        set_bone_rot(arm_obj, "Calf_R", ca_r, 0, 0)
        set_bone_rot(arm_obj, "Foot_R", ft_r, 0, 0)

        set_bone_rot(arm_obj, "UpperArm_L", -32.0 + 12.0 * math.cos(ph), 10, 10)
        set_bone_rot(arm_obj, "Forearm_L", -56.0, 0, -14)
        set_bone_rot(arm_obj, "UpperArm_R", -28.0 - 12.0 * math.cos(ph), -12, -10)
        set_bone_rot(arm_obj, "Forearm_R", -46.0, 0, 14)
        set_bone_rot(arm_obj, "Weapon_R", 16.0 + 4.0 * math.sin(ph), 0, 0)

        lean = -5.0 + hunch_s - 2.0 + hunch_c - 4.0
        set_cape_advanced(base_lift_world=8.0, wave_amp_x=5.0, ph_x=2.0 * ph, wave_amp_lat=5.0, ph_lat=ph, chest_lean_x=lean, torso_yaw=-yaw * 0.70)
    record_anim("Run_Back", 24, 21, eval_run_back)

    # 4. Run_StrafeL & Run_StrafeR (24 frames, 0.8s loop, 21 dense keys)
    def make_strafe_eval(sgn: float) -> Callable[[float], None]:
        def eval_strafe(t: float) -> None:
            ph = 2.0 * math.pi * t
            dz = (-0.032 + 0.030 * abs(math.sin(ph))) * H
            dx = sgn * 0.022 * H * math.sin(ph)
            set_bone_loc(arm_obj, "Hips", Vector((dx, 0, dz)))

            hip_roll = sgn * (7.0 + 3.0 * math.cos(ph))
            hip_yaw = sgn * 8.0 * math.sin(ph)
            set_bone_rot(arm_obj, "Hips", 5.0, hip_roll, hip_yaw)

            set_bone_rot(arm_obj, "Spine", hunch_s + 5.0, -hip_roll * 0.40, -hip_yaw * 0.50)
            set_bone_rot(arm_obj, "Chest", hunch_c + 4.0, -hip_roll * 0.50, -hip_yaw * 0.85)
            set_bone_rot(arm_obj, "Head", head_c - 4.0, -hip_roll * 0.20, -(hip_yaw - hip_yaw * 0.85) * 0.95)

            ry_lead = _eval_cyclic_pwl(t, [(0.0, 24.0), (0.25, 8.0), (0.50, -12.0), (0.75, 10.0), (1.0, 24.0)])
            ry_trail = _eval_cyclic_pwl(t + 0.5, [(0.0, 24.0), (0.25, 8.0), (0.50, -12.0), (0.75, 10.0), (1.0, 24.0)])
            pitch_l = -16.0 + 10.0 * math.sin(ph)
            pitch_r = -16.0 - 10.0 * math.sin(ph)

            if sgn > 0:
                set_bone_rot(arm_obj, "Thigh_L", pitch_l, -ry_lead, 6.0 * math.cos(ph))
                set_bone_rot(arm_obj, "Thigh_R", pitch_r, ry_trail, -6.0 * math.cos(ph))
            else:
                set_bone_rot(arm_obj, "Thigh_L", pitch_l, -ry_trail, 6.0 * math.cos(ph))
                set_bone_rot(arm_obj, "Thigh_R", pitch_r, ry_lead, -6.0 * math.cos(ph))

            set_bone_rot(arm_obj, "Calf_L", 32.0 + 20.0 * max(0.0, math.sin(ph)), 0, 0)
            set_bone_rot(arm_obj, "Calf_R", 32.0 + 20.0 * max(0.0, -math.sin(ph)), 0, 0)
            set_bone_rot(arm_obj, "Foot_L", -8.0, sgn * 8.0 * math.cos(ph), 0)
            set_bone_rot(arm_obj, "Foot_R", -8.0, -sgn * 8.0 * math.cos(ph), 0)

            apply_base_guard(breath=0.30 * math.sin(ph))
            set_bone_rot(arm_obj, "Weapon_R", 20.0 + 4.0 * math.cos(ph), sgn * 6.0, 0)

            lean = 5.0 + hunch_s + 5.0 + hunch_c + 4.0
            set_cape_advanced(
                base_lift_world=22.0,
                wave_amp_x=6.5,
                ph_x=2.0 * ph,
                wave_amp_lat=12.0,
                ph_lat=ph,
                chest_lean_x=lean,
                torso_yaw=-sgn * 16.0,
            )
        return eval_strafe
    record_anim("Run_StrafeL", 24, 21, make_strafe_eval(1.0))
    record_anim("Run_StrafeR", 24, 21, make_strafe_eval(-1.0))

    # 5. Cast_Primary (24 frames, 0.8s, 19 dense keys)
    def eval_cast_primary(t: float) -> None:
        dz = _eval_pwl(t, [(0.0, 0.0), (0.25, -0.035), (0.45, 0.016), (0.68, -0.008), (1.0, 0.0)])
        dy = _eval_pwl(t, [(0.0, 0.0), (0.25, 0.024), (0.45, -0.042), (0.72, -0.012), (1.0, 0.0)])
        yaw = _eval_pwl(t, [(0.0, 0.0), (0.25, -28.0), (0.45, 28.0), (0.70, 12.0), (1.0, 0.0)])
        pitch = _eval_pwl(t, [(0.0, 0.0), (0.25, -15.0), (0.45, 20.0), (0.70, 8.0), (1.0, 0.0)])

        uar = _eval_pwl(t, [(0.0, -28.0), (0.25, 18.0), (0.45, -96.0), (0.65, -72.0), (1.0, -28.0)])
        far = _eval_pwl(t, [(0.0, -38.0), (0.25, -74.0), (0.45, -8.0), (0.65, -20.0), (1.0, -38.0)])
        wr  = _eval_pwl(t, [(0.0, 18.0), (0.25, -26.0), (0.45, 72.0), (0.65, 46.0), (1.0, 18.0)])

        ual = _eval_pwl(t, [(0.0, -18.0), (0.25, -38.0), (0.45, -46.0), (0.75, -28.0), (1.0, -18.0)])
        fal = _eval_pwl(t, [(0.0, -35.0), (0.25, -60.0), (0.45, -16.0), (0.75, -28.0), (1.0, -35.0)])

        set_bone_loc(arm_obj, "Hips", Vector((0, dy * H, dz * H)))
        set_bone_rot(arm_obj, "Hips", pitch * 0.25, 0, yaw * 0.25)
        set_bone_rot(arm_obj, "Spine", hunch_s + pitch * 0.45, 0, yaw * 0.45)
        set_bone_rot(arm_obj, "Chest", hunch_c + pitch * 0.65, 0, yaw * 0.65)
        set_bone_rot(arm_obj, "Head", head_c - pitch * 0.35, 0, -yaw * 0.40)

        set_bone_rot(arm_obj, "UpperArm_R", uar, -12, -8)
        set_bone_rot(arm_obj, "Forearm_R", far, 0, 12)
        set_bone_rot(arm_obj, "Weapon_R", wr, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", ual, 18, 16)
        set_bone_rot(arm_obj, "Forearm_L", fal, 0, -12)

        lunge = max(0.0, yaw / 28.0)
        set_bone_rot(arm_obj, "Thigh_L", -5.0 - 14.0 * lunge, -4, 0)
        set_bone_rot(arm_obj, "Calf_L", 10.0 + 22.0 * lunge, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", -2.0 + 16.0 * lunge, 4, 0)
        set_bone_rot(arm_obj, "Calf_R", 10.0 + 8.0 * lunge, 0, 0)

        lean = pitch * 0.25 + hunch_s + pitch * 0.45 + hunch_c + pitch * 0.65
        cape_lift = 14.0 + 36.0 * lunge
        set_cape_advanced(base_lift_world=cape_lift, wave_amp_x=6.0, ph_x=2.0 * math.pi * t, wave_amp_lat=8.0, ph_lat=math.pi * t, chest_lean_x=lean, torso_yaw=yaw * 0.65)
    record_anim("Cast_Primary", 24, 19, eval_cast_primary)

    # 6. Cast_Heavy (42 frames, 1.4s, 23 dense keys)
    def eval_cast_heavy(t: float) -> None:
        dz = _eval_pwl(t, [(0.0, 0.0), (0.24, -0.095), (0.50, 0.038), (0.68, 0.024), (0.80, -0.055), (1.0, 0.0)])
        tp = _eval_pwl(t, [(0.0, 0.0), (0.24, 22.0), (0.50, -22.0), (0.68, -18.0), (0.80, 26.0), (1.0, 0.0)])
        uar = _eval_pwl(t, [(0.0, -28.0), (0.24, -48.0), (0.50, -140.0), (0.68, -140.0), (0.80, -58.0), (1.0, -28.0)])
        far = _eval_pwl(t, [(0.0, -38.0), (0.24, -78.0), (0.50, -12.0), (0.68, -8.0), (0.80, -20.0), (1.0, -38.0)])
        wr  = _eval_pwl(t, [(0.0, 18.0), (0.24, 0.0), (0.50, 80.0), (0.68, 84.0), (0.80, 42.0), (1.0, 18.0)])
        knee = _eval_pwl(t, [(0.0, 8.0), (0.24, 38.0), (0.50, 6.0), (0.68, 4.0), (0.80, 28.0), (1.0, 8.0)])
        cape = _eval_pwl(t, [(0.0, 10.0), (0.24, 18.0), (0.50, 42.0), (0.68, 45.0), (0.80, 44.0), (1.0, 10.0)])

        set_bone_loc(arm_obj, "Hips", Vector((0, 0, dz * H)))
        set_bone_rot(arm_obj, "Spine", hunch_s + tp * 0.50, 0, 0)
        set_bone_rot(arm_obj, "Chest", hunch_c + tp * 0.70, 0, 0)
        set_bone_rot(arm_obj, "Head", head_c + tp * 0.55, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_R", uar, -16, 0)
        set_bone_rot(arm_obj, "Forearm_R", far, 0, 0)
        set_bone_rot(arm_obj, "Weapon_R", wr, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", uar, 16, 0)
        set_bone_rot(arm_obj, "Forearm_L", far, 0, 0)

        set_bone_rot(arm_obj, "Thigh_L", -knee * 0.52, -5, 0)
        set_bone_rot(arm_obj, "Calf_L", knee, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", -knee * 0.48, 5, 0)
        set_bone_rot(arm_obj, "Calf_R", knee, 0, 0)

        lean = hunch_s + hunch_c + tp * 1.2
        set_cape_advanced(base_lift_world=cape, wave_amp_x=6.0, ph_x=2.0 * math.pi * t, wave_amp_lat=4.0, ph_lat=math.pi * t, chest_lean_x=lean)
    record_anim("Cast_Heavy", 42, 23, eval_cast_heavy)

    # 7. Dash (18 frames, 0.6s, 19 dense keys)
    def eval_dash(t: float) -> None:
        env = _eval_pwl(t, [(0.0, 0.0), (0.16, 1.0), (0.68, 1.0), (1.0, 0.0)])
        set_bone_loc(arm_obj, "Hips", Vector((0, -0.03 * H * env, -0.155 * H * env)))
        set_bone_rot(arm_obj, "Hips", 18.0 * env, 0, 0)
        set_bone_rot(arm_obj, "Spine", hunch_s + 26.0 * env, 0, 0)
        set_bone_rot(arm_obj, "Chest", hunch_c + 22.0 * env, 0, 0)
        set_bone_rot(arm_obj, "Head", head_c - 34.0 * env, 0, 0)

        set_bone_rot(arm_obj, "Thigh_L", -4.0 - 54.0 * env, -4, 0)
        set_bone_rot(arm_obj, "Calf_L", 8.0 + 66.0 * env, 0, 0)
        set_bone_rot(arm_obj, "Foot_L", -4.0 + 20.0 * env, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", -2.0 + 48.0 * env, 4, 0)
        set_bone_rot(arm_obj, "Calf_R", 8.0 + 20.0 * env, 0, 0)
        set_bone_rot(arm_obj, "Foot_R", -2.0 + 26.0 * env, 0, 0)

        set_bone_rot(arm_obj, "UpperArm_R", -26.0 + 72.0 * env, -16, -15 * env)
        set_bone_rot(arm_obj, "Forearm_R", -34.0 + 10.0 * env, 0, 10)
        set_bone_rot(arm_obj, "Weapon_R", 16.0 + 38.0 * env, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", -18.0 + 65.0 * env, 16, 15 * env)
        set_bone_rot(arm_obj, "Forearm_L", -34.0 + 10.0 * env, 0, -10)

        lean = 18.0 * env + hunch_s + 26.0 * env + hunch_c + 22.0 * env
        set_cape_advanced(base_lift_world=14.0 + 42.0 * env, wave_amp_x=8.0, ph_x=3.0 * math.pi * t, wave_amp_lat=4.0, ph_lat=2.0 * math.pi * t, chest_lean_x=lean)
    record_anim("Dash", 18, 19, eval_dash)

    # 8. Attack_Melee (28 frames, 0.93s, 21 dense keys)
    def eval_attack_melee(t: float) -> None:
        dz    = _eval_pwl(t, [(0.0, 0.0), (0.30, -0.035), (0.50, -0.052), (0.72, -0.018), (1.0, 0.0)])
        yaw   = _eval_pwl(t, [(0.0, 0.0), (0.30, -46.0), (0.50, 44.0), (0.72, 26.0), (1.0, 0.0)])
        pitch = _eval_pwl(t, [(0.0, 0.0), (0.30, -12.0), (0.50, 22.0), (0.72, 10.0), (1.0, 0.0)])
        uar_x = _eval_pwl(t, [(0.0, -24.0), (0.30, -92.0), (0.50, -66.0), (0.72, -34.0), (1.0, -24.0)])
        uar_y = _eval_pwl(t, [(0.0, -10.0), (0.30, -42.0), (0.50, 32.0), (0.72, 18.0), (1.0, -10.0)])
        uar_z = _eval_pwl(t, [(0.0, -8.0), (0.30, -24.0), (0.50, 30.0), (0.72, 16.0), (1.0, -8.0)])
        far   = _eval_pwl(t, [(0.0, -34.0), (0.30, -52.0), (0.50, -12.0), (0.72, -20.0), (1.0, -34.0)])
        wr    = _eval_pwl(t, [(0.0, 14.0), (0.30, -42.0), (0.50, 64.0), (0.72, 40.0), (1.0, 14.0)])
        ual   = _eval_pwl(t, [(0.0, -18.0), (0.30, -44.0), (0.50, 26.0), (0.72, 12.0), (1.0, -18.0)])

        set_bone_loc(arm_obj, "Hips", Vector((0, 0, dz * H)))
        set_bone_rot(arm_obj, "Hips", pitch * 0.20, 0, yaw * 0.35)
        set_bone_rot(arm_obj, "Spine", hunch_s + pitch * 0.50, 0, yaw * 0.45)
        set_bone_rot(arm_obj, "Chest", hunch_c + pitch * 0.70, 0, yaw * 0.65)
        set_bone_rot(arm_obj, "Head", head_c - pitch * 0.30, 0, -yaw * 0.45)

        set_bone_rot(arm_obj, "UpperArm_R", uar_x, uar_y, uar_z)
        set_bone_rot(arm_obj, "Forearm_R", far, 0, 10)
        set_bone_rot(arm_obj, "Weapon_R", wr, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", ual, 16, 10)
        set_bone_rot(arm_obj, "Forearm_L", -38.0, 0, -10)

        lunge = max(0.0, yaw / 44.0)
        set_bone_rot(arm_obj, "Thigh_L", -5.0 - 20.0 * lunge, -4, 0)
        set_bone_rot(arm_obj, "Calf_L", 10.0 + 28.0 * lunge, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", 5.0 + 16.0 * lunge, 4, 0)
        set_bone_rot(arm_obj, "Calf_R", 14.0, 0, 0)

        lean = hunch_s + hunch_c + pitch * 1.4
        set_cape_advanced(base_lift_world=14.0 + 24.0 * lunge, wave_amp_x=6.0, ph_x=2.0 * math.pi * t, wave_amp_lat=12.0, ph_lat=math.pi * t, chest_lean_x=lean, torso_yaw=yaw * 0.65)
    record_anim("Attack_Melee", 28, 21, eval_attack_melee)

    # 9. Attack_Slam (38 frames, 1.27s, 23 dense keys)
    def eval_attack_slam(t: float) -> None:
        dz   = _eval_pwl(t, [(0.0, 0.0), (0.20, -0.10), (0.40, 0.095), (0.56, -0.165), (0.72, -0.12), (1.0, 0.0)])
        tp   = _eval_pwl(t, [(0.0, 0.0), (0.20, 16.0), (0.40, -26.0), (0.56, 42.0), (0.72, 30.0), (1.0, 0.0)])
        uar  = _eval_pwl(t, [(0.0, -22.0), (0.20, -56.0), (0.40, -145.0), (0.56, -58.0), (0.72, -46.0), (1.0, -22.0)])
        far  = _eval_pwl(t, [(0.0, -32.0), (0.20, -72.0), (0.40, -18.0), (0.56, -10.0), (0.72, -14.0), (1.0, -32.0)])
        wr   = _eval_pwl(t, [(0.0, 14.0), (0.20, -12.0), (0.40, 40.0), (0.56, 76.0), (0.72, 58.0), (1.0, 14.0)])
        knee = _eval_pwl(t, [(0.0, 10.0), (0.20, 46.0), (0.40, 12.0), (0.56, 60.0), (0.72, 46.0), (1.0, 10.0)])
        cape = _eval_pwl(t, [(0.0, 10.0), (0.20, 20.0), (0.40, 4.0), (0.56, 44.0), (0.72, 30.0), (1.0, 10.0)])

        set_bone_loc(arm_obj, "Hips", Vector((0, 0, dz * H)))
        set_bone_rot(arm_obj, "Spine", hunch_s + tp * 0.50, 0, 0)
        set_bone_rot(arm_obj, "Chest", hunch_c + tp * 0.70, 0, 0)
        set_bone_rot(arm_obj, "Head", head_c - tp * 0.25, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_R", uar, -8, 12)
        set_bone_rot(arm_obj, "Forearm_R", far, 0, 12)
        set_bone_rot(arm_obj, "Weapon_R", wr, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", uar, 8, -12)
        set_bone_rot(arm_obj, "Forearm_L", far, 0, -12)

        set_bone_rot(arm_obj, "Thigh_L", -knee * 0.65, -6, 0)
        set_bone_rot(arm_obj, "Calf_L", knee, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", -knee * 0.45, 6, 0)
        set_bone_rot(arm_obj, "Calf_R", knee * 0.90, 0, 0)

        lean = hunch_s + hunch_c + tp * 1.2
        set_cape_advanced(base_lift_world=cape, wave_amp_x=6.0, ph_x=2.0 * math.pi * t, wave_amp_lat=4.0, ph_lat=math.pi * t, chest_lean_x=lean)
    record_anim("Attack_Slam", 38, 23, eval_attack_slam)

    # 10. Hit_React (18 frames, 0.6s, 17 dense keys)
    def eval_hit_react(t: float) -> None:
        imp = _eval_pwl(t, [(0.0, 0.0), (0.22, 1.0), (0.55, 0.40), (1.0, 0.0)])
        apply_base_guard(breath=0.0)
        set_bone_loc(arm_obj, "Hips", Vector((0, 0.065 * H * imp, -0.048 * H * imp)))
        set_bone_rot(arm_obj, "Hips", -7.0 * imp, 0, 0)
        set_bone_rot(arm_obj, "Spine", hunch_s - 22.0 * imp, 8.0 * imp, -10.0 * imp)
        set_bone_rot(arm_obj, "Chest", hunch_c - 20.0 * imp, 6.0 * imp, -12.0 * imp)
        set_bone_rot(arm_obj, "Head", head_c - 26.0 * imp, 0, 14.0 * imp)
        set_bone_rot(arm_obj, "UpperArm_L", -18.0 - 12.0 * imp, 10 + 30.0 * imp, 12 + 18.0 * imp)
        set_bone_rot(arm_obj, "UpperArm_R", -24.0 + 10.0 * imp, -10 - 30.0 * imp, -10 - 18.0 * imp)
        set_bone_rot(arm_obj, "Thigh_L", -4.0 - 14.0 * imp, -5, 0)
        set_bone_rot(arm_obj, "Calf_L", 8.0 + 20.0 * imp, 0, 0)
        lean = -7.0 * imp + hunch_s - 22.0 * imp + hunch_c - 20.0 * imp
        set_cape_advanced(base_lift_world=8.0 - 16.0 * imp, wave_amp_x=4.0, ph_x=2.0 * math.pi * t, wave_amp_lat=4.0, ph_lat=math.pi * t, chest_lean_x=lean)
    record_anim("Hit_React", 18, 17, eval_hit_react)

    # 11. Downed_Loop (48 frames, 1.6s loop, 21 dense keys)
    def _set_downed_pose(breath: float) -> None:
        set_bone_loc(arm_obj, "Hips", Vector((0, 0, (-0.245 + 0.008 * breath) * H)))
        set_bone_rot(arm_obj, "Hips", 9.0, 0, 0)
        set_bone_rot(arm_obj, "Spine", 20.0 + breath * 2.8, 0, 0)
        set_bone_rot(arm_obj, "Chest", 24.0 + breath * 4.2, 0, 0)
        set_bone_rot(arm_obj, "Head", 26.0 - breath * 2.6, 0, 0)
        set_bone_rot(arm_obj, "Thigh_L", -80.0, -6.0, 0)
        set_bone_rot(arm_obj, "Calf_L", 82.0, 0, 0)
        set_bone_rot(arm_obj, "Foot_L", -4.0, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", 14.0, 5.0, 0)
        set_bone_rot(arm_obj, "Calf_R", 94.0, 0, 0)
        set_bone_rot(arm_obj, "Foot_R", 26.0, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_R", -44.0 - breath * 2.0, -14.0, -10.0)
        set_bone_rot(arm_obj, "Forearm_R", -18.0, 0, 8.0)
        set_bone_rot(arm_obj, "Weapon_R", 22.0, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", -50.0 + breath * 2.0, 12.0, 15.0)
        set_bone_rot(arm_obj, "Forearm_L", -66.0, 0, -24.0)
        lean = 53.0 + breath * 7.0
        set_cape_advanced(base_lift_world=16.0, wave_amp_x=2.5, ph_x=2.0 * math.pi * breath, wave_amp_lat=2.0, ph_lat=0.0, chest_lean_x=lean)

    def eval_downed_loop(t: float) -> None:
        _set_downed_pose(math.sin(2.0 * math.pi * t))
    record_anim("Downed_Loop", 48, 21, eval_downed_loop)

    # 12. Revive_Channel (36 frames, 1.2s loop, 19 dense keys)
    def eval_revive_channel(t: float) -> None:
        pulse = math.sin(2.0 * math.pi * t)
        set_bone_loc(arm_obj, "Hips", Vector((0, 0, (-0.080 + 0.014 * pulse) * H)))
        set_bone_rot(arm_obj, "Hips", 6.0, 0, 0)
        set_bone_rot(arm_obj, "Spine", hunch_s + 15.0, 0, 0)
        set_bone_rot(arm_obj, "Chest", hunch_c + 15.0 + 3.8 * pulse, 0, 0)
        set_bone_rot(arm_obj, "Head", 20.0 - 2.5 * pulse, 0, 0)
        set_bone_rot(arm_obj, "Thigh_L", -28.0, -4, 0)
        set_bone_rot(arm_obj, "Calf_L", 40.0, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", 16.0, 4, 0)
        set_bone_rot(arm_obj, "Calf_R", 26.0, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", -74.0 + 9.5 * pulse, 10, -10)
        set_bone_rot(arm_obj, "Forearm_L", -18.0 + 4.5 * math.cos(2.0 * math.pi * t), 0, 0)
        set_bone_rot(arm_obj, "UpperArm_R", -56.0 + 7.0 * pulse, -10, 8)
        set_bone_rot(arm_obj, "Forearm_R", -26.0, 0, 0)
        set_bone_rot(arm_obj, "Weapon_R", 40.0, 0, 0)
        lean = 6.0 + hunch_s + 15.0 + hunch_c + 15.0 + 3.8 * pulse
        set_cape_advanced(base_lift_world=24.0, wave_amp_x=6.0, ph_x=2.0 * math.pi * t, wave_amp_lat=4.0, ph_lat=math.pi * t, chest_lean_x=lean)
    record_anim("Revive_Channel", 36, 19, eval_revive_channel)

    # 13. Revive_Rise (30 frames, 1.0s, 19 dense keys)
    def eval_revive_rise(t: float) -> None:
        dz = _eval_pwl(t, [(0.0, -0.245), (0.45, -0.105), (0.78, -0.018), (1.0, 0.0)])
        hip_p = _eval_pwl(t, [(0.0, 9.0), (0.45, 10.0), (1.0, 0.0)])
        sp_p  = _eval_pwl(t, [(0.0, 20.0), (0.45, hunch_s + 14.0), (1.0, hunch_s)])
        ch_p  = _eval_pwl(t, [(0.0, 24.0), (0.45, hunch_c + 12.0), (1.0, hunch_c)])
        hd_p  = _eval_pwl(t, [(0.0, 26.0), (0.45, head_c + 6.0), (1.0, head_c)])
        th_l  = _eval_pwl(t, [(0.0, -80.0), (0.45, -36.0), (1.0, -4.0)])
        ca_l  = _eval_pwl(t, [(0.0, 82.0), (0.45, 46.0), (1.0, 8.0)])
        th_r  = _eval_pwl(t, [(0.0, 14.0), (0.45, 8.0), (1.0, -2.0)])
        ca_r  = _eval_pwl(t, [(0.0, 94.0), (0.45, 34.0), (1.0, 8.0)])
        uar_r = _eval_pwl(t, [(0.0, -44.0), (0.45, -34.0), (1.0, -28.0 if arch in ("archmage", "lich") else -22.0)])
        far_r = _eval_pwl(t, [(0.0, -18.0), (0.45, -34.0), (1.0, -38.0 if arch in ("archmage", "lich") else -32.0)])
        uar_l = _eval_pwl(t, [(0.0, -50.0), (0.45, -32.0), (1.0, -18.0)])
        far_l = _eval_pwl(t, [(0.0, -66.0), (0.45, -42.0), (1.0, -35.0)])

        set_bone_loc(arm_obj, "Hips", Vector((0, 0, dz * H)))
        set_bone_rot(arm_obj, "Hips", hip_p, 0, 0)
        set_bone_rot(arm_obj, "Spine", sp_p, 0, 0)
        set_bone_rot(arm_obj, "Chest", ch_p, 0, 0)
        set_bone_rot(arm_obj, "Head", hd_p, 0, 0)
        set_bone_rot(arm_obj, "Thigh_L", th_l, -4, 0)
        set_bone_rot(arm_obj, "Calf_L", ca_l, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", th_r, 4, 0)
        set_bone_rot(arm_obj, "Calf_R", ca_r, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_R", uar_r, -12, -10)
        set_bone_rot(arm_obj, "Forearm_R", far_r, 0, 12)
        set_bone_rot(arm_obj, "Weapon_R", 18, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", uar_l, 12, 12)
        set_bone_rot(arm_obj, "Forearm_L", far_l, 0, -12)
        set_cape_advanced(base_lift_world=16.0, wave_amp_x=4.0, ph_x=2.0 * math.pi * t, wave_amp_lat=3.0, ph_lat=math.pi * t, chest_lean_x=hip_p + sp_p + ch_p)
    record_anim("Revive_Rise", 30, 19, eval_revive_rise)

    # 14. Death (46 frames, 1.53s, 23 dense keys)
    def eval_death(t: float) -> None:
        dz = _eval_pwl(t, [(0.0, 0.0), (0.20, -0.095), (0.46, -0.26), (0.76, -0.42), (0.88, -0.38), (1.0, -0.42)])
        dy = _eval_pwl(t, [(0.0, 0.0), (0.20, 0.045), (0.46, 0.015), (0.76, -0.105), (1.0, -0.115)])
        hx = _eval_pwl(t, [(0.0, 0.0), (0.20, -10.0), (0.46, 24.0), (0.76, 86.0), (0.88, 82.0), (1.0, 86.0)])
        hz = _eval_pwl(t, [(0.0, 0.0), (0.20, -4.0), (0.46, 4.0), (1.0, 8.0)])
        sp = _eval_pwl(t, [(0.0, hunch_s), (0.20, -14.0), (0.46, 18.0), (0.76, 4.0), (1.0, 4.0)])
        ch = _eval_pwl(t, [(0.0, hunch_c), (0.20, -16.0), (0.46, 18.0), (0.76, 4.0), (1.0, 4.0)])
        hd = _eval_pwl(t, [(0.0, head_c), (0.20, -26.0), (0.46, 22.0), (0.76, -12.0), (1.0, -14.0)])
        th = _eval_pwl(t, [(0.0, -4.0), (0.20, -22.0), (0.46, -4.0), (0.76, 6.0), (1.0, 5.0)])
        ca = _eval_pwl(t, [(0.0, 8.0), (0.20, 42.0), (0.46, 96.0), (0.76, 10.0), (1.0, 12.0)])
        ual_x = _eval_pwl(t, [(0.0, -18.0), (0.20, -26.0), (0.46, -16.0), (0.76, -68.0), (1.0, -66.0)])
        uar_x = _eval_pwl(t, [(0.0, -26.0), (0.20, -20.0), (0.46, -16.0), (0.76, -58.0), (1.0, -56.0)])

        set_bone_loc(arm_obj, "Hips", Vector((0, dy * H, dz * H)))
        set_bone_rot(arm_obj, "Hips", hx, 0, hz)
        set_bone_rot(arm_obj, "Spine", sp, 6.0 if t < 0.4 else 0.0, 0)
        set_bone_rot(arm_obj, "Chest", ch, 8.0 if t < 0.4 else 0.0, 0)
        set_bone_rot(arm_obj, "Head", hd, 20.0 if t > 0.6 else 0.0, 8.0 if t < 0.4 else 0.0)
        set_bone_rot(arm_obj, "Thigh_L", th, -7.0, 0)
        set_bone_rot(arm_obj, "Calf_L", ca, 0, 0)
        set_bone_rot(arm_obj, "Thigh_R", th, 7.0, 0)
        set_bone_rot(arm_obj, "Calf_R", ca, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_L", ual_x, 38.0, 18.0)
        set_bone_rot(arm_obj, "Forearm_L", -18.0, 0, 0)
        set_bone_rot(arm_obj, "UpperArm_R", uar_x, -38.0, -18.0)
        set_bone_rot(arm_obj, "Forearm_R", -16.0, 0, 0)
        set_bone_rot(arm_obj, "Weapon_R", 16.0, 0, 0)
        set_bone_rot(arm_obj, "Cape_1", -12.0 if t > 0.5 else -4.0, 0, 0)
        set_bone_rot(arm_obj, "Cape_2", -4.0, 0, 0)
        set_bone_rot(arm_obj, "Cape_3", -2.0, 0, 0)
    record_anim("Death", 46, 23, eval_death)

    return created


# ---------------------------------------------------------------------------
# 5 Diablo IV Character Specifications
# ---------------------------------------------------------------------------

CHARACTER_SPECS: List[dict] = [
    {
        "filename": "hero_archmage.glb",
        "archetype": "archmage",
        "height": 1.88,
        "width_scale": 0.96,
        "hunch_y": 0.0,
        "armor_tex": "d4_hammered_steel",
        "armor_tint": (0.24, 0.28, 0.40),
        "coat": 0.35,
        "cloth_tint": (0.14, 0.16, 0.30),
        "accent_tex": "d4_gilded_brass",
        "accent_tint": (0.94, 0.80, 0.42),
        "glow_color": (0.20, 0.88, 1.00),
        "glow_strength": 6.5,
        "weapon_tint": (0.55, 0.62, 0.74),
        "rune_base_tint": (0.38, 0.22, 0.26),
        "rune_strength": 3.2,
    },
    {
        "filename": "mob_swarm_reaper.glb",
        "archetype": "reaper",
        "height": 1.72,
        "width_scale": 0.88,
        "hunch_y": -0.095,
        "armor_tex": "d4_hammered_steel",
        "armor_tint": (0.26, 0.22, 0.22),
        "coat": 0.15,
        "cloth_tint": (0.24, 0.09, 0.11),
        "accent_tex": "d4_grim_bone",
        "accent_tint": (0.86, 0.80, 0.68),
        "glow_color": (1.00, 0.26, 0.06),
        "glow_strength": 6.5,
        "weapon_tint": (0.48, 0.44, 0.44),
        "rune_base_tint": (0.28, 0.22, 0.22),
        "rune_strength": 3.5,
    },
    {
        "filename": "mob_ranged_lich.glb",
        "archetype": "lich",
        "height": 1.96,
        "width_scale": 0.90,
        "hunch_y": -0.025,
        "armor_tex": "d4_hammered_steel",
        "armor_tint": (0.22, 0.28, 0.26),
        "coat": 0.20,
        "cloth_tint": (0.18, 0.11, 0.28),
        "accent_tex": "d4_grim_bone",
        "accent_tint": (0.78, 0.78, 0.66),
        "glow_color": (0.22, 1.00, 0.54),
        "glow_strength": 6.8,
        "weapon_tint": (0.36, 0.44, 0.40),
        "rune_base_tint": (0.22, 0.28, 0.26),
        "rune_strength": 4.0,
    },
    {
        "filename": "mob_bruiser_juggernaut.glb",
        "archetype": "juggernaut",
        "height": 2.62,
        "width_scale": 1.24,
        "hunch_y": -0.028,
        "armor_tex": "d4_hammered_steel",
        "armor_tint": (0.36, 0.34, 0.35),
        "coat": 0.22,
        "cloth_tint": (0.34, 0.09, 0.08),
        "accent_tex": "d4_gilded_brass",
        "accent_tint": (0.78, 0.58, 0.32),
        "glow_color": (1.00, 0.42, 0.08),
        "glow_strength": 6.4,
        "weapon_tint": (0.34, 0.32, 0.33),
        "rune_base_tint": (0.32, 0.26, 0.24),
        "rune_strength": 3.8,
    },
    {
        "filename": "mob_boss_archon.glb",
        "archetype": "archon",
        "height": 3.75,
        "width_scale": 1.18,
        "hunch_y": -0.020,
        "armor_tex": "d4_obsidian_carapace",
        "armor_tint": (0.24, 0.22, 0.30),
        "coat": 0.45,
        "cloth_tint": (0.38, 0.08, 0.14),
        "accent_tex": "d4_gilded_brass",
        "accent_tint": (0.96, 0.76, 0.30),
        "glow_color": (0.28, 0.92, 1.00),
        "glow_strength": 8.0,
        "weapon_tint": (0.42, 0.46, 0.56),
        "rune_base_tint": (0.30, 0.26, 0.34),
        "rune_strength": 5.5,
    },
]


# ---------------------------------------------------------------------------
# Binary GLB JSON Post-Processing & Full Verification
# ---------------------------------------------------------------------------

def postprocess_and_verify_glb(glb_path: Path, expected_bones: int, expected_height: float) -> dict:
    """Normalize GLB animation names strictly to CANONICAL_ANIMS and verify geometry/rig/PBR."""
    raw = glb_path.read_bytes()
    magic, version, total_len = struct.unpack("<4sII", raw[:12])
    if magic != b"glTF" or version != 2:
        raise RuntimeError(f"Invalid GLB header in {glb_path}")

    json_len, json_type = struct.unpack("<II", raw[12:20])
    assert json_type == 0x4E4F534A
    json_bytes = raw[20:20 + json_len]
    rest_chunks = raw[20 + json_len:]
    j = json.loads(json_bytes.decode("utf-8"))

    anims = j.get("animations", [])
    canon_lookup = set(CANONICAL_ANIMS)
    modified = False
    for a in anims:
        nm = str(a.get("name", ""))
        clean = nm
        for sfx in ("_Armature", "_Rig", "Action"):
            if clean.endswith(sfx):
                clean = clean[:-len(sfx)]
        if "." in clean and clean.split(".", 1)[0] in canon_lookup:
            clean = clean.split(".", 1)[0]
        if clean != nm:
            a["name"] = clean
            modified = True

    name_to_idx = {name: idx for idx, name in enumerate(CANONICAL_ANIMS)}
    if len(anims) == len(CANONICAL_ANIMS) and all(a.get("name") in name_to_idx for a in anims):
        sorted_anims = sorted(anims, key=lambda a: name_to_idx[a["name"]])
        if [a["name"] for a in sorted_anims] != [a["name"] for a in anims]:
            j["animations"] = sorted_anims
            modified = True

    if modified:
        new_json = json.dumps(j, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
        pad = (4 - (len(new_json) % 4)) % 4
        new_json += b" " * pad
        new_total = 12 + 8 + len(new_json) + len(rest_chunks)
        new_raw = (
            struct.pack("<4sII", b"glTF", 2, new_total)
            + struct.pack("<II", len(new_json), 0x4E4F534A)
            + new_json
            + rest_chunks
        )
        glb_path.write_bytes(new_raw)

    anim_names = [a.get("name") for a in j.get("animations", [])]
    if anim_names != CANONICAL_ANIMS:
        raise RuntimeError(f"{glb_path.name}: animation mismatch {anim_names} != {CANONICAL_ANIMS}")

    skins = j.get("skins", [])
    joints = len(skins[0].get("joints", [])) if skins else 0
    if joints != expected_bones:
        raise RuntimeError(f"{glb_path.name}: expected {expected_bones} bones, got {joints}")

    vert_count = 0
    min_y, max_y = 1e9, -1e9
    min_z, max_z = 1e9, -1e9
    for m in j.get("meshes", []):
        for pr in m.get("primitives", []):
            pos_idx = pr.get("attributes", {}).get("POSITION")
            if pos_idx is not None:
                acc = j["accessors"][pos_idx]
                vert_count += int(acc["count"])
                if "min" in acc and "max" in acc:
                    min_y = min(min_y, acc["min"][1])
                    max_y = max(max_y, acc["max"][1])
                    min_z = min(min_z, acc["min"][2])
                    max_z = max(max_z, acc["max"][2])

    if not (4000 <= vert_count <= 12000):
        raise RuntimeError(f"{glb_path.name}: vertex count {vert_count} outside [4000, 12000]")
    if abs(min_y) > 0.005:
        raise RuntimeError(f"{glb_path.name}: min_y={min_y:.4f} (feet not at ground Y=0!)")
    if abs(max_z) <= abs(min_z) * 0.1:
        raise RuntimeError(f"{glb_path.name}: suspicious Z bounds [{min_z:.3f}, {max_z:.3f}]")

    mats = j.get("materials", [])
    imgs = j.get("images", [])
    return {
        "file": glb_path.name,
        "bytes": glb_path.stat().st_size,
        "verts": vert_count,
        "bones": joints,
        "materials": len(mats),
        "images": len(imgs),
        "height_m": round(max_y - min_y, 3),
        "min_y": round(min_y, 5),
        "forward_z": round(max_z, 3),
        "anims": anim_names,
    }


def build_character(spec: dict) -> dict:
    bpy.ops.wm.read_factory_settings(use_empty=True)
    arm_obj, bone_segments = build_armature_and_spec(spec)
    acc = build_character_geometry(spec, bone_segments)
    mesh_obj, vert_tags = acc.build_object(f"{spec['archetype']}_Mesh")

    verts = mesh_obj.data.vertices
    min_z = min(v.co.z for v in verts)
    if abs(min_z) > 1e-5:
        for v in verts:
            v.co.z -= min_z
        bpy.context.view_layer.objects.active = arm_obj
        bpy.ops.object.mode_set(mode="EDIT")
        for eb in arm_obj.data.edit_bones:
            eb.head.z -= min_z
            eb.tail.z -= min_z
            bone_segments[eb.name] = (eb.head.copy(), eb.tail.copy())
        bpy.ops.object.mode_set(mode="OBJECT")

    bind_and_weight_mesh(mesh_obj, arm_obj, bone_segments, vert_tags, spec["height"])
    build_all_animations(arm_obj, spec)

    import time

    stage_dir = SCRIPT_DIR / "_cache" / "staging"
    stage_dir.mkdir(parents=True, exist_ok=True)
    stage_out = stage_dir / spec["filename"]
    bpy.ops.export_scene.gltf(
        filepath=str(stage_out),
        export_format="GLB",
        export_yup=True,
        export_apply=False,
        export_texcoords=True,
        export_normals=True,
        export_materials="EXPORT",
        export_image_format="AUTO",
        export_animations=True,
        export_animation_mode="NLA_TRACKS",
        export_nla_strips=True,
        export_def_bones=True,
        export_bake_animation=True,
        export_anim_slide_to_zero=True,
        export_optimize_animation_size=False,
    )

    stats = postprocess_and_verify_glb(stage_out, len(bone_segments), spec["height"])
    raw_bytes = stage_out.read_bytes()
    for out_dir in OUT_DIRS:
        out_dir.mkdir(parents=True, exist_ok=True)
        dst = out_dir / spec["filename"]
        for attempt in range(12):
            try:
                dst.write_bytes(raw_bytes)
                break
            except PermissionError:
                if attempt == 11:
                    raise
                time.sleep(0.3)
        postprocess_and_verify_glb(dst, len(bone_segments), spec["height"])

    return stats


def render_walk_fwd_overhaul_validation(output_path: Path) -> Path:
    """Render a comprehensive Full HD (1920x1080) verification contact sheet for Run_Fwd.
    Top row: Hero Cinematic 3/4 Action Shot at peak flight stride (1920x600).
    Bottom row: 5 Biomechanical Phase Breakdown Panels (each 384x476):
      Panel 1: Contact (Heel strike, dorsiflexion, forward reach)
      Panel 2: Down (Shock compression, knee flex, pelvic drop, Trendelenburg tilt)
      Panel 3: Passing (Mid-stance pass-through, opposite knee tuck)
      Panel 4: Up (Extension push-off, apex pelvis rise, powerful plantarflexion)
      Panel 5: Follow-Through (Deceleration & pendulum reach)
    """
    output_path.parent.mkdir(parents=True, exist_ok=True)
    hero_glb = OUT_DIRS[0] / "hero_archmage.glb"
    if not hero_glb.exists():
        hero_glb = SCRIPT_DIR / "_cache" / "staging" / "hero_archmage.glb"

    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.gltf(filepath=str(hero_glb))

    arm = [o for o in bpy.data.objects if o.type == "ARMATURE"][0]
    act = [a for a in bpy.data.actions if "Run_Fwd" in a.name][0]
    arm.animation_data.action = act

    # Dark basalt stone ground plane
    bpy.ops.mesh.primitive_plane_add(size=18, location=(0, 0, 0))
    floor = bpy.context.object
    fmat = bpy.data.materials.new("DarkBasaltFloor")
    fmat.use_nodes = True
    bsdf = fmat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = (0.035, 0.035, 0.042, 1.0)
        bsdf.inputs["Roughness"].default_value = 0.62
    floor.data.materials.append(fmat)

    # 3-Point Dark Fantasy Cinematic Lighting
    bpy.ops.object.light_add(type="SUN", location=(-2.8, -4.6, 4.2), rotation=(1.10, 0.15, -0.45))
    sun_key = bpy.context.object
    sun_key.data.energy = 5.2
    sun_key.data.color = (0.74, 0.86, 1.00)

    bpy.ops.object.light_add(type="SUN", location=(2.8, 4.2, 3.6), rotation=(-0.95, -0.20, 2.70))
    sun_rim = bpy.context.object
    sun_rim.data.energy = 8.0
    sun_rim.data.color = (1.00, 0.72, 0.32)

    bpy.ops.object.light_add(type="POINT", location=(0.0, -2.4, 0.9))
    fill = bpy.context.object
    fill.data.energy = 75.0
    fill.data.color = (0.16, 0.20, 0.32)

    bpy.context.scene.render.engine = "BLENDER_EEVEE_NEXT"

    # Camera 1: Hero Cinematic 3/4 Perspective
    bpy.ops.object.camera_add(location=(-1.85, -3.65, 1.35), rotation=(1.38, 0.0, -0.46))
    cam_hero = bpy.context.object
    cam_hero.data.lens = 52.0

    # Camera 2: Biomechanical Analysis Panels
    bpy.ops.object.camera_add(location=(-1.35, -3.10, 1.08), rotation=(1.43, 0.0, -0.40))
    cam_panel = bpy.context.object
    cam_panel.data.lens = 46.0

    # Render Hero Shot at Up/Apex push-off (frame 11)
    bpy.context.scene.camera = cam_hero
    bpy.context.scene.render.resolution_x = 1920
    bpy.context.scene.render.resolution_y = 600
    tmp_hero = SCRIPT_DIR / "_cache" / "tmp_hero_shot.png"
    bpy.context.scene.render.filepath = str(tmp_hero)
    bpy.context.scene.frame_set(11)
    bpy.ops.render.render(write_still=True)

    img_h = bpy.data.images.load(str(tmp_hero))
    arr_h = np.array(img_h.pixels).reshape((600, 1920, 4))
    hero_rgb = (np.clip(arr_h[::-1, :, :3], 0.0, 1.0) * 255.0).astype(np.uint8)
    bpy.data.images.remove(img_h)
    tmp_hero.unlink(missing_ok=True)

    # Render 5 Phase Panels (each 384x480)
    bpy.context.scene.camera = cam_panel
    bpy.context.scene.render.resolution_x = 384
    bpy.context.scene.render.resolution_y = 480

    phase_frames = [1, 4, 7, 11, 16]
    panel_rgbs = []
    for fr in phase_frames:
        bpy.context.scene.frame_set(fr)
        tmp_p = SCRIPT_DIR / "_cache" / f"tmp_panel_{fr}.png"
        bpy.context.scene.render.filepath = str(tmp_p)
        bpy.ops.render.render(write_still=True)
        img_p = bpy.data.images.load(str(tmp_p))
        arr_p = np.array(img_p.pixels).reshape((480, 384, 4))
        p_rgb = (np.clip(arr_p[::-1, :, :3], 0.0, 1.0) * 255.0).astype(np.uint8)
        p_rgb[:, :2, :] = [45, 50, 65]
        p_rgb[:, -2:, :] = [45, 50, 65]
        panel_rgbs.append(p_rgb)
        bpy.data.images.remove(img_p)
        tmp_p.unlink(missing_ok=True)

    bottom_row = np.hstack(panel_rgbs)

    divider = np.zeros((4, 1920, 3), dtype=np.uint8)
    divider[:, :] = [180, 145, 65]

    composite = np.vstack([hero_rgb, divider, bottom_row[:476, :, :]])
    _write_png_rgb(output_path, composite)
    print(f"[RENDER] Verified 1920x1080 gait overhaul validation written to {output_path} ({output_path.stat().st_size} bytes)")
    return output_path


def main() -> int:
    ensure_d4_2k_textures()
    all_stats = []
    for spec in CHARACTER_SPECS:
        print(f"=== Sculpting & Rigging {spec['filename']} (target {spec['height']}m) ===")
        st = build_character(spec)
        all_stats.append(st)
        print(
            f"[VERIFIED] {st['file']}: verts={st['verts']} | bones={st['bones']} | "
            f"mats={st['materials']} | imgs={st['images']} | height={st['height_m']}m | "
            f"min_y={st['min_y']} | fwd_+Z={st['forward_z']} | bytes={st['bytes']} | "
            f"anims={len(st['anims'])} {st['anims']}"
        )

    print("\n=== ALL 5 GRIMDARK CHARACTERS VERIFIED IDENTICALLY IN BOTH TARGETS ===")
    for d in OUT_DIRS:
        print(f"Target dir: {d}")
        for st in all_stats:
            p = d / st["file"]
            assert p.exists() and p.stat().st_size == st["bytes"], f"Mismatch at {p}"

    # Render verification still of gait overhaul
    val_img = Path(r"C:\Users\tagir\.gemini\antigravity\brain\e1af9820-2ab0-45d5-8cb4-364efea981db\blender_walk_fwd_overhaul.png")
    print(f"\n=== Rendering Gait Overhaul Validation Sheet -> {val_img} ===")
    render_walk_fwd_overhaul_validation(val_img)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
