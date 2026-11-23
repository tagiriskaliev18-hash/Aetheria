"""
Dark Fantasy Painted Arcanum Illustrations for the 10 Aetheria Spells.
Generates rich atmospheric spell iconography & oil-painted chiaroscuro plaques (512x512)
into BOTH RawAssets/Grimoire/SpellPlate_01..10.png and RawAssets/Textures/SpellPlate_01..10.png.
Zero cheap primitives, zero white Voronoi soccer balls, zero plastic tubes.
"""

import math
import os
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
OUT_DIRS = [ROOT / "RawAssets" / "Grimoire"]
SIZE = 512

# Normalized coordinate grid in [-1, 1]
_lin = np.linspace(-1.0, 1.0, SIZE, dtype=np.float32)
GX, GY = np.meshgrid(_lin, _lin)
R_GRID = np.sqrt(GX * GX + GY * GY)
ANG_GRID = np.arctan2(GY, GX)


def value_noise(px: np.ndarray, py: np.ndarray, res: int, seed: int) -> np.ndarray:
    rng = np.random.default_rng(seed)
    grid = rng.random((res + 2, res + 2), dtype=np.float32)
    # Periodic wrap avoids clamped edge bands
    u = np.mod((px * 0.5 + 0.5) * res, float(res))
    v = np.mod((py * 0.5 + 0.5) * res, float(res))
    ix = np.floor(u).astype(np.int32)
    iy = np.floor(v).astype(np.int32)
    fx = u - ix
    fy = v - iy
    fx = fx * fx * fx * (fx * (fx * 6.0 - 15.0) + 10.0)
    fy = fy * fy * fy * (fy * (fy * 6.0 - 15.0) + 10.0)
    n00 = grid[iy, ix]
    n10 = grid[iy, ix + 1]
    n01 = grid[iy + 1, ix]
    n11 = grid[iy + 1, ix + 1]
    nx0 = n00 * (1.0 - fx) + n10 * fx
    nx1 = n01 * (1.0 - fx) + n11 * fx
    return nx0 * (1.0 - fy) + nx1 * fy


def fbm(px: np.ndarray, py: np.ndarray, seed: int, octaves: int = 5, base_res: int = 6) -> np.ndarray:
    acc = np.zeros_like(px, dtype=np.float32)
    amp = 0.5
    norm = 0.0
    freq = 1.0
    cx, cy = px, py
    # Rotate each octave by 37 deg so zero axis-aligned banding ever appears!
    cs, sn = math.cos(0.645), math.sin(0.645)
    for o in range(octaves):
        acc += amp * value_noise(cx * freq, cy * freq, max(5, int(base_res * freq)), seed + o * 101)
        cx, cy = cx * cs - cy * sn, cx * sn + cy * cs
        norm += amp
        amp *= 0.52
        freq *= 1.92
    return acc / max(norm, 1e-6)


def new_obsidian_canvas(seed: int, tint=(0.05, 0.04, 0.09)) -> np.ndarray:
    canvas = np.zeros((SIZE, SIZE, 3), dtype=np.float32)
    mist = fbm(GX * 0.85, GY * 0.85, seed, octaves=4, base_res=5)
    vign = np.clip(1.18 - R_GRID * 0.72, 0.08, 1.0)
    bg_base = np.array([0.022, 0.020, 0.032], dtype=np.float32)
    tint_arr = np.array(tint, dtype=np.float32)
    for c in range(3):
        canvas[:, :, c] = (bg_base[c] + tint_arr[c] * (mist * 0.9 + 0.15)) * vign
    return canvas


def add_glow(canvas: np.ndarray, mask: np.ndarray, color, strength: float = 1.0):
    m = np.clip(mask, 0.0, 2.0) * strength
    for c in range(3):
        canvas[:, :, c] += m * color[c]


