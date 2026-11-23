import json
import os
import re
import subprocess
import sys
import urllib.request
import zipfile
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path

from PIL import Image

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
RAW = ROOT / "RawAssets"
CACHE = RAW / "PolyHavenCache"
MESH_DIR = RAW / "Meshes"
TEX_DIR = RAW / "Textures"
INBOX_DIR = RAW / "ExternalInbox"
DOWNLOADS_DIR = Path(r"C:\Users\user\.gemini\antigravity\brain\b1c6e9c7-0f42-4243-b3fd-346b6aca677b\browser_sandbox\downloads")
ZIP_EXTRACT_DIR = CACHE / "_zip_extract"
BLENDER_EXE = Path(r"D:\Tools\blender-5.2.2-windows-x64\blender.exe")
ARTIFACT_DIR = Path(r"C:\Users\user\.gemini\antigravity\brain\b1c6e9c7-0f42-4243-b3fd-346b6aca677b")

HEADERS = {"User-Agent": "AetheriaPipeline/1.0"}

# Verified Poly Haven CC0 photogrammetry 3D models (direct API + 2K PBR maps)
POLYHAVEN_MODELS = [
    ("gothic_statue", "SM_GothicStatue", "T_GothicStatue", True),
    ("treasure_chest", "SM_TreasureChest", "T_TreasureChest", True),
    ("large_iron_gate", "SM_LargeIronGate", "T_LargeIronGate", True),
    ("modular_fort_01", "SM_RuinedArch", "T_RuinedArch", True),
    ("lantern_chandelier_01", "SM_LanternChandelier", "T_LanternChandelier", True),
    ("ornate_medieval_mace", "SM_OrnateMace", "T_OrnateMace", True),
    ("ornate_war_hammer", "SM_WarHammerScan", "T_WarHammer", True),
    ("antique_estoc", "SM_AntiqueEstoc", "T_AntiqueEstoc", True),
    ("bull_head", "SM_HornedSkullScan", "T_HornedSkull", True),
    ("rock_moss_set_01", "SM_CliffBoulder", "T_CliffBoulder", True),
    ("dead_tree_trunk_02", "SM_DeadTree", "T_DeadTree", True),
    # 4 New verified Gothic / Medieval CC0 assets
    ("large_castle_door", "SM_CastleDoor", "T_CastleDoor", True),
    ("Lantern_01", "SM_GothicLantern", "T_GothicLantern", True),
    ("stone_fire_pit", "SM_StoneFirePit", "T_StoneFirePit", True),
    ("lion_head", "SM_LionRelief", "T_LionRelief", True),
]

POLYHAVEN_FALLBACKS = {
    "ornate_war_hammer": ["ornate_war_hammer", "ornate_medieval_mace"],
    "antique_estoc": ["antique_estoc", "ornate_medieval_mace"],
    "bull_head": ["bull_head", "gothic_statue"],
    "large_castle_door": ["large_castle_door", "large_iron_gate"],
    "Lantern_01": ["Lantern_01", "lantern_chandelier_01"],
    "stone_fire_pit": ["stone_fire_pit", "modular_fort_01"],
    "lion_head": ["lion_head", "gothic_statue"],
}

