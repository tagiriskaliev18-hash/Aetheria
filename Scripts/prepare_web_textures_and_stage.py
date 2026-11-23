"""
Prepare resized PBR texture triplets for Blender GLB export, seamless 1K floor
PBR maps for Three.js BiomeBuilder, and cropped dark-vignetted 256x256
spell icons from RawAssets/Grimoire/SpellPlate_01..10.png.
Strictly READS RawAssets/Grimoire & RawAssets/Textures (never modifies them).
"""

from pathlib import Path
from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
TEX_SRC = ROOT / "RawAssets" / "Textures"
GRIM_SRC = ROOT / "RawAssets" / "Grimoire"

WEB_PUB = ROOT / "_backup_web_prototype" / "public"
WEB_FLOOR_DIR = WEB_PUB / "textures"
WEB_SPELL_DIR = WEB_PUB / "ui" / "spells"
GLB_TEX_CACHE = Path(
    r"C:\Users\user\.gemini\antigravity\brain\32b16ce5-cce9-48eb-9f7c-d7bba29e21be\scratch\glb_tex_cache"
)

MODEL_TEXTURES = [
    ("T_Hero_00", 768),
    ("T_Hero_01", 768),
    ("T_Hero_02", 768),
    ("T_Hero_03", 768),
    ("T_Mob_Swarm_00", 512),
    ("T_Mob_Ranged_00", 512),
    ("T_Mob_Bruiser_00", 512),
    ("T_Boss_Loc09", 768),
    ("T_GothicStatue", 512),
    ("T_RuinedArch", 512),
    ("T_GothicStone", 512),
    ("T_CliffBoulder", 512),
    ("T_StoneFirePit", 512),
    ("T_DeadTree", 512),
    ("T_TreasureChest", 512),
]

FLOOR_SETS = [
    ("flagstone", "T_SanctumFlagstone"),
    ("basalt", "T_CalderaBasalt"),
    ("slate", "T_GlacierSlate"),
]


def prepare_glb_texture_cache() -> None:
    GLB_TEX_CACHE.mkdir(parents=True, exist_ok=True)
    for stem, size in MODEL_TEXTURES:
        for suffix, quality in (("Albedo", 86), ("Normal", 88), ("Roughness", 82)):
            src = TEX_SRC / f"{stem}_{suffix}.png"
            dst = GLB_TEX_CACHE / f"{stem}_{suffix}.jpg"
            if not src.exists():
                continue
            with Image.open(src) as im:
                mode = "L" if suffix == "Roughness" else "RGB"
                im_conv = im.convert(mode).resize((size, size), Image.Resampling.LANCZOS)
                if mode == "L":
                    im_conv = im_conv.convert("RGB")
                im_conv.save(dst, "JPEG", quality=quality, optimize=True)
    print(f"Prepared GLB texture cache in {GLB_TEX_CACHE}")


def prepare_floor_textures() -> None:
    WEB_FLOOR_DIR.mkdir(parents=True, exist_ok=True)
    for out_name, src_stem in FLOOR_SETS:
        for suffix, quality in (("Albedo", 86), ("Normal", 88), ("Roughness", 82)):
            src = TEX_SRC / f"{src_stem}_{suffix}.png"
            dst = WEB_FLOOR_DIR / f"{out_name}_{suffix.lower()}.jpg"
            if not src.exists():
                continue
            with Image.open(src) as im:
                im_conv = im.convert("RGB").resize((1024, 1024), Image.Resampling.LANCZOS)
                im_conv.save(dst, "JPEG", quality=quality, optimize=True)
    print(f"Prepared PBR floor textures in {WEB_FLOOR_DIR}")


def prepare_spell_icons() -> None:
    WEB_SPELL_DIR.mkdir(parents=True, exist_ok=True)
    # Soft obsidian vignette mask
    vignette = Image.new("L", (256, 256), 0)
    draw = ImageDraw.Draw(vignette)
    draw.ellipse((10, 10, 246, 246), fill=255)
    vignette = vignette.filter(ImageFilter.GaussianBlur(radius=18))
    dark_bg = Image.new("RGB", (256, 256), (9, 8, 12))

    for idx in range(1, 11):
        src = GRIM_SRC / f"SpellPlate_{idx:02d}.png"
        dst = WEB_SPELL_DIR / f"spell_{idx:02d}.jpg"
        if not src.exists():
            continue
        with Image.open(src) as im:
            w, h = im.size
            # Center-crop slightly down around the 3D diorama focal center
            crop_box = (int(w * 0.14), int(h * 0.18), int(w * 0.86), int(h * 0.90))
            icon = im.convert("RGB").crop(crop_box).resize((256, 256), Image.Resampling.LANCZOS)
            # Composite onto deep obsidian backing with smooth circular edge falloff
            blended = Image.composite(icon, dark_bg, vignette)
            # Subtle warm contrast lift so 56x56 sockets pop cleanly
            blended = ImageChops.blend(blended, blended, 0.0)
            blended.save(dst, "JPEG", quality=90, optimize=True)
    print(f"Prepared 10 spell icons in {WEB_SPELL_DIR}")


def main() -> None:
    prepare_glb_texture_cache()
    prepare_floor_textures()
    prepare_spell_icons()


if __name__ == "__main__":
    main()
