"""
Generate 8 crisp dark-fantasy painted 128x128 Diablo IV item slot icons:
  - icon_item_helm.png
  - icon_item_chest.png
  - icon_item_gloves.png
  - icon_item_boots.png
  - icon_item_staff.png
  - icon_item_focus.png
  - icon_item_amulet.png
  - icon_item_ring.png

Uses 512x512 -> 128x128 Lanczos supersampling, hammered PBR micro-relief normals,
faceted gemstone cutting, cavity AO, and atmospheric dark-obsidian lighting.
Outputs identically to BOTH:
  - c:/Users/tagir/OneDrive/Desktop/Aetheria/GodotAetheria/assets/ui/
  - C:/Games/AETHERIA/AetheriaData/assets/ui/
"""

import math
from pathlib import Path
import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
OUT_DIRS = [
    ROOT / "GodotAetheria" / "assets" / "ui",
    Path(r"C:\Games\AETHERIA\AetheriaData\assets\ui"),
]

SIZE_HI = 512
SIZE_OUT = 128


def _make_grid(sz: int = SIZE_HI):
    yy, xx = np.mgrid[0:sz, 0:sz].astype(np.float32)
    nx = (xx / (sz - 1)) * 2.0 - 1.0
    ny = (yy / (sz - 1)) * 2.0 - 1.0
    return nx, ny


def _fbm(sz: int, seed: int, octaves=(5, 14, 34, 76)) -> np.ndarray:
    rng = np.random.default_rng(seed)
    acc = np.zeros((sz, sz), dtype=np.float32)
    w = 0.55
    tot = 0.0
    for g in octaves:
        c = rng.uniform(0.0, 1.0, size=(g, g)).astype(np.float32)
        im = Image.fromarray(c, mode="F").resize((sz, sz), Image.Resampling.BICUBIC)
        acc += np.asarray(im, dtype=np.float32) * w
        tot += w
        w *= 0.52
    return np.clip(acc / max(tot, 1e-5), 0.0, 1.0)


def _sd_box(px, py, cx, cy, hx, hy, r=0.0):
    qx = np.abs(px - cx) - hx + r
    qy = np.abs(py - cy) - hy + r
    return np.sqrt(np.maximum(qx, 0.0) ** 2 + np.maximum(qy, 0.0) ** 2) + np.minimum(np.maximum(qx, qy), 0.0) - r


def _sd_capsule(px, py, ax, ay, bx, by, r):
    pax = px - ax
    pay = py - ay
    bax = bx - ax
    bay = by - ay
    denom = max(1e-6, bax * bax + bay * bay)
    h = np.clip((pax * bax + pay * bay) / denom, 0.0, 1.0)
    dx = pax - bax * h
    dy = pay - bay * h
    return np.sqrt(dx * dx + dy * dy) - r


def _sd_circle(px, py, cx, cy, r):
    return np.sqrt((px - cx) ** 2 + (py - cy) ** 2) - r


def _sd_ellipse(px, py, cx, cy, rx, ry):
    return np.sqrt(((px - cx) / rx) ** 2 + ((py - cy) / ry) ** 2) * min(rx, ry) - min(rx, ry)


def _sd_diamond(px, py, cx, cy, rx, ry):
    return (np.abs(px - cx) / rx + np.abs(py - cy) / ry - 1.0) * min(rx, ry)


def _rot(px, py, angle):
    c, s = math.cos(angle), math.sin(angle)
    return px * c - py * s, px * s + py * c


def _paint_shape(sdf, height, albedo, metallic, roughness, mask, h_fn, col, met, rgh, bevel=0.055):
    inside = np.clip(-sdf / 0.014 + 0.5, 0.0, 1.0)
    sel = sdf < 0.012
    if not np.any(sel):
        return
    d_in = np.maximum(-sdf, 0.0)
    domed = np.sqrt(np.clip(d_in / bevel, 0.0, 1.0))
    new_h = h_fn(domed)
    mask[sel] = np.maximum(mask[sel], inside[sel])
    higher = sel & (new_h >= height - 0.015)
    height[higher] = new_h[higher]
    for c in range(3):
        albedo[higher, c] = col[c]
    metallic[higher] = met
    roughness[higher] = rgh


