# -*- coding: utf-8 -*-
"""
Aetheria UE 5.8.4 port editor automation.
Modes: quarantine | import | anims | map | all
Run:
  UnrealEditor-Cmd.exe Aetheria.uproject -unattended -nopause -nosplash
    -ExecutePythonScript="C:/Users/tagir/OneDrive/Desktop/Aetheria/Scripts/ue_port_editor.py all"
"""
import os
import sys
import unreal

GODOT_MODELS = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\GodotAetheria\assets\models"
CHARACTERS = [
    {
        "glb": "hero_archmage.glb",
        "folder": "/Game/Characters/Archmage",
        "mesh_name": "SKM_Archmage",
        "skel_name": "SK_Archmage",
        "aliases": ("archmage", "hero_archmage"),
    },
    {
        "glb": "mob_swarm_reaper.glb",
        "folder": "/Game/Characters/SwarmReaper",
        "mesh_name": "SKM_SwarmReaper",
        "skel_name": "SK_SwarmReaper",
        "aliases": ("reaper", "swarm", "mob_swarm_reaper"),
    },
    {
        "glb": "mob_ranged_lich.glb",
        "folder": "/Game/Characters/RangedLich",
        "mesh_name": "SKM_RangedLich",
        "skel_name": "SK_RangedLich",
        "aliases": ("lich", "ranged", "mob_ranged_lich"),
    },
    {
        "glb": "mob_bruiser_juggernaut.glb",
        "folder": "/Game/Characters/BruiserJuggernaut",
        "mesh_name": "SKM_BruiserJuggernaut",
        "skel_name": "SK_BruiserJuggernaut",
        "aliases": ("juggernaut", "bruiser", "mob_bruiser_juggernaut"),
    },
    {
        "glb": "mob_boss_archon.glb",
        "folder": "/Game/Characters/BossArchon",
        "mesh_name": "SKM_BossArchon",
        "skel_name": "SK_BossArchon",
        "aliases": ("archon", "boss", "mob_boss_archon"),
    },
]

PROPS = [
    ("prop_arch.glb", "/Game/Props", "SM_PropArch"),
    ("prop_barrel_intact.glb", "/Game/Props", "SM_PropBarrelIntact"),
    ("prop_boulder.glb", "/Game/Props", "SM_PropBoulder"),
    ("prop_brazier.glb", "/Game/Props", "SM_PropBrazier"),
    ("prop_chest.glb", "/Game/Props", "SM_PropChest"),
    ("prop_iron_gate.glb", "/Game/Props", "SM_PropIronGate"),
    ("prop_pillar.glb", "/Game/Props", "SM_PropPillar"),
    ("prop_shrine_altar.glb", "/Game/Props", "SM_PropShrineAltar"),
    ("prop_statue.glb", "/Game/Props", "SM_PropStatue"),
    ("prop_urn_intact.glb", "/Game/Props", "SM_PropUrnIntact"),
    ("prop_urn_shattered.glb", "/Game/Props", "SM_PropUrnShattered"),
]

LOCOMOTION = ("Idle", "Run_Fwd", "Run_Back", "Run_StrafeL", "Run_StrafeR")
ACTIONS = ("Cast_Primary", "Cast_Heavy", "Dash", "Attack_Melee", "Attack_Slam", "Hit_React", "Death")

# Leftover prefixes/names not referenced by Source (verified 2026-11-21).
QUARANTINE_MESH_PREFIXES = (
    "SKM_ArenaHero",
    "SKM_CyberFrog",
    "SKM_Hero_Dragoon",
    "SKM_Hero_Striker",
    "SKM_Hero_Tactician",
    "SKM_Hero_Vanguard",
    "SKM_Hero_Vincit",
    "SM_Hero_Sorcerer",
    "SM_FP_",
    "SM_WP_",
    "SM_BioFrog",
    "SM_LilyPadPlatform",
    "SM_Overwatch",
    "SM_Reinhardt",
    "SM_Orbital_",
    "SM_Stadium_",
    "SM_JumpPad",
    "SM_HealthPack_",
    "SM_ChicaneBarrier",
    "SM_Arena_CourtFloor",
    "SM_ArenaFloor",
    "SM_Elite_Gargoyle",
    "SM_Colossus",
    "SM_Vista_CityTower",
    "SM_SkyBridge_Architectural",
    "SM_Sector_CornerPylon",
    "SM_TitanCoverBlock",
    "SM_CoreCrystal",
    "SM_Earth_Spike",
    "SM_RubyObelisk",
    "M_Frog_",
    "M_SOL_",
    "M_VNT_",
    "M_Arch_",
    "M_Manga_",
    "M_Holo_",
    "M_Overwatch_",
    "M_Arena_Ruby",
    "M_Earth_",
    "M_Weapon_",
    "M_Health_Emerald",
)

QUARANTINE_TEX_PREFIXES = (
    "T_Arena_",
    "T_ArenaFloor_",
    "T_Holo_",
    "T_Barrier_VoronoiCrack",
)

