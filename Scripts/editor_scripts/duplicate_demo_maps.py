"""Duplicate artist demo maps into /Game/Maps/AetheriaLayouts/ without opening ParagonSample."""
import unreal

unreal.EditorAssetLibrary.make_directory("/Game/Maps/AetheriaLayouts")

pairs = [
    ("/Game/Gothic_Environment/Maps/Review", "/Game/Maps/AetheriaLayouts/Layout_GothicCourtyard"),
    ("/Game/AncientTreasures/Maps/Demonstration", "/Game/Maps/AetheriaLayouts/Layout_TreasureSanctum"),
    ("/Game/MedievalDungeon/Maps/Demonstration", "/Game/Maps/AetheriaLayouts/Layout_MedievalDungeon"),
]

for src, dest in pairs:
    if not unreal.EditorAssetLibrary.does_asset_exist(src):
        # Some packs use slightly different map names — probe.
        unreal.log_warning(f"[Aetheria] Missing source map {src}")
        parent = src.rsplit("/", 1)[0]
        try:
            assets = unreal.EditorAssetLibrary.list_assets(parent, recursive=False)
            for a in assets:
                unreal.log(f"[Aetheria]   candidate in {parent}: {a}")
        except Exception as e:
            unreal.log_warning(f"[Aetheria] list_assets failed for {parent}: {e}")
        continue
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        unreal.log(f"[Aetheria] Layout already exists, skip duplicate: {dest}")
        continue
    new_asset = unreal.EditorAssetLibrary.duplicate_asset(src, dest)
    if new_asset:
        unreal.EditorAssetLibrary.save_asset(dest)
        unreal.log(f"[Aetheria] Duplicated {src} -> {dest}")
    else:
        unreal.log_error(f"[Aetheria] Failed to duplicate {src} -> {dest}")
print("DONE duplicate_demo_maps")

try:
    cmd = ""
    try:
        cmd = str(unreal.SystemLibrary.get_command_line())
    except Exception:
        cmd = ""
    if "-unattended" in cmd.lower():
        unreal.log("[Aetheria] -unattended set, quitting editor after script")
        unreal.SystemLibrary.quit_editor()
except Exception as e:
    print("quit_editor failed", e)