def paint_lightning_bolt(img_acc: np.ndarray, p0, p1, seed: int, width_px: float, color, depth: int = 0):
    rng = np.random.default_rng(seed)
    pts = [np.array(p0, dtype=np.float32), np.array(p1, dtype=np.float32)]
    disp = 0.22 if depth == 0 else 0.10
    for _ in range(5):
        new_pts = [pts[0]]
        for i in range(len(pts) - 1):
            a, b = pts[i], pts[i + 1]
            mid = 0.5 * (a + b)
            seg = b - a
            perp = np.array([-seg[1], seg[0]], dtype=np.float32)
            plen = np.linalg.norm(perp)
            if plen > 1e-5:
                perp /= plen
            offset = (rng.random() - 0.5) * 2.0 * disp * np.linalg.norm(seg)
            new_pts.append(mid + perp * offset)
            new_pts.append(b)
        pts = new_pts
        disp *= 0.62

    # Rasterize onto SDF distance field
    min_d2 = np.full((SIZE, SIZE), 1e4, dtype=np.float32)
    for i in range(len(pts) - 1):
        ax, ay = pts[i]
        bx, by = pts[i + 1]
        vx, vy = bx - ax, by - ay
        vlen2 = max(vx * vx + vy * vy, 1e-6)
        t = np.clip(((GX - ax) * vx + (GY - ay) * vy) / vlen2, 0.0, 1.0)
        dx = GX - (ax + t * vx)
        dy = GY - (ay + t * vy)
        d2 = dx * dx + dy * dy
        min_d2 = np.minimum(min_d2, d2)

    d = np.sqrt(min_d2)
    w = width_px / (SIZE * 0.5)
    core = np.exp(- (d / (w * 0.55)) ** 2)
    halo = np.exp(- (d / (w * 4.2)) ** 1.35) * 0.65
    add_glow(img_acc, halo, color, 1.15)
    add_glow(img_acc, core, (1.0, 0.99, 0.96), 1.65)

    if depth == 0:
        for bi in range(4, len(pts) - 4, 6):
            if rng.random() < 0.7:
                ang = rng.uniform(-1.1, 1.1)
                step = pts[bi + 1] - pts[bi]
                c, s = math.cos(ang), math.sin(ang)
                bdir = np.array([step[0] * c - step[1] * s, step[0] * s + step[1] * c])
                blen = max(np.linalg.norm(bdir), 1e-5)
                branch_end = pts[bi] + (bdir / blen) * rng.uniform(0.18, 0.36)
                paint_lightning_bolt(img_acc, pts[bi], branch_end, seed + 500 + bi, width_px * 0.45, color, depth=1)


def apply_frame_and_tonemap(canvas: np.ndarray, accent_rgb, seed: int) -> Image.Image:
    # Subtle gothic astrolabe hairline etching in periphery
    ring1 = np.exp(-((R_GRID - 0.84) / 0.0055) ** 2) * 0.28
    ring2 = np.exp(-((R_GRID - 0.78) / 0.0035) ** 2) * 0.16
    tick_mask = np.where((np.abs(np.sin(ANG_GRID * 12.0)) > 0.992) & (R_GRID > 0.77) & (R_GRID < 0.85), 0.25, 0.0)
    etched = (ring1 + ring2 + tick_mask) * np.clip((R_GRID - 0.50) * 2.2, 0.0, 1.0)
    add_glow(canvas, etched, (0.58, 0.44, 0.24), 0.75)

    # Subtle ambient arcane embers
    rng = np.random.default_rng(seed + 77)
    for _ in range(38):
        ex = rng.uniform(-0.88, 0.88)
        ey = rng.uniform(-0.88, 0.88)
        er = rng.uniform(0.005, 0.014)
        ed = np.sqrt((GX - ex) ** 2 + (GY - ey) ** 2)
        spark = np.exp(-(ed / er) ** 2) * rng.uniform(0.35, 0.95)
        add_glow(canvas, spark, accent_rgb, 0.9)

    # ACES-like filmic tone curve + crisp obsidian grain
    x = np.nan_to_num(canvas, nan=0.0, posinf=4.0, neginf=0.0)
    x = np.maximum(x, 0.0)
    mapped = (x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14)
    mapped = np.clip(mapped, 0.0, 1.0)
    rgb = (np.power(mapped, 1.0 / 1.12) * 255.0).astype(np.uint8)
    img = Image.fromarray(rgb, mode="RGB").convert("RGBA")

    # Subtle forged bronze double-hairline museum border
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, SIZE - 1, SIZE - 1], outline=(24, 18, 14, 255), width=4)
    draw.rectangle([4, 4, SIZE - 5, SIZE - 5], outline=(140, 108, 62, 210), width=1)
    draw.rectangle([8, 8, SIZE - 9, SIZE - 9], outline=(62, 48, 34, 160), width=1)
    return img


