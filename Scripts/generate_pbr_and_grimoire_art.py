import math
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parent.parent
POLY_TEX = Path(r"D:\Tools\assets\polyhaven\textures")
OUT_TEX = ROOT / "RawAssets" / "Textures"
OUT_GRIM = ROOT / "RawAssets" / "Grimoire"

TEX_SIZE = 2048


def load_resized(rel_path: str, size: int = TEX_SIZE, mode: str = "RGB") -> np.ndarray:
    img = Image.open(POLY_TEX / rel_path).convert(mode)
    if img.size != (size, size):
        img = img.resize((size, size), Image.Resampling.LANCZOS)
    return np.asarray(img, dtype=np.float32)


def save_png(arr: np.ndarray, out_path: Path, mode: str = "RGB") -> None:
    clamped = np.clip(arr, 0.0, 255.0).astype(np.uint8)
    Image.fromarray(clamped, mode=mode).save(out_path, optimize=True)


def to_dx_normal(nor_gl: np.ndarray) -> np.ndarray:
    out = nor_gl.copy()
    out[..., 1] = 255.0 - out[..., 1]  # Flip Green channel OpenGL -> DirectX (UE5)
    return out


def generate_pbr_sets() -> None:
    OUT_TEX.mkdir(parents=True, exist_ok=True)

    # 1. T_SanctumFlagstone (Diablo-style damp gothic flagstone with puddles in crevices)
    diff = load_resized("large_grey_tiles/large_grey_tiles_diff_2k.jpg")
    nor = to_dx_normal(load_resized("large_grey_tiles/large_grey_tiles_nor_gl_2k.jpg"))
    rough = load_resized("large_grey_tiles/large_grey_tiles_rough_2k.jpg", mode="L")
    disp = load_resized("large_grey_tiles/large_grey_tiles_disp_2k.jpg", mode="L")

    # Moody dark gothic slate grading + damp puddle mask in low displacement regions
    puddle = np.clip((118.0 - disp) / 48.0, 0.0, 1.0)
    albedo = diff * np.array([0.46, 0.48, 0.54], dtype=np.float32)
    albedo = albedo * (1.0 - puddle[..., None] * 0.42)
    rough_wet = rough * (1.0 - puddle * 0.80) + puddle * 28.0  # ~0.11 wet specular puddles
    # Flatten normals slightly inside puddles for realistic standing water reflections
    flat_n = np.array([128.0, 128.0, 255.0], dtype=np.float32)
    nor_wet = nor * (1.0 - puddle[..., None] * 0.72) + flat_n * (puddle[..., None] * 0.72)

    save_png(albedo, OUT_TEX / "T_SanctumFlagstone_Albedo.png")
    save_png(nor_wet, OUT_TEX / "T_SanctumFlagstone_Normal.png")
    save_png(rough_wet, OUT_TEX / "T_SanctumFlagstone_Roughness.png", mode="L")
    print("Built T_SanctumFlagstone (Albedo, Normal DX, Roughness)")

    # 2. T_CalderaBasalt (Charred volcanic stone + glowing magma fissures)
    r_diff = load_resized("rocky_trail_02/rocky_trail_02_diff_2k.jpg")
    r_nor = to_dx_normal(load_resized("rocky_trail_02/rocky_trail_02_nor_gl_2k.jpg"))
    r_disp = load_resized("rocky_trail_02/rocky_trail_02_disp_2k.jpg", mode="L")

    cracks = np.clip((92.0 - r_disp) / 36.0, 0.0, 1.0) ** 1.6
    basalt_alb = r_diff * np.array([0.28, 0.22, 0.20], dtype=np.float32)
    basalt_alb = basalt_alb * (1.0 - cracks[..., None] * 0.5) + cracks[..., None] * np.array([255.0, 95.0, 22.0])
    emissive = np.zeros_like(r_diff)
    emissive[..., 0] = cracks * 255.0
    emissive[..., 1] = (cracks ** 1.4) * 125.0
    emissive[..., 2] = (cracks ** 2.2) * 35.0

    save_png(basalt_alb, OUT_TEX / "T_CalderaBasalt_Albedo.png")
    save_png(r_nor, OUT_TEX / "T_CalderaBasalt_Normal.png")
    save_png(emissive, OUT_TEX / "T_CalderaBasalt_Emissive.png")
    print("Built T_CalderaBasalt (Albedo, Normal DX, Emissive)")

    # 3. T_GlacierSlate (Frozen blue-black slate with glossy ice veins)
    g_diff = load_resized("granite_tile/granite_tile_diff_2k.jpg")
    g_nor = to_dx_normal(load_resized("granite_tile/granite_tile_nor_gl_2k.jpg"))
    g_rough = load_resized("granite_tile/granite_tile_rough_2k.jpg", mode="L")
    g_disp = load_resized("granite_tile/granite_tile_disp_2k.jpg", mode="L")

    frost = np.clip((g_disp - 135.0) / 70.0, 0.0, 1.0)
    ice_alb = g_diff * np.array([0.36, 0.48, 0.68], dtype=np.float32) + frost[..., None] * np.array([55.0, 95.0, 125.0])
    ice_rough = np.clip(g_rough * 0.55 - frost * 45.0, 22.0, 210.0)

    save_png(ice_alb, OUT_TEX / "T_GlacierSlate_Albedo.png")
    save_png(g_nor, OUT_TEX / "T_GlacierSlate_Normal.png")
    save_png(ice_rough, OUT_TEX / "T_GlacierSlate_Roughness.png", mode="L")
    print("Built T_GlacierSlate (Albedo, Normal DX, Roughness)")

    # 4. T_GothicStone (Weathered carved cathedral stonework for pillars, arches, golems)
    stone_alb = g_diff * np.array([0.42, 0.41, 0.44], dtype=np.float32)
    ao = np.clip(g_disp / 160.0, 0.45, 1.0)
    stone_alb *= ao[..., None]
    save_png(stone_alb, OUT_TEX / "T_GothicStone_Albedo.png")
    save_png(g_nor, OUT_TEX / "T_GothicStone_Normal.png")
    save_png(np.clip(g_rough * 1.05, 80.0, 240.0), OUT_TEX / "T_GothicStone_Roughness.png", mode="L")
    print("Built T_GothicStone (Albedo, Normal DX, Roughness)")

    # 5. T_DarkIronCloth (Forged dark iron & heavy charcoal cloth for characters & braziers)
    c_diff = load_resized("concrete_floor_02/concrete_floor_02_diff_2k.jpg")
    c_nor = to_dx_normal(load_resized("concrete_floor_02/concrete_floor_02_nor_gl_2k.jpg"))
    c_rough = load_resized("concrete_floor_02/concrete_floor_02_rough_2k.jpg", mode="L")

    iron_alb = c_diff * np.array([0.22, 0.23, 0.27], dtype=np.float32)
    iron_rough = np.clip(c_rough * 0.68, 45.0, 185.0)
    save_png(iron_alb, OUT_TEX / "T_DarkIronCloth_Albedo.png")
    save_png(c_nor, OUT_TEX / "T_DarkIronCloth_Normal.png")
    save_png(iron_rough, OUT_TEX / "T_DarkIronCloth_Roughness.png", mode="L")
    print("Built T_DarkIronCloth (Albedo, Normal DX, Roughness)")