def _paint_faceted_gem(nx, ny, cx, cy, rx, ry, col_core, col_edge, height, albedo, metallic, roughness, emissive, mask):
    dx = (nx - cx) / rx
    dy = (ny - cy) / ry
    r_ell = np.sqrt(dx * dx + dy * dy)
    in_gem = r_ell < 1.03
    if not np.any(in_gem):
        return

    # 8-sector faceted crystal cut
    ang = np.arctan2(dy, dx)
    facet = np.floor((ang + math.pi) / (math.pi / 4.0))
    facet_light = 0.55 + 0.45 * np.cos(facet * 1.1 + 0.7)
    table = r_ell < 0.46
    gem_h = np.where(table, 1.08, 1.08 - (r_ell - 0.46) * 0.55 + facet_light * 0.06)

    mask[in_gem] = np.maximum(mask[in_gem], np.clip((1.02 - r_ell[in_gem]) / 0.06, 0.0, 1.0))
    height[in_gem] = np.maximum(height[in_gem], gem_h[in_gem])
    metallic[in_gem] = 0.15
    roughness[in_gem] = 0.10

    t_rad = np.clip(r_ell[in_gem], 0.0, 1.0)[..., None]
    fl = facet_light[in_gem][..., None]
    base_g = np.array(col_core) * (1.0 - t_rad) + np.array(col_edge) * t_rad
    albedo[in_gem] = np.clip(base_g * (0.72 + 0.48 * fl), 0, 255)

    # Bright top-left glint on gem table
    glint = np.exp(-((dx + 0.22) ** 2 + (dy + 0.25) ** 2) / 0.06)
    em_g = base_g * (0.45 + 0.55 * (1.0 - t_rad)) * (0.75 + 0.45 * fl) + np.array([255.0, 248.0, 235.0]) * (glint[in_gem][..., None] * 0.55)
    emissive[in_gem] = np.clip(em_g, 0, 255)


def _add_sparks(nx, ny, emissive, mask, seed, col, count=14):
    rng = np.random.default_rng(seed)
    for _ in range(count):
        sx = rng.uniform(-0.82, 0.82)
        sy = rng.uniform(-0.82, 0.82)
        sr = rng.uniform(0.010, 0.024)
        d = np.sqrt((nx - sx) ** 2 + (ny - sy) ** 2)
        sp = np.exp(-(d / sr) ** 2) * rng.uniform(0.4, 0.95)
        sel = sp > 0.03
        emissive[sel] += np.array(col) * sp[sel][..., None]
        mask[sel] = np.maximum(mask[sel], np.clip(sp[sel] * 0.85, 0.0, 1.0))


