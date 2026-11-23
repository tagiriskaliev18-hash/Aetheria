# Run: UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script="<this>"
# Void terrain audit:
#  1) floor coverage map around spawn (which cells have floor-like mesh, which are holes)
#  2) actor+material inventory near the biggest hole cluster and near spawn
# Bounds-only approach (no physics traces): topmost floor-height bbox surface per cell.
import unreal, sys, os, json

proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
LOG = os.path.join(proj, "Saved", "grok_void_terrain_log.txt")
OUT = os.path.join(proj, "Saved", "grok_void_terrain.json")

def log(msg):
    unreal.log("[VoidAudit] %s" % msg)
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")

open(LOG, "w").write("void terrain audit start\n")

les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
if not les.load_level("/Game/Maps/AetheriaWorlds/World_Void"):
    log("FATAL: cannot load World_Void")
    sys.exit(1)

els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = els.get_all_level_actors()

FLOOR_Z_MAX = 400.0      # ignore surfaces above this (walls top, decorations)
MIN_MESH_SIZE = 60.0     # floor-like mesh must span at least this in X or Y

# collect floor-like surfaces: (min_x, min_y, max_x, max_y, top_z, actor, mesh, mats)
surfaces = []
all_actors = []
for actor in actors:
    try:
        origin, ext = actor.get_actor_bounds(False)
    except Exception:
        continue
    all_actors.append((actor.get_name(), actor.get_class().get_name(),
                       origin.x, origin.y, origin.z, ext.x, ext.y, ext.z))
    if actor.get_class().get_name() not in ("StaticMeshActor",):
        continue
    if ext.z > 500.0 and ext.x > 500.0:
        continue  # huge backdrops, not floor patches
    top = origin.z + ext.z
    if top > FLOOR_Z_MAX or top < -50.0:
        continue
    if ext.x < MIN_MESH_SIZE and ext.y < MIN_MESH_SIZE:
        continue
    mats = []
    try:
        for c in actor.get_components_by_class(unreal.StaticMeshComponent):
            mesh = c.get_editor_property("static_mesh")
            mname = mesh.get_name() if mesh else "None"
            for i in range(min(4, c.get_num_materials())):
                try:
                    mi = c.get_material(i)
                    mats.append(mi.get_name() if mi else "None")
                except Exception:
                    pass
            mats.append("mesh=" + mname)
    except Exception:
        pass
    surfaces.append((origin.x - ext.x, origin.y - ext.y, origin.x + ext.x,
                     origin.y + ext.y, top, actor.get_name(), ";".join(mats[:6])))

log("floor-like surfaces: %d (actors total: %d)" % (len(surfaces), len(all_actors)))

# grid coverage around spawn area
STEP = 100.0
HALF = 2000.0
holes = []
covered_cells = []
gx = -HALF
while gx <= HALF:
    gy = -HALF
    while gy <= HALF:
        top_here = None
        for (x0, y0, x1, y1, top, an, m) in surfaces:
            if x0 <= gx <= x1 and y0 <= gy <= y1:
                if top_here is None or top > top_here:
                    top_here = top
        if top_here is None:
            holes.append((gx, gy))
        else:
            covered_cells.append((gx, gy))
        gy += STEP
    gx += STEP

covered = len(covered_cells)

total_cells = len(holes) + covered
log("grid cells: %d, covered: %d, holes: %d (%.0f%%)" %
    (total_cells, covered, len(holes), 100.0 * len(holes) / max(1, total_cells)))

# cluster holes: flood-fill from grid border first — holes reachable from the
# border are the designed void around the arena; the rest are INTERIOR holes
# (gaps enclosed by floor = "a hole in the field").
hole_set = set(holes)
border_reachable = set()
stack = [(x, y) for (x, y) in holes
         if abs(x) >= HALF or abs(y) >= HALF]
border_reachable.update(stack)
while stack:
    cx, cy = stack.pop()
    for dx in (-STEP, 0.0, STEP):
        for dy in (-STEP, 0.0, STEP):
            n = (cx + dx, cy + dy)
            if n in hole_set and n not in border_reachable:
                border_reachable.add(n)
                stack.append(n)
interior = hole_set - border_reachable
log("interior holes (enclosed by floor): %d cells" % len(interior))

def clusters_of(cellset):
    seen = set()
    out = []
    for c in cellset:
        if c in seen:
            continue
        st = [c]
        comp = []
        seen.add(c)
        while st:
            cx, cy = st.pop()
            comp.append((cx, cy))
            for dx in (-STEP, 0.0, STEP):
                for dy in (-STEP, 0.0, STEP):
                    n = (cx + dx, cy + dy)
                    if n in cellset and n not in seen:
                        seen.add(n)
                        st.append(n)
        xs = [p[0] for p in comp]
        ys = [p[1] for p in comp]
        out.append({"cells": len(comp), "x0": min(xs), "x1": max(xs),
                    "y0": min(ys), "y1": max(ys)})
    out.sort(key=lambda c: -c["cells"])
    return out

interior_clusters = clusters_of(interior)
for c in interior_clusters[:8]:
    log("INTERIOR hole cluster: cells=%d x[%.0f..%.0f] y[%.0f..%.0f]" %
        (c["cells"], c["x0"], c["x1"], c["y0"], c["y1"]))

# PNG coverage map: covered=grey, border-void=dark, interior hole=red, spawn=green
try:
    from PIL import Image, ImageDraw
    CELL_PX = 6
    n_cells = int(HALF * 2 / STEP) + 1
    img = Image.new("RGB", (n_cells * CELL_PX, n_cells * CELL_PX), (20, 16, 26))
    d = ImageDraw.Draw(img)

    def cell_px(v):
        return int((v + HALF) / STEP) * CELL_PX
    for (x, y) in covered_cells:
        d.rectangle([cell_px(x), cell_px(y), cell_px(x) + CELL_PX - 1,
                     cell_px(y) + CELL_PX - 1], fill=(120, 120, 140))
    for (x, y) in interior:
        d.rectangle([cell_px(x), cell_px(y), cell_px(x) + CELL_PX - 1,
                     cell_px(y) + CELL_PX - 1], fill=(220, 40, 40))
    d.rectangle([cell_px(0) - 4, cell_px(-150) - 4, cell_px(0) + 4,
                 cell_px(-150) + 4], fill=(60, 220, 80))
    map_png = os.path.join(proj, "Saved", "grok_void_coverage.png")
    img.save(map_png)
    log("coverage map: %s" % map_png)
except Exception as e:
    log("png failed: %s" % e)

report = {"holes_total": len(holes), "covered": covered,
          "interior_holes": len(interior),
          "interior_clusters": interior_clusters[:8],
          "big_cluster_actors": [], "surface_samples": []}
if interior_clusters:
    b = interior_clusters[0]
    for (an, cn, ox, oy, oz, ex, ey, ez) in all_actors:
        if (b["x0"] - 300 <= ox <= b["x1"] + 300 and b["y0"] - 300 <= oy <= b["y1"] + 300
                and len(report["big_cluster_actors"]) < 40):
            report["big_cluster_actors"].append(
                {"name": an, "class": cn, "x": ox, "y": oy, "z": oz,
                 "ext": [ex, ey, ez]})
for s in surfaces[:25]:
    report["surface_samples"].append(
        {"box": s[:4], "top_z": s[4], "actor": s[5], "mats": s[6]})

with open(OUT, "w", encoding="utf-8") as f:
    json.dump(report, f, indent=1, default=str)
log("report written: %s" % OUT)
