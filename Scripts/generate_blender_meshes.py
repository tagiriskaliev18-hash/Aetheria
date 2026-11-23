import math
import os
from pathlib import Path
import bpy
import bmesh
from mathutils import Vector

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
POLY_MODELS = Path(r"D:\Tools\assets\polyhaven\models")
TEX_DIR = ROOT / "RawAssets" / "Textures"
MESH_DIR = ROOT / "RawAssets" / "Meshes"
LOOKDEV_PATH = ROOT / "RawAssets" / "LookDev_Diablo_Check.png"

MESH_DIR.mkdir(parents=True, exist_ok=True)


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def finalize_and_export(objs, fbx_name: str, smooth_angle_deg: float = 38.0, origin_bottom: bool = True):
    bpy.ops.object.select_all(action="DESELECT")
    for o in objs:
        if o and o.type == "MESH":
            o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    if len(objs) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active
    obj.name = fbx_name.replace(".fbx", "")

    # Center XY and put bottom at Z=0 if requested
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
    bbox = [obj.matrix_world @ Vector(corner) for corner in obj.bound_box]
    cx = (min(v.x for v in bbox) + max(v.x for v in bbox)) * 0.5
    cy = (min(v.y for v in bbox) + max(v.y for v in bbox)) * 0.5
    min_z = min(v.z for v in bbox)
    for v in obj.data.vertices:
        v.co.x -= cx
        v.co.y -= cy
        if origin_bottom:
            v.co.z -= min_z
    obj.data.update()

    # Smart UV unwrap + smooth shading
    bpy.context.view_layer.objects.active = obj
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.select_all(action="SELECT")
    try:
        bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.02)
    except Exception:
        pass
    bpy.ops.object.mode_set(mode="OBJECT")

    try:
        bpy.ops.object.shade_smooth()
    except Exception:
        pass

    out_path = str(MESH_DIR / fbx_name)
    bpy.ops.export_scene.fbx(
        filepath=out_path,
        use_selection=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="-Y",
        axis_up="Z",
        mesh_smooth_type="FACE",
        use_mesh_modifiers=True,
    )
    print(f"Exported {out_path} (verts={len(obj.data.vertices)}, polys={len(obj.data.polygons)})")
    return obj


def add_bevel(obj, width=0.03, segments=2):
    mod = obj.modifiers.new(name="Bevel", type="BEVEL")
    mod.width = width
    mod.segments = segments
    mod.limit_method = "ANGLE"
    mod.angle_limit = math.radians(30.0)


def add_displace(obj, strength=0.08, scale=1.4, name="Displace"):
    tex = bpy.data.textures.new(name=f"{name}_Tex", type="CLOUDS")
    tex.noise_scale = scale
    tex.noise_depth = 3
    mod = obj.modifiers.new(name=name, type="DISPLACE")
    mod.texture = tex
    mod.strength = strength


def export_polyhaven_prop(blend_rel: str, fbx_name: str, target_height_m: float, decimate_ratio: float):
    clear_scene()
    blend_path = str(POLY_MODELS / blend_rel)
    with bpy.data.libraries.load(blend_path, link=False) as (data_from, data_to):
        data_to.objects = [name for name in data_from.objects]

    meshes = []
    for obj in data_to.objects:
        if obj is not None and obj.type == "MESH":
            bpy.context.collection.objects.link(obj)
            meshes.append(obj)

    if not meshes:
        return None

    bpy.ops.object.select_all(action="DESELECT")
    for m in meshes:
        m.select_set(True)
    bpy.context.view_layer.objects.active = meshes[0]
    if len(meshes) > 1:
        bpy.ops.object.join()
    obj = bpy.context.view_layer.objects.active

    # Decimate for crisp real-time UE5 Nanite/Instanced use
    if len(obj.data.polygons) > 6500:
        dec = obj.modifiers.new(name="Decimate", type="DECIMATE")
        dec.ratio = decimate_ratio
        bpy.ops.object.modifier_apply(modifier=dec.name)

    bbox = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
    cur_h = max(0.01, max(v.z for v in bbox) - min(v.z for v in bbox))
    scale_f = target_height_m / cur_h
    obj.scale = (scale_f, scale_f, scale_f)
    bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)

    return finalize_and_export([obj], fbx_name, origin_bottom=True)