# ==============================================================================
# Individual 10 Dark Fantasy Arcanum Paintings
# ==============================================================================

def render_01_fireball():
    c = new_obsidian_canvas(101, tint=(0.18, 0.04, 0.02))
    # Domain-warped turbulent inferno wake moving upper-right -> lower-left or left -> center
    wx = fbm(GX * 1.6, GY * 1.6, 111, 5, 6) - 0.5
    wy = fbm(GX * 1.6 + 2.3, GY * 1.6 - 1.7, 112, 5, 6) - 0.5
    nx, ny = GX + wx * 0.42, GY + wy * 0.42

    # Comet tail smeared toward top-left
    cx, cy = 0.08, 0.04
    tail_t = np.clip((cx - GX) * 0.72 + (cy - GY) * 0.35, 0.0, 1.4)
    dist_core = np.sqrt((nx - cx) ** 2 + (ny - cy) ** 2)
    dist_tail = np.sqrt((GX - cx + tail_t * 0.62) ** 2 + (GY - cy + tail_t * 0.28) ** 2) + np.abs(wx) * 0.18

    smoke = np.exp(- (np.maximum(dist_tail, 0.0) / (0.34 + tail_t * 0.32)) ** 2) * fbm(nx * 2.2, ny * 2.2, 115, 4, 8)
    flame_outer = np.exp(- (np.maximum(dist_tail, 0.0) / (0.20 + tail_t * 0.22)) ** 1.7) * (0.45 + 0.9 * fbm(nx * 3.0, ny * 3.0, 116, 4, 10))
    flame_inner = np.exp(- (np.maximum(dist_core, 0.0) / 0.24) ** 2.2) * (0.65 + 0.75 * fbm(nx * 3.5, ny * 3.5, 117, 4, 12))
    white_core = np.exp(- (np.maximum(np.sqrt((GX - cx) ** 2 + (GY - cy) ** 2) + wx * 0.08, 0.0) / 0.12) ** 2.4)

    # Dark scorch crater halo & crimson flames & molten white-gold solar heart
    add_glow(c, smoke, (0.48, 0.08, 0.02), 0.95)
    add_glow(c, flame_outer, (1.15, 0.26, 0.03), 1.25)
    add_glow(c, flame_inner, (1.55, 0.72, 0.14), 1.35)
    add_glow(c, white_core, (2.2, 1.95, 1.35), 1.45)

    # Shockwave fire ring around forward impact arc
    shock = np.exp(- ((R_GRID - 0.56 - wx * 0.09) / 0.035) ** 2) * np.clip(0.55 + GX * 0.7, 0.0, 1.0)
    add_glow(c, shock, (1.2, 0.42, 0.08), 0.9)
    return apply_frame_and_tonemap(c, (1.1, 0.45, 0.08), 101)


