"""
Procedural 3D Normal-Lit PBR Volumetric Diablo IV UI Texture Kit Generator.
Generates 9-slice obsidian/bronze panels, 3D beveled crimson/stone buttons
(Normal/Hovered/Pressed), recessed cast-bronze skill sockets, circular 3D
spherically shaded HP/MP globes with specular glass & gothic bronze bezels,
and the sculpted bottom action dock pedestal into RawAssets/UI/.
"""

import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
UI_DIR = ROOT / "RawAssets" / "UI"


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
    """Signed distance to inner rounded rectangle (negative inside, positive outside)."""
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
    bump_scale: float = 4.2,
    light_dir: tuple[float, float, float] = (-0.28, -0.78, 0.56),
) -> Image.Image:
    """Compute per-pixel 3D normal-map + GGX/Blinn metallic PBR shading from heightfield."""
    gy, gx = np.gradient(height.astype(np.float32))
    nx = -gx * bump_scale
    ny = -gy * bump_scale
    nz = np.ones_like(height, dtype=np.float32)
    inv_len = 1.0 / np.sqrt(nx * nx + ny * ny + nz * nz + 1e-8)
    nx *= inv_len
    ny *= inv_len
    nz *= inv_len

    # Key directional light (top/slightly left warm torch-moon mix)
    lx, ly, lz = light_dir
    ll = math.sqrt(lx * lx + ly * ly + lz * lz)
    lx, ly, lz = lx / ll, ly / ll, lz / ll

    ndotl = np.clip(nx * lx + ny * ly + nz * lz, 0.0, 1.0)

    # Secondary warm torch bounce from upper-right
    rx, ry, rz = 0.42, -0.58, 0.70
    rl = math.sqrt(rx * rx + ry * ry + rz * rz)
    ndot_fill = np.clip(nx * (rx / rl) + ny * (ry / rl) + nz * (rz / rl), 0.0, 1.0)

    # Half-vector with view = (0, 0, 1)
    hx, hy, hz = lx, ly, lz + 1.0
    hl = math.sqrt(hx * hx + hy * hy + hz * hz)
    ndoth = np.clip(nx * (hx / hl) + ny * (hy / hl) + nz * (hz / hl), 0.0, 1.0)

    spec_power = np.clip(2.0 / np.maximum(roughness ** 3.2, 0.012), 4.0, 140.0)
    spec = (ndoth ** spec_power) * (0.28 + 1.35 * metallic)

    # Cavity ambient occlusion (darkens recessed grooves vs neighbors)
    blur_h = np.asarray(
        Image.fromarray(np.clip(height * 255.0, 0, 255).astype(np.uint8)).filter(
            ImageFilter.GaussianBlur(radius=3.5)
        ),
        dtype=np.float32,
    ) / 255.0
    cavity_ao = np.clip(0.86 + (height - blur_h) * 1.45, 0.42, 1.15)

    # Diffuse drops for metals, specular takes metal tint
    diffuse_col = base_rgb * (1.0 - 0.65 * metallic[..., None])
    ambient = 0.34 * cavity_ao[..., None]
    light_col = (
        ambient * diffuse_col
        + ndotl[..., None] * 0.85 * cavity_ao[..., None] * diffuse_col
        + ndot_fill[..., None] * 0.24 * diffuse_col
    )

    spec_tint = (1.0 - metallic[..., None]) * np.array([1.0, 0.96, 0.90], dtype=np.float32) + metallic[..., None] * np.clip(
        base_rgb * 1.35 + 0.15, 0.0, 1.0
    )
    out = light_col + spec[..., None] * spec_tint * 0.72
    if emissive is not None:
        out += emissive

    out_u8 = np.clip(out * 255.0, 0.0, 255.0).astype(np.uint8)
    a_u8 = np.clip(alpha * 255.0, 0.0, 255.0).astype(np.uint8)
    rgba = np.dstack([out_u8, a_u8])
    return Image.fromarray(rgba, mode="RGBA")