def _render_icon(
    height: np.ndarray,
    albedo: np.ndarray,
    metallic: np.ndarray,
    roughness: np.ndarray,
    emissive: np.ndarray,
    mask: np.ndarray,
    bg_tint: tuple[float, float, float],
    seed: int = 10,
) -> Image.Image:
    sz = height.shape[0]
    nx, ny = _make_grid(sz)
    r_bg = np.sqrt(nx * nx + ny * ny)
    grain = _fbm(sz, seed)
    fine = _fbm(sz, seed + 100, octaves=(18, 48, 110))

    # Add authentic hammered-iron / forged-bronze relief onto height before normal calc
    relief = (grain - 0.5) * 0.045 + (fine - 0.5) * 0.025
    h_lit = height + relief * (mask > 0.2)

    # Dark obsidian forged backdrop with subtle warm/arcane atmospheric radial glow
    bg_center = np.array([17.0, 15.0, 21.0], dtype=np.float32) + np.array(bg_tint, dtype=np.float32) * 28.0
    bg_edge = np.array([6.0, 6.0, 9.0], dtype=np.float32)
    glow_t = np.clip(1.0 - (r_bg / 1.15) ** 1.35, 0.0, 1.0)[..., None]
    bg = bg_edge + (bg_center - bg_edge) * glow_t + (grain[..., None] - 0.5) * 7.0

    # Ambient drop shadow + soft aura behind silhouette
    mask_blur = np.asarray(
        Image.fromarray((np.clip(mask, 0.0, 1.0) * 255.0).astype(np.uint8), mode="L").filter(
            ImageFilter.GaussianBlur(radius=18)
        ),
        dtype=np.float32,
    ) / 255.0
    shadow_blur = np.asarray(
        Image.fromarray((np.clip(mask, 0.0, 1.0) * 255.0).astype(np.uint8), mode="L").filter(
            ImageFilter.GaussianBlur(radius=9)
        ),
        dtype=np.float32,
    ) / 255.0
    bg *= (1.0 - shadow_blur[..., None] * 0.65)
    bg += np.array(bg_tint, dtype=np.float32) * (mask_blur[..., None] * 38.0)

    # Surface normals
    gy, gx = np.gradient(h_lit.astype(np.float32))
    snx = -gx * 24.0
    sny = -gy * 24.0
    snz = np.ones_like(h_lit, dtype=np.float32)
    inv_l = 1.0 / np.sqrt(snx * snx + sny * sny + snz * snz + 1e-8)
    snx *= inv_l
    sny *= inv_l
    snz *= inv_l

    lx, ly, lz = -0.45, -0.68, 0.58
    ll = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ll, ly / ll, lz / ll
    ndotl = np.clip(snx * lx + sny * ly + snz * lz, 0.0, 1.0)

    rx, ry, rz = 0.58, 0.42, 0.69
    rl = math.sqrt(rx * rx + ry * ry + rz * rz)
    ndot_rim = np.clip(snx * (rx / rl) + sny * (ry / rl) + snz * (rz / rl), 0.0, 1.0)

    hx, hy, hz = lx, ly, lz + 1.0
    hl = math.sqrt(hx * hx + hy * hy + hz * hz)
    ndoth = np.clip(snx * (hx / hl) + sny * (hy / hl) + snz * (hz / hl), 0.0, 1.0)

    spec_pow = np.clip(2.0 / np.maximum(roughness ** 3.2, 0.015), 6.0, 120.0)
    spec = (ndoth ** spec_pow) * (0.35 + 1.55 * metallic)

    h_blur = np.asarray(
        Image.fromarray(np.clip(height * 255.0, 0, 255).astype(np.uint8), mode="L").filter(
            ImageFilter.GaussianBlur(radius=6)
        ),
        dtype=np.float32,
    ) / 255.0
    ao = np.clip(0.84 + (height - h_blur) * 2.1, 0.30, 1.24)

    albedo_p = np.clip(albedo * (0.85 + 0.30 * grain[..., None]), 0.0, 255.0)
    diffuse = albedo_p * (1.0 - 0.56 * metallic[..., None])
    fg = (
        diffuse * (0.28 + 0.98 * ndotl[..., None]) * ao[..., None]
        + albedo_p * ndot_rim[..., None] * 0.30 * np.array([0.70, 0.84, 1.05])
    )

    spec_col = (1.0 - metallic[..., None]) * np.array([255.0, 244.0, 225.0]) + metallic[..., None] * np.clip(
        albedo_p * 1.45 + 48.0, 0.0, 255.0
    )
    fg += spec[..., None] * spec_col * 0.78 + emissive

    # Crisp dark iron contour around silhouette
    edge_band = (mask > 0.06) & (mask < 0.86)
    fg[edge_band] *= 0.42

    m = np.clip(mask, 0.0, 1.0)[..., None]
    comp = bg * (1.0 - m) + fg * m

    d_edge = np.minimum(1.0 - np.abs(nx), 1.0 - np.abs(ny))
    vign = np.clip(d_edge / 0.14, 0.55, 1.0)[..., None]
    comp *= vign

    rgba = np.dstack([np.clip(comp, 0, 255), np.full((sz, sz), 255.0)]).astype(np.uint8)
    return Image.fromarray(rgba, mode="RGBA").resize((SIZE_OUT, SIZE_OUT), Image.Resampling.LANCZOS)