def render_02_black_hole():
    c = new_obsidian_canvas(202, tint=(0.06, 0.02, 0.15))
    wx = fbm(GX * 2.0, GY * 2.0, 210, 4, 8) - 0.5
    wy = fbm(GX * 2.0 + 3.1, GY * 2.0, 211, 4, 8) - 0.5

    # Tilted accretion plane (Gargantua style)
    tilt = math.radians(-18.0)
    rx = GX * math.cos(tilt) - GY * math.sin(tilt)
    ry = GX * math.sin(tilt) + GY * math.cos(tilt)
    disk_r = np.sqrt(rx * rx + (ry * 3.6) ** 2) + (wx * 0.055)
    swirl = fbm(rx * 2.8 + np.cos(ANG_GRID * 2.0) * 0.35, ry * 6.5, 215, 5, 10)
    doppler = np.clip(1.22 - rx * 0.65, 0.42, 1.75)

    # Outer violet-indigo plasma swirl & inner incandescent amber-white ISCO rim
    disk_outer = np.exp(- ((disk_r - 0.56) / 0.17) ** 2) * swirl * doppler
    disk_inner = np.exp(- ((disk_r - 0.33) / 0.055) ** 2) * (0.65 + 0.5 * swirl) * doppler
    # Bent gravitational lensing arch over top and bottom poles
    lens_arch = np.exp(- ((R_GRID - 0.33 - wx * 0.015) / 0.038) ** 2) * (0.45 + 0.55 * (np.abs(GY) / np.maximum(R_GRID, 0.01)))
    photon_ring = np.exp(- ((R_GRID - 0.255) / 0.012) ** 2)

    add_glow(c, disk_outer, (0.62, 0.24, 1.25), 1.45)
    add_glow(c, lens_arch, (0.95, 0.72, 1.35), 1.15)
    add_glow(c, disk_inner, (1.45, 0.74, 0.28), 1.35)
    add_glow(c, photon_ring, (1.85, 2.05, 2.25), 1.30)

    # Cut out pure pitch-black Schwarzschild Event Horizon Shadow at r <= 0.245
    horizon_mask = np.clip((R_GRID - 0.232) / 0.022, 0.0, 1.0)
    # Also draw front lower foreground bridge crossing in front of horizon shadow
    front_bridge = np.where((ry > 0.01) & (disk_r > 0.26) & (disk_r < 0.72), 1.0, 0.0)
    for ch in range(3):
        c[:, :, ch] = c[:, :, ch] * np.maximum(horizon_mask, front_bridge * 0.85)
    # Polar relativistic jet whisper
    jet = np.exp(- (np.abs(rx) / 0.028) ** 2) * np.exp(- (np.abs(ry) / 0.85) ** 2) * (R_GRID > 0.26)
    add_glow(c, jet, (0.45, 0.85, 1.45), 0.45)
    return apply_frame_and_tonemap(c, (0.62, 0.32, 1.25), 202)


def render_03_chronosphere():
    c = new_obsidian_canvas(303, tint=(0.02, 0.12, 0.22))
    turb = fbm(GX * 2.2, GY * 2.2, 310, 5, 8)
    vor = np.abs(np.sin(GX * 9.0 + turb * 3.2) * np.cos(GY * 9.0 - turb * 3.2))

    # Cosmic fracture dome edge + translucent sapphire time nebula
    rim = np.exp(- ((R_GRID - 0.62) / 0.055) ** 2) * (0.75 + 0.5 * turb)
    inner_glow = np.exp(- (R_GRID / 0.56) ** 2) * turb * 0.55
    crystal_vein = np.exp(- (vor / 0.08) ** 2) * (R_GRID < 0.62) * 0.42

    # Fine temporal dial rings (delicate celestial threads, NOT heavy bars)
    dial_a = np.exp(- ((R_GRID - 0.46) / 0.006) ** 2)
    dial_b = np.exp(- ((R_GRID - 0.28) / 0.005) ** 2)
    spokes = np.where((np.abs(np.sin(ANG_GRID * 6.0)) > 0.994) & (R_GRID > 0.14) & (R_GRID < 0.62), 0.75, 0.0)

    add_glow(c, inner_glow, (0.14, 0.42, 1.15), 1.1)
    add_glow(c, crystal_vein, (0.28, 0.85, 1.35), 1.1)
    add_glow(c, rim, (0.15, 0.95, 1.45), 1.45)
    add_glow(c, dial_a + dial_b + spokes, (0.72, 0.96, 1.35), 0.95)

    # Suspended frost/clockwork star core
    core = np.exp(- (R_GRID / 0.11) ** 2)
    add_glow(c, core, (1.4, 1.85, 2.1), 1.0)
    return apply_frame_and_tonemap(c, (0.18, 0.92, 1.25), 303)


