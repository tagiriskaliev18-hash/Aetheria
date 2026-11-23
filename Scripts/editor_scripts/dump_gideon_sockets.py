import traceback, unreal, json

LOG = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\gideon_sockets.json"
TXT = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\gideon_sockets.txt"

def log(msg):
    try: unreal.log("[Sockets] " + str(msg))
    except Exception: pass
    with open(TXT, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")

open(TXT, "w", encoding="utf-8").write("start\n")

def sock_info(s):
    return {
        "name": str(s.socket_name),
        "bone": str(s.bone_name),
        "loc": [s.relative_location.x, s.relative_location.y, s.relative_location.z],
        "rot": [s.relative_rotation.pitch, s.relative_rotation.yaw, s.relative_rotation.roll],
        "scale": [s.relative_scale.x, s.relative_scale.y, s.relative_scale.z],
    }

def list_sockets(asset, label):
    rows = []
    if not asset:
        log("MISSING " + label)
        return rows
    log("asset " + label + " class=" + asset.get_class().get_name())
    sockets = []
    # SkeletalMesh and Skeleton both have sockets
    for getter in ("sockets", "active_socket_list"):
        try:
            sockets = list(asset.get_editor_property(getter) or [])
            if sockets:
                log("  via " + getter + " n=" + str(len(sockets)))
                break
        except Exception as e:
            log("  " + getter + " " + str(e))
    # Also try get_sockets if any
    try:
        if hasattr(asset, "get_num_sockets"):
            log("  get_num_sockets=" + str(asset.get_num_sockets()))
    except Exception:
        pass
    for s in sockets:
        info = sock_info(s)
        rows.append(info)
        log("  SOCKET %s on %s loc=%s rot=%s" % (info["name"], info["bone"], info["loc"], info["rot"]))
    return rows

def list_bones(skel, keys):
    names = []
    try:
        n = skel.get_reference_skeleton().num() if hasattr(skel, "get_reference_skeleton") else None
        log("ref skel num=" + str(n))
    except Exception as e:
        log("ref skel " + str(e))
    # bone names via editor
    try:
        bone_names = skel.get_editor_property("bone_tree")
        log("bone_tree type=" + str(type(bone_names)))
    except Exception as e:
        log("bone_tree " + str(e))
    # SkeletalMesh.get_bone_name
    return names

def mesh_bounds(path):
    m = unreal.EditorAssetLibrary.load_asset(path)
    if not m:
        log("missing mesh " + path)
        return None
    try:
        box = m.get_bounding_box()
        mn, mx = box.min, box.max
        log("bounds %s min=(%.2f,%.2f,%.2f) max=(%.2f,%.2f,%.2f) extent=(%.2f,%.2f,%.2f)" % (
            path, mn.x, mn.y, mn.z, mx.x, mx.y, mx.z, mx.x-mn.x, mx.y-mn.y, mx.z-mn.z))
        return {"min":[mn.x,mn.y,mn.z], "max":[mx.x,mx.y,mx.z]}
    except Exception as e:
        log("bounds fail " + str(e))
        # try get_bounds
        try:
            b = m.get_bounds()
            log("get_bounds " + str(b))
        except Exception as e2:
            log("get_bounds " + str(e2))
    return None

def main():
    out = {}
    skel = unreal.EditorAssetLibrary.load_asset("/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon_Skeleton")
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon")
    out["skeleton_sockets"] = list_sockets(skel, "Gideon_Skeleton")
    out["mesh_sockets"] = list_sockets(mesh, "Gideon mesh")

    # bones of interest via skeletal mesh
    if mesh:
        interested = ("hand_r","hand_l","hand_lSocket","ik_hand_r","ik_hand_l","ik_hand_gun","ik_hand_root",
                      "weapon_r","Weapon_R","Muzzle_01","Muzzle_02","Muzzle_03","Trail","FX_Head","FX_Ring",
                      "faceAttach","HoverAttachPoint","PalmGlowRight","lowerarm_r","upperarm_r","index_01_r",
                      "middle_01_r","thumb_01_r","index_02_r")
        # get all bone names
        all_bones = []
        try:
            num = mesh.get_num_bones() if hasattr(mesh, "get_num_bones") else None
            log("mesh get_num_bones=" + str(num))
            if num:
                for i in range(int(num)):
                    try:
                        bn = str(mesh.get_bone_name(i))
                        all_bones.append(bn)
                    except Exception:
                        pass
        except Exception as e:
            log("get_num_bones " + str(e))
        # try reference skeleton on USkeleton
        if not all_bones and skel:
            try:
                # UE5 Python: skeleton.get_bone_names? 
                for meth in dir(skel):
                    if "bone" in meth.lower() or "socket" in meth.lower():
                        log("skel method " + meth)
            except Exception:
                pass
        log("bone count=" + str(len(all_bones)))
        for b in all_bones:
            low = b.lower()
            if any(k in low for k in ("hand","weapon","muzzle","trail","fx_","socket","attach","palm","gun","finger","thumb","index","middle","ring","pinky","grip")):
                log("  BONE " + b)
        out["handish_bones"] = [b for b in all_bones if any(k in b.lower() for k in ("hand","weapon","muzzle","trail","fx_","socket","attach","palm","gun"))]
        out["all_bone_count"] = len(all_bones)

        # DoesSocketExist style: query mesh sockets through a temp component? skip

    for path in (
        "/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Lunar_Wizard_Staff",
        "/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Crystalline_Conduit_Staff",
        "/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Winged_Golden_Staff",
        "/Game/RPG_Magic_Staff_Pack/Meshes/SM_Staff_Forgotten_Relic_Staff",
    ):
        out[path] = mesh_bounds(path)

    with open(LOG, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    log("DUMP COMPLETE")
    print("DUMP COMPLETE")
    try:
        if "-unattended" in str(unreal.SystemLibrary.get_command_line()).lower():
            unreal.SystemLibrary.quit_editor()
    except Exception as e:
        log("quit " + str(e))

if __name__ == "__main__":
    try:
        main()
    except Exception:
        log("FATAL\n" + traceback.format_exc())
        try: unreal.SystemLibrary.quit_editor()
        except Exception: pass