# Exact Source references that must never be moved.
SOURCE_PROTECTED = {
    "SM_ArchmageRobed",
    "SM_ArchonBoss",
    "SM_VoidFiend",
    "SM_StoneColossus",
    "SM_CitadelFortress",
    "SM_CliffBoulder",
    "SM_CrystalShard",
    "SM_DeadTree",
    "SM_Familiar_Astrolabe",
    "SM_Familiar_OpenGrimoire",
    "SM_Familiar_SpectralSkull",
    "SM_GnarledStaff",
    "SM_GothicLantern",
    "SM_GothicPillar",
    "SM_GothicStatue",
    "SM_IronBrazier",
    "SM_LanternChandelier",
    "SM_LargeIronGate",
    "SM_LionRelief",
    "SM_OrnateMace",
    "SM_RuinedArch",
    "SM_RunicAltar",
    "SM_Staff_Arcane01",
    "SM_Staff_Arcane02",
    "SM_StoneFirePit",
    "SM_StrategyMoveArrow",
    "SM_TreasureChest",
    "SM_Hero_00",
    "SM_Hero_Archmage",
    "SM_Mob_Ranged_00",
    "SM_Mob_Swarm_00",
    "SM_Mob_Swarm_Static",
    "SM_SiphonAltar",
    "SM_Spawn_BastionGate",
    "SM_Head_AbyssHood",
    "SM_Head_BoneCrown",
    "SM_Head_ChronoHalo",
    "SM_Head_HopeMask",
    "SM_Head_HorrorHorns",
}


def log(msg):
    unreal.log("[AetheriaPort] " + str(msg))


def log_warn(msg):
    unreal.log_warning("[AetheriaPort] " + str(msg))


def ensure_dir(path):
    if not unreal.EditorAssetLibrary.does_directory_exist(path):
        unreal.EditorAssetLibrary.make_directory(path)


def asset_name(path):
    return path.split(".")[-1].split("/")[-1]


def list_assets(folder):
    if not unreal.EditorAssetLibrary.does_directory_exist(folder):
        return []
    return list(unreal.EditorAssetLibrary.list_assets(folder, True, False) or [])


def is_protected(name):
    if name in SOURCE_PROTECTED:
        return True
    for p in SOURCE_PROTECTED:
        if name == p or name.startswith(p + "_") or name.startswith(p):
            # SM_Colossus should not protect anything named StoneColossus vice versa
            if p == "SM_Colossus" and name.startswith("SM_Stone"):
                return True
            if name.startswith(p):
                # Protect exact and obvious variants only for character hero/mob ids
                if p in ("SM_Hero_00", "SM_Hero_Archmage", "SM_Mob_Ranged_00", "SM_Mob_Swarm_00", "SM_Mob_Swarm_Static"):
                    return name.startswith(p)
                if p == name:
                    return True
                if name.startswith(p + "_") or name.startswith(p):
                    # Gothic / used props and their LODs
                    if p in SOURCE_PROTECTED and (
                        name == p or name.startswith(p + "_") or name.startswith(p)
                    ) and not name.startswith("SM_Colossus"):
                        if name.startswith(p):
                            # Don't let SM_Hero_Archmage protect SM_Hero_Sorcerer
                            if p.startswith("SM_Hero_") and name.startswith("SM_Hero_Sorcerer"):
                                return False
                            if p == "SM_Hero_00" or p == "SM_Hero_Archmage":
                                return name.startswith(p)
                            return True
    return name in SOURCE_PROTECTED


def should_quarantine(path, prefixes):
    name = asset_name(path)
    if is_protected(name):
        return False
    # Special case: SM_Colossus is leftover; SM_StoneColossus is protected (may not exist)
    if name.startswith("SM_StoneColossus"):
        return False
    for prefix in prefixes:
        if name.startswith(prefix):
            return True
    return False


def fixup_redirectors(folders):
    redirectors = []
    for folder in folders:
        for path in list_assets(folder):
            data = unreal.EditorAssetLibrary.find_asset_data(path)
            if data and data.is_redirector():
                asset = unreal.EditorAssetLibrary.load_asset(path)
                if asset:
                    redirectors.append(asset)
    if not redirectors:
        log("No redirectors to fix up.")
        return
    log("Fixing %d redirectors..." % len(redirectors))
    try:
        unreal.AssetToolsHelpers.get_asset_tools().fix_up_referencers(redirectors)
    except Exception as exc:
        log_warn("fix_up_referencers failed: %s" % exc)
        try:
            unreal.EditorAssetLibrary.consolidate_assets(redirectors[0], redirectors[1:])
        except Exception as exc2:
            log_warn("consolidate fallback failed: %s" % exc2)


def do_quarantine():
    ensure_dir("/Game/_Quarantine")
    ensure_dir("/Game/_Quarantine/Meshes")
    ensure_dir("/Game/_Quarantine/Textures")
    moved = 0
    skipped = 0
    for path in list_assets("/Game/Meshes"):
        if should_quarantine(path, QUARANTINE_MESH_PREFIXES):
            name = asset_name(path)
            dest = "/Game/_Quarantine/Meshes/" + name
            ok = unreal.EditorAssetLibrary.rename_asset(path, dest)
            log(("MOVED " if ok else "FAILED ") + path + " -> " + dest)
            moved += int(bool(ok))
            skipped += int(not ok)
        elif is_protected(asset_name(path)):
            pass
    for path in list_assets("/Game/Textures"):
        if should_quarantine(path, QUARANTINE_TEX_PREFIXES):
            name = asset_name(path)
            dest = "/Game/_Quarantine/Textures/" + name
            ok = unreal.EditorAssetLibrary.rename_asset(path, dest)
            log(("MOVED " if ok else "FAILED ") + path + " -> " + dest)
            moved += int(bool(ok))
            skipped += int(not ok)
    unreal.EditorAssetLibrary.save_directory("/Game/_Quarantine", True, True)
    fixup_redirectors(["/Game/Meshes", "/Game/Textures", "/Game/_Quarantine"])
    log("Quarantine complete. moved=%d failed=%d" % (moved, skipped))