def render_04_chaos_meteor():
    c = new_obsidian_canvas(404, tint=(0.14, 0.03, 0.10))
    wx = fbm(GX * 2.4, GY * 2.4, 410, 5, 8) - 0.5
    wy = fbm(GX * 2.4 + 1.9, GY * 2.4 - 2.2, 411, 5, 8) - 0.5

    # Diagonal sky tear at upper-left
    rift_d = np.abs((GX + 0.36) * 0.65 - (GY + 0.42) * 0.76 + wx * 0.09)
    rift_len = np.exp(- (((GX + 0.36) ** 2 + (GY + 0.42) ** 2) / 0.22))
    sky_rift = np.exp(- (rift_d / 0.032) ** 2) * rift_len
    add_glow(c, sky_rift, (1.15, 0.22, 1.35), 1.45)

    # Diagonal supersonic re-entry inferno wake
    ax, ay, bx, by = -0.38, -0.40, 0.18, 0.22
    vx, vy = bx - ax, by - ay
    t = np.clip(((GX - ax) * vx + (GY - ay) * vy) / (vx * vx + vy * vy), 0.0, 1.05)
    d_wake = np.maximum(np.sqrt((GX - (ax + t * vx) + wx * 0.14) ** 2 + (GY - (ay + t * vy) + wy * 0.14) ** 2), 0.0)
    wake = np.exp(- (d_wake / (0.04 + 0.14 * t)) ** 1.6) * (0.25 + 0.75 * t) * fbm((GX + wx) * 3.0, (GY + wy) * 3.0, 415, 4, 10)
    add_glow(c, wake, (1.45, 0.42, 0.08), 1.45)

    # Craggy asymmetric obsidian meteoroid rock sculpted by multi-frequency polar roughness
    m_dx, m_dy = GX - 0.18, GY - 0.22
    rock_ang = np.arctan2(m_dy, m_dx)
    lobe = 0.032 * np.sin(rock_ang * 3.0 + 0.7) + 0.022 * np.cos(rock_ang * 5.0 - 1.2) + wx * 0.095
    rock_r = np.sqrt(m_dx * m_dx + m_dy * m_dy) + lobe
    rock_alpha = np.clip((0.19 - rock_r) / 0.015, 0.0, 1.0)
    c_vein = fbm(GX * 5.5 + wx * 1.4, GY * 5.5 + wy * 1.4, 418, 4, 10)
    veins = np.exp(- (np.abs(c_vein - 0.5) / 0.038) ** 2) * rock_alpha
    # Shade rock sphere volume + scorch magma seams
    for ch in range(3):
        c[:, :, ch] *= (1.0 - rock_alpha * 0.90)
    add_glow(c, veins, (2.2, 0.95, 0.24), 1.45)
    bow_shock = np.exp(- ((rock_r - 0.195) / 0.028) ** 2) * np.clip(0.30 + (m_dx + m_dy) * 2.5, 0.0, 1.0)
    add_glow(c, bow_shock, (1.85, 1.05, 0.42), 1.35)

    furrow = np.exp(- ((GY - 0.56 + wy * 0.06) / 0.12) ** 2) * np.exp(- (GX / 0.68) ** 2)
    add_glow(c, furrow, (1.1, 0.24, 0.04), 0.85)
    return apply_frame_and_tonemap(c, (1.3, 0.42, 0.08), 404)


def render_05_chain_lightning():
    c = new_obsidian_canvas(505, tint=(0.03, 0.09, 0.18))
    clouds = fbm(GX * 2.1, GY * 2.1, 510, 5, 8)
    add_glow(c, np.exp(- (R_GRID / 0.82) ** 2) * clouds, (0.08, 0.28, 0.72), 0.95)

    # 3 cascading storm chain vertices
    paint_lightning_bolt(c, (-0.72, 0.38), (-0.12, -0.28), 511, 3.6, (0.18, 0.88, 1.45))
    paint_lightning_bolt(c, (-0.12, -0.28), (0.34, 0.26), 512, 3.4, (0.32, 0.72, 1.45))
    paint_lightning_bolt(c, (0.34, 0.26), (0.76, -0.34), 513, 3.0, (0.62, 0.38, 1.45))

    # Impact flash nodes
    for nx, ny in [(-0.72, 0.38), (-0.12, -0.28), (0.34, 0.26), (0.76, -0.34)]:
        nd = np.sqrt((GX - nx) ** 2 + (GY - ny) ** 2)
        add_glow(c, np.exp(- (nd / 0.08) ** 2), (0.55, 0.95, 1.65), 1.25)
    return apply_frame_and_tonemap(c, (0.18, 0.95, 1.35), 505)