def build_gothic_pillar():
    clear_scene()
    parts = []
    # Stepped octagonal plinth
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.95, depth=0.45, location=(0, 0, 0.225))
    plinth = bpy.context.active_object
    add_bevel(plinth, 0.04, 2)
    parts.append(plinth)

    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.72, depth=0.35, location=(0, 0, 0.625))
    plinth2 = bpy.context.active_object
    add_bevel(plinth2, 0.03, 2)
    parts.append(plinth2)

    # Fluted column shaft (16 sides, alternating inset flutes)
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.52, depth=4.6, location=(0, 0, 3.1))
    shaft = bpy.context.active_object
    bm = bmesh.new()
    bm.from_mesh(shaft.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=4, use_grid_fill=True)
    for v in bm.verts:
        ang = math.atan2(v.co.y, v.co.x)
        flute = 0.055 * math.cos(ang * 8.0)
        if abs(v.co.z) < 2.1:
            r = math.hypot(v.co.x, v.co.y)
            if r > 0.1:
                v.co.x *= (r + flute) / r
                v.co.y *= (r + flute) / r
    bm.to_mesh(shaft.data)
    bm.free()
    add_displace(shaft, strength=0.04, scale=1.1)
    parts.append(shaft)

    # Broken angled gothic capital
    bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.82, depth=0.65, location=(0, 0, 5.7))
    cap = bpy.context.active_object
    cap.rotation_euler = (0.12, -0.08, 0.2)
    add_bevel(cap, 0.04, 2)
    add_displace(cap, strength=0.07, scale=0.8)
    parts.append(cap)

    finalize_and_export(parts, "SM_GothicPillar.fbx")


def build_ruined_arch():
    clear_scene()
    parts = []
    # Two weathered piers
    for sx in (-2.2, 2.2):
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(sx, 0, 2.1))
        pier = bpy.context.active_object
        pier.scale = (0.95, 0.95, 4.2)
        bpy.ops.object.transform_apply(scale=True)
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.subdivide(number_cuts=3)
        bpy.ops.object.mode_set(mode="OBJECT")
        add_bevel(pier, 0.04, 2)
        add_displace(pier, 0.06, 1.2)
        parts.append(pier)

    # Pointed gothic arch voussoirs (stone blocks)
    for side in (-1, 1):
        for i in range(6):
            t = (i + 0.5) / 6.0
            ang = t * (math.pi * 0.44)
            x = side * (2.2 * math.cos(ang))
            z = 4.2 + 2.4 * math.sin(ang)
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=(x, 0, z))
            blk = bpy.context.active_object
            blk.scale = (0.78, 1.05, 0.68)
            blk.rotation_euler = (0, side * ang, 0)
            bpy.ops.object.transform_apply(scale=True)
            add_bevel(blk, 0.04, 2)
            add_displace(blk, 0.04, 0.9)
            parts.append(blk)

    finalize_and_export(parts, "SM_RuinedArch.fbx")


def build_iron_brazier():
    clear_scene()
    parts = []
    # 3 forged iron legs
    for i in range(3):
        a = 2.0 * math.pi * i / 3.0
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=0.06, depth=1.55, location=(math.cos(a) * 0.35, math.sin(a) * 0.35, 0.72))
        leg = bpy.context.active_object
        leg.rotation_euler = (math.sin(a) * 0.25, -math.cos(a) * 0.25, 0)
        add_bevel(leg, 0.015, 2)
        parts.append(leg)

    # Flared fire bowl with spiked gothic crown
    bpy.ops.mesh.primitive_cone_add(vertices=12, radius1=0.22, radius2=0.58, depth=0.45, location=(0, 0, 1.38))
    bowl = bpy.context.active_object
    add_bevel(bowl, 0.02, 2)
    parts.append(bowl)

    for i in range(6):
        a = 2.0 * math.pi * i / 6.0
        bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=0.07, radius2=0.01, depth=0.28, location=(math.cos(a) * 0.56, math.sin(a) * 0.56, 1.65))
        spike = bpy.context.active_object
        parts.append(spike)

    finalize_and_export(parts, "SM_IronBrazier.fbx")


