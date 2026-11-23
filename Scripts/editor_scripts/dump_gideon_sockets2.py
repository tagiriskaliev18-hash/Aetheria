import traceback, unreal, json

TXT = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\gideon_sockets2.txt"
JSON = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\gideon_sockets2.json"

def log(msg):
    try: unreal.log("[Sock2] " + str(msg))
    except Exception: pass
    with open(TXT, "a", encoding="utf-8") as f:
        f.write(str(msg) + "\n")

open(TXT, "w", encoding="utf-8").write("start\n")

def v3(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]

def r3(r):
    return [round(r.pitch, 3), round(r.yaw, 3), round(r.roll, 3)]

def main():
    log("unreal socket/skel types:")
    for n in dir(unreal):
        low = n.lower()
        if "socket" in low or "skeleton" in low or "skeletalmesh" in low:
            log("  " + n)

    mesh = unreal.EditorAssetLibrary.load_asset("/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon")
    skel = unreal.EditorAssetLibrary.load_asset("/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon_Skeleton")
    log("mesh=" + str(mesh) + " skel=" + str(skel))

    # spawn a temporary skeletal mesh actor in the current editor world
    loc = unreal.Vector(0, 0, 100)
    actor = None
    try:
        actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, loc)
    except Exception as e:
        log("spawn ELL " + str(e))
        try:
            actor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).spawn_actor_from_class(unreal.SkeletalMeshActor, loc)
        except Exception as e2:
            log("spawn ALS " + str(e2))
    if not actor:
        log("NO ACTOR")
        return
    comp = actor.skeletal_mesh_component
    try:
        comp.set_skeletal_mesh_asset(mesh)
    except Exception as e:
        log("set_skeletal_mesh_asset " + str(e))
        try:
            comp.set_skeletal_mesh(mesh)
        except Exception as e2:
            log("set_skeletal_mesh " + str(e2))

    names = []
    try:
        names = list(comp.get_all_socket_names())
    except Exception as e:
        log("get_all_socket_names " + str(e))
    log("socket names n=" + str(len(names)))
    out = {"sockets": []}
    for n in names:
        info = {"name": str(n)}
        try:
            rel = comp.get_socket_transform(n, unreal.RelativeTransformSpace.RTS_PARENT_BONE_SPACE)
            info["parent_bone_space_loc"] = v3(rel.translation)
            info["parent_bone_space_rot"] = r3(rel.rotation.rotator())
        except Exception as e:
            info["pbs"] = str(e)
        try:
            comp_t = comp.get_socket_transform(n, unreal.RelativeTransformSpace.RTS_COMPONENT)
            info["comp_loc"] = v3(comp_t.translation)
        except Exception as e:
            info["comp"] = str(e)
        try:
            info["exists"] = bool(comp.does_socket_exist(n))
        except Exception:
            pass
        log("SOCKET " + json.dumps(info))
        out["sockets"].append(info)

    bones = ["hand_r","hand_l","hand_lSocket","ik_hand_r","ik_hand_l","ik_hand_gun","ik_hand_root",
             "lowerarm_r","index_01_r","index_02_r","thumb_01_r","middle_01_r","weapon_r",
             "Muzzle_01","Muzzle_02","Muzzle_03","Trail","FX_Head","FX_Ring","HoverAttachPoint","faceAttach"]
    out["bones"] = []
    for b in bones:
        exists = False
        try:
            exists = bool(comp.does_socket_exist(b))
        except Exception:
            pass
        info = {"name": b, "exists": exists}
        if exists:
            try:
                t = comp.get_socket_transform(b, unreal.RelativeTransformSpace.RTS_COMPONENT)
                info["comp_loc"] = v3(t.translation)
                info["comp_rot"] = r3(t.rotation.rotator())
                # axes in component space
                q = t.rotation
                info["axis_x"] = v3(q.get_forward_vector())
                info["axis_y"] = v3(q.get_right_vector())
                info["axis_z"] = v3(q.get_up_vector())
            except Exception as e:
                info["err"] = str(e)
        log("BONE " + json.dumps(info))
        out["bones"].append(info)

    # Try APIs to add a socket
    for obj, label in ((skel, "skel"), (mesh, "mesh"), (comp, "comp")):
        log("methods on " + label)
        for m in dir(obj):
            if "socket" in m.lower() or m.lower() in ("add","create","modify"):
                log("  " + label + "." + m)

    # subsystems
    for name in dir(unreal):
        if "Subsystem" in name and ("Skeletal" in name or "Skeleton" in name or "Anim" in name):
            log("subsystem type " + name)

    # try add
    try:
        sock = unreal.SkeletalMeshSocket()
        log("created SkeletalMeshSocket " + str(sock))
        for prop in dir(sock):
            if not prop.startswith("_"):
                log("  sock." + prop)
        try:
            sock.set_editor_property("socket_name", "weapon_r")
            sock.set_editor_property("bone_name", "hand_r")
            log("set name/bone ok")
        except Exception as e:
            log("set_editor_property " + str(e))
    except Exception as e:
        log("new socket " + str(e))

    with open(JSON, "w", encoding="utf-8") as f:
        json.dump(out, f, indent=2)
    log("DUMP2 COMPLETE")
    print("DUMP2 COMPLETE")
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
