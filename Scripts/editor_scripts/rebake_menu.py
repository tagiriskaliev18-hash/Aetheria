import traceback, unreal

LOG = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\rebake_menu_log.txt"

def log(msg):
    line = "[MenuBake] " + str(msg)
    try:
        unreal.log(line)
    except Exception:
        pass
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")

with open(LOG, "w", encoding="utf-8") as f:
    f.write("start\n")

def les():
    return unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)

def load_map(path):
    log("load " + path)
    ok = les().load_level(path)
    log("load -> " + str(ok))
    return ok

def save_current(path=None):
    ok = les().save_current_level()
    log("save_current -> " + str(ok))
    if path:
        try:
            unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        except Exception as e:
            log("save_asset " + str(e))

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
        log("sm " + label + " " + str(e))
    return actor

def prepare_and_courtyard():
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
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
    d = spawn_actor(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(-40, -20, 0))
    if d and getattr(d, "light_component", None):
        set_movable(d.light_component)
        try:
            d.light_component.set_editor_property("intensity", 32.0)
            d.light_component.set_editor_property("atmosphere_sun_light", True)
            d.light_component.set_editor_property("cast_shadows", True)
        except Exception as e:
            log("dir " + str(e))
    s = spawn_actor(unreal.SkyLight, unreal.Vector(0, 0, 400))
    if s and getattr(s, "light_component", None):
        set_movable(s.light_component)
        try:
            s.light_component.set_editor_property("real_time_capture", True)
            s.light_component.set_editor_property("intensity", 14.0)
        except Exception as e:
            log("sky " + str(e))
    spawn_actor(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
    try:
        spawn_actor(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
    except Exception as e:
        log("atmo " + str(e))
    pp = spawn_actor(unreal.PostProcessVolume, unreal.Vector(0, 0, 200))
    if pp:
        try:
            pp.set_editor_property("unbound", True)
            pp.set_editor_property("blend_weight", 1.0)
        except Exception:
            pass
    pl = spawn_actor(unreal.PointLight, unreal.Vector(0, 0, 400))
    if pl and getattr(pl, "light_component", None):
        set_movable(pl.light_component)
        try:
            pl.light_component.set_editor_property("intensity", 15000.0)
            pl.light_component.set_editor_property("attenuation_radius", 6000.0)
            pl.light_component.set_editor_property("cast_shadows", False)
        except Exception as e:
            log("point " + str(e))
    spawn_actor(unreal.PlayerStart, unreal.Vector(0, 0, 110))
    floor = "/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Floor"
    wall = "/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Dungeon_Wall"
    pillar = "/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Pillar"
    gothic = "/Game/Gothic_Environment/Meshes/Gothic_buildings/SM_Gothic_building_09"
    n = 0
    for x in (-450.0, 150.0, 750.0):
        for y in (-400.0, 200.0, 800.0):
            if spawn_sm(floor, (x, y, 0.0), (0, 0, 0), (1, 1, 1), "MenuFloor_%d" % n):
                n += 1
    for i, (loc, rot) in enumerate([
        ((150.0, -1000.0, 0.0), (0, 0, 90)),
        ((150.0, 1400.0, 0.0), (0, 0, 90)),
        ((-1050.0, 200.0, 0.0), (0, 0, 0)),
        ((1350.0, 200.0, 0.0), (0, 0, 0)),
    ]):
        spawn_sm(wall, loc, rot, (1, 1, 1), "MenuWall_%d" % i)
    for i, loc in enumerate([(150.0, -400.0, 0.0), (150.0, 800.0, 0.0), (-450.0, 200.0, 0.0), (750.0, 200.0, 0.0)]):
        spawn_sm(pillar, loc, (0, 0, 0), (1, 1, 1), "MenuPillar_%d" % i)
    spawn_sm(gothic, (900.0, -900.0, 0.0), (0, 45, 0), (1, 1, 1), "MenuGothicFacade")
    log("courtyard floors=" + str(n))

def main():
    dest = "/Game/Maps/AetheriaWorlds/MenuCourtyard"
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        unreal.EditorAssetLibrary.delete_asset(dest)
        log("deleted old " + dest)
    # New blank level — do not inherit AetheriaWorld world-settings.
    try:
        les().new_level(dest)
        log("new_level " + dest)
    except Exception as e:
        log("LES new_level failed " + str(e))
        try:
            unreal.EditorLevelLibrary.new_level(dest)
            log("EditorLevelLibrary.new_level")
        except Exception as e2:
            log("new_level failed " + str(e2))
            return
    prepare_and_courtyard()
    save_current(dest)

    # Swap over AetheriaWorld so GameDefaultMap stays the same.
    old = "/Game/Maps/AetheriaWorld"
    # Load dest (already current), duplicate onto a temp, then replace old.
    tmp = "/Game/Maps/AetheriaWorlds/MenuCourtyard_Swap"
    if unreal.EditorAssetLibrary.does_asset_exist(tmp):
        unreal.EditorAssetLibrary.delete_asset(tmp)
    # Save dest first, then load a different map so we can replace AetheriaWorld.
    if not load_map("/Game/Maps/AetheriaWorlds/World_Bastion"):
        log("could not leave menu map to swap")
    if unreal.EditorAssetLibrary.does_asset_exist(old):
        try:
            unreal.EditorAssetLibrary.delete_asset(old)
            log("deleted old AetheriaWorld")
        except Exception as e:
            log("delete AetheriaWorld failed " + str(e))
    if unreal.EditorAssetLibrary.duplicate_asset(dest, old):
        unreal.EditorAssetLibrary.save_asset(old)
        log("copied MenuCourtyard -> AetheriaWorld")
    else:
        log("duplicate to AetheriaWorld FAILED")

    # Sanctum: stand on the hoard, off the dark mass that filled the right side.
    if load_map("/Game/Maps/AetheriaWorlds/World_Sanctum"):
        for a in unreal.GameplayStatics.get_all_actors_of_class(
                unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), unreal.Actor):
            if "PlayerStart" in a.get_class().get_name():
                a.set_actor_location(unreal.Vector(-420.0, -80.0, 90.0), False, False)
                log("sanctum PlayerStart moved")
        save_current("/Game/Maps/AetheriaWorlds/World_Sanctum")

    log("MENU BAKE COMPLETE")
    print("MENU BAKE COMPLETE")
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