def _blank_buffers():
    sz = SIZE_HI
    return (
        np.zeros((sz, sz), dtype=np.float32),
        np.zeros((sz, sz, 3), dtype=np.float32),
        np.zeros((sz, sz), dtype=np.float32),
        np.full((sz, sz), 0.55, dtype=np.float32),
        np.zeros((sz, sz, 3), dtype=np.float32),
        np.zeros((sz, sz), dtype=np.float32),
    )


def build_helm() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    iron = np.array([66.0, 68.0, 78.0], dtype=np.float32)
    dark_steel = np.array([38.0, 40.0, 48.0], dtype=np.float32)
    gold = np.array([196.0, 152.0, 72.0], dtype=np.float32)
    cloth = np.array([78.0, 20.0, 26.0], dtype=np.float32)

    sdf_hood = _sd_ellipse(nx, ny, 0.0, 0.08, 0.48, 0.62)
    _paint_shape(sdf_hood, h, alb, met, rgh, msk, lambda d: 0.22 + 0.22 * d, cloth, 0.10, 0.82, 0.15)

    sdf_dome = np.maximum(_sd_ellipse(nx, ny, 0.0, -0.06, 0.45, 0.52), ny - 0.12)
    _paint_shape(sdf_dome, h, alb, met, rgh, msk, lambda d: 0.42 + 0.44 * d, iron, 0.88, 0.32, 0.16)

    # Flared bronze brow arch
    for sgn in (-1.0, 1.0):
        brow = _sd_capsule(nx, ny, 0.0, 0.04, sgn * 0.40, -0.10, 0.042)
        _paint_shape(brow, h, alb, met, rgh, msk, lambda d: 0.72 + 0.18 * d, gold, 0.94, 0.24, 0.04)
        cheek = _sd_capsule(nx, ny, sgn * 0.36, 0.0, sgn * 0.22, 0.52, 0.105)
        _paint_shape(cheek, h, alb, met, rgh, msk, lambda d: 0.48 + 0.34 * d, dark_steel, 0.90, 0.34, 0.08)

    # Twin curved horned crown spikes
    for sgn in (-1.0, 1.0):
        sdf_horn = np.full_like(nx, 10.0)
        for ti in np.linspace(0.0, 1.0, 16):
            hx = sgn * (0.28 + 0.38 * ti - 0.09 * ti * ti)
            hy = -0.14 - 0.52 * (ti ** 1.12)
            rad = 0.082 * (1.0 - 0.74 * ti) + 0.014
            sdf_horn = np.minimum(sdf_horn, _sd_circle(nx, ny, hx, hy, rad))
        _paint_shape(sdf_horn, h, alb, met, rgh, msk, lambda d: 0.58 + 0.36 * d, gold, 0.94, 0.25, 0.06)

    crest = _sd_capsule(nx, ny, 0.0, -0.58, 0.0, 0.25, 0.040)
    _paint_shape(crest, h, alb, met, rgh, msk, lambda d: 0.76 + 0.22 * d, gold, 0.95, 0.22, 0.04)

    _paint_faceted_gem(nx, ny, 0.0, -0.20, 0.072, 0.10, [255.0, 190.0, 60.0], [220.0, 65.0, 15.0], h, alb, met, rgh, em, msk)

    for sgn in (-1.0, 1.0):
        eye = _sd_capsule(nx, ny, sgn * 0.075, 0.14, sgn * 0.23, 0.08, 0.026) < 0.0
        h[eye] = 0.06
        alb[eye] = np.array([10.0, 14.0, 22.0])
        em[eye] = np.array([45.0, 205.0, 255.0]) * 0.7

    return _render_icon(h, alb, met, rgh, em, msk, (0.95, 0.45, 0.14), seed=11)