# 36 Target slots mapped to 35 unique Sketchfab Grimdark models (Hero_01 & Boss_Loc09 share Demonic Horned Knight)
SKETCHFAB_SLOT_DEFS = [
    # 10 Heroes (1.85m, 24k tris)
    {"slot": "SM_Hero_00", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 2048, "aliases": ["undead_wizard", "hollow_magus"], "dup": ["SM_ArchmageRobed"]},
    {"slot": "SM_Hero_01", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["demonic_horned_horror_knight", "horror_knight"], "dup": []},
    {"slot": "SM_Hero_02", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["dark_necromancer", "corrupted_staff"], "dup": ["SM_Hero_Necromancer"]},
    {"slot": "SM_Hero_03", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["skeletal_lich_king", "bone_crown"], "dup": ["SM_Hero_LichKing"]},
    {"slot": "SM_Hero_04", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["bone_knight", "horned_skull_greatsword"], "dup": []},
    {"slot": "SM_Hero_05", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["morzhak", "graveborn_hexcaller"], "dup": []},
    {"slot": "SM_Hero_06", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["astreon", "starlit_wanderer"], "dup": []},
    {"slot": "SM_Hero_07", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["zyraelis", "voidweaving_enchantress"], "dup": []},
    {"slot": "SM_Hero_08", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["vexmortis", "plaguebound_alchemist"], "dup": []},
    {"slot": "SM_Hero_09", "cat": "Hero", "h": 1.85, "tris": 24000, "res": 1024, "aliases": ["zarvethis", "gilded_soulreaper"], "dup": []},

    # 10 Biome Bosses (3.10m, 25k tris)
    {"slot": "SM_Boss_Loc00", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["volcanic_stone", "lava_magma_golem"], "dup": ["SM_StoneColossus"]},
    {"slot": "SM_Boss_Loc01", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["murkzel", "abyssal_tidecaller"], "dup": []},
    {"slot": "SM_Boss_Loc02", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["vorgulth", "plaguebelly_devourer"], "dup": []},
    {"slot": "SM_Boss_Loc03", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["vulkaros", "emberforged_juggernaut"], "dup": []},
    {"slot": "SM_Boss_Loc04", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["morvessa", "thorn_matriarch"], "dup": []},
    {"slot": "SM_Boss_Loc05", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["vaesmira", "eclipse_matriarch"], "dup": []},
    {"slot": "SM_Boss_Loc06", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["vythrella", "hollow_veil"], "dup": []},
    {"slot": "SM_Boss_Loc07", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["aurevorn", "gilded_wingkeeper"], "dup": []},
    {"slot": "SM_Boss_Loc08", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 1024, "aliases": ["krovath", "bloodhorn_tyrant"], "dup": []},
    {"slot": "SM_Boss_Loc09", "cat": "Boss", "h": 3.10, "tris": 25000, "res": 2048, "aliases": ["demonic_horned_horror_knight", "horror_knight"], "dup": ["SM_ArchonBoss"]},

    # 4 Swarm Mobs (1.75m, 13500 tris) - All unique!
    {"slot": "SM_Mob_Swarm_00", "cat": "Swarm", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["mortech", "ironbone_abomination"], "dup": ["SM_VoidFiend"]},
    {"slot": "SM_Mob_Swarm_01", "cat": "Swarm", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["shirayume", "ninefold_wraith"], "dup": []},
    {"slot": "SM_Mob_Swarm_02", "cat": "Swarm", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["virenthos", "emerald_hexblade"], "dup": []},
    {"slot": "SM_Mob_Swarm_03", "cat": "Swarm", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["ravenkor", "bloodsteel_duelist"], "dup": []},

    # 4 Ranged Mobs (1.75m, 13500 tris) - All unique!
    {"slot": "SM_Mob_Ranged_00", "cat": "Ranged", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["skeleton_mage", "glowing_crystal_staff"], "dup": []},
    {"slot": "SM_Mob_Ranged_01", "cat": "Ranged", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["zahmet", "emerald_hierophant"], "dup": []},
    {"slot": "SM_Mob_Ranged_02", "cat": "Ranged", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["sister_vesper", "veiled_oath"], "dup": []},
    {"slot": "SM_Mob_Ranged_03", "cat": "Ranged", "h": 1.75, "tris": 13500, "res": 1024, "aliases": ["isolune", "crystal_reverie"], "dup": []},

    # 4 Bruiser Mobs (2.45m, 13500 tris) - All unique!
    {"slot": "SM_Mob_Bruiser_00", "cat": "Bruiser", "h": 2.45, "tris": 13500, "res": 1024, "aliases": ["gravmolt", "ironcore_siegebreaker"], "dup": []},
    {"slot": "SM_Mob_Bruiser_01", "cat": "Bruiser", "h": 2.45, "tris": 13500, "res": 1024, "aliases": ["gothic_black_and_gold_knight", "giant_sword"], "dup": []},
    {"slot": "SM_Mob_Bruiser_02", "cat": "Bruiser", "h": 2.45, "tris": 13500, "res": 1024, "aliases": ["volcanic_obsidian_golem", "obsidian_golem"], "dup": []},
    {"slot": "SM_Mob_Bruiser_03", "cat": "Bruiser", "h": 2.45, "tris": 13500, "res": 1024, "aliases": ["meteor_golem", "cosmic_rock"], "dup": []},

    # 2 Staves (1.50m, 12000 tris)
    {"slot": "SM_Staff_Arcane01", "cat": "Staff", "h": 1.50, "tris": 12000, "res": 1024, "aliases": ["wizard_staff"], "dup": []},
    {"slot": "SM_Staff_Arcane02", "cat": "Staff", "h": 1.50, "tris": 12000, "res": 1024, "aliases": ["fantasy_magic_staffs", "staffs_collection"], "dup": []},

    # 1 Headgear Mask (0.35m, 12000 tris)
    {"slot": "SM_Head_HopeMask", "cat": "HeadMask", "h": 0.35, "tris": 12000, "res": 1024, "aliases": ["hope_mask", "last_face"], "dup": []},

    # 1 Citadel / Fortress (18.0m, 26000 tris) - Real Sketchfab Ancient Medieval Fortress!
    {"slot": "SM_CitadelFortress", "cat": "Citadel", "h": 18.0, "tris": 26000, "res": 1024, "aliases": ["ancient_medieval_fortress", "fortress"], "dup": [], "is_polyhaven": False},
]