def _make_pipeline(skeletal):
    pipeline = unreal.InterchangeGenericAssetsPipeline()
    try:
        pipeline.asset_name = ""
    except Exception:
        pass
    try:
        pipeline.use_source_name_for_asset = True
    except Exception:
        pass
    # Avoid extra type subfolders so we can rename predictably.
    try:
        pipeline.use_asset_type_folder = False
    except Exception:
        pass
    try:
        meshes = pipeline.common_meshes_properties
        force = unreal.InterchangeForceMeshType
        meshes.force_all_mesh_as_type = force.IFMT_SKELETAL if skeletal else force.IFMT_STATIC
    except Exception as exc:
        log_warn("force mesh type: %s" % exc)
    try:
        if skeletal:
            skel_props = pipeline.common_skeletal_meshes_and_animations_properties
            skel_props.import_only_animation = False
            skel_props.import_meshes = True
        anim = pipeline.animation_pipeline
        anim.import_animations = bool(skeletal)
    except Exception as exc:
        log_warn("anim pipeline: %s" % exc)
    try:
        mesh_pipe = pipeline.mesh_pipeline
        if not skeletal:
            # Nanite + simple collision + LODs on static props
            try:
                mesh_pipe.lods_to_generate = 3
            except Exception:
                pass
    except Exception:
        pass
    try:
        pipeline.material_pipeline.import_materials = True
        pipeline.material_pipeline.import_textures = True
    except Exception:
        pass
    return pipeline


def import_glb(src, dest_folder, dest_name, skeletal):
    ensure_dir(dest_folder)
    if not os.path.isfile(src):
        log_warn("Missing source: " + src)
        return False
    log("Importing %s -> %s/%s (skeletal=%s)" % (os.path.basename(src), dest_folder, dest_name, skeletal))
    # AssetImportTask + Interchange default stack (do not pass a broken pipeline override)
    task = unreal.AssetImportTask()
    task.filename = src
    task.destination_path = dest_folder
    task.destination_name = dest_name
    task.replace_existing = True
    task.automated = True
    task.save = True
    try:
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        tools.import_asset_tasks([task])
        paths = list(task.imported_object_paths) if task.imported_object_paths else []
        log("AssetImportTask imported: %s" % paths)
        if paths:
            return True
    except Exception as exc:
        log_warn("AssetImportTask failed: %s" % exc)
    # Fallback: InterchangeManager with default pipelines, synchronous import, no override
    try:
        mgr = unreal.InterchangeManager.get_interchange_manager_scripted()
        source = unreal.InterchangeManager.create_source_data(src)
        params = unreal.ImportAssetParameters()
        params.is_automated = True
        params.replace_existing = True
        params.destination_name = dest_name
        ok = mgr.import_asset(dest_folder, source, params)
        log("Interchange import_asset result: %s" % ok)
        return bool(ok)
    except Exception as exc:
        log_warn("Interchange fallback failed: %s" % exc)
        return False

def find_assets_of_class(folder, uclass):
    out = []
    for path in list_assets(folder):
        data = unreal.EditorAssetLibrary.find_asset_data(path)
        if not data:
            continue
        try:
            if data.asset_class_path.asset_name == uclass or str(data.asset_class) == uclass:
                out.append(path)
                continue
        except Exception:
            pass
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset and asset.get_class() and uclass in str(asset.get_class().get_name()):
            out.append(path)
    return out


def rename_if_needed(path, dest_folder, new_name):
    dest = dest_folder.rstrip("/") + "/" + new_name
    if path.split(".")[-1] == new_name or asset_name(path) == new_name:
        return path if path.startswith(dest_folder) else path
    # If dest exists and is the same asset, skip
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        return dest
    ok = unreal.EditorAssetLibrary.rename_asset(path, dest)
    log(("Renamed " if ok else "Rename failed ") + path + " -> " + dest)
    return dest if ok else path


def finalize_character(spec):
    folder = spec["folder"]
    skel_meshes = []
    for path in list_assets(folder):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            continue
        cname = asset.get_class().get_name()
        if cname in ("SkeletalMesh", "SkeletalMeshEditorData"):
            if cname == "SkeletalMesh":
                skel_meshes.append(path)
    if not skel_meshes:
        log_warn("No skeletal mesh imported in " + folder)
        return
    # Prefer names matching aliases
    chosen = skel_meshes[0]
    for path in skel_meshes:
        n = asset_name(path).lower()
        if any(a in n for a in spec["aliases"]) or "mesh" in n:
            chosen = path
            break
    rename_if_needed(chosen, folder, spec["mesh_name"])
    # Skeleton
    for path in list_assets(folder):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset and asset.get_class().get_name() == "Skeleton":
            rename_if_needed(path, folder, spec["skel_name"])
            break
    unreal.EditorAssetLibrary.save_directory(folder, True, True)