def build_runic_altar():
    clear_scene()
    parts = []
    for i, (r, h, z) in enumerate([(2.4, 0.28, 0.14), (1.85, 0.28, 0.42), (1.25, 0.55, 0.83)]):
        bpy.ops.mesh.primitive_cylinder_add(vertices=8, radius=r, depth=h, location=(0, 0, z))
        step = bpy.context.active_object
        add_bevel(step, 0.04, 2)
        add_displace(step, 0.03, 1.5)
        parts.append(step)

    for i in range(4):
        a = math.pi * 0.25 + i * math.pi * 0.5
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(math.cos(a) * 1.45, math.sin(a) * 1.45, 0.85))
        post = bpy.context.active_object
        post.scale = (0.32, 0.32, 1.35)
        post.rotation_euler = (0, 0, a)
        add_bevel(post, 0.03, 2)
        parts.append(post)

    finalize_and_export(parts, "SM_RunicAltar.fbx")


def build_archmage_robed():
    clear_scene()
    parts = []
    # Layered draped wizard cloak with vertical folds
    bpy.ops.mesh.primitive_cone_add(vertices=28, radius1=0.58, radius2=0.24, depth=1.32, location=(0, 0, 0.66))
    robe = bpy.context.active_object
    bm = bmesh.new()
    bm.from_mesh(robe.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=3, use_grid_fill=True)
    for v in bm.verts:
        ang = math.atan2(v.co.y, v.co.x)
        height_t = max(0.0, (0.66 - v.co.z) / 1.32)
        fold = 0.055 * math.sin(ang * 7.0 + height_t * 2.0) * height_t
        v.co.x += math.cos(ang) * fold
        v.co.y += math.sin(ang) * fold
    bm.to_mesh(robe.data)
    bm.free()
    parts.append(robe)

    # Broad armored/ornate shoulder mantle
    bpy.ops.mesh.primitive_cylinder_add(vertices=16, radius=0.38, depth=0.32, location=(0, 0, 1.32))
    mantle = bpy.context.active_object
    mantle.scale = (1.18, 0.82, 1.0)
    add_bevel(mantle, 0.03, 2)
    parts.append(mantle)

    # Pauldrons
    for sx in (-0.38, 0.38):
        bpy.ops.mesh.primitive_uv_sphere_add(segments=14, ring_count=10, radius=0.18, location=(sx, 0, 1.38))
        p = bpy.context.active_object
        p.scale = (1.15, 1.0, 0.72)
        parts.append(p)

    # Sculpted pointed cowl hood facing +X
    bpy.ops.mesh.primitive_cone_add(vertices=16, radius1=0.22, radius2=0.04, depth=0.48, location=(-0.03, 0, 1.66))
    hood = bpy.context.active_object
    hood.rotation_euler = (0, -0.45, 0)
    parts.append(hood)

    bpy.ops.mesh.primitive_uv_sphere_add(segments=14, ring_count=10, radius=0.14, location=(0.03, 0, 1.58))
    head = bpy.context.active_object
    parts.append(head)

    finalize_and_export(parts, "SM_ArchmageRobed.fbx")


def build_gnarled_staff():
    clear_scene()
    parts = []
    bpy.ops.mesh.primitive_cylinder_add(vertices=10, radius=0.042, depth=2.1, location=(0, 0, 1.05))
    shaft = bpy.context.active_object
    bm = bmesh.new()
    bm.from_mesh(shaft.data)
    bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=6, use_grid_fill=True)
    for v in bm.verts:
        t = (v.co.z + 1.05) / 2.1
        v.co.x += 0.05 * math.sin(t * 7.0)
        v.co.y += 0.04 * math.cos(t * 5.0)
    bm.to_mesh(shaft.data)
    bm.free()
    parts.append(shaft)

    # Crescent horned staff cradle at top
    for sx in (-1, 1):
        bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.055, radius2=0.01, depth=0.38, location=(0, sx * 0.14, 2.2))
        prong = bpy.context.active_object
        prong.rotation_euler = (sx * -0.45, 0, 0)
        parts.append(prong)

    finalize_and_export(parts, "SM_GnarledStaff.fbx")