def build_chest() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    slate_armor = np.array([58.0, 60.0, 72.0], dtype=np.float32)
    dark_plate = np.array([38.0, 40.0, 48.0], dtype=np.float32)
    crimson_robe = np.array([96.0, 18.0, 24.0], dtype=np.float32)
    gold = np.array([194.0, 148.0, 70.0], dtype=np.float32)

    # Draped surcoat skirt + high archmage collar
    collar = _sd_ellipse(nx, ny, 0.0, -0.34, 0.26, 0.20)
    _paint_shape(collar, h, alb, met, rgh, msk, lambda d: 0.35 + 0.22 * d, crimson_robe, 0.15, 0.75, 0.10)
    skirt = _sd_box(nx, ny, 0.0, 0.28, 0.32, 0.44, 0.10)
    _paint_shape(skirt, h, alb, met, rgh, msk, lambda d: 0.26 + 0.24 * d, crimson_robe, 0.15, 0.78, 0.12)

    # Segmented torso breastplate
    tor = _sd_ellipse(nx, ny, 0.0, -0.05, 0.38, 0.42)
    _paint_shape(tor, h, alb, met, rgh, msk, lambda d: 0.44 + 0.40 * d, slate_armor, 0.88, 0.32, 0.16)

    # Engraved abs ribs
    for ry in (0.10, 0.22):
        seg = _sd_box(nx, ny, 0.0, ry, 0.24, 0.04, 0.02)
        _paint_shape(seg, h, alb, met, rgh, msk, lambda d: 0.56 + 0.22 * d, dark_plate, 0.90, 0.30, 0.04)

    # Ornate horned gothic pauldrons
    for sgn in (-1.0, 1.0):
        for layer in range(2):
            px = sgn * (0.44 + layer * 0.11)
            py = -0.26 + layer * 0.11
            p_sdf = _sd_ellipse(nx, ny, px, py, 0.24 - layer * 0.03, 0.16)
            _paint_shape(p_sdf, h, alb, met, rgh, msk, lambda d: 0.58 + 0.32 * d, slate_armor if layer == 1 else gold, 0.92, 0.26, 0.07)
        sp = _sd_capsule(nx, ny, sgn * 0.46, -0.34, sgn * 0.68, -0.50, 0.032)
        _paint_shape(sp, h, alb, met, rgh, msk, lambda d: 0.72 + 0.20 * d, gold, 0.95, 0.22, 0.03)

    # Gold chest V-filigree & belt clasp
    for sgn in (-1.0, 1.0):
        rib = _sd_capsule(nx, ny, 0.0, 0.20, sgn * 0.34, -0.20, 0.025)
        _paint_shape(rib, h, alb, met, rgh, msk, lambda d: 0.80 + 0.16 * d, gold, 0.95, 0.22, 0.03)
    belt = _sd_diamond(nx, ny, 0.0, 0.34, 0.11, 0.09)
    _paint_shape(belt, h, alb, met, rgh, msk, lambda d: 0.78 + 0.18 * d, gold, 0.95, 0.22, 0.04)

    _paint_faceted_gem(nx, ny, 0.0, -0.06, 0.088, 0.125, [110.0, 235.0, 255.0], [20.0, 115.0, 235.0], h, alb, met, rgh, em, msk)
    return _render_icon(h, alb, met, rgh, em, msk, (0.20, 0.72, 0.95), seed=22)


