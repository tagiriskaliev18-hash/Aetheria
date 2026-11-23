import unreal
LOG = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir()) + "/Saved/grok_void_min_log.txt"
with open(LOG, "a", encoding="utf-8") as f:
    f.write("min: start\n")
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
ok = les.load_level("/Game/Maps/AetheriaWorlds/World_Void")
with open(LOG, "a", encoding="utf-8") as f:
    f.write("min: loaded=%s\n" % ok)
els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
n = len(els.get_all_level_actors())
with open(LOG, "a", encoding="utf-8") as f:
    f.write("min: actors=%d\n" % n)