def enable_nanite_and_collision(folder):
    for path in list_assets(folder):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if not asset:
            continue
        if asset.get_class().get_name() != "StaticMesh":
            continue
        try:
            mesh = asset
            # Nanite
            try:
                settings = mesh.get_editor_property("nanite_settings")
                settings.enabled = True
                mesh.set_editor_property("nanite_settings", settings)
            except Exception as exc:
                log_warn("Nanite set failed on %s: %s" % (path, exc))
            # Simple collision (simple box + complex as simple off)
            try:
                body = mesh.get_editor_property("body_setup")
                if body:
                    body.set_editor_property(
                        "collision_trace_flag",
                        unreal.CollisionTraceFlag.CTF_USE_DEFAULT,
                    )
            except Exception:
                pass
            try:
                unreal.EditorStaticMeshLibrary.add_simple_collisions(mesh, unreal.ScriptCollisionShapeType.BOX)
            except Exception as exc:
                try:
                    unreal.EditorStaticMeshLibrary.remove_collisions(mesh)
                    unreal.EditorStaticMeshLibrary.add_simple_collisions(
                        mesh, unreal.ScriptCollisionShapeType.NDOP26
                    )
                except Exception as exc2:
                    log_warn("Collision on %s: %s / %s" % (path, exc, exc2))
            try:
                # Generate light / LOD if API exists
                if hasattr(unreal.EditorStaticMeshLibrary, "set_generate_lightmap_uv"):
                    unreal.EditorStaticMeshLibrary.set_generate_lightmap_uv(mesh, True)
            except Exception:
                pass
            try:
                # Reduce LODs for 4060 if the mesh is huge
                if hasattr(unreal.EditorStaticMeshLibrary, "regenerate_lod"):
                    unreal.EditorStaticMeshLibrary.regenerate_lod(mesh, 4, True)
            except Exception as exc:
                log_warn("LOD gen %s: %s" % (path, exc))
            unreal.EditorAssetLibrary.save_asset(path, True)
            log("Configured Nanite/collision/LOD: " + path)
        except Exception as exc:
            log_warn("Static mesh finalize failed %s: %s" % (path, exc))


def finalize_prop(folder, dest_name):
    statics = []
    for path in list_assets(folder):
        asset = unreal.EditorAssetLibrary.load_asset(path)
        if asset and asset.get_class().get_name() == "StaticMesh":
            statics.append(path)
    if not statics:
        log_warn("No static mesh in %s for %s" % (folder, dest_name))
        return
    # If multiple, keep the first/largest-looking name; rename primary
    # When a prop has several meshes, leave them but also rename the first.
    rename_if_needed(statics[0], folder, dest_name)
    enable_nanite_and_collision(folder)


def do_import():
    for spec in CHARACTERS:
        src = os.path.join(GODOT_MODELS, spec["glb"])
        import_glb(src, spec["folder"], spec["mesh_name"], True)
        finalize_character(spec)
    for glb, folder, name in PROPS:
        src = os.path.join(GODOT_MODELS, glb)
        # Import each prop into its own subfolder to avoid name clashes, then hoist primary mesh
        sub = folder + "/" + name
        import_glb(src, sub, name, False)
        finalize_prop(sub, name)
        # Also try to copy/rename primary into /Game/Props/SM_*
        src_mesh = sub + "/" + name
        dest_mesh = folder + "/" + name
        if unreal.EditorAssetLibrary.does_asset_exist(src_mesh) and not unreal.EditorAssetLibrary.does_asset_exist(dest_mesh):
            # duplicate pointer via rename? Keep subfolder assets, also duplicate asset path by leaving a same-name in parent
            try:
                unreal.EditorAssetLibrary.duplicate_asset(src_mesh, dest_mesh)
            except Exception as exc:
                log_warn("duplicate prop %s: %s" % (name, exc))
    unreal.EditorAssetLibrary.save_directory("/Game/Characters", True, True)
    unreal.EditorAssetLibrary.save_directory("/Game/Props", True, True)
    log("Import pass complete.")


def _norm(n):
    return n.replace(" ", "").replace("_", "").replace("-", "").lower()


def find_anim(folder, clip_name):
    needle = _norm(clip_name)
    for path in list_assets(folder):
        data = unreal.EditorAssetLibrary.find_asset_data(path)
        class_name = ""
        try:
            class_name = str(data.asset_class_path.asset_name)
        except Exception:
            try:
                class_name = str(data.asset_class)
            except Exception:
                class_name = ""
        if "AnimSequence" not in class_name:
            continue
        if needle not in _norm(asset_name(path)):
            continue
        try:
            asset = unreal.EditorAssetLibrary.load_asset(path)
        except Exception as exc:
            log_warn("load anim %s failed: %s" % (path, exc))
            continue
        if asset:
            return asset, path
    return None, None


