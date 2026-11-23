#!/usr/bin/env python3
"""
AETHERIA: GLB Asset Import Script для UE5
Запускается через UnrealEditor-Cmd.exe как Python командлет.
Импортирует hero_archmage.glb и все остальные модели из Godot в Content/Meshes/.

Использование (запустить через bat):
  UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script=import_all_assets.py
"""

import unreal
import os

GODOT_MODELS = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\GodotAetheria\assets\models"
GODOT_TEXTURES = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\GodotAetheria\assets\models"
GODOT_UI = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\GodotAetheria\assets\ui"
GODOT_AUDIO = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\GodotAetheria\assets\audio"

# ============================================================
# 1. Импорт GLB моделей как SkeletalMesh / StaticMesh
# ============================================================
mesh_imports = [
    # (source_file, ue5_destination_path, is_skeletal)
    ("hero_archmage.glb", "/Game/Meshes", True),   # Скелетная меш героя
    ("enemy_bruiser.glb", "/Game/Meshes", True),
    ("enemy_swarm.glb", "/Game/Meshes", True),
    ("enemy_ranged.glb", "/Game/Meshes", True),
    ("prop_barrel.glb", "/Game/Meshes", False),
    ("prop_column.glb", "/Game/Meshes", False),
    ("prop_shrine_altar.glb", "/Game/Meshes", False),
    ("prop_statue.glb", "/Game/Meshes", False),
    ("prop_urn_intact.glb", "/Game/Meshes", False),
    ("prop_urn_shattered.glb", "/Game/Meshes", False),
]

def import_mesh(source_file, dest_path, is_skeletal):
    full_path = os.path.join(GODOT_MODELS, source_file)
    if not os.path.exists(full_path):
        unreal.log_warning(f"[AETHERIA] Mesh not found: {full_path}")
        return None

    task = unreal.AssetImportTask()
    task.filename = full_path
    task.destination_path = dest_path
    task.replace_existing = True
    task.automated = True
    task.save = True

    options = unreal.FbxImportUI()
    options.import_mesh = True
    options.import_textures = True
    options.import_materials = True
    options.import_as_skeletal = is_skeletal
    options.create_physics_asset = False
    options.mesh_type_to_import = (
        unreal.FBXImportType.FBXIT_SKELETAL_MESH if is_skeletal
        else unreal.FBXImportType.FBXIT_STATIC_MESH
    )
    
    if not is_skeletal:
        # Enable Nanite for static meshes
        static_opts = options.static_mesh_import_data
        static_opts.build_nanite = True
        static_opts.combine_meshes = True
        static_opts.generate_lightmap_u_vs = True
        static_opts.remove_degenerates = True
    
    task.options = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log(f"[AETHERIA] Imported: {source_file} → {dest_path}")
    return task.imported_object_paths

# ============================================================
# 2. Импорт PBR текстур героя
# ============================================================
hero_textures = [
    "hero_archmage_char_dark_cloth_albedo.png",
    "hero_archmage_char_dark_cloth_normal.png",
    "hero_archmage_char_dark_cloth_roughness.png",
    "hero_archmage_char_obsidian_steel_albedo.png",
    "hero_archmage_char_obsidian_steel_normal.png",
    "hero_archmage_char_obsidian_steel_roughness.png",
    "hero_archmage_d4_gilded_brass_albedo.png",
    "hero_archmage_d4_gilded_brass_normal.png",
    "hero_archmage_d4_gilded_brass_orm.png",
    "hero_archmage_d4_rune_filigree_albedo.png",
    "hero_archmage_d4_rune_filigree_emissive.png",
    "hero_archmage_d4_rune_filigree_normal.png",
    "hero_archmage_d4_rune_filigree_orm.png",
]

def import_texture(filename, source_dir, dest_path):
    full_path = os.path.join(source_dir, filename)
    if not os.path.exists(full_path):
        unreal.log_warning(f"[AETHERIA] Texture not found: {full_path}")
        return

    task = unreal.AssetImportTask()
    task.filename = full_path
    task.destination_path = dest_path
    task.replace_existing = True
    task.automated = True
    task.save = True

    options = unreal.TextureFactory()
    task.factory = options

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log(f"[AETHERIA] Texture imported: {filename}")

# ============================================================
# 3. Импорт UI иконок заклинаний
# ============================================================
def import_ui_textures():
    if not os.path.exists(GODOT_UI):
        unreal.log_warning(f"[AETHERIA] UI dir not found: {GODOT_UI}")
        return
    
    for f in os.listdir(GODOT_UI):
        if f.endswith('.png'):
            import_texture(f, GODOT_UI, "/Game/UI/Icons")

# ============================================================
# 4. Импорт аудио (WAV → USoundWave)
# ============================================================
audio_files = [
    "BGM_Sanctuary_Gothic.wav",
    "BGM_Boss_Dread.wav",
    "BGM_Abyssal_Expedition.wav",
    "SFX_BasicBolt.wav",
    "SFX_BlackHole.wav",
    "SFX_Chronosphere.wav",
    "SFX_Fireball.wav",
    "SFX_BossRoar.wav",
    "SFX_Step_Stone.wav",
]

def import_audio(filename):
    full_path = os.path.join(GODOT_AUDIO, filename)
    if not os.path.exists(full_path):
        # Try WAV variant
        full_path = full_path.replace('.ogg', '.wav')
        if not os.path.exists(full_path):
            unreal.log_warning(f"[AETHERIA] Audio not found: {filename}")
            return

    task = unreal.AssetImportTask()
    task.filename = full_path
    task.destination_path = "/Game/Audio/SFX"
    task.replace_existing = True
    task.automated = True
    task.save = True

    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log(f"[AETHERIA] Audio imported: {filename}")

# ============================================================
# MAIN EXECUTION
# ============================================================
unreal.log("=" * 60)
unreal.log("[AETHERIA] Starting full Godot → UE5 asset import...")
unreal.log("=" * 60)

# Import meshes
for mesh_file, dest, skeletal in mesh_imports:
    import_mesh(mesh_file, dest, skeletal)

# Import hero textures
for tex in hero_textures:
    import_texture(tex, GODOT_TEXTURES, "/Game/Textures/Hero")

# Import UI
import_ui_textures()

# Import all audio WAVs
if os.path.exists(GODOT_AUDIO):
    for f in os.listdir(GODOT_AUDIO):
        if f.endswith('.wav') and not f.endswith('.import'):
            import_audio(f)

unreal.log("[AETHERIA] ✅ Import complete! Check Content Browser.")
