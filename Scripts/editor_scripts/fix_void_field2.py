# Run via -run=pythonscript. Fix + save + reload + verify in ONE session.
import unreal

LOG = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir()) + "/Saved/grok_void_fix2_log.txt"

def log(m):
    unreal.log("[VoidFix2] %s" % m)
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
wall = find("StaticMeshActor_555")
sibling = find("StaticMeshActor_550")
if not all([base, wall, sibling]):
    log("FATAL: missing actors %s" % [bool(base), bool(wall), bool(sibling)])
    raise SystemExit(1)

# sanity: wall sunk?
wo, we = wall.get_actor_bounds(False)
log("wall top z = %.0f (floor plane is ~40; sunk if ~0 or lower)" % (wo.z + we.z))

# 1) raise the sunken wall: match sibling SM_Dungeon_Wall vertical placement
sibling_loc = sibling.get_actor_location()
wall_loc = wall.get_actor_location()
dz = sibling_loc.z - wall_loc.z
if dz != 0:
    wall.set_actor_location(unreal.Vector(wall_loc.x, wall_loc.y,
                                          wall_loc.z + dz), False, False)
    log("wall raised by dz=%d -> z=%d" % (dz, wall_loc.z + dz))

# 2) WorldGridMaterial -> sibling's stone
ref = sibling.get_components_by_class(unreal.StaticMeshComponent)[0]
good = ref.get_material(1)
fixed = 0
for c in wall.get_components_by_class(unreal.StaticMeshComponent):
    for i in range(c.get_num_materials()):
        mi = c.get_material(i)
        if mi is not None and mi.get_name() == "WorldGridMaterial":
            c.set_material(i, good)
            fixed += 1
log("WorldGridMaterial replaced: %d" % fixed)

# 3) floor tiles: spawn from class + set mesh (worked in run 6)
comp = base.get_components_by_class(unreal.StaticMeshComponent)[0]
mesh = comp.get_editor_property("static_mesh")
rot = base.get_actor_rotation()
placed = 0
for (tx, ty) in [(1650, 800), (1650, 1100)]:
    actor = els.spawn_actor_from_class(unreal.StaticMeshActor,
                                       unreal.Vector(tx, ty, 0.0), rot)
    if actor is None:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(
            unreal.StaticMeshActor, unreal.Vector(tx, ty, 0.0), rot)
    if actor is None:
        log("WARN spawn None at (%d,%d)" % (tx, ty))
        continue
    c0 = actor.get_components_by_class(unreal.StaticMeshComponent)[0]
    c0.set_static_mesh(mesh)
    for i, mi in enumerate([comp.get_material(i)
                            for i in range(comp.get_num_materials())]):
        if mi is not None and i < c0.get_num_materials():
            c0.set_material(i, mi)
    placed += 1
    log("tile placed at (%d,%d)" % (tx, ty))

placed_saved = unreal.EditorLoadingAndSavingUtils.save_dirty_packages(True, True)
log("save_dirty_packages: %s, tiles placed: %d" % (placed_saved, placed))

# direct world save — the commandlet "dirty packages" pass misses spawned actors
world = les.get_editor_world()
direct = unreal.EditorLoadingAndSavingUtils.save_map(
    world, "/Game/Maps/AetheriaWorlds/World_Void")
log("save_map direct: %s" % direct)
if not (placed_saved or direct):
    ok2 = les.save_current_level()
    log("fallback save_current_level: %s" % ok2)

# NOTE: no in-process reload here — it AVs the commandlet; verify by fresh boot.