def _add_corner_rivets(height: np.ndarray, metallic: np.ndarray, base_rgb: np.ndarray, pts: list[tuple[float, float]], rad: float):
    h, w = height.shape
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    gold = np.array([0.84, 0.68, 0.38], dtype=np.float32)
    for px, py in pts:
        d = np.sqrt((xx - px) ** 2 + (yy - py) ** 2)
        dome = np.clip(1.0 - (d / rad) ** 2, 0.0, 1.0)
        mask = d <= rad
        height[mask] = np.maximum(height[mask], height[mask] + 0.26 * np.sqrt(dome[mask]))
        metallic[mask] = 0.95
        base_rgb[mask] = gold * (0.85 + 0.25 * dome[mask, None])
        # Dark recessed seat ring right around stud
        seat = (d > rad) & (d <= rad + 2.2)
        height[seat] -= 0.10


def build_panel_obsidian() -> None:
    size = 512
    sdf = _rounded_rect_sdf(size, size, margin_x=6.0, margin_y=6.0, radius=16.0)
    grain = _fbm_noise(size, size, seed=101, octaves=(5, 12, 28, 64))
    fine = _fbm_noise(size, size, seed=202, octaves=(24, 60, 120))

    inside = sdf <= 0.0
    alpha = np.clip(-sdf * 0.75, 0.0, 0.96)
    alpha[~inside] = 0.0

    # Distance inward from outer silhouette edge (d_in >= 0 inside)
    d_in = np.maximum(-sdf, 0.0)

    # Height profile: outer bronze chamfer (0..18px) -> recessed groove (18..26px) -> inner hairline (26..30px) -> recessed stone face
    height = np.zeros((size, size), dtype=np.float32)
    metallic = np.zeros((size, size), dtype=np.float32)
    roughness = np.full((size, size), 0.78, dtype=np.float32)
    base_rgb = np.zeros((size, size, 3), dtype=np.float32)

    obsidian_col = np.array([0.055, 0.058, 0.075], dtype=np.float32)
    bronze_col = np.array([0.76, 0.60, 0.34], dtype=np.float32)
    iron_col = np.array([0.16, 0.17, 0.21], dtype=np.float32)

    # Deep stone interior with granite relief
    height[:] = 0.20 + (grain - 0.5) * 0.045 + (fine - 0.5) * 0.025
    base_rgb[:] = obsidian_col * (0.78 + 0.42 * grain[..., None]) * (0.90 + 0.20 * fine[..., None])

    # Outer forged iron lip (0..6px)
    m_iron = (d_in >= 0.0) & (d_in < 6.5)
    t_iron = np.clip(d_in / 6.5, 0.0, 1.0)
    height[m_iron] = 0.55 + 0.16 * np.sin(t_iron[m_iron] * math.pi)
    base_rgb[m_iron] = iron_col
    metallic[m_iron] = 0.78
    roughness[m_iron] = 0.36

    # Main 3D beveled antique-bronze molding (6.5..18px)
    m_bronze = (d_in >= 6.5) & (d_in < 18.5)
    t_br = np.clip((d_in - 6.5) / 12.0, 0.0, 1.0)
    height[m_bronze] = 0.68 + 0.32 * np.sin(t_br[m_bronze] * math.pi)
    base_rgb[m_bronze] = bronze_col * (0.85 + 0.25 * fine[m_bronze, None])
    metallic[m_bronze] = 0.92
    roughness[m_bronze] = 0.26

    # Recessed dark moat groove (18.5..25px)
    m_moat = (d_in >= 18.5) & (d_in < 25.0)
    height[m_moat] = 0.12
    base_rgb[m_moat] = obsidian_col * 0.55
    metallic[m_moat] = 0.20
    roughness[m_moat] = 0.75

    # Inner bronze filigree trim line (25..29px)
    m_inner = (d_in >= 25.0) & (d_in < 29.0)
    height[m_inner] = 0.48
    base_rgb[m_inner] = bronze_col * 0.78
    metallic[m_inner] = 0.88
    roughness[m_inner] = 0.30

    # Inner drop shadow bleeding into stone face (29..54px)
    m_shadow = (d_in >= 29.0) & (d_in < 54.0)
    ts = (d_in[m_shadow] - 29.0) / 25.0
    base_rgb[m_shadow] *= (0.48 + 0.52 * ts)[..., None]

    # Four embossed corner medallions & rivets strictly inside 9-slice margin (margin = 61px)
    _add_corner_rivets(height, metallic, base_rgb, [(34.0, 34.0), (size - 35.0, 34.0), (34.0, size - 35.0), (size - 35.0, size - 35.0)], rad=6.8)

    img = _shade_heightfield_pbr(height, base_rgb, metallic, roughness, alpha, bump_scale=5.0)
    img.save(UI_DIR / "T_UI_PanelObsidian.png", "PNG")