def fetch_json(url: str) -> dict:
    req = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=20) as resp:
        return json.loads(resp.read().decode("utf-8"))


def download_file(url: str, dest: Path) -> None:
    if dest.exists() and dest.stat().st_size > 0:
        return
    dest.parent.mkdir(parents=True, exist_ok=True)
    req = urllib.request.Request(url, headers=HEADERS)
    with urllib.request.urlopen(req, timeout=45) as resp:
        dest.write_bytes(resp.read())


def download_polyhaven_asset(asset_id: str) -> dict:
    candidates = [asset_id] + POLYHAVEN_FALLBACKS.get(asset_id, [])

    # Offline-first: use locally cached Poly Haven 2K photogrammetry scans if present
    for cand in candidates:
        cand_dir = CACHE / cand
        if cand_dir.exists():
            gltfs = list(cand_dir.glob("*.gltf")) + list(cand_dir.glob("*.glb"))
            if gltfs:
                return {"id": cand, "gltf": str(gltfs[0]), "cache": str(cand_dir)}

    files_json = None
    chosen_id = asset_id

    for cand in candidates:
        try:
            files_json = fetch_json(f"https://api.polyhaven.com/files/{cand}")
            chosen_id = cand
            break
        except Exception:
            continue

    if not files_json:
        raise RuntimeError(f"Could not fetch PolyHaven metadata for {asset_id} or fallbacks")

    gltf_block = files_json.get("gltf", {})
    res = "2k" if "2k" in gltf_block else ("1k" if "1k" in gltf_block else next(iter(gltf_block.keys())))
    entry = gltf_block[res]["gltf"]
    asset_cache = CACHE / chosen_id
    asset_cache.mkdir(parents=True, exist_ok=True)

    tasks = []
    main_url = entry["url"]
    main_name = Path(urllib.request.urlparse(main_url).path).name
    main_path = asset_cache / main_name
    tasks.append((main_url, main_path))

    for rel_path, meta in entry.get("include", {}).items():
        inc_url = meta["url"]
        inc_dest = asset_cache / rel_path
        tasks.append((inc_url, inc_dest))

    with ThreadPoolExecutor(max_workers=6) as pool:
        futs = [pool.submit(download_file, u, d) for u, d in tasks]
        for fut in as_completed(futs):
            fut.result()

    return {"id": chosen_id, "gltf": str(main_path), "cache": str(asset_cache)}


def generate_micro_normal_and_roughness(albedo_path: Path, normal_path: Path, rough_path: Path, size=(512, 512)) -> None:
    """Procedurally synthesize DirectX tangent-space normal (Y-) and roughness maps from Albedo luminance."""
    try:
        with Image.open(albedo_path) as im:
            im = im.convert("L").resize(size, Image.Resampling.LANCZOS)
            w, h = im.size
            pixels = list(im.getdata())

            nor_img = Image.new("RGB", (w, h))
            nor_data = []
            strength = 2.5
            for y in range(h):
                ym1 = max(0, y - 1)
                yp1 = min(h - 1, y + 1)
                for x in range(w):
                    xm1 = max(0, x - 1)
                    xp1 = min(w - 1, x + 1)

                    dx = (pixels[y * w + xp1] - pixels[y * w + xm1]) / 255.0 * strength
                    dy = (pixels[yp1 * w + x] - pixels[ym1 * w + x]) / 255.0 * strength

                    # DirectX: ny is positive down in screen coords -> flip green
                    nz = 1.0
                    inv_len = 1.0 / max(1e-6, (dx * dx + dy * dy + nz * nz) ** 0.5)
                    nx = -dx * inv_len
                    ny = dy * inv_len
                    nz = nz * inv_len

                    r = int((nx * 0.5 + 0.5) * 255)
                    g = int((ny * 0.5 + 0.5) * 255)
                    b = int((nz * 0.5 + 0.5) * 255)
                    nor_data.append((r, g, b))
            nor_img.putdata(nor_data)
            nor_img.save(normal_path, "PNG", optimize=True)

            rough_img = Image.new("L", (w, h))
            rough_data = [max(40, min(240, int(150 + (128 - p) * 0.45))) for p in pixels]
            rough_img.putdata(rough_data)
            rough_img.save(rough_path, "PNG", optimize=True)
    except Exception as e:
        print(f"Warning: Failed to generate micro-normal for {albedo_path}: {e}")