def create_montage(folder, name, sequence):
    dest = folder.rstrip("/") + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        return unreal.EditorAssetLibrary.load_asset(dest)
    try:
        factory = unreal.AnimMontageFactory()
        try:
            factory.target_skeleton = sequence.get_editor_property("skeleton")
        except Exception:
            pass
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        montage = tools.create_asset(name, folder, unreal.AnimMontage, factory)
        if montage is None:
            log_warn("Montage factory returned None for " + dest)
            return None
        # Add a default slot track from the sequence if API allows
        try:
            unreal.EditorAssetLibrary.save_asset(dest, False)
        except Exception:
            pass
        log("Created montage asset " + dest + " (sequence " + str(sequence.get_name()) + ")")
        return montage
    except Exception as exc:
        log_warn("create_montage %s: %s" % (name, exc))
        return None


def create_blendspace(folder, skeleton, sequences):
    name = "BS_Locomotion"
    dest = folder.rstrip("/") + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        return unreal.EditorAssetLibrary.load_asset(dest)
    try:
        factory = unreal.BlendSpaceFactoryNew()
        try:
            factory.target_skeleton = skeleton
        except Exception:
            pass
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        bs = tools.create_asset(name, folder, unreal.BlendSpace, factory)
        if not bs:
            log_warn("BlendSpace factory returned None")
            return None
        # Axes: X = Strafe (-1..1), Y = Forward (-1..1)
        try:
            px = unreal.BlendParameter()
            px.display_name = "Strafe"
            px.min = -1.0
            px.max = 1.0
            px.grid_num = 2
            py = unreal.BlendParameter()
            py.display_name = "Forward"
            py.min = -1.0
            py.max = 1.0
            py.grid_num = 2
            pz = unreal.BlendParameter()
            pz.display_name = "Unused"
            pz.min = 0.0
            pz.max = 0.0
            pz.grid_num = 1
            bs.set_editor_property("blend_parameters", [px, py, pz])
        except Exception as exc:
            log_warn("blend params: %s" % exc)
        samples = {
            "Idle": (0.0, 0.0),
            "Run_Fwd": (0.0, 1.0),
            "Run_Back": (0.0, -1.0),
            "Run_StrafeL": (-1.0, 0.0),
            "Run_StrafeR": (1.0, 0.0),
        }
        for clip, (x, y) in samples.items():
            seq = sequences.get(clip)
            if not seq:
                continue
            try:
                if hasattr(bs, "add_sample"):
                    bs.add_sample(seq, unreal.Vector(x, y, 0.0))
                else:
                    log_warn("BlendSpace.add_sample missing")
            except Exception as exc:
                log_warn("add_sample %s: %s" % (clip, exc))
        unreal.EditorAssetLibrary.save_asset(dest, True)
        log("Created blendspace " + dest)
        return bs
    except Exception as exc:
        log_warn("create_blendspace failed: %s" % exc)
        return None


def create_anim_bp(folder, skeleton, name):
    dest = folder.rstrip("/") + "/" + name
    if unreal.EditorAssetLibrary.does_asset_exist(dest):
        return unreal.EditorAssetLibrary.load_asset(dest)
    try:
        factory = unreal.AnimBlueprintFactory()
        factory.target_skeleton = skeleton
        try:
            factory.parent_class = unreal.AetheriaAnimInstance
        except Exception:
            try:
                factory.parent_class = unreal.load_class(None, "/Script/Aetheria.AetheriaAnimInstance")
            except Exception as exc:
                log_warn("AnimInstance parent class not loaded yet (%s); using UAnimInstance" % exc)
                factory.parent_class = unreal.AnimInstance
        tools = unreal.AssetToolsHelpers.get_asset_tools()
        abp = tools.create_asset(name, folder, unreal.AnimBlueprint, factory)
        if abp:
            unreal.EditorAssetLibrary.save_asset(dest, True)
            log("Created AnimBP " + dest)
        return abp
    except Exception as exc:
        log_warn("create_anim_bp failed: %s" % exc)
        return None


def repair_skeleton_refs():
    """Restore skeletons to their original Interchange paths so anims resolve."""
    for spec in CHARACTERS:
        folder = spec["folder"]
        stem = os.path.splitext(spec["glb"])[0]
        original = "%s/%s/SkeletalMeshes/%s_Skeleton" % (folder, stem, spec["mesh_name"])
        renamed = folder + "/" + spec["skel_name"]
        if unreal.EditorAssetLibrary.does_asset_exist(renamed) and not unreal.EditorAssetLibrary.does_asset_exist(original):
            ok = unreal.EditorAssetLibrary.rename_asset(renamed, original)
            log(("Restored skeleton " if ok else "Failed restoring skeleton ") + renamed + " -> " + original)
        elif unreal.EditorAssetLibrary.does_asset_exist(original):
            log("Skeleton already at " + original)

    for spec in CHARACTERS:
        folder = spec["folder"]
        skel = None
        skel_path = folder + "/" + spec["skel_name"]
        if unreal.EditorAssetLibrary.does_asset_exist(skel_path):
            try:
                skel = unreal.EditorAssetLibrary.load_asset(skel_path)
            except Exception as exc:
                log_warn("load skeleton %s: %s" % (skel_path, exc))
        if not skel:
            for path in list_assets(folder):
                data = unreal.EditorAssetLibrary.find_asset_data(path)
                try:
                    cname = str(data.asset_class_path.asset_name)
                except Exception:
                    cname = ""
                if cname == "Skeleton":
                    try:
                        skel = unreal.EditorAssetLibrary.load_asset(path)
                        break
                    except Exception:
                        pass
        if not skel:
            log_warn("No skeleton in " + folder)
            continue
        fixed = 0
        for path in list_assets(folder):
            data = unreal.EditorAssetLibrary.find_asset_data(path)
            try:
                cname = str(data.asset_class_path.asset_name)
            except Exception:
                cname = ""
            if cname not in ("AnimSequence", "SkeletalMesh", "PhysicsAsset"):
                continue
            try:
                asset = unreal.EditorAssetLibrary.load_asset(path)
            except Exception as exc:
                log_warn("repair load %s: %s" % (path, exc))
                continue
            if not asset:
                continue
            try:
                if cname in ("AnimSequence", "SkeletalMesh"):
                    asset.set_editor_property("skeleton", skel)
                    unreal.EditorAssetLibrary.save_asset(path, True)
                    fixed += 1
            except Exception as exc:
                log_warn("set skeleton %s: %s" % (path, exc))
        log("Repaired skeleton refs in %s: %d" % (folder, fixed))


