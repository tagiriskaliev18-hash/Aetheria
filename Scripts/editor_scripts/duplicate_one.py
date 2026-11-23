import os
import unreal

unreal.EditorAssetLibrary.make_directory("/Game/Maps/AetheriaLayouts")
src = os.environ.get("AETHERIA_DUP_SRC", "")
dest = os.environ.get("AETHERIA_DUP_DEST", "")
unreal.log(f"[Aetheria] duplicate one {src} -> {dest}")
if not src or not dest:
    raise RuntimeError("AETHERIA_DUP_SRC/DEST not set")
if not unreal.EditorAssetLibrary.does_asset_exist(src):
    parent = src.rsplit("/", 1)[0]
    unreal.log_warning(f"[Aetheria] Missing source map {src}")
    try:
        for a in unreal.EditorAssetLibrary.list_assets(parent, recursive=False):
            unreal.log(f"[Aetheria]   candidate: {a}")
    except Exception as e:
        unreal.log_warning(f"list failed {e}")
else:
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        unreal.log(f"[Aetheria] already exists {dest}")
    else:
        new_asset = unreal.EditorAssetLibrary.duplicate_asset(src, dest)
        if new_asset:
            unreal.EditorAssetLibrary.save_asset(dest)
            unreal.log(f"[Aetheria] Duplicated {src} -> {dest}")
        else:
            unreal.log_error(f"[Aetheria] Failed to duplicate {src}")
print("DONE duplicate_one")
try:
    cmd = str(unreal.SystemLibrary.get_command_line())
    if "-unattended" in cmd.lower():
        unreal.SystemLibrary.quit_editor()
except Exception as e:
    print("quit failed", e)