def render_06_supernova_beam():
    c = new_obsidian_canvas(606, tint=(0.12, 0.05, 0.04))
    wx = fbm(GX * 2.8, GY * 2.8, 610, 4, 8) - 0.5
    # Diagonal concentrated stellar plasma lance from (-0.78, 0.56) -> (0.72, -0.52)
    ax, ay, bx, by = -0.78, 0.56, 0.72, -0.52
    vx, vy = bx - ax, by - ay
    v2 = vx * vx + vy * vy
    t = np.clip(((GX - ax) * vx + (GY - ay) * vy) / v2, 0.0, 1.0)
    px = ax + t * vx
    py = ay + t * vy
    d = np.sqrt((GX - px) ** 2 + (GY - py) ** 2)

    # Double helix plasma ribbons twisting along beam
    nx_dir, ny_dir = -vy / math.sqrt(v2), vx / math.sqrt(v2)
    signed_perp = (GX - px) * nx_dir + (GY - py) * ny_dir
    coil_a = np.abs(signed_perp - np.sin(t * 16.0) * 0.11)
    coil_b = np.abs(signed_perp + np.sin(t * 16.0) * 0.11)

    core = np.exp(- (d / 0.036) ** 2)
    sheath = np.exp(- (np.maximum(d + np.abs(wx) * 0.08, 0.0) / 0.16) ** 1.5)
    helix = np.exp(- (coil_a / 0.022) ** 2) + np.exp(- (coil_b / 0.022) ** 2)

    add_glow(c, sheath, (1.35, 0.28, 0.06), 1.2)
    add_glow(c, helix, (0.25, 0.92, 1.45), 0.95)
    add_glow(c, core, (2.2, 2.05, 1.55), 1.45)

    # Impact nova flare at tip
    tip_d = np.sqrt((GX - bx) ** 2 + (GY - by) ** 2)
    add_glow(c, np.exp(- (tip_d / 0.22) ** 2), (1.45, 0.65, 0.22), 1.35)
    return apply_frame_and_tonemap(c, (1.4, 0.55, 0.15), 606)


def render_07_chrono_blink():
    c = new_obsidian_canvas(707, tint=(0.03, 0.07, 0.18))
    wx = fbm(GX * 2.5, GY * 2.5, 710, 4, 8) - 0.5
    # Two spectral hooded silhouette echoes joined by tearing horizontal phase streaks
    for sx, amp, col in [(-0.34, 0.85, (0.45, 0.24, 1.25)), (0.28, 1.35, (0.18, 0.95, 1.45))]:
        # Hooded mage silhouette (head + shoulders + tattered cloak cone)
        head = np.exp(- (((GX - sx) / 0.10) ** 2 + ((GY + 0.34) / 0.12) ** 2) ** 1.5)
        body_w = 0.13 + np.clip((GY + 0.22) * 0.28, 0.0, 0.24)
        body = np.exp(- ((GX - sx + wx * 0.07) / body_w) ** 2) * np.clip(1.0 - np.abs(GY - 0.14) / 0.45, 0.0, 1.0)
        ghost = np.clip(head + body * 0.85, 0.0, 1.3)
        add_glow(c, ghost, col, amp)

    # Phase warp rift streaks slicing horizontally across
    streak = np.exp(- ((GY - wx * 0.25) / 0.025) ** 2) * np.exp(- (GX / 0.72) ** 2) * fbm(GX * 5.0, GY * 18.0, 715, 3, 12)
    add_glow(c, streak * 2.4, (0.35, 0.95, 1.45), 1.35)
    return apply_frame_and_tonemap(c, (0.24, 0.88, 1.35), 707)