def build_gloves() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    steel = np.array([64.0, 66.0, 76.0], dtype=np.float32)
    leather = np.array([78.0, 46.0, 34.0], dtype=np.float32)
    gold = np.array([192.0, 146.0, 70.0], dtype=np.float32)

    for idx, (ox, oy, ang) in enumerate([(-0.27, 0.06, -0.14), (0.27, -0.04, 0.14)]):
        gx, gy = _rot(nx - ox, ny - oy, ang)
        # Flared winged cuff
        cuff = _sd_box(gx, gy, 0.0, 0.36, 0.18, 0.24, 0.06)
        _paint_shape(cuff, h, alb, met, rgh, msk, lambda d: 0.38 + 0.36 * d, steel, 0.88, 0.32, 0.08)
        band = _sd_box(gx, gy, 0.0, 0.20, 0.20, 0.042, 0.02)
        _paint_shape(band, h, alb, met, rgh, msk, lambda d: 0.76 + 0.16 * d, gold, 0.94, 0.24, 0.03)
        palm = _sd_ellipse(gx, gy, 0.0, 0.0, 0.17, 0.17)
        _paint_shape(palm, h, alb, met, rgh, msk, lambda d: 0.50 + 0.34 * d, leather, 0.30, 0.58, 0.07)
        # Thumb
        th_x = 0.17 if idx == 0 else -0.17
        th = _sd_capsule(gx, gy, th_x * 0.7, 0.06, th_x * 1.25, -0.14, 0.036)
        _paint_shape(th, h, alb, met, rgh, msk, lambda d: 0.52 + 0.28 * d, steel, 0.88, 0.32, 0.035)
        # 4 articulated armored fingers with gold knuckle ridge
        for f in range(4):
            fx = (f - 1.5) * 0.082
            fy_top = -0.44 + abs(f - 1.5) * 0.055
            fing = _sd_capsule(gx, gy, fx, -0.11, fx * 1.05, fy_top, 0.035)
            _paint_shape(fing, h, alb, met, rgh, msk, lambda d: 0.56 + 0.30 * d, steel, 0.88, 0.30, 0.035)
        kn = _sd_box(gx, gy, 0.0, -0.12, 0.17, 0.03, 0.015)
        _paint_shape(kn, h, alb, met, rgh, msk, lambda d: 0.80 + 0.14 * d, gold, 0.94, 0.22, 0.025)
        _paint_faceted_gem(gx, gy, 0.0, 0.01, 0.052, 0.065, [255.0, 185.0, 60.0], [220.0, 75.0, 18.0], h, alb, met, rgh, em, msk)

    return _render_icon(h, alb, met, rgh, em, msk, (0.90, 0.46, 0.16), seed=33)


def build_boots() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    steel = np.array([62.0, 64.0, 74.0], dtype=np.float32)
    leather = np.array([72.0, 44.0, 32.0], dtype=np.float32)
    gold = np.array([190.0, 144.0, 68.0], dtype=np.float32)

    for sgn, ox, oy in [(-1.0, -0.25, 0.03), (1.0, 0.25, -0.03)]:
        shaft = _sd_capsule(nx, ny, ox, -0.46, ox, 0.34, 0.13)
        _paint_shape(shaft, h, alb, met, rgh, msk, lambda d: 0.42 + 0.40 * d, steel, 0.88, 0.32, 0.09)
        foot = _sd_capsule(nx, ny, ox, 0.38, ox + sgn * 0.22, 0.54, 0.105)
        _paint_shape(foot, h, alb, met, rgh, msk, lambda d: 0.46 + 0.36 * d, leather, 0.35, 0.56, 0.08)
        # Pointed armored toe cap
        toe = _sd_circle(nx, ny, ox + sgn * 0.24, 0.54, 0.085)
        _paint_shape(toe, h, alb, met, rgh, msk, lambda d: 0.54 + 0.32 * d, steel, 0.90, 0.28, 0.06)
        # Winged knee guard
        knee = _sd_diamond(nx, ny, ox, -0.46, 0.17, 0.13)
        _paint_shape(knee, h, alb, met, rgh, msk, lambda d: 0.74 + 0.22 * d, gold, 0.95, 0.22, 0.06)
        strap = _sd_box(nx, ny, ox, 0.06, 0.145, 0.036, 0.015)
        _paint_shape(strap, h, alb, met, rgh, msk, lambda d: 0.76 + 0.14 * d, gold, 0.93, 0.24, 0.025)
        _paint_faceted_gem(nx, ny, ox, -0.20, 0.042, 0.09, [105.0, 230.0, 255.0], [22.0, 115.0, 235.0], h, alb, met, rgh, em, msk)

    return _render_icon(h, alb, met, rgh, em, msk, (0.28, 0.65, 0.95), seed=44)


