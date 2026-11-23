#!/usr/bin/env python3
import unreal, os

RAW = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\RawAssets\Models"
DEST = "/Game/Meshes"

# Import Godot gothic characters as STATIC Nanite meshes with clean names
JOBS = [
    ("hero_archmage.glb", "SM_Hero_Archmage"),
    ("SM_Hero_00.glb", "SM_Hero_00"),
    ("mob_swarm_reaper.glb", "SM_VoidFiend"),
    ("SM_Mob_Swarm_00.glb", "SM_Mob_Swarm_Static"),
    ("mob_ranged_lich.glb", "SM_Mob_Ranged_00"),
    ("mob_bruiser_juggernaut.glb", "SM_Colossus"),
    ("mob_boss_archon.glb", "SM_ArchonBoss"),
]

def import_static(src_name, asset_name):
    full = os.path.join(RAW, src_name)
    if not os.path.exists(full):
        unreal.log_warning(f"[AETHERIA] missing {full}")
        return
    task = unreal.AssetImportTask()
    task.filename = full
    task.destination_path = DEST
    task.destination_name = asset_name
    task.replace_existing = True
    task.automated = True
    task.save = True
    # Interchange/FBX UI
    try:
        options = unreal.FbxImportUI()
        options.import_mesh = True
        options.import_textures = True
        options.import_materials = True
        options.import_as_skeletal = False
        options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
        options.static_mesh_import_data.build_nanite = True
        options.static_mesh_import_data.combine_meshes = True
        options.static_mesh_import_data.auto_generate_collision = False
        task.options = options
    except Exception as e:
        unreal.log_warning(f"[AETHERIA] options fallback: {e}")
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    unreal.log(f"[AETHERIA] imported static {src_name} -> {asset_name}")

for src, name in JOBS:
    import_static(src, name)

unreal.log("[AETHERIA] gothic reimport complete")