def build_void_fiend():
    clear_scene()
    parts = []
    # Hunched demonic torso
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=0.45, location=(0.05, 0, 0.95))
    torso = bpy.context.active_object
    torso.scale = (1.35, 0.88, 1.15)
    add_displace(torso, 0.06, 0.8)
    parts.append(torso)

    # Horned skull forward (+X)
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.26, location=(0.45, 0, 1.25))
    head = bpy.context.active_object
    head.scale = (1.25, 0.9, 0.9)
    parts.append(head)

    for sy in (-0.16, 0.16):
        bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.065, radius2=0.01, depth=0.42, location=(0.35, sy, 1.52))
        horn = bpy.context.active_object
        horn.rotation_euler = (sy * 1.4, -0.55, 0)
        parts.append(horn)

        # Clawed arms
        bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.11, radius2=0.02, depth=0.85, location=(0.35, sy * 2.6, 0.72))
        arm = bpy.context.active_object
        arm.rotation_euler = (0, 1.15, sy * 0.8)
        parts.append(arm)

    # Dorsal spines
    for i in range(4):
        bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=0.07, radius2=0.01, depth=0.35, location=(-0.15 - i * 0.12, 0, 1.35 - i * 0.14))
        sp = bpy.context.active_object
        sp.rotation_euler = (0, -0.7, 0)
        parts.append(sp)

    finalize_and_export(parts, "SM_VoidFiend.fbx")


def build_stone_colossus():
    clear_scene()
    parts = []
    # Massive monolithic chest + shoulders + twin fists
    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 1.35))
    chest = bpy.context.active_object
    chest.scale = (0.95, 1.45, 1.35)
    bpy.ops.object.transform_apply(scale=True)
    bpy.ops.object.mode_set(mode="EDIT")
    bpy.ops.mesh.subdivide(number_cuts=2)
    bpy.ops.object.mode_set(mode="OBJECT")
    add_bevel(chest, 0.06, 2)
    add_displace(chest, 0.08, 1.0)
    parts.append(chest)

    for sy in (-1.05, 1.05):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.42, location=(0.15, sy, 1.75))
        sh = bpy.context.active_object
        add_displace(sh, 0.07, 0.9)
        parts.append(sh)

        bpy.ops.mesh.primitive_cube_add(size=0.58, location=(0.35, sy * 1.05, 0.95))
        arm = bpy.context.active_object
        arm.scale = (1.0, 0.9, 1.65)
        add_bevel(arm, 0.05, 2)
        parts.append(arm)

    bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=0.32, radius2=0.14, depth=0.52, location=(0.15, 0, 2.15))
    head = bpy.context.active_object
    parts.append(head)

    finalize_and_export(parts, "SM_StoneColossus.fbx")


def build_archon_boss():
    clear_scene()
    parts = []
    # Towering robed armored form + wide horned crown + jagged back halo
    bpy.ops.mesh.primitive_cone_add(vertices=16, radius1=0.82, radius2=0.35, depth=2.1, location=(0, 0, 1.05))
    body = bpy.context.active_object
    add_displace(body, 0.06, 1.1)
    parts.append(body)

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0, 0, 2.05))
    torso = bpy.context.active_object
    torso.scale = (0.75, 1.25, 0.72)
    add_bevel(torso, 0.05, 2)
    parts.append(torso)

    bpy.ops.mesh.primitive_uv_sphere_add(segments=14, ring_count=10, radius=0.25, location=(0.08, 0, 2.55))
    head = bpy.context.active_object
    parts.append(head)

    # Crown horns
    for sy in (-0.28, 0.28):
        bpy.ops.mesh.primitive_cone_add(vertices=8, radius1=0.09, radius2=0.01, depth=0.72, location=(0.05, sy, 2.85))
        horn = bpy.context.active_object
        horn.rotation_euler = (sy * 1.1, -0.3, 0)
        parts.append(horn)

    # Broken floating obsidian halo behind back
    for i in range(7):
        a = -math.pi * 0.45 + (math.pi * 0.9 * i / 6.0)
        bpy.ops.mesh.primitive_cone_add(vertices=4, radius1=0.11, radius2=0.02, depth=0.55, location=(-0.48, math.sin(a) * 1.25, 2.25 + math.cos(a) * 0.95))
        shard = bpy.context.active_object
        shard.rotation_euler = (a, 0, 0)
        parts.append(shard)

    finalize_and_export(parts, "SM_ArchonBoss.fbx")


