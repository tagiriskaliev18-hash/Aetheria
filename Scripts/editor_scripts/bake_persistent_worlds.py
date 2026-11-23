import os
import json
import traceback
import unreal

LOG = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved\bake_worlds_log.txt"
INV_DIR = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Saved"

def log(msg):
    line = "[AetheriaBake] " + str(msg)
    try:
        unreal.log(line)
    except Exception:
        pass
    with open(LOG, "a", encoding="utf-8") as f:
        f.write(line + "\n")
        f.flush()

# reset log
with open(LOG, "w", encoding="utf-8") as f:
    f.write("bake start\n")

def editor_world():
    try:
        return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    except Exception:
        return unreal.EditorLevelLibrary.get_editor_world()

def load_map(path):
    log("load " + path)
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        ok = les.load_level(path)
        log("LevelEditorSubsystem.load_level -> " + str(ok))
        return True
    except Exception as e:
        log("LevelEditorSubsystem failed: " + str(e))
    try:
        unreal.EditorLoadingAndSavingUtils.load_map(path)
        log("EditorLoadingAndSavingUtils.load_map done")
        return True
    except Exception as e:
        log("load_map failed: " + str(e))
    try:
        unreal.EditorLevelLibrary.load_level(path)
        log("EditorLevelLibrary.load_level done")
        return True
    except Exception as e:
        log("EditorLevelLibrary.load_level failed: " + str(e))
    return False

def save_current():
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        ok = les.save_current_level()
        log("save_current_level -> " + str(ok))
        return True
    except Exception as e:
        log("LES save failed: " + str(e))
    try:
        unreal.EditorLevelLibrary.save_current_level()
        log("EditorLevelLibrary.save_current_level done")
        return True
    except Exception as e:
        log("save failed: " + str(e))
        return False

def save_asset(path):
    try:
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        log("saved asset " + path)
    except Exception as e:
        log("save_asset failed " + path + " " + str(e))

def duplicate_map(src, dest):
    unreal.EditorAssetLibrary.make_directory("/Game/Maps/AetheriaWorlds")
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        log("dest exists, will fix in place: " + dest)
        return True
    if not unreal.EditorAssetLibrary.does_asset_exist(src):
        log("MISSING src " + src)
        return False
    new_asset = unreal.EditorAssetLibrary.duplicate_asset(src, dest)
    if new_asset:
        unreal.EditorAssetLibrary.save_asset(dest)
        log("duplicated " + src + " -> " + dest)
        return True
    log("duplicate failed " + src + " -> " + dest)
    return False

def set_movable(comp):
    if not comp:
        return False
    try:
        comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        return True
    except Exception:
        pass
    try:
        comp.set_mobility(unreal.ComponentMobility.MOVABLE)
        return True
    except Exception:
        return False

def spawn_actor(cls, loc, rot=None):
    if rot is None:
        rot = unreal.Rotator(0, 0, 0)
    try:
        return unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    except Exception as e:
        log("spawn_actor_from_class failed " + str(e))
        try:
            als = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
            return als.spawn_actor_from_class(cls, loc, rot)
        except Exception as e2:
            log("EditorActorSubsystem spawn failed " + str(e2))
            return None

def all_actors():
    world = editor_world()
    return list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))