def build_staff() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    ebony = np.array([50.0, 36.0, 28.0], dtype=np.float32)
    bronze = np.array([192.0, 146.0, 68.0], dtype=np.float32)
    iron = np.array([72.0, 74.0, 86.0], dtype=np.float32)

    shaft = _sd_capsule(nx, ny, -0.64, 0.66, 0.26, -0.24, 0.046)
    _paint_shape(shaft, h, alb, met, rgh, msk, lambda d: 0.42 + 0.36 * d, ebony, 0.35, 0.54, 0.045)

    for t in (0.16, 0.46, 0.84):
        cx = -0.64 + t * 0.90
        cy = 0.66 - t * 0.90
        ring = _sd_circle(nx, ny, cx, cy, 0.064)
        _paint_shape(ring, h, alb, met, rgh, msk, lambda d: 0.66 + 0.24 * d, bronze, 0.94, 0.24, 0.04)

    # Sweeping curved gothic crescent cradle
    sdf_cradle = np.full_like(nx, 10.0)
    for ti in np.linspace(-1.0, 1.0, 22):
        ang = -0.78 + ti * 1.45
        cx = 0.46 - math.cos(ang) * 0.26
        cy = -0.44 - math.sin(ang) * 0.26
        rad = 0.048 * (1.0 - 0.55 * abs(ti)) + 0.014
        sdf_cradle = np.minimum(sdf_cradle, _sd_circle(nx, ny, cx, cy, rad))
    _paint_shape(sdf_cradle, h, alb, met, rgh, msk, lambda d: 0.64 + 0.32 * d, bronze, 0.95, 0.22, 0.045)

    pom = _sd_capsule(nx, ny, -0.62, 0.64, -0.76, 0.78, 0.036)
    _paint_shape(pom, h, alb, met, rgh, msk, lambda d: 0.56 + 0.30 * d, iron, 0.92, 0.26, 0.03)

    _paint_faceted_gem(nx, ny, 0.48, -0.46, 0.14, 0.14, [255.0, 225.0, 110.0], [245.0, 105.0, 20.0], h, alb, met, rgh, em, msk)
    _add_sparks(nx, ny, em, msk, seed=555, col=[255.0, 150.0, 40.0], count=12)
    return _render_icon(h, alb, met, rgh, em, msk, (1.0, 0.50, 0.14), seed=55)


def build_focus() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    bronze = np.array([192.0, 146.0, 68.0], dtype=np.float32)
    obsidian = np.array([36.0, 34.0, 48.0], dtype=np.float32)

    r = np.sqrt(nx * nx + ny * ny)
    ring1 = np.abs(r - 0.58) - 0.056
    _paint_shape(ring1, h, alb, met, rgh, msk, lambda d: 0.62 + 0.30 * d, bronze, 0.94, 0.22, 0.05)

    gx, gy = _rot(nx, ny, 0.65)
    r_ell = np.sqrt((gx / 0.56) ** 2 + (gy / 0.28) ** 2) * 0.42
    ring2 = np.abs(r_ell - 0.42) - 0.042
    _paint_shape(ring2, h, alb, met, rgh, msk, lambda d: 0.72 + 0.24 * d, obsidian, 0.88, 0.28, 0.04)

    for ang in (0.0, math.pi * 0.5, math.pi, math.pi * 1.5):
        pr = _sd_capsule(nx, ny, math.cos(ang) * 0.56, math.sin(ang) * 0.56, math.cos(ang) * 0.78, math.sin(ang) * 0.78, 0.042)
        _paint_shape(pr, h, alb, met, rgh, msk, lambda d: 0.76 + 0.20 * d, bronze, 0.95, 0.22, 0.03)

    _paint_faceted_gem(nx, ny, 0.0, 0.0, 0.23, 0.23, [155.0, 225.0, 255.0], [140.0, 48.0, 240.0], h, alb, met, rgh, em, msk)
    _add_sparks(nx, ny, em, msk, seed=666, col=[165.0, 85.0, 255.0], count=12)
    return _render_icon(h, alb, met, rgh, em, msk, (0.62, 0.24, 0.95), seed=66)