def build_strategy_move_arrow():
    clear_scene()
    # Clean crisp 3D beveled RTS chevron pointing along +X (nose at +0.5, tail at -0.45)
    bm = bmesh.new()
    verts = [
        bm.verts.new((0.52, 0.0, 0.04)),    # 0 Nose top
        bm.verts.new((-0.38, 0.34, 0.06)),  # 1 Left wing top
        bm.verts.new((-0.14, 0.0, 0.08)),   # 2 Inner notch top
        bm.verts.new((-0.38, -0.34, 0.06)), # 3 Right wing top
        bm.verts.new((0.52, 0.0, -0.04)),   # 4 Nose bot
        bm.verts.new((-0.38, 0.34, -0.04)), # 5 Left wing bot
        bm.verts.new((-0.14, 0.0, -0.04)),  # 6 Inner notch bot
        bm.verts.new((-0.38, -0.34, -0.04)) # 7 Right wing bot
    ]
    faces = [
        (0, 1, 2), (0, 2, 3),
        (4, 6, 5), (4, 7, 6),
        (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)
    ]
    for f in faces:
        bm.faces.new([verts[i] for i in f])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])

    mesh = bpy.data.meshes.new("SM_StrategyMoveArrow")
    bm.to_mesh(mesh)
    bm.free()

    obj = bpy.data.objects.new("SM_StrategyMoveArrow", mesh)
    bpy.context.collection.objects.link(obj)
    add_bevel(obj, 0.015, 2)
    finalize_and_export([obj], "SM_StrategyMoveArrow.fbx", origin_bottom=False)


def make_pbr_mat(name: str, albedo_name: str, normal_name: str, rough_name: str, uv_scale: float = 1.0, metallic: float = 0.05):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()

    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bsdf = nt.nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.inputs["Metallic"].default_value = metallic
    nt.links.new(bsdf.outputs["BSDF"], out.inputs["Surface"])

    tc = nt.nodes.new("ShaderNodeTexCoord")
    mp = nt.nodes.new("ShaderNodeMapping")
    mp.inputs["Scale"].default_value = (uv_scale, uv_scale, uv_scale)
    nt.links.new(tc.outputs["UV"], mp.inputs["Vector"])

    def load_tex(fname, colorspace):
        p = str(TEX_DIR / fname)
        if not os.path.exists(p):
            return None
        img = bpy.data.images.load(p)
        img.colorspace_settings.name = colorspace
        node = nt.nodes.new("ShaderNodeTexImage")
        node.image = img
        nt.links.new(mp.outputs["Vector"], node.inputs["Vector"])
        return node

    if n_alb := load_tex(albedo_name, "sRGB"):
        nt.links.new(n_alb.outputs["Color"], bsdf.inputs["Base Color"])
    if n_rgh := load_tex(rough_name, "Non-Color"):
        nt.links.new(n_rgh.outputs["Color"], bsdf.inputs["Roughness"])
    if n_nor := load_tex(normal_name, "Non-Color"):
        # Invert green channel back for Blender OpenGL preview only (PNG is DirectX for UE5!)
        sep = nt.nodes.new("ShaderNodeSeparateColor")
        inv = nt.nodes.new("ShaderNodeMath")
        inv.operation = "SUBTRACT"
        inv.inputs[0].default_value = 1.0
        comb = nt.nodes.new("ShaderNodeCombineColor")
        nrm = nt.nodes.new("ShaderNodeNormalMap")
        nrm.inputs["Strength"].default_value = 1.15
        nt.links.new(n_nor.outputs["Color"], sep.inputs["Color"])
        nt.links.new(sep.outputs["Red"], comb.inputs["Red"])
        nt.links.new(sep.outputs["Green"], inv.inputs[1])
        nt.links.new(inv.outputs["Value"], comb.inputs["Green"])
        nt.links.new(sep.outputs["Blue"], comb.inputs["Blue"])
        nt.links.new(comb.outputs["Color"], nrm.inputs["Color"])
        nt.links.new(nrm.outputs["Normal"], bsdf.inputs["Normal"])

    return mat


