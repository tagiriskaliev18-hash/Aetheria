# Run AFTER a C++ build that includes GetCinematicBiomeMaterial:
# UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script="<this>"
# Re-skins baked world floors and walls with the cinematic PBR sets
# (calls the C++ builder so materials are built + cached as real assets).
import unreal, os, json

WORLDS = [
    ("World_Bastion", 0),
    ("World_Ruins", 1),
    ("World_Void", 2),
    ("World_Sanctum", 3),
]
FOLIAGE_KEYS = ("/Foliage/", "/Trees/", "Leaves", "Grass", "Flower", "Fern", "Bush")
LOG = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "grok_cine_swap.json")

def log(msg):
    unreal.log("[CineSwap] " + str(msg))

report = {}
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

for map_name, world_id in WORLDS:
    path = "/Game/Maps/AetheriaWorlds/" + map_name
    if not les.load_level(path):
        report[map_name] = "load failed"
        continue
    floor_mat = unreal.AetheriaMaterialLibrary.get_cinematic_biome_material(world_id, False)
    wall_mat = unreal.AetheriaMaterialLibrary.get_cinematic_biome_material(world_id, True)
    # The C++ builder creates transient packages: persist them to disk or the
    # saved maps would reference materials that don't exist in a cook.
    for mat in [floor_mat, wall_mat]:
        if mat:
            unreal.EditorAssetLibrary.save_loaded_asset(mat, only_if_is_dirty=False)
    floors = walls = 0
    for actor in els.get_all_level_actors():
        try:
            for c in actor.get_components_by_class(unreal.StaticMeshComponent):
                mesh = c.get_editor_property("static_mesh")
                if not mesh:
                    continue
                mp = str(mesh.get_path_name())
                if any(k.lower() in mp.lower() for k in FOLIAGE_KEYS):
                    continue
                # Commandlet components have no render-state bounds: derive
                # size from the mesh asset bounds times the component scale.
                base = mesh.get_bounds().box_extent
                sc = c.get_editor_property("relative_scale3d")
                ext = unreal.Vector(base.x * abs(sc.x), base.y * abs(sc.y), base.z * abs(sc.z))
                groundish = ext.z < 300 and max(ext.x, ext.y) > 250
                wallish = ext.z > 300 and ext.z >= max(ext.x, ext.y) * 0.8 and max(ext.x, ext.y) > 150
                if groundish and floor_mat:
                    c.set_material(0, floor_mat)
                    actor.modify()
                    floors += 1
                elif wallish and wall_mat:
                    c.set_material(0, wall_mat)
                    actor.modify()
                    walls += 1
        except Exception as e:
            log(f"actor error: {e}")
    les.save_current_level()
    report[map_name] = {"floors": floors, "walls": walls}
    log(f"{map_name}: floors={floors} walls={walls} saved")

json.dump(report, open(LOG, "w"), indent=1)
unreal.log_warning(f"[CineSwap] done -> {LOG}")