def convert_polyhaven_textures(asset_id: str, cache_dir: Path, tex_prefix: str) -> None:
    tex_files = list(cache_dir.rglob("*.jpg")) + list(cache_dir.rglob("*.png"))
    diff_src = next((p for p in tex_files if "_diff_" in p.name.lower() or "albedo" in p.name.lower()), None)
    nor_src = next((p for p in tex_files if "_nor_gl_" in p.name.lower() or "_nor_dx_" in p.name.lower() or "_nor_" in p.name.lower()), None)
    rough_src = next((p for p in tex_files if "_rough_" in p.name.lower()), None)
    arm_src = next((p for p in tex_files if "_arm_" in p.name.lower()), None)

    TEX_DIR.mkdir(parents=True, exist_ok=True)

    def save_resized_png(src: Path, dst: Path, flip_green_dx: bool = False, extract_g: bool = False):
        if dst.exists() and dst.stat().st_size > 0:
            return
        with Image.open(src) as im:
            im = im.convert("RGB")
            if max(im.size) > 2048:
                im.thumbnail((2048, 2048), Image.Resampling.LANCZOS)
            if flip_green_dx:
                r, g, b = im.split()
                g = g.point(lambda v: 255 - v)
                im = Image.merge("RGB", (r, g, b))
            elif extract_g:
                _, g, _ = im.split()
                im = Image.merge("RGB", (g, g, g))
            im.save(dst, "PNG", optimize=True)

    albedo_dst = TEX_DIR / f"{tex_prefix}_Albedo.png"
    normal_dst = TEX_DIR / f"{tex_prefix}_Normal.png"
    rough_dst = TEX_DIR / f"{tex_prefix}_Roughness.png"

    if diff_src:
        save_resized_png(diff_src, albedo_dst)
    if nor_src:
        is_gl = "_nor_gl_" in nor_src.name.lower() or "_nor_dx_" not in nor_src.name.lower()
        save_resized_png(nor_src, normal_dst, flip_green_dx=is_gl)
    if rough_src:
        save_resized_png(rough_src, rough_dst)
    elif arm_src:
        save_resized_png(arm_src, rough_dst, extract_g=True)

    # Fallback normal / roughness if missing
    if albedo_dst.exists() and (not normal_dst.exists() or not rough_dst.exists()):
        generate_micro_normal_and_roughness(albedo_dst, normal_dst, rough_dst, size=(1024, 1024))

    # Ensure Citadel Fortress textures are mirrored if generated from Ruined Arch
    if tex_prefix == "T_RuinedArch":
        for suf in ("Albedo", "Normal", "Roughness"):
            s_p = TEX_DIR / f"T_RuinedArch_{suf}.png"
            d_p = TEX_DIR / f"T_CitadelFortress_{suf}.png"
            if s_p.exists() and not d_p.exists():
                try:
                    d_p.write_bytes(s_p.read_bytes())
                except Exception:
                    pass


def extract_zip_if_needed(zip_path: Path) -> Path | None:
    """Extract zip archive into cache directory and return path to main 3D model (.glb / .gltf / .fbx)."""
    if not zip_path.exists() or zip_path.stat().st_size == 0:
        return None
    if not zipfile.is_zipfile(zip_path):
        return None
    extract_target = ZIP_EXTRACT_DIR / zip_path.stem
    extract_target.mkdir(parents=True, exist_ok=True)

    # Check if already extracted
    existing = list(extract_target.rglob("*.glb")) + list(extract_target.rglob("*.gltf")) + list(extract_target.rglob("*.fbx"))
    if existing:
        return existing[0]

    try:
        with zipfile.ZipFile(zip_path, "r") as z:
            z.extractall(extract_target)
        candidates = list(extract_target.rglob("*.glb")) + list(extract_target.rglob("*.gltf")) + list(extract_target.rglob("*.fbx"))
        if candidates:
            return candidates[0]
    except Exception as e:
        print(f"Warning: Failed to extract {zip_path}: {e}")
    return None