def ensure_lighting(preset):
    world = editor_world()
    actors = all_actors()
    has_dir = False
    has_sky = False
    has_fog = False
    has_atmo = False
    has_pp = False
    has_ps = False
    for a in actors:
        name = a.get_class().get_name()
        if isinstance(a, unreal.DirectionalLight) or "DirectionalLight" in name:
            has_dir = True
            a.set_actor_rotation(unreal.Rotator(preset["dir_pitch"], preset["dir_yaw"], 0), False)
            try:
                lc = a.get_editor_property("light_component") if hasattr(a, "get_editor_property") else None
            except Exception:
                lc = None
            try:
                lc = a.light_component
            except Exception:
                pass
            if lc:
                set_movable(lc)
                try:
                    lc.set_editor_property("intensity", preset["dir_i"])
                    lc.set_editor_property("light_color", unreal.LinearColor(*preset["dir_c"]))
                    lc.set_editor_property("atmosphere_sun_light", True)
                    lc.set_editor_property("cast_shadows", True)
                    lc.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
                except Exception as e:
                    log("dir light props " + str(e))
        if isinstance(a, unreal.SkyLight) or name.startswith("SkyLight"):
            has_sky = True
            try:
                lc = a.light_component
            except Exception:
                lc = None
            if lc:
                set_movable(lc)
                try:
                    lc.set_editor_property("real_time_capture", True)
                    lc.set_editor_property("intensity", preset["sky_i"])
                    lc.set_editor_property("light_color", unreal.LinearColor(*preset["sky_c"]))
                except Exception as e:
                    log("skylight props " + str(e))
        if isinstance(a, unreal.ExponentialHeightFog) or "ExponentialHeightFog" in name:
            has_fog = True
            try:
                fc = a.get_component()
                fc.set_editor_property("fog_density", preset["fog_d"])
                fc.set_editor_property("fog_inscattering_color", unreal.LinearColor(*preset["fog_c"]))
                fc.set_editor_property("volumetric_fog", True)
            except Exception as e:
                log("fog props " + str(e))
        if "SkyAtmosphere" in name:
            has_atmo = True
        if "PostProcessVolume" in name:
            has_pp = True
        if "PlayerStart" in name:
            has_ps = True

    if not has_dir:
        d = spawn_actor(unreal.DirectionalLight, unreal.Vector(0, 0, 800), unreal.Rotator(preset["dir_pitch"], preset["dir_yaw"], 0))
        log("spawned DirectionalLight " + str(d))
        has_dir = True
        if d:
            try:
                lc = d.light_component
                set_movable(lc)
                lc.set_editor_property("intensity", preset["dir_i"])
                lc.set_editor_property("light_color", unreal.LinearColor(*preset["dir_c"]))
                lc.set_editor_property("atmosphere_sun_light", True)
            except Exception as e:
                log("new dir " + str(e))
    if not has_sky:
        s = spawn_actor(unreal.SkyLight, unreal.Vector(0, 0, 400))
        log("spawned SkyLight " + str(s))
        if s:
            try:
                lc = s.light_component
                set_movable(lc)
                lc.set_editor_property("real_time_capture", True)
                lc.set_editor_property("intensity", preset["sky_i"])
                lc.set_editor_property("light_color", unreal.LinearColor(*preset["sky_c"]))
            except Exception as e:
                log("new sky " + str(e))
    if not has_fog:
        f = spawn_actor(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
        log("spawned fog " + str(f))
        if f:
            try:
                fc = f.get_component()
                fc.set_editor_property("fog_density", preset["fog_d"])
                fc.set_editor_property("fog_inscattering_color", unreal.LinearColor(*preset["fog_c"]))
                fc.set_editor_property("volumetric_fog", True)
            except Exception as e:
                log("new fog " + str(e))
    if not has_atmo:
        try:
            a = spawn_actor(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
            log("spawned SkyAtmosphere " + str(a))
        except Exception as e:
            log("SkyAtmosphere spawn failed " + str(e))
    if not has_pp:
        try:
            pp = spawn_actor(unreal.PostProcessVolume, unreal.Vector(0, 0, 200))
            if pp:
                pp.set_editor_property("unbound", True)
                pp.set_editor_property("blend_weight", 1.0)
                pp.set_editor_property("priority", 10.0)
                log("spawned PostProcessVolume")
        except Exception as e:
            log("PP spawn failed " + str(e))
    return has_ps

def place_player_start(loc, already):
    if already:
        # move existing
        for a in all_actors():
            if "PlayerStart" in a.get_class().get_name():
                a.set_actor_location(loc, False, False)
                log("moved PlayerStart to " + str(loc))
                return
    ps = spawn_actor(unreal.PlayerStart, loc)
    log("spawned PlayerStart " + str(ps) + " at " + str(loc))

def prepare_loaded_map(preset, spawn_loc, hide_ceilings=True):
    world = editor_world()
    ws = world.get_world_settings()
    try:
        ws.set_editor_property("force_no_precomputed_lighting", True)
        log("force_no_precomputed_lighting=True")
    except Exception as e:
        log("WS force_no_precomputed failed " + str(e))
    try:
        gm = unreal.load_class(None, "/Script/Aetheria.AetheriaGameMode")
        if gm:
            ws.set_editor_property("default_game_mode", gm)
            log("set DefaultGameMode AetheriaGameMode")
        else:
            log("could not load AetheriaGameMode class")
    except Exception as e:
        log("set game mode failed " + str(e))

    n_prim = 0
    n_moved = 0
    n_lights = 0
    n_hidden = 0
    n_ceil = 0
    for a in all_actors():
        cls = a.get_class().get_name()
        name = a.get_name()
        label = ""
        try:
            label = str(a.get_actor_label())
        except Exception:
            label = name
        low = (cls + " " + name + " " + label).lower()
        if "skysphere" in low or "sky_sphere" in low or "atmosphericfog" in low:
            try:
                a.set_actor_hidden_in_game(True)
                a.set_is_temporarily_hidden_in_editor(True)
                a.set_actor_enable_collision(False)
            except Exception:
                pass
            n_hidden += 1
            continue
        if hide_ceilings and "ceiling" in low:
            try:
                a.set_actor_hidden_in_game(True)
                a.set_actor_enable_collision(False)
            except Exception:
                pass
            n_ceil += 1
            # still convert mobility in case we unhide later
        try:
            prims = a.get_components_by_class(unreal.PrimitiveComponent)
        except Exception:
            prims = []
        for c in prims:
            n_prim += 1
            if set_movable(c):
                n_moved += 1
            try:
                if hide_ceilings and "ceiling" in low:
                    c.set_visibility(False, True)
                    c.set_hidden_in_game(True, True)
                else:
                    c.set_visibility(True, True)
                    c.set_hidden_in_game(False, True)
            except Exception:
                pass
        try:
            lights = a.get_components_by_class(unreal.LightComponent)
        except Exception:
            lights = []
        for c in lights:
            if set_movable(c):
                n_lights += 1
            try:
                c.set_visibility(True)
            except Exception:
                pass
            if "SkyLight" in cls or isinstance(c, unreal.SkyLightComponent):
                try:
                    c.set_editor_property("real_time_capture", True)
                    c.set_editor_property("intensity", preset["sky_i"])
                    c.set_editor_property("light_color", unreal.LinearColor(*preset["sky_c"]))
                except Exception:
                    pass

    has_ps = ensure_lighting(preset)
    place_player_start(unreal.Vector(*spawn_loc), has_ps)
    log("prepare done prim=%d moved=%d lights=%d skyHidden=%d ceilingsHidden=%d actors=%d" % (
        n_prim, n_moved, n_lights, n_hidden, n_ceil, len(all_actors())))

def spawn_sm(path, loc, rot, scale, label):
    mesh = unreal.EditorAssetLibrary.load_asset(path)
    if not mesh:
        log("MISSING mesh " + path)
        return None
    actor = spawn_actor(unreal.StaticMeshActor, unreal.Vector(*loc), unreal.Rotator(*rot))
    if not actor:
        log("failed spawn SM " + label)
        return None
    try:
        actor.set_actor_label(label)
    except Exception:
        pass
    try:
        actor.set_actor_scale3d(unreal.Vector(*scale))
    except Exception:
        pass
    try:
        c = actor.static_mesh_component
        set_movable(c)
        c.set_static_mesh(mesh)
        c.set_world_scale3d(unreal.Vector(*scale))
        c.set_visibility(True, True)
        c.set_hidden_in_game(False, True)
    except Exception as e:
        log("sm setup failed " + label + " " + str(e))
    return actor

def decorate_menu_courtyard():
    # Artist pieces: dungeon floors under Gideon + gothic facade in the look-dir backdrop.
    floor = "/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Floor"
    wall = "/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Dungeon_Wall"
    pillar = "/Game/MedievalDungeon/Meshes/Architecture/Dungeon/SM_Pillar"
    gothic = "/Game/Gothic_Environment/Meshes/Gothic_buildings/SM_Gothic_building_09"
    # 3x3 floors, 600 uu, grid used by the demo (centers at 150+600k, -400+600k)
    xs = [-450.0, 150.0, 750.0]
    ys = [-400.0, 200.0, 800.0]
    n = 0
    for x in xs:
        for y in ys:
            if spawn_sm(floor, (x, y, 0.0), (0, 0, 0), (1, 1, 1), "MenuFloor_%d" % n):
                n += 1
    # low walls framing a courtyard, kept outside the lobby camera near-plane
    for i, (loc, rot) in enumerate([
        ((150.0, -1000.0, 0.0), (0, 0, 90)),
        ((150.0, 1400.0, 0.0), (0, 0, 90)),
        ((-1050.0, 200.0, 0.0), (0, 0, 0)),
        ((1350.0, 200.0, 0.0), (0, 0, 0)),
    ]):
        spawn_sm(wall, loc, rot, (1, 1, 1), "MenuWall_%d" % i)
    for i, loc in enumerate([(150.0, -400.0, 0.0), (150.0, 800.0, 0.0), (-450.0, 200.0, 0.0), (750.0, 200.0, 0.0)]):
        spawn_sm(pillar, loc, (0, 0, 0), (1, 1, 1), "MenuPillar_%d" % i)
    # Backdrop in camera look direction (+X, -Y) so lobby arm 720 / yaw -45 sees the facade, not a wall in Gideon's face.
    spawn_sm(gothic, (1600.0, -1600.0, 0.0), (0, 45, 0), (1, 1, 1), "MenuGothicFacade")
    log("menu courtyard pieces spawned, floors=" + str(n))

PRESETS = {
    "bastion": dict(dir_i=38.0, sky_i=16.0, fog_d=0.010, dir_pitch=-56.0, dir_yaw=26.0,
                    dir_c=(0.58, 0.705, 0.95, 1), sky_c=(0.40, 0.48, 0.62, 1), fog_c=(0.22, 0.24, 0.30, 1)),
    "ruins": dict(dir_i=42.0, sky_i=18.0, fog_d=0.011, dir_pitch=-48.0, dir_yaw=40.0,
                  dir_c=(0.85, 0.78, 0.55, 1), sky_c=(0.45, 0.55, 0.42, 1), fog_c=(0.28, 0.32, 0.22, 1)),
    "void": dict(dir_i=22.0, sky_i=10.0, fog_d=0.014, dir_pitch=-62.0, dir_yaw=-10.0,
                 dir_c=(0.45, 0.35, 0.95, 1), sky_c=(0.22, 0.18, 0.40, 1), fog_c=(0.12, 0.08, 0.22, 1)),
    "sanctum": dict(dir_i=32.0, sky_i=12.0, fog_d=0.012, dir_pitch=-70.0, dir_yaw=15.0,
                    dir_c=(0.95, 0.62, 0.35, 1), sky_c=(0.35, 0.28, 0.32, 1), fog_c=(0.20, 0.12, 0.10, 1)),
    "menu": dict(dir_i=28.0, sky_i=12.0, fog_d=0.016, dir_pitch=-40.0, dir_yaw=-20.0,
                 dir_c=(0.70, 0.55, 0.40, 1), sky_c=(0.22, 0.20, 0.32, 1), fog_c=(0.08, 0.07, 0.12, 1)),
}

def bake_from_layout(src, dest, preset_name, spawn, hide_ceilings=True, extra=None):
    log("==== bake " + dest + " from " + src + " preset=" + preset_name)
    if not duplicate_map(src, dest):
        return False
    if not load_map(dest):
        log("FAILED to load " + dest)
        return False
    prepare_loaded_map(PRESETS[preset_name], spawn, hide_ceilings=hide_ceilings)
    if extra:
        extra()
    save_current()
    save_asset(dest)
    log("==== saved " + dest)
    return True

def spawn_from_inventory(json_path, radius=None, center=None, label_prefix="Agora"):
    if not os.path.isfile(json_path):
        log("no inventory " + json_path)
        return 0
    data = json.load(open(json_path, encoding="utf-8"))
    actors = data.get("actors") or []
    n = 0
    skipped = 0
    for a in actors:
        mesh = a.get("mesh") or ""
        if not mesh:
            skipped += 1
            continue
        loc = a.get("loc") or [0, 0, 0]
        if radius and center:
            dx, dy = loc[0] - center[0], loc[1] - center[1]
            if (dx * dx + dy * dy) ** 0.5 > radius:
                continue
        rot = a.get("rot") or [0, 0, 0]
        scale = a.get("scale") or [1, 1, 1]
        # Unreal Rotator in Python is (pitch, yaw, roll) matching inventory [pitch,yaw,roll] if that's how it was dumped.
        path = mesh.split(".")[0] if mesh.endswith(mesh.split(".")[-1]) else mesh
        # inventory stores "/Game/.../SM_X.SM_X"
        asset_path = mesh.split(".")[0]
        label = label_prefix + "_" + (a.get("label") or a.get("name") or "SM")
        if spawn_sm(asset_path, loc, rot, scale, label[:60]):
            n += 1
        if n and n % 100 == 0:
            log("spawned inventory actors " + str(n))
    log("inventory spawn done n=%d skipped_no_mesh=%d from %s" % (n, skipped, json_path))
    return n

def main():
    log("UE python bake start")
    # Prefer a previously extracted Agora cluster if present (from extract_agora.py).
    agora_json = os.path.join(INV_DIR, "inventory_agora_cluster.json")
    agora_meta_path = os.path.join(INV_DIR, "inventory_agora_cluster_meta.json")
    agora_center = (0.0, 0.0, 0.0)
    agora_ok = os.path.isfile(agora_json)
    if os.path.isfile(agora_meta_path):
        try:
            meta = json.load(open(agora_meta_path, encoding="utf-8"))
            agora_center = tuple(meta.get("center") or [0, 0, 0])
            log("agora meta center=" + str(agora_center) + " count=" + str(meta.get("count")))
        except Exception as e:
            log("agora meta read failed " + str(e))

    # Bastion: Gothic facade + Agora cluster if we have it, else Medieval dungeon (allowed fallback).
    if agora_ok:
        unreal.EditorAssetLibrary.make_directory("/Game/Maps/AetheriaWorlds")
        # Start from a duplicate of the tiny persistent map so lighting/game mode exist, then stamp artist pieces.
        src_tiny = "/Game/Maps/AetheriaWorld"
        dest = "/Game/Maps/AetheriaWorlds/World_Bastion"
        if unreal.EditorAssetLibrary.does_asset_exist(dest):
            log("World_Bastion exists — reloading to restamp")
        elif not duplicate_map(src_tiny, dest):
            log("failed to create World_Bastion shell")
        if load_map(dest):
            # clear leftover procedural / previous stamp? we don't delete AetheriaWorld leftover (lights, player start).
            bastion_spawn = (0.0, 0.0, 120.0)
            prepare_loaded_map(PRESETS["bastion"], bastion_spawn, hide_ceilings=False)
            n = spawn_from_inventory(agora_json, radius=6000.0, center=(0.0, 0.0, 0.0), label_prefix="BastionAgora")
            # Gothic landmark in the look direction, clear of the PlayerStart at origin.
            spawn_sm(
                "/Game/Gothic_Environment/Meshes/Gothic_buildings/SM_Gothic_building_09",
                (2000.0, -2000.0, 0.0),
                (0, 30, 0), (1, 1, 1), "BastionGothic"
            )
            place_player_start(unreal.Vector(*bastion_spawn), True)
            save_current(); save_asset(dest)
            log("World_Bastion agora+gothic stamped n=" + str(n))
        else:
            log("FAILED load World_Bastion shell")
    else:
        bake_from_layout(
            "/Game/Maps/AetheriaLayouts/Layout_MedievalDungeon",
            "/Game/Maps/AetheriaWorlds/World_Bastion",
            "bastion", (0.0, -150.0, 120.0), hide_ceilings=True)

    # Ruins: Agora cluster if present, else Medieval with ruins lighting.
    if agora_ok:
        dest = "/Game/Maps/AetheriaWorlds/World_Ruins"
        if not unreal.EditorAssetLibrary.does_asset_exist(dest):
            duplicate_map("/Game/Maps/AetheriaWorld", dest)
        if load_map(dest):
            # Distinct southern Agora plaza (Gold buff floor), not the north PlayerStart used by Bastion.
            ruins_center = (4520.0, -4585.0, 118.0)
            ruins_spawn = (4520.0, -4585.0, 200.0)
            prepare_loaded_map(PRESETS["ruins"], ruins_spawn, hide_ceilings=False)
            n = spawn_from_inventory(agora_json, radius=8000.0, center=ruins_center, label_prefix="RuinsAgora")
            place_player_start(unreal.Vector(*ruins_spawn), True)
            save_current(); save_asset(dest)
            log("World_Ruins agora stamped n=" + str(n))
        else:
            log("FAILED load World_Ruins shell")
    else:
        bake_from_layout(
            "/Game/Maps/AetheriaLayouts/Layout_MedievalDungeon",
            "/Game/Maps/AetheriaWorlds/World_Ruins",
            "ruins", (0.0, -150.0, 120.0), hide_ceilings=True)

    bake_from_layout(
        "/Game/Maps/AetheriaLayouts/Layout_MedievalDungeon",
        "/Game/Maps/AetheriaWorlds/World_Void",
        "void", (0.0, -150.0, 120.0), hide_ceilings=True)

    bake_from_layout(
        "/Game/Maps/AetheriaLayouts/Layout_TreasureSanctum",
        "/Game/Maps/AetheriaWorlds/World_Sanctum",
        "sanctum", (0.0, 0.0, 90.0), hide_ceilings=False)

    # Menu courtyard on the persistent menu map
    log("==== decorate AetheriaWorld menu courtyard")
    if load_map("/Game/Maps/AetheriaWorld"):
        prepare_loaded_map(PRESETS["menu"], (0.0, 0.0, 110.0), hide_ceilings=False)
        decorate_menu_courtyard()
        save_current()
        save_asset("/Game/Maps/AetheriaWorld")
        log("==== saved AetheriaWorld courtyard")
    else:
        log("FAILED to load AetheriaWorld")

    log("BAKE COMPLETE")
    print("BAKE COMPLETE")
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
