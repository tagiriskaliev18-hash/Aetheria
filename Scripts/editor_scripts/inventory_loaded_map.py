"""Dump actors + component bounds of the currently loaded editor map."""
import json
import os
import unreal

out_dir = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved"
os.makedirs(out_dir, exist_ok=True)

world = unreal.EditorLevelLibrary.get_editor_world()
map_name = world.get_name() if world else "unknown"
actors = unreal.EditorLevelLibrary.get_all_level_actors()
rows = []
min_v = [1e12, 1e12, 1e12]
max_v = [-1e12, -1e12, -1e12]
bmin = [1e12, 1e12, 1e12]
bmax = [-1e12, -1e12, -1e12]

for a in actors:
    loc = a.get_actor_location()
    rot = a.get_actor_rotation()
    sc = a.get_actor_scale3d()
    cls = a.get_class().get_name()
    mesh_path = ""
    origin = loc
    extent = None
    try:
        origin, extent = a.get_actor_bounds(True)
    except Exception:
        try:
            origin, extent = a.get_actor_bounds(False)
        except Exception:
            origin, extent = loc, None
    if hasattr(a, "static_mesh_component") and a.static_mesh_component:
        smc = a.static_mesh_component
        try:
            if smc.static_mesh:
                mesh_path = smc.static_mesh.get_path_name()
        except Exception:
            pass
    rec = {
        "name": a.get_name(),
        "label": a.get_actor_label(),
        "class": cls,
        "loc": [loc.x, loc.y, loc.z],
        "rot": [rot.pitch, rot.yaw, rot.roll],
        "scale": [sc.x, sc.y, sc.z],
        "mesh": mesh_path,
        "folder": str(a.get_folder_path()) if hasattr(a, "get_folder_path") else "",
        "hidden": bool(a.is_hidden_ed()),
        "origin": [origin.x, origin.y, origin.z] if origin is not None else None,
        "extent": [extent.x, extent.y, extent.z] if extent is not None else None,
    }
    rows.append(rec)
    for i, v in enumerate((loc.x, loc.y, loc.z)):
        min_v[i] = min(min_v[i], v)
        max_v[i] = max(max_v[i], v)
    if extent is not None:
        for i, (o, e) in enumerate(zip((origin.x, origin.y, origin.z), (extent.x, extent.y, extent.z))):
            bmin[i] = min(bmin[i], o - e)
            bmax[i] = max(bmax[i], o + e)

payload = {
    "map": map_name,
    "actor_count": len(rows),
    "loc_min": min_v,
    "loc_max": max_v,
    "bounds_min": bmin,
    "bounds_max": bmax,
    "actors": rows,
}
out = os.path.join(out_dir, f"inventory_{map_name}.json")
with open(out, "w", encoding="utf-8") as f:
    json.dump(payload, f, indent=1)
unreal.log(f"[Aetheria] Inventoried {len(rows)} actors from {map_name} loc={min_v}..{max_v} bounds={bmin}..{bmax} -> {out}")
print(f"WROTE {out} count={len(rows)}")

try:
    cmd = ""
    try:
        cmd = str(unreal.SystemLibrary.get_command_line())
    except Exception:
        cmd = ""
    if "-unattended" in cmd.lower():
        unreal.log("[Aetheria] -unattended set, quitting editor after script")
        unreal.SystemLibrary.quit_editor()
except Exception as e:
    print("quit_editor failed", e)