def render_08_gravity_cataclysm():
    c = new_obsidian_canvas(808, tint=(0.12, 0.03, 0.14))
    wx = fbm(GX * 3.0, GY * 3.0, 810, 4, 9) - 0.5
    wy = fbm(GX * 3.0 + 4.1, GY * 3.0, 811, 4, 9) - 0.5
    # Shattered glowing magma fissure on ground plane + jagged natural volcanic rock pillars
    fissure = np.exp(- ((GY - 0.46 + wy * 0.05) / 0.16) ** 2) * np.exp(- (GX / 0.72) ** 2)
    cracks = fbm(GX * 4.5 + wx, GY * 6.0 + wy, 812, 4, 10)
    add_glow(c, fissure * (cracks ** 1.3), (1.75, 0.48, 0.12), 1.45)

    spires = [(-0.46, 0.46, 0.11, -0.14, -0.14),
              (-0.18, 0.50, 0.13, -0.44, -0.05),
              (0.02, 0.54, 0.15, -0.66, 0.02),
              (0.24, 0.48, 0.12, -0.36, 0.08),
              (0.48, 0.44, 0.10, -0.12, 0.16)]
    for idx, (cx, by_pos, half_w, top_y, tilt) in enumerate(spires):
        t_h = np.clip((by_pos - GY) / max(by_pos - top_y, 0.05), 0.0, 1.0)
        center_x = cx + (1.0 - t_h) * (-tilt * 0.4) + tilt * 0.4 + wx * 0.055
        width_at_y = half_w * (1.0 - t_h * 0.78) + np.abs(wy) * 0.025
        dist_edge = np.abs(GX - center_x) - width_at_y
        inside = (GY <= by_pos + wy * 0.05) & (GY >= top_y + wy * 0.06) & (dist_edge < 0.0)
        # Basalt surface variation & fiery rim + violet aura
        shade = 0.06 + 0.12 * np.clip((GX - center_x) / np.maximum(width_at_y, 0.01) + 0.4, 0.0, 1.0)
        c[inside] = c[inside] * 0.15 + shade[inside][:, None] * 0.08
        edge = (GY <= by_pos) & (GY >= top_y + wy * 0.06) & (np.abs(dist_edge) < 0.014)
        add_glow(c, np.where(edge, 1.0, 0.0), (1.55, 0.45, 0.14), 1.25)

    shock = np.exp(- ((R_GRID - 0.58 + wx * 0.06) / 0.055) ** 2) * np.clip(0.65 - GY * 0.5, 0.0, 1.0)
    add_glow(c, shock, (0.72, 0.25, 1.35), 1.15)
    return apply_frame_and_tonemap(c, (1.25, 0.35, 0.85), 808)


def render_09_prism_wall():
    c = new_obsidian_canvas(909, tint=(0.03, 0.08, 0.19))
    wx = fbm(GX * 3.0, GY * 3.0, 910, 4, 8) - 0.5
    wy = fbm(GX * 3.0 + 2.4, GY * 3.0, 911, 4, 8) - 0.5
    # Incoming blazing ember bolt refracted into 3 prismatic beams across crystalline shards
    in_d = np.abs(GY - 0.05 + wy * 0.02) + np.where(GX > 0.0, 10.0, 0.0)
    add_glow(c, np.exp(- (in_d / 0.025) ** 2) * 1.4, (1.55, 0.52, 0.14), 1.35)

    for slope, rcol in [(-0.36, (0.18, 0.96, 1.45)), (0.04, (0.65, 0.88, 1.55)), (0.42, (0.75, 0.28, 1.45))]:
        ray_d = np.abs((GY - 0.05) - slope * np.maximum(GX, 0.0) + wy * 0.018) + np.where(GX < -0.02, 10.0, 0.0)
        add_glow(c, np.exp(- (ray_d / 0.024) ** 2) * 1.2, rcol, 1.25)

    # Organic faceted quartz-void shards with frosty halo + caustic veins
    shards = [(-0.20, 0.42, 0.09, -0.36), (0.0, 0.48, 0.13, -0.60), (0.20, 0.42, 0.09, -0.32)]
    frost = np.exp(- (R_GRID / 0.56) ** 2) * fbm(GX * 3.0, GY * 3.0, 915, 4, 8) * 0.45
    add_glow(c, frost, (0.14, 0.55, 1.05), 1.0)
    for sx, bot, hw, top in shards:
        t_h = np.clip((bot - GY) / (bot - top), 0.0, 1.0)
        in_y = (GY <= bot + wy * 0.04) & (GY >= top + wx * 0.05)
        w_y = hw * (1.0 - np.abs(t_h - 0.35) * 1.25) + wy * 0.015
        dist_e = np.abs(GX - sx + wx * 0.025) - np.maximum(w_y, 0.008)
        poly = in_y & (dist_e < 0.0)
        caustic = np.clip((GX - sx) / np.maximum(hw, 0.01) + 0.5, 0.1, 1.0)
        add_glow(c, np.where(poly, caustic * 0.58, 0.0), (0.18, 0.72, 1.35), 1.0)
        facet = in_y & (np.abs(dist_e) < 0.012)
        add_glow(c, np.where(facet, 1.0, 0.0), (0.78, 0.98, 1.55), 1.30)

    return apply_frame_and_tonemap(c, (0.22, 0.92, 1.45), 909)


