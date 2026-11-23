import os, json, traceback, unreal

LOG = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\rebake_fix_log.txt"

def log(msg):
    line = "[Rebake] " + str(msg)
    try:
        unreal.log(line)
    except Exception:
        pass
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")
        f.flush()

with open(LOG, "w", encoding="utf-8") as f:
    f.write("rebake start\n")

def editor_world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:
        return unreal.EditorLevelLibrary.get_editor_world()

def load_map(path):
    log("load " + path)
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ok = les.load_level(path)
    log("load -> " + str(ok))
    return ok

def save_current(path=None):
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    ok = les.save_current_level()
    log("save_current -> " + str(ok))
    if path:
        try:
            unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        except Exception as e:
            log("save_asset " + str(e))

def all_actors():
    return list(unreal.GameplayStatics.get_all_actors_of_class(editor_world(), unreal.Actor))

def set_movable(comp):
    if not comp:
        return
    try:
        comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    except Exception:
        try:
            comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        except Exception:
            pass

def spawn_actor(cls, loc, rot=None):
    if rot is None:
        rot = unreal.Rotator(0, 0, 0)
    try:
        return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    except Exception:
        als = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        return als.spawn_actor_from_class(cls, loc, rot)

def spawn_sm(path, loc, rot, scale, label):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not mesh:
        log("MISSING " + path)
        return None
    actor = spawn_actor(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(*rot))
    if not actor:
        return None
    try:
        actor.set_actor_label(label)
        actor.set_actor_scale3d(unreal.Vector(*scale))
        c = actor.static_mesh_component
        set_movable(c)
        c.set_static_mesh(mesh)
        c.set_world_scale3d(unreal.Vector(*scale))
        c.set_visibility(True, True)
        c.set_hidden_in_game(False, True)
    except Exception as e:
        log("sm setup " + label + " " + str(e))
    return actor