def make_parchment_noise(w: int, h: int, seed: int = 42) -> np.ndarray:
    rng = np.random.default_rng(seed)
    low = rng.normal(0.0, 1.0, (h // 32, w // 32))
    mid = rng.normal(0.0, 1.0, (h // 8, w // 8))
    high = rng.normal(0.0, 1.0, (h, w))
    low_img = np.asarray(Image.fromarray(((low * 25 + 128).clip(0, 255)).astype(np.uint8)).resize((w, h), Image.Resampling.BICUBIC), dtype=np.float32) - 128.0
    mid_img = np.asarray(Image.fromarray(((mid * 25 + 128).clip(0, 255)).astype(np.uint8)).resize((w, h), Image.Resampling.BICUBIC), dtype=np.float32) - 128.0
    return low_img * 0.45 + mid_img * 0.28 + high * 4.5


def build_grimoire_spread() -> None:
    OUT_GRIM.mkdir(parents=True, exist_ok=True)
    W, H = 1600, 1040

    # Deep leather base
    arr = np.zeros((H, W, 3), dtype=np.float32)
    arr[..., 0] = 34.0
    arr[..., 1] = 22.0
    arr[..., 2] = 17.0
    grain = make_parchment_noise(W, H, seed=77)
    arr += grain[..., None] * 0.35

    img = Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8), mode="RGB")
    draw = ImageDraw.Draw(img)

    # Outer leather frame + tarnished brass clasps
    draw.rectangle([18, 18, W - 18, H - 18], outline=(138, 104, 54), width=5)
    draw.rectangle([30, 30, W - 30, H - 30], outline=(82, 58, 32), width=2)
    for cx, cy in [(18, 18), (W - 76, 18), (18, H - 76), (W - 76, H - 76)]:
        draw.rectangle([cx, cy, cx + 58, cy + 58], fill=(120, 92, 46), outline=(196, 162, 92), width=2)

    # Two warm aged vellum pages
    page_l = (66, 52, W // 2 - 14, H - 52)
    page_r = (W // 2 + 14, 52, W - 66, H - 52)
    p_arr = np.asarray(img, dtype=np.float32)
    yy, xx = np.mgrid[0:H, 0:W]

    for px0, py0, px1, py1 in [page_l, page_r]:
        mask = (xx >= px0) & (xx <= px1) & (yy >= py0) & (yy <= py1)
        cx_p = (px0 + px1) * 0.5
        cy_p = (py0 + py1) * 0.5
        nx = np.abs(xx - cx_p) / max(1.0, (px1 - px0) * 0.5)
        ny = np.abs(yy - cy_p) / max(1.0, (py1 - py0) * 0.5)
        vign = np.clip(1.0 - (nx ** 3.2) * 0.22 - (ny ** 3.2) * 0.22, 0.55, 1.0)
        # Central spine fold shadow
        spine_dist = np.abs(xx - W * 0.5)
        spine_shade = np.clip(spine_dist / 95.0, 0.42, 1.0)

        vellum = np.zeros_like(p_arr)
        vellum[..., 0] = (224.0 + grain * 0.45) * vign * spine_shade
        vellum[..., 1] = (204.0 + grain * 0.40) * vign * spine_shade
        vellum[..., 2] = (164.0 + grain * 0.34) * vign * spine_shade
        p_arr[mask] = vellum[mask]

    # Central spine leather trough
    spine_mask = (np.abs(xx - W * 0.5) < 14) & (yy >= 44) & (yy <= H - 44)
    p_arr[spine_mask] *= 0.32

    spread = Image.fromarray(np.clip(p_arr, 0, 255).astype(np.uint8), mode="RGB")
    sdraw = ImageDraw.Draw(spread)

    # Inked border filigree & astrolabe tick rings on each page
    for px0, py0, px1, py1 in [page_l, page_r]:
        sdraw.rectangle([px0 + 18, py0 + 18, px1 - 18, py1 - 18], outline=(78, 56, 38), width=2)
        sdraw.rectangle([px0 + 26, py0 + 26, px1 - 26, py1 - 26], outline=(112, 82, 54), width=1)

    # Bookmark ribbon in bottom spine
    sdraw.polygon([(W // 2 - 12, H - 52), (W // 2 + 12, H - 52), (W // 2 + 16, H - 6), (W // 2, H - 22), (W // 2 - 16, H - 6)], fill=(128, 28, 28), outline=(185, 145, 72))
    spread.save(OUT_GRIM / "T_GrimoireBookSpread.png", optimize=True)
    print("Built T_GrimoireBookSpread.png")


def build_spell_plates() -> None:
    OUT_GRIM.mkdir(parents=True, exist_ok=True)
    S = 640
    cx, cy = S // 2, S // 2
    noise = make_parchment_noise(S, S, seed=909)
    yy, xx = np.mgrid[0:S, 0:S]
    rad = np.hypot(xx - cx, yy - cy)

    plates = [
        ("SpellPlate_01.png", "QQQ", (255, 105, 32), (255, 220, 120), "fireball"),
        ("SpellPlate_02.png", "EEE", (135, 65, 255), (0, 240, 255), "blackhole"),
        ("SpellPlate_03.png", "WWW", (0, 225, 255), (140, 190, 255), "chronosphere"),
        ("SpellPlate_04.png", "QQE", (255, 85, 25), (165, 75, 255), "meteor"),
        ("SpellPlate_05.png", "QWW", (40, 235, 255), (245, 250, 255), "lightning"),
        ("SpellPlate_06.png", "QQW", (255, 130, 45), (0, 245, 255), "beam"),
        ("SpellPlate_07.png", "WWE", (60, 170, 255), (0, 245, 255), "blink"),
        ("SpellPlate_08.png", "QEE", (175, 65, 255), (255, 95, 35), "pillars"),
        ("SpellPlate_09.png", "WEE", (95, 120, 255), (0, 245, 255), "prism"),
        ("SpellPlate_10.png", "QWE", (245, 240, 225), (0, 245, 255), "triad"),
    ]

    for fname, combo, col1, col2, kind in plates:
        # Dark antique woodcut plate base inside warm parchment vignette
        plate = np.zeros((S, S, 3), dtype=np.float32)
        bg_vign = np.clip(1.0 - (rad / (S * 0.56)) ** 2.2, 0.08, 1.0)
        plate[..., 0] = (24.0 + noise * 0.28) * (1.2 - bg_vign * 0.35) + bg_vign * 18.0
        plate[..., 1] = (20.0 + noise * 0.24) * (1.2 - bg_vign * 0.35) + bg_vign * 15.0
        plate[..., 2] = (18.0 + noise * 0.22) * (1.2 - bg_vign * 0.35) + bg_vign * 19.0

        # Painterly volumetric glow field tailored per spell
        ang = np.arctan2(yy - cy, xx - cx)
        glow = np.exp(-(rad ** 2) / (2.0 * 115.0 ** 2))
        if kind == "blackhole":
            ring = np.exp(-((rad - 115.0) ** 2) / (2.0 * 24.0 ** 2)) * (0.65 + 0.35 * np.sin(ang * 3.0 + rad * 0.04))
            horizon = (rad < 62.0).astype(np.float32)
            energy = ring * (1.0 - horizon)
            plate += energy[..., None] * np.array(col1, dtype=np.float32) * 1.15
            plate += (np.exp(-((rad - 70.0) ** 2) / 90.0))[..., None] * np.array(col2, dtype=np.float32) * 0.9
            plate[rad < 58.0] = np.array([4.0, 4.0, 6.0])
        elif kind == "chronosphere":
            dome = np.exp(-((rad - 155.0) ** 2) / (2.0 * 14.0 ** 2)) + 0.32 * glow
            plate += dome[..., None] * np.array(col1, dtype=np.float32)
        elif kind == "prism":
            wall = np.exp(-((np.abs(xx - cx)) ** 2) / 450.0) * (np.abs(yy - cy) < 180.0)
            plate += wall[..., None] * np.array(col2, dtype=np.float32)
        else:
            rays = 0.65 + 0.35 * np.cos(ang * 7.0 + rad * 0.03)
            plate += (glow * rays)[..., None] * np.array(col1, dtype=np.float32) * 0.95
            core = np.exp(-(rad ** 2) / (2.0 * 44.0 ** 2))
            plate += core[..., None] * np.array(col2, dtype=np.float32) * 0.85

        img = Image.fromarray(np.clip(plate, 0, 255).astype(np.uint8), mode="RGB")
        draw = ImageDraw.Draw(img)

        # Crisp inked sacred geometry & brass frame
        draw.rectangle([16, 16, S - 16, S - 16], outline=(168, 134, 78), width=4)
        draw.rectangle([28, 28, S - 28, S - 28], outline=(98, 76, 48), width=2)
        draw.ellipse([cx - 210, cy - 210, cx + 210, cy + 210], outline=(168, 134, 78), width=2)
        draw.ellipse([cx - 175, cy - 175, cx + 175, cy + 175], outline=(col1[0], col1[1], col1[2]), width=2)

        # Engraved tick marks around outer astrolabe ring
        for i in range(24):
            a = 2.0 * math.pi * i / 24.0
            r0 = 200.0 if i % 3 == 0 else 206.0
            r1 = 218.0
            draw.line([(cx + math.cos(a) * r0, cy + math.sin(a) * r0), (cx + math.cos(a) * r1, cy + math.sin(a) * r1)], fill=(190, 156, 94), width=2)

        # Spell-specific engraved line art
        if kind == "fireball":
            for k in range(12):
                a = 2.0 * math.pi * k / 12.0
                draw.line([(cx + math.cos(a) * 65, cy + math.sin(a) * 65), (cx + math.cos(a) * 155, cy + math.sin(a) * 155)], fill=col2, width=3)
            draw.ellipse([cx - 72, cy - 72, cx + 72, cy + 72], outline=col2, width=4)
        elif kind == "blackhole":
            draw.ellipse([cx - 60, cy - 60, cx + 60, cy + 60], fill=(5, 5, 8), outline=col2, width=3)
            draw.arc([cx - 170, cy - 64, cx + 170, cy + 64], 0, 360, fill=col1, width=4)
        elif kind == "chronosphere":
            for r in [95, 135, 165]:
                draw.ellipse([cx - r, cy - r, cx + r, cy + r], outline=col1, width=2)
            draw.line([(cx, cy), (cx, cy - 120)], fill=col2, width=4)
            draw.line([(cx, cy), (cx + 85, cy + 45)], fill=col2, width=3)
        elif kind == "meteor":
            draw.polygon([(cx + 120, cy - 140), (cx + 150, cy - 110), (cx - 15, cy + 65), (cx - 65, cy + 25)], outline=col1, width=4)
            draw.ellipse([cx - 80, cy - 10, cx + 20, cy + 90], fill=(35, 18, 16), outline=col1, width=4)
        elif kind == "lightning":
            pts = [(cx - 140, cy - 120), (cx - 40, cy - 30), (cx - 75, cy + 15), (cx + 45, cy + 40), (cx + 15, cy + 90), (cx + 145, cy + 135)]
            draw.line(pts, fill=col2, width=5)
        elif kind == "beam":
            draw.line([(cx - 175, cy), (cx + 175, cy)], fill=col2, width=8)
            for x in range(cx - 150, cx + 150, 40):
                draw.arc([x, cy - 35, x + 40, cy + 35], 0, 180, fill=col1, width=3)
        elif kind == "blink":
            draw.polygon([(cx, cy - 120), (cx + 75, cy), (cx, cy + 120), (cx - 75, cy)], outline=col2, width=4)
            draw.ellipse([cx - 35, cy - 35, cx + 35, cy + 35], outline=col1, width=3)
        elif kind == "pillars":
            for ox, h in [(-110, 110), (-55, 150), (0, 185), (55, 145), (110, 105)]:
                draw.polygon([(cx + ox - 18, cy + 110), (cx + ox + 18, cy + 110), (cx + ox + 6, cy + 110 - h), (cx + ox - 6, cy + 110 - h)], fill=(28, 18, 36), outline=col2, width=3)
        elif kind == "prism":
            draw.polygon([(cx, cy - 135), (cx + 90, cy + 95), (cx - 90, cy + 95)], outline=col2, width=4)
            draw.line([(cx - 180, cy), (cx - 40, cy)], fill=(255, 120, 40), width=5)
            draw.line([(cx + 35, cy), (cx + 175, cy - 75)], fill=col2, width=4)
            draw.line([(cx + 35, cy), (cx + 175, cy)], fill=col1, width=4)
            draw.line([(cx + 35, cy), (cx + 175, cy + 75)], fill=(200, 110, 255), width=4)
        elif kind == "triad":
            p1 = (cx, cy - 125)
            p2 = (cx - 115, cy + 85)
            p3 = (cx + 115, cy + 85)
            draw.polygon([p1, p2, p3], outline=(240, 225, 195), width=3)
            for px, py, oc in [(p1[0], p1[1], (255, 90, 35)), (p2[0], p2[1], (0, 235, 255)), (p3[0], p3[1], (160, 75, 255))]:
                draw.ellipse([px - 24, py - 24, px + 24, py + 24], fill=oc, outline=(255, 245, 230), width=2)

        # 3 small elemental orb pips along bottom inside frame
        orb_cols = {"Q": (255, 90, 35), "W": (0, 235, 255), "E": (160, 75, 255)}
        for idx_o, ch in enumerate(combo):
            ox = cx + (idx_o - 1) * 44
            oy = S - 62
            draw.ellipse([ox - 13, oy - 13, ox + 13, oy + 13], fill=orb_cols[ch], outline=(235, 215, 175), width=2)

        img = img.filter(ImageFilter.SMOOTH_MORE)
        img.save(OUT_GRIM / fname, optimize=True)
        print(f"Built {fname}")


def main() -> None:
    generate_pbr_sets()
    build_grimoire_spread()
    build_spell_plates()


if __name__ == "__main__":
    main()