def build_button_triplet(stem: str, face_col_top: tuple[float, float, float], face_col_bot: tuple[float, float, float], frame_col: tuple[float, float, float]) -> None:
    w, h = 512, 128
    sdf = _rounded_rect_sdf(h, w, margin_x=5.0, margin_y=5.0, radius=10.0)
    grain = _fbm_noise(h, w, seed=303 if "Crimson" in stem else 404, octaves=(6, 16, 40))
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)
    v_norm = yy / float(h - 1)
    d_in = np.maximum(-sdf, 0.0)

    for state in ("Normal", "Hovered", "Pressed"):
        alpha = np.clip(-sdf * 0.85, 0.0, 0.99)
        alpha[sdf > 0.0] = 0.0

        height = np.zeros((h, w), dtype=np.float32)
        metallic = np.zeros((h, w), dtype=np.float32)
        roughness = np.full((h, w), 0.44, dtype=np.float32)
        base_rgb = np.zeros((h, w, 3), dtype=np.float32)
        emissive = np.zeros((h, w, 3), dtype=np.float32)

        # Face gradient + subtle convex pill curvature
        c_top = np.array(face_col_top, dtype=np.float32)
        c_bot = np.array(face_col_bot, dtype=np.float32)
        face = c_top * (1.0 - v_norm[..., None]) + c_bot * v_norm[..., None]
        if state == "Hovered":
            face *= 1.24
        elif state == "Pressed":
            face *= 0.72

        base_rgb[:] = face * (0.86 + 0.28 * grain[..., None])
        convex = np.sin(np.clip((d_in - 16.0) / max(1.0, (h * 0.5 - 16.0)), 0.0, 1.0) * (math.pi * 0.5))
        height[:] = 0.44 + 0.14 * convex + (grain - 0.5) * 0.03

        # Outer iron chamfer (0..5px)
        m_lip = (d_in >= 0.0) & (d_in < 5.5)
        height[m_lip] = 0.52 + 0.12 * (d_in[m_lip] / 5.5)
        base_rgb[m_lip] = np.array([0.16, 0.14, 0.14], dtype=np.float32)
        metallic[m_lip] = 0.82
        roughness[m_lip] = 0.34

        # Cast bronze/gold 3D bevel frame (5.5..14.5px)
        m_frame = (d_in >= 5.5) & (d_in < 14.5)
        tf = (d_in[m_frame] - 5.5) / 9.0
        height[m_frame] = 0.72 + 0.28 * np.sin(tf * math.pi)
        fcol = np.array(frame_col, dtype=np.float32) * (1.18 if state == "Hovered" else (0.85 if state == "Pressed" else 1.0))
        base_rgb[m_frame] = fcol
        metallic[m_frame] = 0.94
        roughness[m_frame] = 0.22 if state != "Pressed" else 0.34

        # Inner drop-shadow groove (14.5..20px)
        m_gr = (d_in >= 14.5) & (d_in < 20.5)
        height[m_gr] = 0.28
        base_rgb[m_gr] *= 0.55

        if state == "Hovered":
            rim_glow = np.exp(-((d_in - 16.0) / 7.5) ** 2)
            glow_col = np.array([1.0, 0.58, 0.16], dtype=np.float32) if "Crimson" in stem else np.array([0.85, 0.64, 0.24], dtype=np.float32)
            emissive += rim_glow[..., None] * glow_col * 0.24

        _add_corner_rivets(height, metallic, base_rgb, [(24.0, 22.0), (w - 25.0, 22.0), (24.0, h - 23.0), (w - 25.0, h - 23.0)], rad=4.6)

        # Pressed inverts top-down light so button physically sinks into bezel!
        ld = (0.25, 0.80, 0.54) if state == "Pressed" else (-0.25, -0.82, 0.52)
        img = _shade_heightfield_pbr(height, base_rgb, metallic, roughness, alpha, emissive=emissive, bump_scale=5.2, light_dir=ld)
        img.save(UI_DIR / f"{stem}_{state}.png", "PNG")


