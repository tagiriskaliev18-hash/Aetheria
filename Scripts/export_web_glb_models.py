"""
Blender 5.2 LTS headless exporter: converts RawAssets/Meshes/*.fbx + resized PBR
texture triplets into web-ready Y-up GLB models in _backup_web_prototype/public/models/.
"""

import os
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
MESH_DIR = ROOT / "RawAssets" / "Meshes"
TEX_CACHE = Path(
    r"C:\Users\user\.gemini\antigravity\brain\32b16ce5-cce9-48eb-9f7c-d7bba29e21be\scratch\glb_tex_cache"
)
OUT_DIR = ROOT / "_backup_web_prototype" / "public" / "models"

MODELS = [
    # (out_glb, fbx_name, tex_stem, target_height_m, max_tris, metallic, smooth)
    ("hero_00.glb", "SM_Hero_00.fbx", "T_Hero_00", 2.15, 9500, 0.18, True),
    ("hero_01.glb", "SM_Hero_01.fbx", "T_Hero_01", 2.20, 9500, 0.45, True),
    ("hero_02.glb", "SM_Hero_02.fbx", "T_Hero_02", 2.15, 9500, 0.28, True),
    ("hero_03.glb", "SM_Hero_03.fbx", "T_Hero_03", 2.20, 9500, 0.42, True),
    ("mob_swarm.glb", "SM_Mob_Swarm_00.fbx", "T_Mob_Swarm_00", 1.55, 5500, 0.15, True),
    ("mob_ranged.glb", "SM_Mob_Ranged_00.fbx", "T_Mob_Ranged_00", 1.95, 6000, 0.25, True),
    ("mob_bruiser.glb", "SM_Mob_Bruiser_00.fbx", "T_Mob_Bruiser_00", 2.65, 6500, 0.35, True),
    ("boss_archon.glb", "SM_Boss_Loc09.fbx", "T_Boss_Loc09", 4.20, 10000, 0.48, True),
    ("prop_statue.glb", "SM_GothicStatue.fbx", "T_GothicStatue", 3.80, 5000, 0.08, True),
    ("prop_arch.glb", "SM_LargeIronGate.fbx", "T_LargeIronGate", 4.80, 4500, 0.55, False),
    ("prop_pillar.glb", "SM_GothicPillar.fbx", "T_GothicStone", 5.00, 4000, 0.08, False),
    ("prop_boulder.glb", "SM_CliffBoulder.fbx", "T_CliffBoulder", 1.85, 3500, 0.05, True),
    ("prop_firepit.glb", "SM_IronBrazier.fbx", "T_GothicStone", 1.35, 3500, 0.65, False),
    ("prop_tree.glb", "SM_DeadTree.fbx", "T_DeadTree", 4.80, 4500, 0.02, True),
    ("prop_chest.glb", "SM_TreasureChest.fbx", "T_TreasureChest", 1.15, 4000, 0.48, False),
]


def clear_scene() -> None:
    bpy.ops.wm.read_factory_settings(use_empty=True)


def build_pbr_material(stem: str, metallic: float) -> bpy.types.Material:
    mat = bpy.data.materials.new(name=f"M_{stem}")
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    if not bsdf:
        return mat

    bsdf.inputs["Metallic"].default_value = metallic
    if "Specular IOR Level" in bsdf.inputs:
        bsdf.inputs["Specular IOR Level"].default_value = 0.45

    alb_path = TEX_CACHE / f"{stem}_Albedo.jpg"
    nrm_path = TEX_CACHE / f"{stem}_Normal.jpg"
    rgh_path = TEX_CACHE / f"{stem}_Roughness.jpg"

    if alb_path.exists():
        img = bpy.data.images.load(str(alb_path))
        img.colorspace_settings.name = "sRGB"
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = img
        nt.links.new(node.outputs["Color"], bsdf.inputs["Base Color"])

    if rgh_path.exists():
        img_r = bpy.data.images.load(str(rgh_path))
        img_r.colorspace_settings.name = "Non-Color"
        node_r = nt.nodes.new("ShaderNodeTexImage")
        node_r.image = img_r
        nt.links.new(node_r.outputs["Color"], bsdf.inputs["Roughness"])
    else:
        bsdf.inputs["Roughness"].default_value = 0.62

    if nrm_path.exists():
        img_n = bpy.data.images.load(str(nrm_path))
        img_n.colorspace_settings.name = "Non-Color"
        node_n = nt.nodes.new("ShaderNodeTexImage")
        node_n.image = img_n
        nmap = nt.nodes.new("ShaderNodeNormalMap")
        nmap.inputs["Strength"].default_value = 1.0
        nt.links.new(node_n.outputs["Color"], nmap.inputs["Color"])
        nt.links.new(nmap.outputs["Normal"], bsdf.inputs["Normal"])

    return mat