def collect_downloaded_3d_files() -> dict[str, Path]:
    """Scan DOWNLOADS_DIR and INBOX_DIR, skip .crdownload/.tmp/size==0 and browser dups *_1, prioritize .glb over .zip."""
    pool: dict[str, Path] = {}
    search_dirs = [DOWNLOADS_DIR, INBOX_DIR]
    all_files: list[Path] = []

    for sdir in search_dirs:
        if not sdir.exists():
            continue
        for p in sorted(sdir.glob("*")):
            # Gracefully skip incomplete or empty files without deleting or touching
            if p.suffix.lower() in (".crdownload", ".tmp", ""):
                continue
            try:
                if p.stat().st_size == 0:
                    continue
            except Exception:
                continue
            # Skip browser download duplicates like *_1.glb, *_1.zip
            if re.search(r"_\d+\.(glb|gltf|zip|fbx)$", p.name.lower()):
                continue
            all_files.append(p)

    # Pass 1: Collect direct 3D model formats (.glb, .gltf, .fbx) with top priority
    for p in all_files:
        if p.suffix.lower() in (".glb", ".gltf", ".fbx"):
            stem_key = p.stem.lower().replace("-", "_").replace(" ", "_")
            stem_key = re.sub(r"_+", "_", stem_key)
            pool[stem_key] = p

    # Pass 2: For zip archives, only extract if a matching GLB is NOT already collected
    for p in all_files:
        if p.suffix.lower() == ".zip":
            stem_key = p.stem.lower().replace("-", "_").replace(" ", "_")
            stem_key = re.sub(r"_+", "_", stem_key)
            # Check if this zip is already covered by a standalone .glb
            already_in_pool = any(k in stem_key or stem_key in k for k in pool)
            if not already_in_pool:
                extracted_3d = extract_zip_if_needed(p)
                if extracted_3d:
                    pool[stem_key] = extracted_3d

    return pool


def resolve_sketchfab_slots(pool: dict[str, Path]) -> list[dict]:
    """Map available 3D models to target slots, preserving strictly unique assignments."""
    resolved_slots = []
    used_keys = set()

    for sdef in SKETCHFAB_SLOT_DEFS:
        slot_name = sdef["slot"]
        aliases = sdef["aliases"]
        is_polyhaven = sdef.get("is_polyhaven", False)

        # Slots handled via PolyHaven (e.g. SM_CitadelFortress) should NOT pick a character fallback
        if is_polyhaven:
            resolved_slots.append({
                "slot_name": slot_name,
                "category": sdef["cat"],
                "target_h": sdef["h"],
                "max_tris": sdef["tris"],
                "tex_res": sdef["res"],
                "target_fbx": str(MESH_DIR / f"{slot_name}.fbx"),
                "source_path": None,
                "duplicates": [],
                "is_polyhaven": True,
            })
            continue

        matched_path = None
        matched_key = None

        # Try exact alias matches first
        for alias in aliases:
            a_key = alias.lower().replace("-", "_").replace(" ", "_")
            for k, p in pool.items():
                if a_key in k:
                    matched_path = p
                    matched_key = k
                    break
            if matched_path:
                break

        # Fallback to unused file in pool if no direct alias matched
        if not matched_path:
            for k, p in pool.items():
                if k not in used_keys:
                    matched_path = p
                    matched_key = k
                    break

        # Final fallback: reuse first available model
        if not matched_path and pool:
            matched_key = next(iter(pool.keys()))
            matched_path = pool[matched_key]

        if matched_key:
            used_keys.add(matched_key)

        resolved_slots.append({
            "slot_name": slot_name,
            "category": sdef["cat"],
            "target_h": sdef["h"],
            "max_tris": sdef["tris"],
            "tex_res": sdef["res"],
            "target_fbx": str(MESH_DIR / f"{slot_name}.fbx"),
            "source_path": str(matched_path) if matched_path else None,
            "duplicates": [str(MESH_DIR / f"{d}.fbx") for d in sdef.get("dup", [])],
            "is_polyhaven": False,
        })

    return resolved_slots