def build_skill_sockets() -> None:
    size = 256
    sdf_outer = _rounded_rect_sdf(size, size, margin_x=4.0, margin_y=4.0, radius=14.0)
    sdf_inner = _rounded_rect_sdf(size, size, margin_x=26.0, margin_y=26.0, radius=6.0)
    grain = _fbm_noise(size, size, seed=505, octaves=(6, 18, 48))

    for armed in (False, True):
        height = np.zeros((size, size), dtype=np.float32)
        metallic = np.zeros((size, size), dtype=np.float32)
        roughness = np.full((size, size), 0.30, dtype=np.float32)
        base_rgb = np.zeros((size, size, 3), dtype=np.float32)
        emissive = np.zeros((size, size, 3), dtype=np.float32)

        in_bezel = (sdf_outer <= 0.0) & (sdf_inner >= -10.0)
        alpha = np.zeros((size, size), dtype=np.float32)

        # Solid 3D frame around open center window so bezel overlaps the spell icon cleanly
        d_outer = np.maximum(-sdf_outer, 0.0)
        d_inner = -sdf_inner  # positive inside aperture

        bronze = np.array([0.82, 0.64, 0.34] if not armed else [0.98, 0.80, 0.42], dtype=np.float32)
        dark_iron = np.array([0.14, 0.14, 0.18], dtype=np.float32)

        height[in_bezel] = 0.62 + 0.25 * np.sin(np.clip(d_outer[in_bezel] / 22.0, 0.0, 1.0) * math.pi)
        base_rgb[in_bezel] = bronze * (0.85 + 0.22 * grain[in_bezel, None])
        metallic[in_bezel] = 0.94
        roughness[in_bezel] = 0.24

        m_lip = (d_outer >= 0.0) & (d_outer < 5.0)
        base_rgb[m_lip] = dark_iron
        metallic[m_lip] = 0.80

        # Smooth vignette shadow falling inward over icon aperture (0..18px inside opening)
        in_window = sdf_inner < 0.0
        alpha[(sdf_outer <= 0.0) & (~in_window)] = 1.0
        win_fade = np.clip(1.0 - (d_inner / 16.0), 0.0, 1.0)
        alpha[in_window] = (win_fade[in_window] ** 1.8) * 0.82
        base_rgb[in_window] = dark_iron * 0.35

        if armed:
            glow_band = np.exp(-((np.abs(sdf_inner) - 4.0) / 8.0) ** 2)
            emissive += glow_band[..., None] * np.array([1.0, 0.68, 0.18], dtype=np.float32) * 0.42

        _add_corner_rivets(height, metallic, base_rgb, [(18.0, 18.0), (size - 19.0, 18.0), (18.0, size - 19.0), (size - 19.0, size - 19.0)], rad=5.2)

        suffix = "Armed" if armed else "Normal"
        img = _shade_heightfield_pbr(height, base_rgb, metallic, roughness, alpha, emissive=emissive, bump_scale=5.0)
        img.save(UI_DIR / f"T_UI_SkillSocket_{suffix}.png", "PNG")