def convert_one(
    out_glb: str,
    fbx_name: str,
    tex_stem: str,
    target_h: float,
    max_tris: int,
    metallic: float,
    smooth: bool,
) -> None:
    fbx_path = MESH_DIR / fbx_name
    if not fbx_path.exists():
        raise FileNotFoundError(f"Missing FBX: {fbx_path}")

    clear_scene()
    bpy.ops.import_scene.fbx(filepath=str(fbx_path))

    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH" and len(o.data.vertices) > 0]
    if not meshes:
        raise RuntimeError(f"No mesh geometry in {fbx_name}")

    # Clear parenting while preserving world transforms, then apply transforms
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

    # Remove non-mesh helper objects (armatures, cameras, lights, empties)
    for o in list(bpy.context.scene.objects):
        if o.type != "MESH" or len(o.data.vertices) == 0:
            bpy.data.objects.remove(o, do_unlink=True)

    meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for o in meshes:
        o.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()

    obj = bpy.context.view_layer.objects.active
    obj.name = out_glb.replace(".glb", "")

    # For multi-rock kit SM_CliffBoulder, extract only the single largest standalone boulder island
    if out_glb == "prop_boulder.glb":
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.mesh.separate(type="LOOSE")
        bpy.ops.object.mode_set(mode="OBJECT")
        loose_meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        if len(loose_meshes) > 1:
            largest = max(loose_meshes, key=lambda o: len(o.data.vertices))
            for o in loose_meshes:
                if o != largest:
                    bpy.data.objects.remove(o, do_unlink=True)
            obj = largest
            obj.name = "prop_boulder"
            bpy.context.view_layer.objects.active = obj

    # Clean degenerate geometry so Decimate cleanly hits polygon budgets
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    bmesh.ops.dissolve_degenerate(bm, dist=0.0002, edges=bm.edges[:])
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()

    # Center XY at 0 and snap lowest Z vertex to 0, then scale uniformly to target_h meters
    verts = obj.data.vertices
    min_x = min(v.co.x for v in verts)
    max_x = max(v.co.x for v in verts)
    min_y = min(v.co.y for v in verts)
    max_y = max(v.co.y for v in verts)
    min_z = min(v.co.z for v in verts)
    max_z = max(v.co.z for v in verts)

    cx = 0.5 * (min_x + max_x)
    cy = 0.5 * (min_y + max_y)
    cur_h = max(0.001, max_z - min_z)
    scale_factor = target_h / cur_h

    for v in verts:
        v.co.x = (v.co.x - cx) * scale_factor
        v.co.y = (v.co.y - cy) * scale_factor
        v.co.z = (v.co.z - min_z) * scale_factor
    obj.data.update()

    # Decimate if triangle budget exceeded
    tri_count = sum(max(1, len(p.vertices) - 2) for p in obj.data.polygons)
    if tri_count > max_tris:
        mod = obj.modifiers.new(name="Decimate", type="DECIMATE")
        mod.ratio = max(0.05, float(max_tris) / float(tri_count))
        bpy.ops.object.modifier_apply(modifier=mod.name)

    if smooth:
        for p in obj.data.polygons:
            p.use_smooth = True

    if not obj.data.uv_layers:
        obj.data.uv_layers.new(name="UVMap")
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.01)
        bpy.ops.object.mode_set(mode="OBJECT")

    # Bind crisp PBR material
    mat = build_pbr_material(tex_stem, metallic)
    obj.data.materials.clear()
    obj.data.materials.append(mat)

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    glb_path = OUT_DIR / out_glb
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj

    bpy.ops.export_scene.gltf(
        filepath=str(glb_path),
        export_format="GLB",
        export_yup=True,
        export_image_format="AUTO",
        use_selection=True,
        export_apply=True,
        export_normals=True,
        export_texcoords=True,
        export_materials="EXPORT",
        export_animations=False,
    )
    final_tris = sum(max(1, len(p.vertices) - 2) for p in obj.data.polygons)
    sz_kb = round(glb_path.stat().st_size / 1024.0, 1)
    print(f"[GLB] {out_glb}: tris={final_tris}, size={sz_kb} KB")


def main() -> None:
    targets = set(sys.argv[sys.argv.index("--") + 1 :]) if "--" in sys.argv else None
    for item in MODELS:
        if targets and item[0] not in targets:
            continue
        convert_one(*item)


if __name__ == "__main__":
    main()