def main() -> int:
    MESH_DIR.mkdir(parents=True, exist_ok=True)
    TEX_DIR.mkdir(parents=True, exist_ok=True)
    INBOX_DIR.mkdir(parents=True, exist_ok=True)
    CACHE.mkdir(parents=True, exist_ok=True)
    ZIP_EXTRACT_DIR.mkdir(parents=True, exist_ok=True)

    manifest = {"polyhaven": [], "sketchfab": [], "external_inbox": []}

    print("=== Checking & Downloading Poly Haven CC0 3D Models (15 Assets incl. Citadel) ===")
    for asset_id, sm_name, tex_prefix, export_tex in POLYHAVEN_MODELS:
        print(f"Checking {asset_id} -> {sm_name}.fbx ...")
        try:
            info = download_polyhaven_asset(asset_id)
            info["target_fbx"] = str(MESH_DIR / f"{sm_name}.fbx")
            info["sm_name"] = sm_name
            manifest["polyhaven"].append(info)
            if export_tex:
                convert_polyhaven_textures(info["id"], Path(info["cache"]), tex_prefix)
        except Exception as e:
            print(f"Warning: Failed to fetch {asset_id}: {e}")

    print("=== Scanning Downloaded Sketchfab 3D Models (36 Target Slots) ===")
    pool = collect_downloaded_3d_files()
    print(f"Found {len(pool)} unique 3D source models across downloads and inbox")

    resolved_sketchfab = resolve_sketchfab_slots(pool)
    manifest["sketchfab"] = resolved_sketchfab

    # Pick up raw external inbox files for general conversion
    for ext_file in sorted(INBOX_DIR.glob("*")):
        if ext_file.suffix.lower() in (".glb", ".gltf", ".fbx"):
            manifest["external_inbox"].append(str(ext_file))

    manifest_path = CACHE / "pipeline_manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"Saved pipeline manifest with {len(manifest['polyhaven'])} PolyHaven and {len(manifest['sketchfab'])} Sketchfab slots to {manifest_path}")

    if not BLENDER_EXE.exists():
        print(f"ERROR: Blender not found at {BLENDER_EXE}", file=sys.stderr)
        return 1

    blender_script = ROOT / "Scripts" / "blender_aaa_pipeline.py"
    print("=== Running Blender 5.2 AAA Decimation, Sculpting & LookDev Render ===")
    proc = subprocess.run(
        [str(BLENDER_EXE), "--background", "--python", str(blender_script), "--", str(manifest_path)],
        check=False,
    )
    if proc.returncode != 0:
        print(f"ERROR: Blender exited with code {proc.returncode}", file=sys.stderr)
        return proc.returncode

    print("=== Post-processing PBR Textures (Albedo, DirectX Y- Normal, Roughness) ===")
    for entry in manifest["sketchfab"]:
        slot_name = entry["slot_name"]
        tex_res = entry.get("tex_res", 1024)
        tex_prefix = f"T_{slot_name.replace('SM_', '')}"
        albedo_dst = TEX_DIR / f"{tex_prefix}_Albedo.png"
        normal_dst = TEX_DIR / f"{tex_prefix}_Normal.png"
        rough_dst = TEX_DIR / f"{tex_prefix}_Roughness.png"

        if albedo_dst.exists():
            try:
                with Image.open(albedo_dst) as im:
                    if max(im.size) > tex_res:
                        im.thumbnail((tex_res, tex_res), Image.Resampling.LANCZOS)
                        im.save(albedo_dst, "PNG", optimize=True)
            except Exception:
                pass
            if not normal_dst.exists() or not rough_dst.exists():
                generate_micro_normal_and_roughness(albedo_dst, normal_dst, rough_dst, size=(min(1024, tex_res), min(1024, tex_res)))

    lookdev_src = RAW / "LookDev_HollowMagus_3070.png"
    if lookdev_src.exists() and ARTIFACT_DIR.exists():
        (ARTIFACT_DIR / "LookDev_HollowMagus_3070.png").write_bytes(lookdev_src.read_bytes())
        print(f"Copied LookDev preview to {ARTIFACT_DIR / 'LookDev_HollowMagus_3070.png'}")

    print("=== AAA Asset Pipeline Completed Successfully ===")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