def render_10_primal_overload():
    c = new_obsidian_canvas(1010, tint=(0.08, 0.04, 0.14))
    wx = fbm(GX * 2.4, GY * 2.4, 1011, 5, 8) - 0.5
    # Tri-elemental spiraling arms (Fire orange, Tempest cyan, Graviton violet)
    spiral_ang = ANG_GRID + R_GRID * 4.2 + wx * 1.1
    arm_q = np.clip(np.cos(spiral_ang - 0.0), 0.0, 1.0) ** 3 * np.exp(- (R_GRID / 0.65) ** 2)
    arm_w = np.clip(np.cos(spiral_ang - 2.094), 0.0, 1.0) ** 3 * np.exp(- (R_GRID / 0.65) ** 2)
    arm_e = np.clip(np.cos(spiral_ang - 4.188), 0.0, 1.0) ** 3 * np.exp(- (R_GRID / 0.65) ** 2)

    add_glow(c, arm_q, (1.55, 0.42, 0.08), 1.45)
    add_glow(c, arm_w, (0.14, 0.96, 1.45), 1.45)
    add_glow(c, arm_e, (0.72, 0.24, 1.45), 1.45)

    # Expanding triple shockwave + blinding white-gold singularity core
    ring = np.exp(- ((R_GRID - 0.56 - wx * 0.04) / 0.032) ** 2)
    add_glow(c, ring, (1.25, 0.95, 1.55), 1.15)
    core = np.exp(- (R_GRID / 0.18) ** 2)
    add_glow(c, core, (2.25, 2.15, 1.95), 1.45)
    return apply_frame_and_tonemap(c, (0.85, 0.85, 1.45), 1010)


RENDERERS = [
    ("SpellPlate_01.png", render_01_fireball),
    ("SpellPlate_02.png", render_02_black_hole),
    ("SpellPlate_03.png", render_03_chronosphere),
    ("SpellPlate_04.png", render_04_chaos_meteor),
    ("SpellPlate_05.png", render_05_chain_lightning),
    ("SpellPlate_06.png", render_06_supernova_beam),
    ("SpellPlate_07.png", render_07_chrono_blink),
    ("SpellPlate_08.png", render_08_gravity_cataclysm),
    ("SpellPlate_09.png", render_09_prism_wall),
    ("SpellPlate_10.png", render_10_primal_overload),
]


def main():
    for d in OUT_DIRS:
        d.mkdir(parents=True, exist_ok=True)

    tiles = []
    for name, fn in RENDERERS:
        img = fn()
        for d in OUT_DIRS:
            img.save(d / name)
        tiles.append(img.resize((256, 256), Image.Resampling.LANCZOS))
        print(f"Generated {name} -> RawAssets/Grimoire & RawAssets/Textures")

    # Save contact sheet preview into conversation scratch dir if accessible
    scratch = Path(r"C:\Users\user\.gemini\antigravity\brain\a942cd92-d3f4-43f9-93bd-7806251d920b\scratch")
    if scratch.exists():
        sheet = Image.new("RGBA", (256 * 5, 256 * 2), (8, 8, 12, 255))
        for idx, tile in enumerate(tiles):
            x = (idx % 5) * 256
            y = (idx // 5) * 256
            sheet.paste(tile, (x, y))
        sheet.save(scratch / "spell_plates_preview.png")
        print("Saved preview contact sheet to scratch/spell_plates_preview.png")


if __name__ == "__main__":
    main()
