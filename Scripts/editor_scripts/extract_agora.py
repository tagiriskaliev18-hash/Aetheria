import os
import json
import math
import collections
import traceback
import unreal

OUT_DIR = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved"
LOG = os.path.join(OUT_DIR, "extract_agora_log.txt")

def log(msg):
    line = "[AgoraExtract] " + str(msg)
    try:
        unreal.log(line)
    except Exception:
        pass
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")
        f.flush()

with open(LOG, "w", encoding="utf-8") as f:
    f.write("extract agora start\n")

def actor_info(a):
    loc = a.get_actor_location()
    rot = a.get_actor_rotation()
    scale = a.get_actor_scale3d()
    mesh_path = ""
    origin = [0, 0, 0]
    extent = [0, 0, 0]
    try:
        comps = a.get_components_by_class(unreal.StaticMeshComponent)
    except Exception:
        comps = []
    if comps:
        c = comps[0]
        try:
            m = c.static_mesh
            if m:
                mesh_path = m.get_path_name()
        except Exception:
            pass
        try:
            o, e = c.get_local_bounds()
            origin = [o.x, o.y, o.z]
            extent = [e.x, e.y, e.z]
        except Exception:
            pass
    label = ""
    try:
        label = a.get_actor_label()
    except Exception:
        label = a.get_name()
    hidden = False
    try:
        hidden = bool(a.is_hidden_ed()) or bool(a.get_editor_property("hidden"))
    except Exception:
        pass
    return {
        "name": a.get_name(),
        "label": label,
        "class": a.get_class().get_name(),
        "loc": [loc.x, loc.y, loc.z],
        "rot": [rot.pitch, rot.yaw, rot.roll],
        "scale": [scale.x, scale.y, scale.z],
        "mesh": mesh_path,
        "hidden": hidden,
        "origin": origin,
        "extent": extent,
    }

def main():
    world = None
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:
        world = unreal.EditorLevelLibrary.get_editor_world()
    map_name = world.get_name() if world else "?"
    log("loaded world " + str(map_name))
    actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))
    log("actor count " + str(len(actors)))
    infos = []
    for i, a in enumerate(actors):
        try:
            infos.append(actor_info(a))
        except Exception as e:
            log("actor fail " + a.get_name() + " " + str(e))
        if i and i % 500 == 0:
            log("inventoried " + str(i))

    xs = [a["loc"][0] for a in infos]
    ys = [a["loc"][1] for a in infos]
    zs = [a["loc"][2] for a in infos]
    bounds = {
        "map": map_name,
        "actor_count": len(infos),
        "loc_min": [min(xs), min(ys), min(zs)] if xs else [0, 0, 0],
        "loc_max": [max(xs), max(ys), max(zs)] if xs else [0, 0, 0],
        "class_counts": collections.Counter(a["class"] for a in infos).most_common(30),
        "with_mesh": sum(1 for a in infos if a.get("mesh")),
    }
    log("bounds " + json.dumps(bounds, default=str))

    # 2000 uu cells, pick densest mesh cell, then take actors within radius
    cell = 2000.0
    hist = collections.Counter()
    for a in infos:
        if not a.get("mesh"):
            continue
        cx = int(math.floor(a["loc"][0] / cell))
        cy = int(math.floor(a["loc"][1] / cell))
        hist[(cx, cy)] += 1
    if not hist:
        log("NO MESH ACTORS")
        return
    (cx, cy), count = hist.most_common(1)[0]
    center = [(cx + 0.5) * cell, (cy + 0.5) * cell, 0.0]
    # z = median z of meshes in that cell
    zs_cell = [a["loc"][2] for a in infos if a.get("mesh")
               and int(math.floor(a["loc"][0] / cell)) == cx
               and int(math.floor(a["loc"][1] / cell)) == cy]
    if zs_cell:
        zs_cell.sort()
        center[2] = zs_cell[len(zs_cell) // 2]
    log("densest cell %s count=%d center=%s top5=%s" % ((cx, cy), count, center, hist.most_common(5)))

    radius = 10000.0
    cluster = []
    for a in infos:
        dx = a["loc"][0] - center[0]
        dy = a["loc"][1] - center[1]
        if (dx * dx + dy * dy) ** 0.5 <= radius:
            cluster.append(a)
    log("cluster actors %d (radius %s)" % (len(cluster), radius))

    full_path = os.path.join(OUT_DIR, "inventory_ParagonSample.json")
    # write a compact full inventory without blowing disk too badly
    with open(full_path, "w", encoding="utf-8") as f:
        json.dump({"map": map_name, "actor_count": len(infos), "actors": infos, "bounds": bounds}, f)
    log("wrote " + full_path)

    cluster_path = os.path.join(OUT_DIR, "inventory_agora_cluster.json")
    meta_path = os.path.join(OUT_DIR, "inventory_agora_cluster_meta.json")
    with open(cluster_path, "w", encoding="utf-8") as f:
        json.dump({"map": map_name, "center": center, "radius": radius, "actors": cluster}, f)
    with open(meta_path, "w", encoding="utf-8") as f:
        json.dump({
            "center": center,
            "radius": radius,
            "count": len(cluster),
            "mesh_count": sum(1 for a in cluster if a.get("mesh")),
            "densest_cell": [cx, cy],
            "densest_count": count,
            "bounds": bounds,
            "top_cells": [[list(k), v] for k, v in hist.most_common(10)],
        }, f, indent=2)
    log("wrote cluster %s count=%d" % (cluster_path, len(cluster)))
    log("EXTRACT AGORA COMPLETE")
    print("EXTRACT AGORA COMPLETE")
    try:
        cmd = str(unreal.SystemLibrary.get_command_line())
        if "-unattended" in cmd.lower():
            unreal.SystemLibrary.quit_editor()
    except Exception as e:
        log("quit failed " + str(e))

if __name__ == "__main__":
    try:
        main()
    except Exception:
        log("FATAL\n" + traceback.format_exc())
        try:
            unreal.SystemLibrary.quit_editor()
        except Exception:
            pass
