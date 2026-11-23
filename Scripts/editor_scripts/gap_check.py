# Run via -run=pythonscript. Dump floor tiles around the gap + coverage test.
import unreal, os

proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG = os.path.join(proj, "Saved", "grok_void_gapcheck_log.txt")

def log(m):
    unreal.log("[GapCheck] %s" % m)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(str(m) + "\n")

open(LOG, "w").write("start\n")
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.load_level("/Game/Maps/AetheriaWorlds/World_Void"):
    log("FATAL load");  raise SystemExit(1)
els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

tiles = []
for a in els.get_all_level_actors():
    try:
        if a.get_class().get_name() != "StaticMeshActor":
            continue
        origin, ext = a.get_actor_bounds(False)
        top = origin.z + ext.z
        if top > 400.0 or top < -50.0:
            continue
        if ext.x < 60.0 and ext.y < 60.0:
            continue
        if 1000 <= origin.x <= 2200 and 600 <= origin.y <= 1600:
            comps = a.get_components_by_class(unreal.StaticMeshComponent)
            mesh = comps[0].get_editor_property("static_mesh") if comps else None
            tiles.append((origin.x, origin.y, origin.z + ext.z,
                          ext.x, ext.y,
                          a.get_name(),
                          mesh.get_name() if mesh else "None"))
    except Exception:
        pass

tiles.sort(key=lambda t: (t[0], t[1]))
log("tiles near gap (x 1000-2200, y 600-1600): %d" % len(tiles))
for t in tiles:
    log("  pos=(%.0f,%.0f) top=%.0f ext=(%.0f,%.0f) %s [%s]"
        % (t[0], t[1], t[2], t[3], t[4], t[5][:28], t[6][:30]))

# coverage at the gap column
for gy in (800, 900, 1000, 1100, 1200, 1300, 1400):
    cover = []
    for gx in (1400, 1500, 1600, 1700):
        hit = None
        for (ox, oy, top, ex, ey, an, mn) in tiles:
            if abs(ox - gx) <= ex and abs(oy - gy) <= ey:
                if hit is None or top > hit:
                    hit = top
        cover.append("%d:%s" % (gx, ("%.0f" % hit) if hit is not None else "HOLE"))
    log("coverage y=%d -> %s" % (gy, " | ".join(cover)))
