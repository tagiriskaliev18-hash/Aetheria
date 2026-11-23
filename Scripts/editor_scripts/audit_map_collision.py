# Run: UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script="<this>"
# with env GROK_COLL_FIX=1 to apply fixes.
# Map-level collision audit: ground-like / large static meshes in the baked
# world maps get collision profile "BlockAll" so the capsule can never fall
# through floors, walls or platforms. UE 5.8 python exposes no enum members
# for CollisionResponse, so everything here is profile-string based.
import unreal, sys, os, json

FIX = "fix" in " ".join(sys.argv).lower() or os.environ.get("GROK_COLL_FIX") == "1"
proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
out_path = os.path.join(proj, "Saved", "grok_map_collision.json")
LOG = os.path.join(proj, "Saved", "grok_map_collision_log.txt")

MAPS = [
    "/Game/Maps/AetheriaWorlds/World_Bastion",
    "/Game/Maps/AetheriaWorlds/World_Ruins",
    "/Game/Maps/AetheriaWorlds/World_Sanctum",
    "/Game/Maps/AetheriaWorlds/World_Void",
    "/Game/Maps/AetheriaWorlds/ArtistMenu",
    "/Game/Maps/AetheriaWorlds/MenuCourtyard",
]

FOLIAGE_KEYS = ("/Foliage/", "/Trees/", "Leaves", "Grass", "Flower", "Fern", "Bush")

def log(msg):
    try:
        unreal.log("[MapColl] " + str(msg))
    except Exception:
        pass
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")

open(LOG, "w").write("map collision audit start fix=%s\n" % FIX)

def load_map(path):
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        return les.load_level(path)
    except Exception as e:
        log("load failed %s: %s" % (path, e))
        return False

report = {"fix": FIX, "maps": {}}
total_fixed = 0

for map_path in MAPS:
    name = map_path.split("/")[-1]
    if not load_map(map_path):
        report["maps"][name] = {"error": "load failed"}
        continue
    els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actors = els.get_all_level_actors()
    fixed_comps = 0
    checked = 0
    samples = []
    for actor in actors:
        try:
            comps = actor.get_components_by_class(unreal.StaticMeshComponent)
            for c in comps:
                mesh = c.get_editor_property("static_mesh")
                if not mesh:
                    continue
                checked += 1
                mp = str(mesh.get_path_name())
                if any(k.lower() in mp.lower() for k in FOLIAGE_KEYS):
                    continue
                # Only solid geometry that carries the player: wide flat things
                # (floors/platforms/stairs) and big structures (walls/buildings).
                ext = c.get_bounds().box_extent
                groundish = ext.z < 300 and max(ext.x, ext.y) > 250
                large = max(ext.x, ext.y, ext.z) > 1400
                if not (groundish or large):
                    continue
                prof = str(c.get_collision_profile_name())
                if prof == "BlockAll":
                    continue
                samples.append({"actor": str(actor.get_actor_label()), "mesh": mp.split(".")[0],
                                "profile": prof, "ground": bool(groundish), "large": bool(large)})
                if FIX:
                    try:
                        c.set_collision_profile_name("BlockAll")
                        actor.modify()
                        fixed_comps += 1
                    except Exception as e:
                        log("fix error %s: %s" % (mp, e))
        except Exception as e:
            log("actor error in %s: %s" % (name, e))
    if FIX and fixed_comps > 0:
        try:
            unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
            log("%s: saved level with %d fixes" % (name, fixed_comps))
        except Exception as e:
            log("save failed %s: %s" % (name, e))
    report["maps"][name] = {
        "checked_mesh_comps": checked,
        "fixed_comps": fixed_comps,
        "sample_problems": samples[:15],
        "problem_count": len(samples),
    }
    total_fixed += fixed_comps
    log("%s: checked=%d problems=%d fixed=%d" % (name, checked, len(samples), fixed_comps))

report["total_fixed"] = total_fixed
json.dump(report, open(out_path, "w"), indent=1)
unreal.log_warning("[MapColl] done fixed=%d -> %s" % (total_fixed, out_path))