def do_anims():
    repair_skeleton_refs()
    for spec in CHARACTERS:
        folder = spec["folder"]
        sequences = {}
        skeleton = None
        for clip in LOCOMOTION + ACTIONS:
            asset, path = find_anim(folder, clip)
            if asset:
                sequences[clip] = asset
                # Normalize clip names into /Anims
                ensure_dir(folder + "/Anims")
                rename_if_needed(path, folder + "/Anims", clip)
                if skeleton is None:
                    try:
                        skeleton = asset.get_editor_property("skeleton")
                    except Exception:
                        pass
            else:
                log_warn("Missing clip %s in %s" % (clip, folder))
        if skeleton is None:
            for path in list_assets(folder):
                asset = unreal.EditorAssetLibrary.load_asset(path)
                if asset and asset.get_class().get_name() == "Skeleton":
                    skeleton = asset
                    break
        ensure_dir(folder + "/Montages")
        for clip in ACTIONS:
            seq = sequences.get(clip)
            if seq:
                create_montage(folder + "/Montages", "AM_" + clip, seq)
        if skeleton:
            create_blendspace(folder, skeleton, sequences)
            create_anim_bp(folder, skeleton, "ABP_" + spec["mesh_name"][4:] if spec["mesh_name"].startswith("SKM_") else "ABP_Character")
        unreal.EditorAssetLibrary.save_directory(folder, True, True)
    log("Anim asset pass complete.")


def _set(obj, name, value):
    try:
        obj.set_editor_property(name, value)
        return True
    except Exception:
        try:
            setattr(obj, name, value)
            return True
        except Exception as exc:
            log_warn("set %s.%s failed: %s" % (obj, name, exc))
            return False


