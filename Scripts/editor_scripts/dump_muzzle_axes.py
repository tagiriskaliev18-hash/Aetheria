import traceback, unreal
TXT = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\gideon_muzzle_axes.txt"
def log(msg):
    try: unreal.log(str(msg))
    except Exception: pass
    with open(TXT, "a", encoding="utf-8") as f: f.write(str(msg)+"\n")
open(TXT,"w",encoding="utf-8").write("start\n")

def v3(v):
    return (round(v.x,3), round(v.y,3), round(v.z,3))

def main():
    mesh = unreal.EditorAssetLibrary.load_asset("/Game/ParagonGideon/Characters/Heroes/Gideon/Meshes/Gideon")
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(unreal.SkeletalMeshActor, unreal.Vector(0,0,100))
    comp = actor.skeletal_mesh_component
    try: comp.set_skeletal_mesh_asset(mesh)
    except Exception: comp.set_skeletal_mesh(mesh)

    names = ["Muzzle_01","Muzzle_02","Muzzle_03","hand_r","hand_l","hand_lSocket","lowerarm_r","index_01_r","thumb_01_r","middle_01_r","ik_hand_r","ik_hand_gun"]
    for n in names:
        exists = comp.does_socket_exist(n)
        parent = "?"
        try:
            parent = str(comp.get_socket_bone_name(n))
        except Exception as e:
            parent = str(e)
        if not exists:
            log("%s exists=False parent=%s" % (n, parent))
            continue
        t = comp.get_socket_transform(n, unreal.RelativeTransformSpace.RTS_COMPONENT)
        x = unreal.MathLibrary.transform_direction(t, unreal.Vector(1,0,0))
        y = unreal.MathLibrary.transform_direction(t, unreal.Vector(0,1,0))
        z = unreal.MathLibrary.transform_direction(t, unreal.Vector(0,0,1))
        loc = t.translation
        rot = t.rotation.rotator()
        log("%s parent=%s loc=%s rot=(p%.2f y%.2f r%.2f) X=%s Y=%s Z=%s" % (
            n, parent, v3(loc), rot.pitch, rot.yaw, rot.roll, v3(x), v3(y), v3(z)))

    # how to construct a socket with a name
    try:
        s = unreal.new_object(unreal.SkeletalMeshSocket, mesh, "weapon_r_test")
        log("new_object socket name=" + str(s.get_name()) + " socket_name=" + str(s.socket_name) + " bone=" + str(s.bone_name))
    except Exception as e:
        log("new_object " + str(e))

    log("AXES COMPLETE")
    print("AXES COMPLETE")
    try:
        if "-unattended" in str(unreal.SystemLibrary.get_command_line()).lower():
            unreal.SystemLibrary.quit_editor()
    except Exception as e:
        log("quit "+str(e))

if __name__ == "__main__":
    try: main()
    except Exception:
        log("FATAL\n"+traceback.format_exc())
        try: unreal.SystemLibrary.quit_editor()
        except Exception: pass
