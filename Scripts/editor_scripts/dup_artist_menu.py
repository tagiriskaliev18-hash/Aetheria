import traceback, unreal
LOG = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\dup_artist_menu_log.txt"
def log(msg):
    line = "[DupMenu] " + str(msg)
    try: unreal.log(line)
    except Exception: pass
    with open(LOG, "a", encoding="utf-8") as f: f.write(line + "\n")
open(LOG, "w", encoding="utf-8").write("start\n")

def main():
    src = "/Game/Maps/AetheriaWorlds/World_Void"
    dest = "/Game/Maps/AetheriaWorlds/ArtistMenu"
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        ok = unreal.EditorAssetLibrary.delete_asset(dest)
        log("deleted old dest -> " + str(ok))
    r = unreal.EditorAssetLibrary.duplicate_asset(src, dest)
    log("duplicate %s -> %s : %s" % (src, dest, r))
    if r:
        unreal.EditorAssetLibrary.save_asset(dest)
        log("saved dest")
    log("DUP COMPLETE")
    print("DUP COMPLETE")
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
        try: unreal.SystemLibrary.quit_editor()
        except Exception: pass