def do_map():
    # Hoist any primary props that did not land in /Game/Props
    for src, dest in (
        ("/Game/Props/SM_PropBoulder/prop_boulder/StaticMeshes/SM_PropBoulder", "/Game/Props/SM_PropBoulder"),
        ("/Game/Props/SM_PropStatue/prop_statue/StaticMeshes/SM_PropStatue", "/Game/Props/SM_PropStatue"),
    ):
        if unreal.EditorAssetLibrary.does_asset_exist(src) and not unreal.EditorAssetLibrary.does_asset_exist(dest):
            try:
                unreal.EditorAssetLibrary.duplicate_asset(src, dest)
                log("Hoisted " + src + " -> " + dest)
            except Exception as exc:
                log_warn("hoist %s: %s" % (dest, exc))

    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    eas = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    map_path = "/Game/Maps/AetheriaWorld"
    ensure_dir("/Game/Maps")
    if unreal.EditorAssetLibrary.does_asset_exist(map_path):
        les.load_level(map_path)
        log("Loaded existing map " + map_path)
    else:
        les.new_level(map_path)
        log("Created map " + map_path)

    def find_or_spawn(cls, label):
        for actor in eas.get_all_level_actors():
            if actor.get_class() and cls in str(actor.get_class().get_name()):
                return actor
            if actor.get_actor_label() == label:
                return actor
        actor = eas.spawn_actor_from_class(getattr(unreal, cls), unreal.Vector(0, 0, 0))
        if actor:
            actor.set_actor_label(label)
        return actor

    # PlayerStart in the sanctuary ring
    ps = None
    for actor in eas.get_all_level_actors():
        if "PlayerStart" in str(actor.get_class().get_name()):
            ps = actor
            break
    if ps is None:
        ps = eas.spawn_actor_from_class(unreal.PlayerStart, unreal.Vector(0.0, 0.0, 120.0))
        if ps:
            ps.set_actor_label("PlayerStart_Sanctuary")
    else:
        ps.set_actor_location(unreal.Vector(0.0, 0.0, 120.0), False, True)
        ps.set_actor_label("PlayerStart_Sanctuary")

    # Lighting
    sun = None
    for actor in eas.get_all_level_actors():
        if actor.get_class() and "DirectionalLight" in actor.get_class().get_name():
            sun = actor
            break
    if sun is None:
        sun = eas.spawn_actor_from_class(unreal.DirectionalLight, unreal.Vector(0, 0, 8000))
        if sun:
            sun.set_actor_label("Sun_Bastion")
    if sun:
        sun.set_actor_rotation(unreal.Rotator(-48.0, 35.0, 0.0), False)
        try:
            comp = sun.light_component
            _set(comp, "intensity", 8.0)
            _set(comp, "light_color", unreal.Color(255, 214, 170, 255))
            _set(comp, "atmosphere_sunlight", True)
            _set(comp, "cast_shadows", True)
            _set(comp, "forward_shading_priority", 0)
        except Exception as exc:
            log_warn("sun tweak: %s" % exc)

    sky = None
    for actor in eas.get_all_level_actors():
        if actor.get_class() and "SkyAtmosphere" in actor.get_class().get_name():
            sky = actor
            break
    if sky is None:
        sky = eas.spawn_actor_from_class(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0))
        if sky:
            sky.set_actor_label("SkyAtmosphere_Bastion")

    skylight = None
    for actor in eas.get_all_level_actors():
        if actor.get_class() and actor.get_class().get_name() == "SkyLight":
            skylight = actor
            break
    if skylight is None:
        skylight = eas.spawn_actor_from_class(unreal.SkyLight, unreal.Vector(0, 0, 600))
        if skylight:
            skylight.set_actor_label("SkyLight_Bastion")
    if skylight:
        try:
            comp = skylight.light_component
            _set(comp, "real_time_capture", True)
            _set(comp, "intensity", 1.0)
            _set(comp, "cast_shadows", True)
        except Exception as exc:
            log_warn("skylight: %s" % exc)

    fog = None
    for actor in eas.get_all_level_actors():
        if actor.get_class() and "ExponentialHeightFog" in actor.get_class().get_name():
            fog = actor
            break
    if fog is None:
        fog = eas.spawn_actor_from_class(unreal.ExponentialHeightFog, unreal.Vector(0, 0, 0))
        if fog:
            fog.set_actor_label("HeightFog_Bastion")
    if fog:
        try:
            comp = fog.component if hasattr(fog, "component") else fog.height_fog_component
            _set(comp, "fog_density", 0.018)
            _set(comp, "fog_height_falloff", 0.12)
            _set(comp, "fog_inscattering_color", unreal.LinearColor(0.012, 0.012, 0.025, 1.0))
            _set(comp, "volumetric_fog", True)
            _set(comp, "volumetric_fog_scattering_distribution", 0.7)
        except Exception as exc:
            log_warn("fog: %s" % exc)

    pp = None
    for actor in eas.get_all_level_actors():
        if actor.get_class() and "PostProcessVolume" in actor.get_class().get_name():
            pp = actor
            break
    if pp is None:
        pp = eas.spawn_actor_from_class(unreal.PostProcessVolume, unreal.Vector(0, 0, 0))
        if pp:
            pp.set_actor_label("PP_Bastion")
    if pp:
        try:
            _set(pp, "unbound", True)
            _set(pp, "priority", 1.0)
            _set(pp, "blend_weight", 1.0)
            settings = pp.settings
            settings.bloom_method = unreal.BloomMethod.BM_SOG
            settings.bloom_intensity = 0.35
            settings.auto_exposure_method = unreal.AutoExposureMethod.AEM_MANUAL
            try:
                settings.auto_exposure_bias = 0.0
            except Exception:
                pass
            settings.motion_blur_amount = 0.15
            settings.vignette_intensity = 0.28
            settings.ambient_cubemap_intensity = 0.0
            pp.settings = settings
        except Exception as exc:
            log_warn("post process: %s" % exc)

    # NavMesh bounds covering the 12000 UU Bastion arena (plus margin)
    nav = None
    for actor in eas.get_all_level_actors():
        if actor.get_class() and "NavMeshBoundsVolume" in actor.get_class().get_name():
            nav = actor
            break
    if nav is None:
        nav = eas.spawn_actor_from_class(unreal.NavMeshBoundsVolume, unreal.Vector(0, 0, 0))
        if nav:
            nav.set_actor_label("NavBounds_Bastion")
    if nav:
        nav.set_actor_location(unreal.Vector(0, 0, 0), False, True)
        # Brush scale: 1 unit = 200 uu typically for cube brush? Volume default brush is 200^3?
        # Set actor scale so extent covers radius 13000, height 2000
        nav.set_actor_scale3d(unreal.Vector(140.0, 140.0, 20.0))

    # Decorative lighting-only props that do not sit in the sanctuary (r < 720)
    def place_prop(asset_path, loc, rot, scale, label):
        if not unreal.EditorAssetLibrary.does_asset_exist(asset_path):
            log_warn("Missing prop " + asset_path)
            return None
        mesh = unreal.EditorAssetLibrary.load_asset(asset_path)
        if not mesh:
            return None
        # Don't duplicate labels
        for actor in eas.get_all_level_actors():
            if actor.get_actor_label() == label:
                actor.set_actor_location(loc, False, True)
                actor.set_actor_rotation(rot, False)
                actor.set_actor_scale3d(scale)
                return actor
        actor = eas.spawn_actor_from_class(unreal.StaticMeshActor, loc)
        if not actor:
            return None
        actor.set_actor_label(label)
        actor.set_actor_location(loc, False, True)
        actor.set_actor_rotation(rot, False)
        actor.set_actor_scale3d(scale)
        try:
            smc = actor.static_mesh_component
            smc.set_editor_property("static_mesh", mesh)
            try:
                smc.set_collision_profile_name("BlockAll")
            except Exception:
                pass
        except Exception as exc:
            log_warn("assign mesh %s: %s" % (label, exc))
        return actor

    import math
    # Pillars around the outer court, well outside sanctuary
    for i, ang in enumerate((20, 80, 140, 200, 260, 320)):
        rad = math.radians(ang)
        r = 3200.0
        loc = unreal.Vector(math.cos(rad) * r, math.sin(rad) * r, 0.0)
        place_prop("/Game/Props/SM_PropPillar", loc, unreal.Rotator(0, ang, 0), unreal.Vector(1, 1, 1), "Prop_Pillar_%d" % i)
    # Braziers at mid-ring
    for i, ang in enumerate((50, 170, 290)):
        rad = math.radians(ang)
        r = 2100.0
        loc = unreal.Vector(math.cos(rad) * r, math.sin(rad) * r, 0.0)
        place_prop("/Game/Props/SM_PropBrazier", loc, unreal.Rotator(0, ang, 0), unreal.Vector(1, 1, 1), "Prop_Brazier_%d" % i)
    # Arch / gate at the north approach (boss gate)
    place_prop(
        "/Game/Props/SM_PropIronGate",
        unreal.Vector(5200.0, 0.0, 0.0),
        unreal.Rotator(0, 90, 0),
        unreal.Vector(1, 1, 1),
        "BossGate_Iron",
    )
    place_prop(
        "/Game/Props/SM_PropArch",
        unreal.Vector(5100.0, 0.0, 0.0),
        unreal.Rotator(0, 90, 0),
        unreal.Vector(1, 1, 1),
        "BossGate_Arch",
    )
    place_prop(
        "/Game/Props/SM_PropShrineAltar",
        unreal.Vector(0.0, 0.0, 0.0),
        unreal.Rotator(0, 0, 0),
        unreal.Vector(0.7, 0.7, 0.7),
        "Sanctuary_Altar",
    )
    # Nudge altar so it does not block the player start — offset slightly
    for actor in eas.get_all_level_actors():
        if actor.get_actor_label() == "Sanctuary_Altar":
            actor.set_actor_location(unreal.Vector(0.0, 420.0, 0.0), False, True)
        if actor.get_actor_label() == "PlayerStart_Sanctuary":
            actor.set_actor_location(unreal.Vector(-250.0, -80.0, 120.0), False, True)

    # World settings: game mode + Lumen is already project default
    try:
        world = les.get_world() if hasattr(les, "get_world") else unreal.EditorLevelLibrary.get_editor_world()
        ws = world.get_world_settings() if world else None
        if ws:
            _set(ws, "force_no_precomputed_lighting", True)
    except Exception as exc:
        log_warn("world settings: %s" % exc)

    les.save_current_level()
    log("Map pass complete.")


