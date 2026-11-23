# Run via -run=pythonscript. Dumps mesh + applied materials for named actors
# near the Void interior floor gap, and for floor surfaces near spawn (healthy
# reference). Answers "did the textures change" with concrete material names.
import unreal, os, json

proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG = os.path.join(proj, "Saved", "grok_void_materials_log.txt")

def log(m):
    unreal.log("[VoidMats] %s" % m)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(str(m) + "\n")

open(LOG, "w").write("start\n")

TARGETS = {"StaticMeshActor_32", "StaticMeshActor_33", "StaticMeshActor_36",
           "StaticMeshActor_89", "StaticMeshActor_149", "StaticMeshActor_207",
           "StaticMeshActor_253", "StaticMeshActor_375", "StaticMeshActor_555"}

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.load_level("/Game/Maps/AetheriaWorlds/World_Void"):
    log("FATAL load");  raise SystemExit(1)

els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
out = {}
for actor in els.get_all_level_actors():
    try:
        name = actor.get_name()
        ox, oy = actor.get_actor_location().x, actor.get_actor_location().y
        near_spawn = abs(ox) < 800 and abs(oy + 150) < 800
        if name not in TARGETS and not near_spawn:
            continue
        entry = {"pos": [round(ox), round(oy), round(actor.get_actor_location().z)],
                 "mesh": None, "mats": []}
        for c in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = c.get_editor_property("static_mesh")
            entry["mesh"] = mesh.get_name() if mesh else None
            for i in range(min(3, c.get_num_materials())):
                mi = c.get_material(i)
                entry["mats"].append(mi.get_name() if mi else "None")
        out[name] = entry
    except Exception as e:
        log("skip %s: %s" % (name, e))

for k, v in sorted(out.items()):
    log("%s pos=%s mesh=%s mats=%s" % (k, v["pos"], v["mesh"], v["mats"]))

with open(os.path.join(proj, "Saved", "grok_void_materials.json"), "w") as f:
    json.dump(out, f, indent=1)
log("done: %d actors dumped" % len(out))
