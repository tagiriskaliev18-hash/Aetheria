# Run via -run=pythonscript. Void field fixes (no duplicate_actor — it AVs
# under -nullrhi): spawn SM_Floor from the asset directly, copy transform and
# material overrides from the healthy tile; fix WorldGridMaterial placeholder.
import unreal

LOG = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir()) + "/Saved/grok_void_fix_log.txt"

def log(m):
    unreal.log("[VoidFix] %s" % m)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(str(m) + "\n")

open(LOG, "w").write("start\n")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.load_level("/Game/Maps/AetheriaWorlds/World_Void"):
    log("FATAL load");  raise SystemExit(1)
els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def find(name):
    for a in els.get_all_level_actors():
        if a.get_name() == name:
            return a
    return None

base = find("StaticMeshActor_89")
if base is None:
    log("FATAL: base tile not found");  raise SystemExit(1)
comp = base.get_components_by_class(unreal.StaticMeshComponent)[0]
mesh = comp.get_editor_property("static_mesh")
rot = base.get_actor_rotation()
scale = base.get_actor_scale3d()
overrides = []
for i in range(comp.get_num_materials()):
    overrides.append(comp.get_material(i))
log("base mesh=%s rot=%s scale=%s overrides=%s" %
    (mesh.get_name() if mesh else None, rot, scale, [m.get_name() if m else None for m in overrides]))

for (tx, ty) in [(1650, 800), (1650, 1100)]:
    actor = None
    try:
        actor = els.spawn_actor_from_object(mesh, unreal.Vector(tx, ty, 0.0), rot)
    except Exception as e:
        log("subsystem spawn raised: %s" % e)
    if actor is None:
        try:
            actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
                unreal.StaticMeshActor, unreal.Vector(tx, ty, 0.0), rot)
            if actor is not None:
                c0 = actor.get_components_by_class(unreal.StaticMeshComponent)[0]
                c0.set_static_mesh(mesh)
                actor.set_actor_scale3d(scale)
        except Exception as e:
            log("class spawn raised: %s" % e)
    if actor is None:
        log("WARN: all spawn APIs returned None at (%d,%d)" % (tx, ty))
        continue
    new_comp = actor.get_components_by_class(unreal.StaticMeshComponent)[0]
    for i, mi in enumerate(overrides):
        if mi is not None and i < new_comp.get_num_materials():
            new_comp.set_material(i, mi)
    log("floor tile spawned at (%d, %d)" % (tx, ty))

wall = find("StaticMeshActor_555")
sibling = find("StaticMeshActor_550")
if wall and sibling:
    ref = sibling.get_components_by_class(unreal.StaticMeshComponent)[0]
    good = ref.get_material(1)
    fixed = 0
    for c in wall.get_components_by_class(unreal.StaticMeshComponent):
        for i in range(c.get_num_materials()):
            mi = c.get_material(i)
            if mi is not None and mi.get_name() == "WorldGridMaterial":
                c.set_material(i, good)
                fixed += 1
    log("WorldGridMaterial slots replaced: %d" % fixed)
else:
    log("WARN: wall/sibling not found")

ok = les.save_current_level()
log("level saved: %s" % ok)