def parse_mode():
    mode_file = os.path.join(os.path.dirname(__file__), "ue_port_mode.txt")
    if os.path.isfile(mode_file):
        token = open(mode_file, "r", encoding="utf-8").read().strip().lower()
        if token in ("quarantine", "import", "anims", "map", "visuals", "all"):
            return token
    # -ExecutePythonScript="path all" sometimes appends args after the script path
    args = []
    for a in sys.argv[1:]:
        if a and not a.endswith(".py"):
            args.append(a.lower())
    # Also honor an env-like token in the command line string
    cmd = " ".join(sys.argv).lower()
    for token in ("quarantine", "import", "anims", "map", "all"):
        if token in args or (" " + token) in cmd or cmd.endswith(token):
            if token != "all" or "all" in args or cmd.strip().endswith("all"):
                return token if token in args or cmd.strip().endswith(token) or token == "all" else token
    if args:
        return args[-1]
    return "all"



def do_visuals():
    log("Visuals: bake sanctum/gothic materials and fill locomotion blendspaces")
    saved = 0
    filled = 0
    try:
        saved = int(unreal.AetheriaMaterialLibrary.ensure_bastion_environment_materials())
        log("EnsureBastionEnvironmentMaterials saved=%d" % saved)
    except Exception as exc:
        log_warn("EnsureBastionEnvironmentMaterials: %s" % exc)
    try:
        filled = int(unreal.AetheriaAnimInstance.fill_locomotion_blend_spaces())
        log("FillLocomotionBlendSpaces filled=%d" % filled)
    except Exception as exc:
        log_warn("FillLocomotionBlendSpaces: %s" % exc)
    return saved, filled


def main():
    mode = parse_mode()
    log("Starting mode=" + mode)
    if mode in ("quarantine", "all"):
        do_quarantine()
    if mode in ("import", "all"):
        do_import()
    if mode in ("anims", "all"):
        do_anims()
    if mode in ("map", "all"):
        do_map()
    if mode in ("visuals", "all"):
        do_visuals()
    log("DONE mode=" + mode)
    # Leave the editor so RAM is released when launched as a commandlet.
    try:
        unreal.SystemLibrary.quit_editor()
    except Exception:
        pass


if __name__ == "__main__":
    main()
