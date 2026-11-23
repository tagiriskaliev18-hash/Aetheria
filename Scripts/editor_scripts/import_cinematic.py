# Run: UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script="<this>"
# Imports the cinematic PBR set as REAL packages under /Game/Textures with the
# exact names UAetheriaMaterialLibrary::LoadOrImportTexture looks up, so the
# biome materials reference persisted assets (transient imports die in a cook).
import unreal, os

proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
SRC = os.path.join(proj, "RawAssets", "Cinematic")

SETS = ["pbr_bastion_floor", "pbr_bastion_wall", "pbr_ruins_floor", "pbr_ruins_wall",
        "pbr_sanctum_floor", "pbr_sanctum_wall", "pbr_void_floor", "pbr_void_wall"]
SUFFIX = [("_Albedo", ""), ("_Normal", "_Normal"), ("_Roughness", "_Roughness"), ("_Emissive", "_Emissive")]

try:
    ats = unreal.get_editor_subsystem(unreal.AssetToolsSubsystem)
except AttributeError:
    ats = unreal.AssetToolsHelpers.get_asset_tools()
imported = 0
for stem in SETS:
    for asset_suffix, file_suffix in SUFFIX:
        src = os.path.join(SRC, f"{stem}{file_suffix}.png")
        if not os.path.exists(src):
            continue
        asset_name = f"T_Cine_{stem}{asset_suffix}"
        task = unreal.AssetImportTask()
        task.filename = src
        task.destination_path = "/Game/Textures"
        task.destination_name = asset_name
        task.automated = True
        task.replace_existing = True
        task.save = True
        ats.import_asset_tasks([task])
        imported += 1
        unreal.log(f"[CineImport] {asset_name}")

unreal.EditorAssetLibrary.save_directory("/Game/Textures", only_if_is_dirty=False)
unreal.log_warning(f"[CineImport] done {imported} textures")