def build_globes() -> None:
    size = 256
    yy, xx = np.mgrid[0:size, 0:size].astype(np.float32)
    c = (size - 1) * 0.5
    dx = (xx - c) / c
    dy = (yy - c) / c
    r = np.sqrt(dx * dx + dy * dy)
    swirl = _fbm_noise(size, size, seed=707, octaves=(4, 10, 24, 52))
    swirl2 = _fbm_noise(size, size, seed=808, octaves=(6, 14, 36))

    # 1. Back chamber (dark concave smoked crystal sphere)
    r_liq = 0.82
    alpha_back = np.clip((r_liq - r) * 32.0, 0.0, 0.96)
    nz = np.sqrt(np.maximum(0.0, 1.0 - np.clip(r / r_liq, 0.0, 1.0) ** 2))
    back_rgb = np.dstack([
        0.025 + 0.03 * nz,
        0.028 + 0.03 * nz,
        0.038 + 0.045 * nz,
    ]).astype(np.float32)
    Image.fromarray(
        np.dstack([np.clip(back_rgb * 255, 0, 255).astype(np.uint8), (alpha_back * 255).astype(np.uint8)]),
        mode="RGBA",
    ).save(UI_DIR / "T_UI_GlobeBack.png", "PNG")

    # 2. HP & MP Spherically Shaded Elixirs
    for kind, deep_c, mid_c, core_c in [
        ("HP", np.array([0.24, 0.02, 0.03]), np.array([0.68, 0.07, 0.09]), np.array([0.96, 0.26, 0.18])),
        ("MP", np.array([0.03, 0.10, 0.28]), np.array([0.08, 0.34, 0.78]), np.array([0.24, 0.72, 1.00])),
    ]:
        sphere_z = np.sqrt(np.maximum(0.0, 1.0 - np.clip(r / r_liq, 0.0, 1.0) ** 2))
        light_spot = np.clip(1.0 - np.sqrt((dx + 0.26) ** 2 + (dy + 0.28) ** 2) * 0.95, 0.0, 1.0) ** 1.6
        vein = np.clip((swirl * 0.65 + swirl2 * 0.35 - 0.35) * 1.9, 0.0, 1.0)
        col = (
            deep_c * (1.0 - sphere_z[..., None]) * 1.25
            + mid_c * (sphere_z[..., None] * (0.65 + 0.45 * vein[..., None]))
            + core_c * (light_spot[..., None] * 0.68 + (vein[..., None] ** 2) * 0.25)
        )
        alpha_f = np.clip((r_liq - r) * 32.0, 0.0, 0.98)
        Image.fromarray(
            np.dstack([np.clip(col * 255, 0, 255).astype(np.uint8), (alpha_f * 255).astype(np.uint8)]),
            mode="RGBA",
        ).save(UI_DIR / f"T_UI_GlobeFill_{kind}.png", "PNG")

    # 3. 3D Sculpted Gothic Wrought-Bronze Bezel + Specular Glass Reflection Overlay
    ang = np.arctan2(dy, dx)
    in_ring = (r >= 0.78) & (r <= 0.98)
    # 4 cardinal gothic claws reaching slightly onto glass (r = 0.68..0.82 near cos/sin peaks)
    claws = ((np.abs(np.cos(ang * 2.0)) > 0.92) & (r >= 0.68) & (r <= 0.85))
    metal_mask = in_ring | claws

    height = np.zeros((size, size), dtype=np.float32)
    metallic = np.zeros((size, size), dtype=np.float32)
    roughness = np.full((size, size), 0.28, dtype=np.float32)
    base_rgb = np.zeros((size, size, 3), dtype=np.float32)

    t_ring = np.clip((r - 0.78) / 0.20, 0.0, 1.0)
    height[in_ring] = 0.75 + 0.30 * np.sin(t_ring[in_ring] * math.pi)
    height[claws] = np.maximum(height[claws], 0.92)
    bronze = np.array([0.82, 0.65, 0.36], dtype=np.float32)
    base_rgb[metal_mask] = bronze * (0.86 + 0.24 * swirl[metal_mask, None])
    metallic[metal_mask] = 0.94
    roughness[metal_mask] = 0.24

    # Engraved ticks along bronze ring
    ticks = in_ring & (r >= 0.82) & (r <= 0.94) & (np.cos(ang * 16.0) > 0.78)
    height[ticks] += 0.14

    alpha_rim = np.zeros((size, size), dtype=np.float32)
    alpha_rim[in_ring] = np.clip((0.98 - r[in_ring]) * 36.0, 0.0, 1.0)
    alpha_rim[claws] = 1.0

    bezel_img = np.asarray(
        _shade_heightfield_pbr(height, base_rgb, metallic, roughness, alpha_rim, bump_scale=5.5),
        dtype=np.float32,
    ) / 255.0

    # Specular glass highlight crescent top-left & caustic crescent bottom-right inside glass
    inside_glass = r < 0.79
    spec_crescent = np.clip(1.0 - np.abs(r - 0.62) * 9.5, 0.0, 1.0) * np.clip((-dx - dy) * 0.85, 0.0, 1.0)
    glint_dot = np.exp(-(((dx + 0.34) ** 2 + (dy + 0.36) ** 2) / 0.012))
    bot_caustic = np.clip(1.0 - np.abs(r - 0.68) * 11.0, 0.0, 1.0) * np.clip((dx + dy) * 0.75, 0.0, 1.0)
    glass_a = np.where(inside_glass, np.clip(spec_crescent * 0.42 + glint_dot * 0.55 + bot_caustic * 0.18, 0.0, 0.78), 0.0)
    glass_col = np.dstack([
        np.full_like(glass_a, 0.96),
        np.full_like(glass_a, 0.94),
        np.full_like(glass_a, 0.98) * (1.0 - 0.12 * bot_caustic),
        glass_a,
    ])

    # Composite bezel over glass reflections
    ba = bezel_img[..., 3:4]
    ga = glass_col[..., 3:4]
    out_a = ba + ga * (1.0 - ba)
    out_rgb = bezel_img[..., :3] * ba + glass_col[..., :3] * ga * (1.0 - ba)
    safe_a = np.maximum(out_a, 1e-5)
    out_rgb = np.where(out_a > 0.001, out_rgb / safe_a, 0.0)

    Image.fromarray(
        np.dstack([np.clip(out_rgb * 255, 0, 255).astype(np.uint8), np.clip(out_a[..., 0] * 255, 0, 255).astype(np.uint8)]),
        mode="RGBA",
    ).save(UI_DIR / "T_UI_GlobeGlassRim.png", "PNG")