def build_amulet() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    gold = np.array([198.0, 152.0, 72.0], dtype=np.float32)
    steel = np.array([56.0, 58.0, 68.0], dtype=np.float32)

    # Interlocking chain links
    for sgn in (-1.0, 1.0):
        for i in range(7):
            t = i / 6.0
            cx = sgn * (0.58 * (1.0 - t) + 0.14 * t)
            cy = -0.68 + 0.54 * (t ** 1.2)
            lx, ly = _rot(nx - cx, ny - cy, sgn * 0.65)
            link = np.abs(_sd_ellipse(lx, ly, 0.0, 0.0, 0.068, 0.042)) - 0.020
            _paint_shape(link, h, alb, met, rgh, msk, lambda d: 0.60 + 0.26 * d, gold, 0.95, 0.24, 0.03)

    # Winged pendant guards
    for sgn in (-1.0, 1.0):
        wing = _sd_capsule(nx, ny, 0.0, 0.18, sgn * 0.42, 0.04, 0.052)
        _paint_shape(wing, h, alb, met, rgh, msk, lambda d: 0.64 + 0.26 * d, gold, 0.95, 0.22, 0.04)

    med = _sd_ellipse(nx, ny, 0.0, 0.22, 0.29, 0.36)
    _paint_shape(med, h, alb, met, rgh, msk, lambda d: 0.62 + 0.30 * d, steel, 0.88, 0.30, 0.10)
    bezel = _sd_ellipse(nx, ny, 0.0, 0.22, 0.23, 0.30)
    _paint_shape(bezel, h, alb, met, rgh, msk, lambda d: 0.74 + 0.22 * d, gold, 0.95, 0.22, 0.06)

    tip = _sd_capsule(nx, ny, 0.0, 0.50, 0.0, 0.72, 0.048)
    _paint_shape(tip, h, alb, met, rgh, msk, lambda d: 0.72 + 0.22 * d, gold, 0.95, 0.22, 0.04)

    _paint_faceted_gem(nx, ny, 0.0, 0.22, 0.145, 0.205, [255.0, 90.0, 90.0], [185.0, 18.0, 35.0], h, alb, met, rgh, em, msk)
    return _render_icon(h, alb, met, rgh, em, msk, (0.92, 0.22, 0.28), seed=77)


def build_ring() -> Image.Image:
    nx, ny = _make_grid()
    h, alb, met, rgh, em, msk = _blank_buffers()

    gold = np.array([204.0, 158.0, 74.0], dtype=np.float32)
    obsidian = np.array([42.0, 40.0, 52.0], dtype=np.float32)

    cx, cy = 0.0, 0.12
    rx, ry = 0.50, 0.42
    r_ell = np.sqrt(((nx - cx) / rx) ** 2 + ((ny - cy) / ry) ** 2) * 0.45
    band = np.abs(r_ell - 0.45) - 0.090
    _paint_shape(band, h, alb, met, rgh, msk, lambda d: 0.56 + 0.36 * d, gold, 0.95, 0.22, 0.08)

    crown = _sd_ellipse(nx, ny, 0.0, -0.30, 0.24, 0.19)
    _paint_shape(crown, h, alb, met, rgh, msk, lambda d: 0.72 + 0.24 * d, obsidian, 0.88, 0.28, 0.08)

    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            pr = _sd_circle(nx, ny, sx * 0.14, -0.30 + sy * 0.11, 0.042)
            _paint_shape(pr, h, alb, met, rgh, msk, lambda d: 0.86 + 0.12 * d, gold, 0.96, 0.20, 0.03)

    _paint_faceted_gem(nx, ny, 0.0, -0.30, 0.135, 0.115, [130.0, 240.0, 255.0], [28.0, 125.0, 245.0], h, alb, met, rgh, em, msk)
    return _render_icon(h, alb, met, rgh, em, msk, (0.24, 0.78, 1.0), seed=88)


def main() -> int:
    builders = {
        "icon_item_helm.png": build_helm,
        "icon_item_chest.png": build_chest,
        "icon_item_gloves.png": build_gloves,
        "icon_item_boots.png": build_boots,
        "icon_item_staff.png": build_staff,
        "icon_item_focus.png": build_focus,
        "icon_item_amulet.png": build_amulet,
        "icon_item_ring.png": build_ring,
    }
    for d in OUT_DIRS:
        d.mkdir(parents=True, exist_ok=True)
    for fname, fn in builders.items():
        img = fn()
        assert img.size == (128, 128) and img.mode == "RGBA"
        for d in OUT_DIRS:
            out_path = d / fname
            img.save(out_path, format="PNG", optimize=True)
        print(f"Generated {fname} -> {len(OUT_DIRS)} folders")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
