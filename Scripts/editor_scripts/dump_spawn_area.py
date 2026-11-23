import unreal, os, json
proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
targets = {"World_Bastion": (520, 280, 120), "World_Ruins": (5100, -4300, 200)}
out = {}
for m, (x, y, z) in targets.items():
    world = unreal.EditorLoadingAndSavingUtils.load_map("/Game/Maps/AetheriaWorlds/" + m)
    if not world:
        out[m] = "load failed"; continue
    acts = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.StaticMeshActor)
    rows = []
    for a in acts:
        o, e = a.get_actor_bounds(False)
        # within 1500 uu horizontally of the spawn
        dx = max(0.0, abs(o.x - x) - e.x); dy = max(0.0, abs(o.y - y) - e.y)
        if dx > 1500 or dy > 1500: continue
        c = a.static_mesh_component
        sm = c.static_mesh
        bs = sm.get_editor_property("body_setup") if sm else None
        flag = str(bs.get_editor_property("collision_trace_flag")) if bs else "none"
        n = 0
        if bs:
            g = bs.get_editor_property("agg_geom")
            for k in ("sphere_elems","box_elems","sphyl_elems","convex_elems"):
                n += len(g.get_editor_property(k))
        rows.append({"actor": a.get_name(), "label": a.get_actor_label(), "mesh": sm.get_path_name() if sm else None,
                     "origin": [round(o.x), round(o.y), round(o.z)], "ext": [round(e.x), round(e.y), round(e.z)],
                     "simple": n, "flag": flag, "coll": str(c.get_collision_profile_name()), "enabled": str(c.get_collision_enabled())})
    rows.sort(key=lambda r: (abs(r["origin"][0]-x)+abs(r["origin"][1]-y)))
    ps = [ [round(p.get_actor_location().x), round(p.get_actor_location().y), round(p.get_actor_location().z)] for p in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.PlayerStart)]
    out[m] = {"playerstarts": ps, "count": len(rows), "rows": rows[:60]}
json.dump(out, open(os.path.join(proj, "Saved", "grok_spawn_area.json"), "w"), indent=1)
unreal.log_warning("[spawn_area] done")