def build_action_dock_pedestal() -> None:
    w, h = 1200, 180
    grain = _fbm_noise(h, w, seed=909, octaves=(6, 18, 48))
    yy, xx = np.mgrid[0:h, 0:w].astype(np.float32)

    # Central horizontal bar + 2 globe housings at left & right
    sdf_center = _rounded_rect_sdf(h, w, margin_x=115.0, margin_y=28.0, radius=14.0)
    d_left = np.sqrt((xx - 102.0) ** 2 + (yy - 90.0) ** 2) - 82.0
    d_right = np.sqrt((xx - (w - 103.0)) ** 2 + (yy - 90.0) ** 2) - 82.0
    sdf = np.minimum(sdf_center, np.minimum(d_left, d_right))

    alpha = np.clip(-sdf * 0.75, 0.0, 0.94)
    alpha[sdf > 0.0] = 0.0
    d_in = np.maximum(-sdf, 0.0)

    height = 0.25 + (grain - 0.5) * 0.05
    metallic = np.zeros((h, w), dtype=np.float32)
    roughness = np.full((h, w), 0.70, dtype=np.float32)
    base_rgb = np.zeros((h, w, 3), dtype=np.float32)
    base_rgb[:] = np.array([0.06, 0.06, 0.08], dtype=np.float32) * (0.80 + 0.40 * grain[..., None])

    m_bronze = (d_in >= 3.0) & (d_in < 13.0)
    height[m_bronze] = 0.74 + 0.24 * np.sin(((d_in[m_bronze] - 3.0) / 10.0) * math.pi)
    base_rgb[m_bronze] = np.array([0.74, 0.58, 0.32], dtype=np.float32)
    metallic[m_bronze] = 0.90
    roughness[m_bronze] = 0.28

    img = _shade_heightfield_pbr(height, base_rgb, metallic, roughness, alpha, bump_scale=4.8)
    img.save(UI_DIR / "T_UI_ActionDockPedestal.png", "PNG")


def main() -> int:
    UI_DIR.mkdir(parents=True, exist_ok=True)
    build_panel_obsidian()
    build_button_triplet("T_UI_BtnCrimson", (0.58, 0.09, 0.11), (0.28, 0.03, 0.05), (0.86, 0.69, 0.39))
    build_button_triplet("T_UI_BtnStone", (0.14, 0.14, 0.18), (0.07, 0.07, 0.09), (0.68, 0.54, 0.31))
    build_skill_sockets()
    build_globes()
    build_action_dock_pedestal()

    files = sorted(UI_DIR.glob("T_UI_*.png"))
    print(f"Generated {len(files)} Volumetric Diablo IV UI textures in {UI_DIR}:")
    for f in files:
        print(f"  - {f.name:30s} ({f.stat().st_size // 1024} KB)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