def prepare_shell(spawn, intensity=36.0):
    world = editor_world()
    ws = world.get_world_settings()
    try:
        ws.set_editor_property("force_no_precomputed_lighting", True)
    except Exception as e:
        log("WS " + str(e))
    try:
        gm = unreal.load_class(None, "/Script/Aetheria.AetheriaGameMode")
        if gm:
            ws.set_editor_property("default_game_mode", gm)
    except Exception as e:
        log("GM " + str(e))
    has_dir = has_sky = has_fog = has_atmo = has_pp = has_ps = False
    for a in all_actors():
        n = a.get_class().get_name()
        if "DirectionalLight" in n:
            has_dir = True
            set_movable(getattr(a, "light_component", None))
        elif n.startswith("SkyLight") or "SkyLight" in n:
            has_sky = True
            lc = getattr(a, "light_component", None)
            set_movable(lc)
            try:
                lc.set_editor_property("real_time_capture", True)
                lc.set_editor_property("intensity", intensity * 0.45)
            except Exception:
                pass
        elif "ExponentialHeightFog" in n:
            has_fog = True
        elif "SkyAtmosphere" in n:
            has_atmo = True
        elif "PostProcessVolume" in n:
            has_pp = True
        elif "PlayerStart" in n:
            has_ps = True
            a.set_actor_location(unreal.Vector(*spawn), False, False)
    if not has_dir:
        d = spawn_actor(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(-56, 26, 0))
        if d:
            set_movable(getattr(d, "light_component", None))
            try:
                d.light_component.set_editor_property("intensity", intensity)
                d.light_component.set_editor_property("atmosphere_sun_light", True)
            except Exception:
                pass
    if not has_sky:
        s = spawn_actor(unreal.SkyLight, unreal.Vector(0, 0, 400))
        if s:
            set_movable(getattr(s, "light_component", None))
            try:
                s.light_component.set_editor_property("real_time_capture", True)
                s.light_component.set_editor_property("intensity", intensity * 0.45)
            except Exception:
                pass
    if not has_fog:
        spawn_actor(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    if not has_atmo:
        try:
            spawn_actor(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
        except Exception as e:
            log("atmo " + str(e))
    if not has_pp:
        pp = spawn_actor(unreal.PostProcessVolume, unreal.Vector(0, 0, 200))
        if pp:
            try:
                pp.set_editor_property("unbound", True)
                pp.set_editor_property("blend_weight", 1.0)
            except Exception:
                pass
    if not has_ps:
        spawn_actor(unreal.PlayerStart, unreal.Vector(*spawn))
        log("spawned PlayerStart " + str(spawn))
    # fill light so floors never go to a black pit
    pl = spawn_actor(unreal.PointLight, unreal.Vector(spawn[0], spawn[1], spawn[2] + 280))
    if pl:
        set_movable(getattr(pl, "light_component", None))
        try:
            pl.light_component.set_editor_property("intensity", 8000.0)
            pl.light_component.set_editor_property("attenuation_radius", 4000.0)
            pl.light_component.set_editor_property("cast_shadows", False)
        except Exception as e:
            log("point light " + str(e))
        log("spawned fill PointLight")

def delete_by_label_prefix(prefixes):
    n = 0
    for a in all_actors():
        label = ""
        try:
            label = str(a.get_actor_label())
        except Exception:
            label = a.get_name()
        if any(label.startswith(p) or p.lower() in label.lower() for p in prefixes):
            try:
                a.destroy_actor()
                n += 1
            except Exception:
                try:
                    unreal.EditorLevelLibrary.destroy_actor(a)
                    n += 1
                except Exception as e:
                    log("destroy fail " + label + " " + str(e))
    log("deleted " + str(n) + " actors matching " + str(prefixes))

def move_player_start(loc):
    found = False
    for a in all_actors():
        if "PlayerStart" in a.get_class().get_name():
            a.set_actor_location(unreal.Vector(*loc), False, False)
            found = True
            log("moved PlayerStart to " + str(loc))
    if not found:
        spawn_actor(unreal.PlayerStart, unreal.Vector(*loc))
        log("spawned PlayerStart " + str(loc))

def stamp_inventory(json_path, skip_substrings, label_prefix):
    data = json.load(open(json_path, encoding="utf-8"))
    n = 0
    skipped = 0
    for a in data.get("actors") or []:
        mesh = a.get("mesh") or ""
        if not mesh:
            skipped += 1
            continue
        label = (a.get("label") or a.get("name") or "")
        low = (label + " " + mesh).lower()
        if any(s in low for s in skip_substrings):
            skipped += 1
            continue
        asset_path = mesh.split(".")[0]
        loc = a.get("loc") or [0, 0, 0]
        rot = a.get("rot") or [0, 0, 0]
        scale = a.get("scale") or [1, 1, 1]
        if spawn_sm(asset_path, loc, rot, scale, (label_prefix + "_" + label)[:60]):
            n += 1
        if n and n % 100 == 0:
            log("stamped " + str(n))
    log("stamp done n=%d skipped=%d" % (n, skipped))
    return n

def main():
    tiny = "/Game/Maps/AetheriaWorld"
    # --- World_Void: rebuild from medieval inventory into a clean shell ---
    dest = "/Game/Maps/AetheriaWorlds/World_Void"
    log("==== rebuild World_Void from inventory")
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        # overwrite by loading a fresh duplicate shell
        tmp = "/Game/Maps/AetheriaWorlds/World_VoidShell"
        if unreal.EditorAssetLibrary.does_asset_exist(tmp):
            unreal.EditorAssetLibrary.delete_asset(tmp)
        unreal.EditorAssetLibrary.duplicate_asset(tiny, tmp)
        unreal.EditorAssetLibrary.save_asset(tmp)
        # delete old void and rename shell? simpler: load tmp, stamp, save as dest via duplicate after stamp
        load_map(tmp)
        delete_by_label_prefix(["MenuFloor_", "MenuWall_", "MenuPillar_", "MenuGothic"])
        prepare_shell((0.0, -150.0, 120.0), intensity=22.0)
        n = stamp_inventory(
            r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\inventory_Layout_MedievalDungeon.json",
            skip_substrings=["ceiling", "skysphere", "sky_sphere"],
            label_prefix="Void",
        )
        save_current(tmp)
        # copy over World_Void
        if unreal.EditorAssetLibrary.does_asset_exist(dest):
            unreal.EditorAssetLibrary.delete_asset(dest)
        unreal.EditorAssetLibrary.duplicate_asset(tmp, dest)
        unreal.EditorAssetLibrary.save_asset(dest)
        log("World_Void rebuilt n=" + str(n))
    else:
        log("World_Void missing, unexpected")

    # --- Sanctum: hide photography backdrop, move start onto the treasure floor ---
    log("==== fix World_Sanctum")
    if load_map("/Game/Maps/AetheriaWorlds/World_Sanctum"):
        hidden = 0
        for a in all_actors():
            label = ""
            try:
                label = str(a.get_actor_label())
            except Exception:
                label = a.get_name()
            low = label.lower()
            if "backdrop" in low or "zbackdrop" in low:
                a.set_actor_hidden_in_game(True)
                try:
                    for c in a.get_components_by_class(unreal.PrimitiveComponent):
                        c.set_visibility(False, True)
                        c.set_hidden_in_game(True, True)
                except Exception:
                    pass
                hidden += 1
                log("hid backdrop " + label)
        # stand among the coin piles, slightly off the origin so the camera sees the hall
        move_player_start((-250.0, -250.0, 90.0))
        save_current("/Game/Maps/AetheriaWorlds/World_Sanctum")
        log("sanctum backdrops hidden=" + str(hidden))

    # --- Bastion / Ruins: step off the buff-camp pit onto the visible floor ---
    log("==== offset Bastion/Ruins starts")
    if load_map("/Game/Maps/AetheriaWorlds/World_Bastion"):
        move_player_start((520.0, 280.0, 120.0))
        save_current("/Game/Maps/AetheriaWorlds/World_Bastion")
    if load_map("/Game/Maps/AetheriaWorlds/World_Ruins"):
        move_player_start((5100.0, -4300.0, 200.0))
        save_current("/Game/Maps/AetheriaWorlds/World_Ruins")

    # --- Menu courtyard: pull the gothic facade closer and light the floor ---
    log("==== fix AetheriaWorld courtyard")
    if load_map("/Game/Maps/AetheriaWorld"):
        count_menu = 0
        for a in all_actors():
            label = ""
            try:
                label = str(a.get_actor_label())
            except Exception:
                label = a.get_name()
            if label.startswith("Menu") or "Gothic" in label:
                count_menu += 1
                log("menu actor " + label + " at " + str(a.get_actor_location()))
                if "Gothic" in label or label.startswith("MenuGothic"):
                    a.set_actor_location(unreal.Vector(900.0, -900.0, 0.0), False, False)
                    log("moved gothic closer")
        log("menu-tagged actors " + str(count_menu) + " total actors " + str(len(all_actors())))
        # extra fill light at the hero
        pl = spawn_actor(unreal.PointLight, unreal.Vector(0, 0, 400))
        if pl:
            set_movable(getattr(pl, "light_component", None))
            try:
                pl.light_component.set_editor_property("intensity", 12000.0)
                pl.light_component.set_editor_property("attenuation_radius", 5000.0)
                pl.light_component.set_editor_property("cast_shadows", False)
            except Exception as e:
                log("menu light " + str(e))
        move_player_start((0.0, 0.0, 110.0))
        prepare_shell((0.0, 0.0, 110.0), intensity=32.0)
        save_current("/Game/Maps/AetheriaWorld")

    log("REBAKE COMPLETE")
    print("REBAKE COMPLETE")
    try:
        cmd = str(unreal.SystemLibrary.get_command_line())
        if "-unattended" in cmd.lower():
            unreal.SystemLibrary.quit_editor()
    except Exception as e:
        log("quit " + str(e))

if __name__ == "__main__":
    try:
        main()
    except Exception:
        log("FATAL\n" + traceback.format_exc())
        try:
            unreal.SystemLibrary.quit_editor()
        except Exception:
            pass