def make_emissive_mat(name: str, color: tuple, strength: float):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    bsdf = nt.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (*color, 1.0)
    if "Emission Color" in bsdf.inputs:
        bsdf.inputs["Emission Color"].default_value = (*color, 1.0)
        bsdf.inputs["Emission Strength"].default_value = strength
    return mat


def render_lookdev_scene():
    clear_scene()
    scene = bpy.context.scene

    # Choose best available renderer in Blender 5.2
    engines = [e.identifier for e in bpy.types.RenderSettings.bl_rna.properties["engine"].enum_items]
    if "CYCLES" in engines:
        scene.render.engine = "CYCLES"
        scene.cycles.samples = 40
        scene.cycles.use_denoising = True
    elif "BLENDER_EEVEE_NEXT" in engines:
        scene.render.engine = "BLENDER_EEVEE_NEXT"
    else:
        scene.render.engine = "BLENDER_EEVEE"

    scene.render.resolution_x = 1440
    scene.render.resolution_y = 900
    scene.view_settings.view_transform = "AgX"

    # Dark moody sanctuary world
    world = bpy.data.worlds.new("AetheriaLookDevWorld")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    bg.inputs["Color"].default_value = (0.010, 0.013, 0.025, 1.0)
    bg.inputs["Strength"].default_value = 0.7

    mat_floor = make_pbr_mat("M_Floor", "T_SanctumFlagstone_Albedo.png", "T_SanctumFlagstone_Normal.png", "T_SanctumFlagstone_Roughness.png", uv_scale=8.0)
    mat_stone = make_pbr_mat("M_Stone", "T_GothicStone_Albedo.png", "T_GothicStone_Normal.png", "T_GothicStone_Roughness.png", uv_scale=2.2)
    mat_iron = make_pbr_mat("M_Iron", "T_DarkIronCloth_Albedo.png", "T_DarkIronCloth_Normal.png", "T_DarkIronCloth_Roughness.png", uv_scale=2.0, metallic=0.45)

    # Ground plane
    bpy.ops.mesh.primitive_plane_add(size=42.0, location=(0, 0, 0))
    floor = bpy.context.active_object
    floor.data.materials.append(mat_floor)

    def import_fbx(fname, loc, rot_z=0.0, scale=1.0, mat=mat_stone):
        bpy.ops.import_scene.fbx(filepath=str(MESH_DIR / fname))
        for o in bpy.context.selected_objects:
            o.location = loc
            o.rotation_euler = (0, 0, rot_z)
            o.scale = (scale, scale, scale)
            if o.type == "MESH":
                o.data.materials.clear()
                o.data.materials.append(mat)
        return bpy.context.selected_objects[0]

    # Ruined Sanctuary Architecture
    import_fbx("SM_RunicAltar.fbx", (4.5, -2.5, 0), 0.3, 1.2, mat_stone)
    import_fbx("SM_RuinedArch.fbx", (6.0, 3.8, 0), -0.65, 1.25, mat_stone)
    for px, py in [(-4.8, 3.5), (0.0, 5.5), (5.5, -5.0), (-5.2, -4.5)]:
        import_fbx("SM_GothicPillar.fbx", (px, py, 0), px, 1.0, mat_stone)
    for bx, by in [(-7.5, 1.2), (8.2, 0.5), (2.2, 7.2)]:
        import_fbx("SM_CliffBoulder.fbx", (bx, by, 0), bx, 1.0, mat_stone)

    # Flaming Iron Braziers + Warm Chiaroscuro Point Lights
    mat_fire = make_emissive_mat("M_Fire", (1.0, 0.32, 0.04), 18.0)
    for fx, fy in [(-2.6, 2.2), (2.8, 2.4), (3.4, -3.2)]:
        import_fbx("SM_IronBrazier.fbx", (fx, fy, 0), 0, 1.0, mat_iron)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.22, location=(fx, fy, 1.62))
        flame = bpy.context.active_object
        flame.data.materials.append(mat_fire)
        bpy.ops.object.light_add(type="POINT", location=(fx, fy, 2.0))
        pt = bpy.context.active_object
        pt.data.color = (1.0, 0.42, 0.12)
        pt.data.energy = 650.0
        pt.data.shadow_soft_size = 0.35

    # Robed Archmage + Orbiting Q/W/E Spheres
    import_fbx("SM_ArchmageRobed.fbx", (-0.8, -0.6, 0), 0.45, 1.15, mat_iron)
    import_fbx("SM_GnarledStaff.fbx", (-0.4, -1.1, 0.1), 0.45, 1.0, mat_iron)
    orb_specs = [
        ((-1.4, -0.2, 1.75), (1.0, 0.22, 0.04), "M_OrbQ"),
        ((-0.2, -0.1, 1.95), (0.0, 0.92, 1.0), "M_OrbW"),
        ((-0.9, -1.3, 1.70), (0.48, 0.18, 1.0), "M_OrbE"),
    ]
    for loc, col, mname in orb_specs:
        bpy.ops.mesh.primitive_uv_sphere_add(segments=16, ring_count=12, radius=0.14, location=loc)
        orb = bpy.context.active_object
        orb.data.materials.append(make_emissive_mat(mname, col, 14.0))

    # Enemies
    import_fbx("SM_VoidFiend.fbx", (3.6, 0.9, 0), 3.3, 1.15, mat_iron)
    import_fbx("SM_StoneColossus.fbx", (4.8, 2.1, 0), 3.5, 1.15, mat_stone)

    # 4 Converging RTS Click-Move Arrows at (1.5, -2.0)
    mat_arrow = make_emissive_mat("M_RTSArrow", (0.10, 0.95, 0.65), 10.0)
    click_pt = Vector((1.5, -2.0, 0.12))
    for k in range(4):
        a = k * math.pi * 0.5
        pos = click_pt + Vector((math.cos(a) * 0.68, math.sin(a) * 0.68, 0.15))
        arr = import_fbx("SM_StrategyMoveArrow.fbx", pos, a + math.pi, 0.62, mat_arrow)
        arr.rotation_euler = (0, 0.35, a + math.pi)

    # Cool Moonlight Key Directional Light (Diablo contrast against warm braziers)
    bpy.ops.object.light_add(type="SUN", location=(-10, -10, 14))
    sun = bpy.context.active_object
    sun.rotation_euler = (math.radians(48), math.radians(18), math.radians(-35))
    sun.data.color = (0.58, 0.72, 1.0)
    sun.data.energy = 2.6

    # Isometric Diablo Camera
    bpy.ops.object.camera_add(location=(-10.5, -10.5, 11.8))
    cam = bpy.context.active_object
    direction = Vector((1.8, 0.2, 1.0)) - cam.location
    cam.rotation_euler = direction.to_track_quat("-Z", "Y").to_euler()
    cam.data.lens = 42.0
    scene.camera = cam

    scene.render.filepath = str(LOOKDEV_PATH)
    bpy.ops.render.render(write_still=True)
    print(f"Rendered LookDev preview: {LOOKDEV_PATH}")


def main():
    export_polyhaven_prop("namaqualand_boulder_02/namaqualand_boulder_02_2k.blend", "SM_CliffBoulder.fbx", 2.8, 0.25)
    export_polyhaven_prop("tree_small_02/tree_small_02_2k.blend", "SM_DeadTree.fbx", 5.5, 0.22)
    build_gothic_pillar()
    build_ruined_arch()
    build_iron_brazier()
    build_runic_altar()
    build_archmage_robed()
    build_gnarled_staff()
    build_void_fiend()
    build_stone_colossus()
    build_archon_boss()
    build_strategy_move_arrow()
    render_lookdev_scene()


if __name__ == "__main__":
    main()
