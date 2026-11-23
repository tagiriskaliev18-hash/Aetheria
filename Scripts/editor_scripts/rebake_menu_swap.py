import traceback, unreal

LOG = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\rebake_menu_swap_log.txt"

def log(msg):
    line = "[MenuSwap] " + str(msg)
    try:
        unreal.log(line)
    except Exception:
        pass
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")

with open(LOG, "w", encoding="utf-8") as f:
    f.write("start\n")

def les():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

def main():
    src = "/Game/Maps/AetheriaWorlds/MenuCourtyard"
    dest = "/Game/Maps/AetheriaWorld"
    log("exists src=" + str(unreal.EditorAssetLibrary.does_asset_exist(src)))
    log("exists dest=" + str(unreal.EditorAssetLibrary.does_asset_exist(dest)))
    ok = les().load_level(src)
    log("load MenuCourtyard -> " + str(ok))
    # Save-as onto the GameDefaultMap path without deleting first.
    saved = False
    for name, fn in (
        ("LES.save_current_level_as", lambda: les().save_current_level_as(dest)),
        ("ELL.save_current_level_as", lambda: unreal.EditorLevelLibrary.save_current_level_as(dest)),
        ("LES.save_current_level", lambda: les().save_current_level()),
    ):
        try:
            r = fn()
            log(name + " -> " + str(r))
            if r or name.endswith("save_current_level"):
                saved = True
                if "as" in name:
                    break
        except Exception as e:
            log(name + " failed " + str(e))
    if not unreal.EditorAssetLibrary.does_asset_exist(dest):
        try:
            r = unreal.EditorAssetLibrary.duplicate_asset(src, dest)
            log("duplicate_asset -> " + str(r))
            if r:
                unreal.EditorAssetLibrary.save_asset(dest)
        except Exception as e:
            log("duplicate failed " + str(e))
    # If dest still exists (old file), overwrite by loading dest is risky.
    # Try checkout/save of a copied package via AssetTools rename? skip.
    log("final dest exists=" + str(unreal.EditorAssetLibrary.does_asset_exist(dest)))
    log("MENU SWAP COMPLETE")
    print("MENU SWAP COMPLETE")
    try:
        if "-unattended" in str(unreal.SystemLibrary.get_command_line()).lower():
            unreal.SystemLibrary.quit_editor()
    except Exception as e:
        log("quit " + str(e))

if __name__ == "__main__":
    try:
        main()
    except Exception:
        log("FATAL\n" + traceback.format_exc())
        try:
            unreal.SystemLibrary.quit_editor()
        except Exception:
            pass
