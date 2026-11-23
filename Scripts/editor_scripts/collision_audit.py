# Run: UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script="<this> [fix]"
import unreal, sys, os, shutil, json
FIX = "fix" in " ".join(sys.argv).lower() or os.environ.get("GROK_COLL_FIX") == "1"
proj = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())
out_path = os.path.join(proj, "Saved", "grok_collision_audit.json")
ar = unreal.AssetRegistryHelpers.get_asset_registry()
maps = ["World_Bastion", "World_Ruins", "World_Void", "World_Sanctum", "ArtistMenu", "MenuCourtyard"]
opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
def deps_of(pkg):
    try:
        return ar.get_dependencies(pkg, opts) or []
    except Exception:
        return []
mesh_users = {}
for m in maps:
    pkgs = ["/Game/Maps/AetheriaWorlds/" + m]
    ext = "/Game/__ExternalActors__/Maps/AetheriaWorlds/" + m
    for a in ar.get_assets_by_path(ext, recursive=True) or []:
        pkgs.append(str(a.package_name))
    for p in pkgs:
        for d in deps_of(p):
            d = str(d)
            if d.startswith("/Game/"):
                mesh_users.setdefault(d, set()).add(m)
def simple_count(mesh):
    bs = mesh.get_editor_property("body_setup")
    if not bs:
        return 0
    g = bs.get_editor_property("agg_geom")
    n = 0
    for k in ("sphere_elems", "box_elems", "sphyl_elems", "convex_elems", "tapered_capsule_elems", "level_set_elems"):
        try:
            n += len(g.get_editor_property(k))
        except Exception:
            pass
    return n
report = []
fixed = 0
backup_dir = os.path.join(proj, "Saved", "GrokBackup", "collision")
for pkg, users in sorted(mesh_users.items()):
    datas = ar.get_assets_by_package_name(pkg) or []
    for ad in datas:
        if str(ad.asset_class_path.asset_name) != "StaticMesh":
            continue
        mesh = unreal.load_asset(str(ad.package_name) + "." + str(ad.asset_name))
        if not mesh:
            continue
        simple = simple_count(mesh)
        bs = mesh.get_editor_property("body_setup")
        flag = str(bs.get_editor_property("collision_trace_flag")) if bs else "none"
        nav = True
        bounds = mesh.get_bounds()
        ext = bounds.box_extent
        rec = {"mesh": pkg, "simple": simple, "flag": flag, "users": sorted(users), "extent": [round(ext.x), round(ext.y), round(ext.z)]}
        if simple == 0 and bs and "USE_COMPLEX_AS_SIMPLE" not in flag:
            rec["problem"] = "no simple collision -> capsules fall through"
            is_foliage = any(k in pkg for k in ("/Foliage/", "/Trees/", "Leaves", "Grass", "Flower", "Fern", "Bush"))
            rec["foliage"] = is_foliage
            if FIX and not is_foliage:
                src = unreal.SystemLibrary.convert_to_absolute_path(unreal.Paths.combine([proj, "Content", pkg[len("/Game/"):] + ".uasset"]))
                if os.path.exists(src):
                    dst = os.path.join(backup_dir, pkg[len("/Game/"):].replace("/", os.sep) + ".uasset")
                    os.makedirs(os.path.dirname(dst), exist_ok=True)
                    if not os.path.exists(dst):
                        shutil.copy2(src, dst)
                bs.modify()
                bs.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
                mesh.modify()
                try:
                    bs.invalidate_physics_data()
                except Exception:
                    pass
                unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
                rec["fixed"] = True
                fixed += 1
        report.append(rec)
probs = [r for r in report if "problem" in r]
json.dump({"fix": FIX, "meshes": len(report), "problems": len(probs), "fixed": fixed, "list": probs}, open(out_path, "w"), indent=1)
unreal.log_warning("[collision_audit] meshes=%d problems=%d fixed=%d -> %s" % (len(report), len(probs), fixed, out_path))
