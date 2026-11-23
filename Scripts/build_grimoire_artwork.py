"""
Build genuine dark-fantasy Diablo IV Grimoire open tome spread + 10 painted SpellPlate icons.
Outputs directly to RawAssets/Grimoire/ (loaded at runtime by AetheriaSlateHUD) and
_backup_web_prototype/public/ui/spells/spell_01..10.jpg.
"""

from pathlib import Path
from PIL import Image, ImageDraw, ImageOps

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
ARTIFACTS = Path(r"C:\Users\user\.gemini\antigravity\brain\32b16ce5-cce9-48eb-9f7c-d7bba29e21be")

SPREAD_SRC = ARTIFACTS / "spellbook_open_blank_1794832181327.jpg"
GRID6_SRC = ARTIFACTS / "spells_grid_diablo_1794832181325.jpg"
GRID4_SRC = ARTIFACTS / "diablo_spells_2x2_grid_1794832183106.jpg"

GRIMOIRE_DIR = ROOT / "RawAssets" / "Grimoire"
WEB_SPELLS_DIR = ROOT / "_backup_web_prototype" / "public" / "ui" / "spells"


def build_book_spread() -> None:
    GRIMOIRE_DIR.mkdir(parents=True, exist_ok=True)
    img = Image.open(SPREAD_SRC).convert("RGB")
    # Crop slightly top/bottom black letterboxing and fit 1440x920 cleanly with centered spine at x=720
    w, h = img.size
    target_w, target_h = 1440, 920
    # Center-crop horizontally to 16:10ish aspect (target ratio 1.565 vs source ~1.778)
    crop_w = int(round(h * (target_w / target_h)))
    if crop_w < w:
        x0 = (w - crop_w) // 2
        img = img.crop((x0, 0, x0 + crop_w, h))
    img = ImageOps.fit(img, (target_w, target_h), method=Image.Resampling.LANCZOS, centering=(0.5, 0.5))
    out_path = GRIMOIRE_DIR / "T_GrimoireBookSpread.png"
    img.save(out_path, format="PNG")
    print(f"[OK] Saved {out_path} ({img.size})")


def frame_spell_tile(tile: Image.Image, size: int = 512) -> Image.Image:
    sq = ImageOps.fit(tile.convert("RGB"), (size, size), method=Image.Resampling.LANCZOS, centering=(0.5, 0.5))
    draw = ImageDraw.Draw(sq)
    # Dark wrought-iron outer rim + subtle antique bronze inner hairline
    draw.rectangle([0, 0, size - 1, size - 1], outline=(14, 11, 10), width=4)
    draw.rectangle([4, 4, size - 5, size - 5], outline=(140, 109, 59), width=2)
    return sq


def slice_grid(src: Path, cols: int, rows: int, start_idx: int, trim_frac: float = 0.045) -> None:
    img = Image.open(src).convert("RGB")
    w, h = img.size
    cell_w = w / cols
    cell_h = h / rows
    idx = start_idx
    for r in range(rows):
        for c in range(cols):
            x0 = int(round((c + trim_frac) * cell_w))
            y0 = int(round((r + trim_frac) * cell_h))
            x1 = int(round((c + 1 - trim_frac) * cell_w))
            y1 = int(round((r + 1 - trim_frac) * cell_h))
            crop = img.crop((x0, y0, x1, y1))
            framed = frame_spell_tile(crop, 512)

            ue_out = GRIMOIRE_DIR / f"SpellPlate_{idx:02d}.png"
            framed.save(ue_out, format="PNG")

            web_out = WEB_SPELLS_DIR / f"spell_{idx:02d}.jpg"
            web_out.parent.mkdir(parents=True, exist_ok=True)
            framed.resize((256, 256), Image.Resampling.LANCZOS).save(web_out, format="JPEG", quality=92)
            print(f"[OK] Plate {idx:02d} -> {ue_out.name} + {web_out.name}")
            idx += 1


def main() -> int:
    build_book_spread()
    slice_grid(GRID6_SRC, cols=3, rows=2, start_idx=1, trim_frac=0.042)
    slice_grid(GRID4_SRC, cols=2, rows=2, start_idx=7, trim_frac=0.042)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
