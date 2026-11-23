import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Euler, Vector

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
RAW = ROOT / "RawAssets"
CACHE = RAW / "PolyHavenCache"
MESH_DIR = RAW / "Meshes"
TEX_DIR = RAW / "Textures"
PROCEDURAL_FALLBACK_PASS = False


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def ensure_material(name: str, rgba=(0.15, 0.16, 0.18, 1.0), metallic=0.1, roughness=0.72, emission=(0.0, 0.0, 0.0), emission_strength=0.0):
    if name in bpy.data.materials:
        return bpy.data.materials[name]
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    bsdf = mat.node_tree.nodes.get("Principled BSDF")
    if bsdf:
        bsdf.inputs["Base Color"].default_value = rgba
        bsdf.inputs["Metallic"].default_value = metallic
        bsdf.inputs["Roughness"].default_value = roughness
        if "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = (*emission, 1.0)
        if "Emission Strength" in bsdf.inputs:
            bsdf.inputs["Emission Strength"].default_value = emission_strength
    return mat


def get_3slot_materials():
    m_cloth = ensure_material("M_Slot0_Cloth", (0.08, 0.14, 0.17, 1.0), metallic=0.08, roughness=0.76)
    m_metal = ensure_material("M_Slot1_Metal", (0.42, 0.40, 0.36, 1.0), metallic=0.88, roughness=0.32)
    m_rune = ensure_material("M_Slot2_Rune", (0.08, 0.96, 0.72, 1.0), metallic=0.0, roughness=0.18, emission=(0.08, 0.96, 0.72), emission_strength=6.5)
    return [m_cloth, m_metal, m_rune]


def create_tapered_curve(name: str, points_and_radii, bevel_depth=0.05, res_u=18, bevel_res=6, cyclic=False):
    # points_and_radii: list of ((x,y,z), radius)
    curve_data = bpy.data.curves.new(name=name, type="CURVE")
    curve_data.dimensions = "3D"
    curve_data.resolution_u = res_u
    curve_data.bevel_depth = bevel_depth
    curve_data.bevel_resolution = bevel_res
    curve_data.use_fill_caps = True

    spline = curve_data.splines.new("NURBS")
    spline.points.add(len(points_and_radii) - 1)
    for i, (co, rad) in enumerate(points_and_radii):
        spline.points[i].co = (co[0], co[1], co[2], 1.0)
        spline.points[i].radius = rad
    spline.use_endpoint_u = True
    spline.use_cyclic_u = cyclic

    obj = bpy.data.objects.new(name, curve_data)
    bpy.context.collection.objects.link(obj)
    bpy.context.view_layer.objects.active = obj
    obj.select_set(True)
    bpy.ops.object.convert(target="MESH")
    return bpy.context.active_object


def add_displace(obj, strength=0.025, scale=0.22):
    tex = bpy.data.textures.new(name=f"Tex_{obj.name}", type="CLOUDS")
    tex.noise_scale = scale
    mod = obj.modifiers.new(name="Displace", type="DISPLACE")
    mod.texture = tex
    mod.strength = strength


def add_subdiv(obj, levels=2):
    s = obj.modifiers.new("Subdiv", "SUBSURF")
    s.levels = levels
    s.render_levels = levels


def decimate_object(obj, max_tris: int):
    bpy.context.view_layer.objects.active = obj
    tris = sum(max(0, len(p.vertices) - 2) for p in obj.data.polygons)
    if tris > max_tris and tris > 0:
        mod = obj.modifiers.new(name="RTX3070_Decimate", type="DECIMATE")
        mod.decimate_type = "COLLAPSE"
        mod.ratio = max(0.03, float(max_tris) / float(tris))
        bpy.ops.object.modifier_apply(modifier=mod.name)


def join_and_prepare(parts_with_slots, origin_bottom=True, normalize_height=None, max_tris=28000):
    slots = get_3slot_materials()
    for obj, slot_idx in parts_with_slots:
        bpy.context.view_layer.objects.active = obj
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=True)
        for mod in list(obj.modifiers):
            try:
                bpy.ops.object.modifier_apply(modifier=mod.name)
            except Exception:
                pass
        obj.data.materials.clear()
        for m in slots:
            obj.data.materials.append(m)
        for p in obj.data.polygons:
            p.material_index = slot_idx
            p.use_smooth = True

    bpy.ops.object.select_all(action="DESELECT")
    for obj, _ in parts_with_slots:
        obj.select_set(True)
    bpy.context.view_layer.objects.active = parts_with_slots[0][0]
    if len(parts_with_slots) > 1:
        bpy.ops.object.join()
    merged = bpy.context.view_layer.objects.active

    # IMPORTANT: Only shift Z to floor for grounded actors (preserve exact X/Y anatomical pivot!)
    if origin_bottom:
        bbox = [merged.matrix_world @ Vector(corner) for corner in merged.bound_box]
        min_z = min(v.z for v in bbox)
        max_z = max(v.z for v in bbox)
        for v in merged.data.vertices:
            v.co.z -= min_z
        cur_h = max(0.001, max_z - min_z)
        if normalize_height:
            s = normalize_height / cur_h
            for v in merged.data.vertices:
                v.co *= s

    decimate_object(merged, max_tris)

    bpy.context.view_layer.objects.active = merged
    if not merged.data.uv_layers:
        merged.data.uv_layers.new(name="UVMap")
    try:
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.015)
        bpy.ops.object.mode_set(mode="OBJECT")
    except Exception:
        if bpy.context.mode != "OBJECT":
            bpy.ops.object.mode_set(mode="OBJECT")

    return merged


def export_fbx(obj, filename: str, overwrite: bool = True):
    MESH_DIR.mkdir(parents=True, exist_ok=True)
    out = MESH_DIR / filename
    if (not overwrite or PROCEDURAL_FALLBACK_PASS) and out.exists() and out.stat().st_size > 1000:
        return
    bpy.ops.object.select_all(action="DESELECT")
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj
    bpy.ops.export_scene.fbx(
        filepath=str(out),
        use_selection=True,
        apply_unit_scale=True,
        apply_scale_options="FBX_SCALE_ALL",
        axis_forward="X",
        axis_up="Z",
        mesh_smooth_type="FACE",
        use_mesh_modifiers=True,
        add_leaf_bones=False,
    )
    tris = sum(max(0, len(p.vertices) - 2) for p in obj.data.polygons)
    print(f"Exported {filename:32s} | tris={tris:6d} | size_kb={out.stat().st_size // 1024}")


# ---------------------------------------------------------------------------
# 1. Poly Haven CC0 Importer + Decimator
# ---------------------------------------------------------------------------
def process_polyhaven_assets(manifest: dict):
    budgets = {
        "SM_GothicStatue": (26000, 3.2),
        "SM_TreasureChest": (18000, 1.1),
        "SM_LargeIronGate": (24000, 4.4),
        "SM_RuinedArch": (20000, 5.2),
        "SM_LanternChandelier": (12000, 1.4),
        "SM_OrnateMace": (10000, 0.95),
        "SM_WarHammerScan": (10000, 1.25),
        "SM_AntiqueEstoc": (9000, 1.35),
        "SM_HornedSkullScan": (16000, 0.90),
        "SM_CliffBoulder": (16000, 2.4),
        "SM_DeadTree": (18000, 5.4),
        "SM_CastleDoor": (22000, 3.8),
        "SM_GothicLantern": (12000, 1.2),
        "SM_StoneFirePit": (16000, 0.9),
        "SM_LionRelief": (16000, 1.2),
        "SM_CitadelFortress": (26000, 18.0),
    }

    for entry in manifest.get("polyhaven", []):
        gltf_path = Path(entry["gltf"])
        sm_name = entry["sm_name"]
        if not gltf_path.exists():
            continue
        clear_scene()
        bpy.ops.import_scene.gltf(filepath=str(gltf_path))
        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        if not meshes:
            continue
        bpy.ops.object.select_all(action="DESELECT")
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1:
            bpy.ops.object.join()
        merged = bpy.context.view_layer.objects.active
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

        max_tris, target_h = budgets.get(sm_name, (18000, None))
        bbox = [merged.matrix_world @ Vector(c) for c in merged.bound_box]
        cx = 0.5 * (min(v.x for v in bbox) + max(v.x for v in bbox))
        cy = 0.5 * (min(v.y for v in bbox) + max(v.y for v in bbox))
        min_z = min(v.z for v in bbox)
        max_z = max(v.z for v in bbox)
        for v in merged.data.vertices:
            v.co.x -= cx
            v.co.y -= cy
            v.co.z -= min_z
        if target_h and (max_z - min_z) > 0.01:
            sc = target_h / (max_z - min_z)
            for v in merged.data.vertices:
                v.co *= sc

        decimate_object(merged, max_tris)
        for p in merged.data.polygons:
            p.use_smooth = True
        export_fbx(merged, f"{sm_name}.fbx")


# ---------------------------------------------------------------------------
# Helper: Sculpted Anatomical Skull (cranium + brow ridge + sunken sockets + jaw + teeth)
# ---------------------------------------------------------------------------
def build_anatomical_skull(parts, origin=(0.0, 0.0, 0.0), scale=1.0, mat_slot=1, eye_slot=2):
    ox, oy, oz = origin
    # High-res subdivided cranium with sunken eye sockets and cheekbones sculpted via vertex carving
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=0.115 * scale, location=(ox, oy, oz + 0.025 * scale))
    cranium = bpy.context.active_object
    bm = bmesh.new()
    bm.from_mesh(cranium.data)
    for v in bm.verts:
        lx = (v.co.x - ox) / scale
        ly = (v.co.y - oy) / scale
        lz = (v.co.z - (oz + 0.025 * scale)) / scale
        # Elongate front-to-back & carve temporal hollows
        lx *= 1.12
        ly *= 0.86
        if lx > 0.03:
            # Carve left & right eye orbits
            for ey in (-0.040, 0.040):
                d_eye = math.sqrt((ly - ey) ** 2 + (lz - 0.002) ** 2)
                if d_eye < 0.032:
                    lx -= 0.042 * (1.0 - d_eye / 0.032)
            # Nasal cavity indentation
            d_nose = math.sqrt((ly) ** 2 + (lz + 0.030) ** 2)
            if d_nose < 0.018:
                lx -= 0.030 * (1.0 - d_nose / 0.018)
            # Brow ridge overhang
            if 0.024 <= lz <= 0.050 and abs(ly) < 0.068:
                lx += 0.018
        v.co = Vector((ox + lx * scale, oy + ly * scale, oz + 0.025 * scale + lz * scale))
    bm.to_mesh(cranium.data)
    bm.free()
    add_subdiv(cranium, 1)
    parts.append((cranium, mat_slot))

    # Sculpted mandible / jaw + teeth ridge
    jaw = create_tapered_curve(
        "JawCurve",
        [
            ((ox - 0.01 * scale, oy - 0.068 * scale, oz - 0.025 * scale), 0.28 * scale),
            ((ox + 0.06 * scale, oy - 0.052 * scale, oz - 0.072 * scale), 0.34 * scale),
            ((ox + 0.095 * scale, oy, oz - 0.082 * scale), 0.40 * scale),
            ((ox + 0.06 * scale, oy + 0.052 * scale, oz - 0.072 * scale), 0.34 * scale),
            ((ox - 0.01 * scale, oy + 0.068 * scale, oz - 0.025 * scale), 0.28 * scale),
        ],
        bevel_depth=0.055 * scale,
        res_u=16,
    )
    parts.append((jaw, mat_slot))

    if eye_slot is not None:
        for sy in (-0.040 * scale, 0.040 * scale):
            bpy.ops.mesh.primitive_uv_sphere_add(segments=10, ring_count=8, radius=0.016 * scale, location=(ox + 0.092 * scale, oy + sy, oz + 0.025 * scale))
            parts.append((bpy.context.active_object, eye_slot))


# ---------------------------------------------------------------------------
# 2. High-Detail Hollow Magus & Grimdark Hero Body Archetypes
# ---------------------------------------------------------------------------
def build_hollow_magus_body(parts, variant="hollow_magus"):
    # High-density continuous BMesh sculpted layered robe & chest armor
    bm = bmesh.new()
    rings = 32
    segs = 44
    ring_verts = []
    for r_i in range(rings):
        t = r_i / float(rings - 1)  # 0 (floor) -> 1 (shoulders/collar z=1.44)
        z = 0.04 + t * 1.40
        # Silhouette radius: dramatic flared hem -> cinched belted waist at t=0.58 -> broad chest at t=0.86
        if t < 0.58:
            base_r = 0.62 * (1.0 - t * 0.55)
        elif t < 0.88:
            wt = (t - 0.58) / 0.30
            base_r = 0.27 + wt * 0.09
        else:
            nt = (t - 0.88) / 0.12
            base_r = 0.36 - nt * 0.14

        verts_row = []
        for s_i in range(segs):
            ang = (2.0 * math.pi * s_i) / segs
            # Deep vertical fabric folds on lower skirt + front open coat slit feel
            pleat_amp = 0.065 * ((1.0 - min(1.0, t / 0.62)) ** 1.1)
            pleat = pleat_amp * (math.sin(ang * 10.0 + t * 3.2) + 0.45 * math.cos(ang * 17.0 - t * 2.0))
            tatter_z = 0.0
            if t < 0.12:
                tatter_z = 0.075 * math.sin(ang * 11.0 + 0.4) * (1.0 - t / 0.12)
            rx = (base_r + pleat) * (0.88 if t > 0.5 else 0.94)
            ry = (base_r + pleat) * (1.14 if t > 0.6 else 1.04)
            v = bm.verts.new((math.cos(ang) * rx, math.sin(ang) * ry, z + tatter_z))
            verts_row.append(v)
        ring_verts.append(verts_row)

    for r_i in range(rings - 1):
        for s_i in range(segs):
            n_i = (s_i + 1) % segs
            bm.faces.new([ring_verts[r_i][s_i], ring_verts[r_i][n_i], ring_verts[r_i + 1][n_i], ring_verts[r_i + 1][s_i]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    robe_mesh = bpy.data.meshes.new("HollowRobeMesh")
    bm.to_mesh(robe_mesh)
    bm.free()
    robe_obj = bpy.data.objects.new("HollowRobeObj", robe_mesh)
    bpy.context.collection.objects.link(robe_obj)
    add_displace(robe_obj, 0.015, 0.25)
    add_subdiv(robe_obj, 1)
    parts.append((robe_obj, 0))

    # Layered weathered diagonal leather sash & torso straps (Hollow Magus chest harness!)
    for idx in range(4):
        z_b = 0.78 + idx * 0.055
        sash = create_tapered_curve(
            f"Belt_{idx}",
            [
                ((math.cos(a) * 0.26, math.sin(a) * 0.31, z_b + 0.03 * math.sin(a + idx)), 0.75)
                for a in [k * (2.0 * math.pi / 12.0) for k in range(12)]
            ],
            bevel_depth=0.022,
            cyclic=True,
        )
        parts.append((sash, 0 if idx % 2 == 0 else 1))

    # Segmented Curved Steel Shoulder Pauldrons (continuous Nurbs lames layered over shoulders!)
    lame_count = 4 if variant in ("hollow_magus", "lich_king") else 3
    for sy in (-1, 1):
        for l in range(lame_count):
            pz = 1.43 - l * 0.055
            py = sy * (0.29 + l * 0.048)
            pcurve = create_tapered_curve(
                f"Pauldron_{sy}_{l}",
                [
                    ((-0.18, py - sy * 0.04, pz - 0.02), 0.45),
                    ((-0.02, py + sy * 0.09, pz + 0.05), 1.0),
                    ((0.16, py - sy * 0.02, pz - 0.03), 0.45),
                ],
                bevel_depth=0.055 - l * 0.005,
                res_u=14,
            )
            add_displace(pcurve, 0.007, 0.16)
            parts.append((pcurve, 1))

            if variant == "lich_king" and l == 0:
                sp = create_tapered_curve(
                    f"LichSpike_{sy}",
                    [((0.0, py, pz + 0.04), 0.9), ((0.0, py + sy * 0.12, pz + 0.28), 0.06)],
                    bevel_depth=0.035,
                )
                parts.append((sp, 1))

    # Draped gothic collar cowl scarf around neck
    collar = create_tapered_curve(
        "CollarDraping",
        [
            ((math.cos(a) * 0.23, math.sin(a) * 0.25, 1.38 + 0.04 * math.cos(a * 2)), 0.85 + 0.25 * math.sin(a * 3))
            for a in [k * (2.0 * math.pi / 14.0) for k in range(14)]
        ],
        bevel_depth=0.068,
        cyclic=True,
    )
    add_displace(collar, 0.022, 0.2)
    parts.append((collar, 0))

    # Twin glowing chest rune clasps (signature Hollow Magus dual turquoise medallions!)
    for idx, (mx, my, mz) in enumerate([(0.24, -0.11, 1.32), (0.27, 0.03, 1.24)]):
        bpy.ops.mesh.primitive_torus_add(major_radius=0.058, minor_radius=0.015, major_segments=24, minor_segments=10, location=(mx, my, mz))
        ring = bpy.context.active_object
        ring.rotation_euler = (0, math.pi * 0.44, idx * 0.18)
        parts.append((ring, 1))
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.042, location=(mx, my, mz))
        gem = bpy.context.active_object
        gem.scale = (0.5, 1.0, 1.0)
        parts.append((gem, 2))

    # Sculpted curved arms & skeletal gauntleted hands holding Staff (right +Y) & Book (left -Y)
    for sy in (-1, 1):
        arm = create_tapered_curve(
            f"Arm_{sy}",
            [
                ((0.0, sy * 0.28, 1.35), 1.0),
                ((0.14, sy * 0.40, 1.12), 0.82),
                ((0.36, sy * 0.42, 1.04 if sy < 0 else 1.18), 0.62),
            ],
            bevel_depth=0.065,
        )
        add_displace(arm, 0.012, 0.18)
        parts.append((arm, 0))
        # Armored gauntlet cuff + fingers
        hand_z = 1.04 if sy < 0 else 1.18
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.062, location=(0.39, sy * 0.42, hand_z))
        parts.append((bpy.context.active_object, 1))

    # Detailed anatomical Skull head beneath headgear slot (centered at Z=1.56)
    build_anatomical_skull(parts, origin=(0.02, 0.0, 1.56), scale=1.05, mat_slot=1, eye_slot=2)


def export_hero_bodies():
    specs = [
        ("SM_ArchmageRobed.fbx", "hollow_magus"),
        ("SM_Hero_Necromancer.fbx", "necromancer"),
        ("SM_Hero_LichKing.fbx", "lich_king"),
        ("SM_Hero_Chronomancer.fbx", "chronomancer"),
    ]
    for fname, var in specs:
        clear_scene()
        parts = []
        build_hollow_magus_body(parts, var)
        merged = join_and_prepare(parts, origin_bottom=True, normalize_height=1.82, max_tris=24000)
        export_fbx(merged, fname)


def export_headgear_cosmetics():
    # Centered around (0,0,0) = skull center at Cloak Z=1.56
    # 0: Abyss Hood (continuous draped grimdark cowl framing the skull!)
    clear_scene()
    parts = []
    bm = bmesh.new()
    rings = 16
    segs = 28
    rows = []
    for ri in range(rings):
        t = ri / float(rings - 1)  # 0 (front rim around face) -> 1 (tapered back tail)
        x = 0.11 - t * 0.42
        rad = (0.165 + 0.045 * math.sin(t * math.pi)) * (1.0 - 0.72 * (t ** 1.8))
        z_off = 0.02 + 0.06 * math.sin(t * math.pi) - 0.12 * (t ** 2)
        row = []
        for si in range(segs):
            a = (2.0 * math.pi * si) / segs
            v = bm.verts.new((x - 0.03 * math.cos(a), math.sin(a) * rad * 0.92, z_off + math.cos(a) * rad * 1.18))
            row.append(v)
        rows.append(row)
    for ri in range(rings - 1):
        for si in range(segs):
            ni = (si + 1) % segs
            bm.faces.new([rows[ri][si], rows[ri][ni], rows[ri + 1][ni], rows[ri + 1][si]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    m = bpy.data.meshes.new("HoodMesh")
    bm.to_mesh(m)
    bm.free()
    hood = bpy.data.objects.new("HoodObj", m)
    bpy.context.collection.objects.link(hood)
    add_displace(hood, 0.015, 0.2)
    add_subdiv(hood, 1)
    parts.append((hood, 0))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=12000), "SM_Head_AbyssHood.fbx")

    # 1: Bone Crown (continuous gothic band + curved bone spires)
    clear_scene()
    parts = []
    band = create_tapered_curve(
        "CrownBand",
        [((math.cos(a) * 0.138, math.sin(a) * 0.122, 0.075), 1.0) for a in [k * (2.0 * math.pi / 12.0) for k in range(12)]],
        bevel_depth=0.022,
        cyclic=True,
    )
    parts.append((band, 1))
    for i in range(7):
        a = (2.0 * math.pi * i) / 7.0
        h = 0.20 if i == 0 else 0.13
        sp = create_tapered_curve(
            f"CrownSpire_{i}",
            [((math.cos(a) * 0.135, math.sin(a) * 0.12, 0.08), 0.95), ((math.cos(a) * 0.165, math.sin(a) * 0.145, 0.08 + h), 0.06)],
            bevel_depth=0.024,
        )
        parts.append((sp, 1 if i > 0 else 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=10000), "SM_Head_BoneCrown.fbx")

    # 2: Continuous Swept Demonic Horror Horns + Helm Crest
    clear_scene()
    parts = []
    for sy in (-1, 1):
        horn = create_tapered_curve(
            f"DemonHorn_{sy}",
            [
                ((0.02, sy * 0.11, 0.06), 1.0),
                ((-0.02, sy * 0.24, 0.18), 0.78),
                ((0.06, sy * 0.31, 0.34), 0.45),
                ((0.18, sy * 0.22, 0.48), 0.06),
            ],
            bevel_depth=0.048,
            res_u=20,
        )
        add_displace(horn, 0.008, 0.12)
        parts.append((horn, 1))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=12000), "SM_Head_HorrorHorns.fbx")

    # 3: Chrono Iron Halo
    clear_scene()
    parts = []
    halo = create_tapered_curve(
        "HaloArc",
        [((-0.09, math.sin(a) * 0.28, 0.14 + math.cos(a) * 0.28), 1.0) for a in [k * (2.0 * math.pi / 16.0) for k in range(16)]],
        bevel_depth=0.018,
        cyclic=True,
    )
    parts.append((halo, 1))
    for i in range(6):
        a = (2.0 * math.pi * i) / 6.0
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.028, location=(-0.09, math.sin(a) * 0.28, 0.14 + math.cos(a) * 0.28))
        parts.append((bpy.context.active_object, 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=10000), "SM_Head_ChronoHalo.fbx")


def export_staff_cosmetics():
    def make_root_shaft(parts_list):
        shaft = create_tapered_curve(
            "GnarledShaft",
            [
                ((-0.02, 0.0, -0.78), 0.65),
                ((0.01, -0.02, -0.15), 0.82),
                ((0.0, 0.0, 0.35), 0.85),
                ((-0.02, 0.03, 0.88), 0.95),
                ((0.02, 0.0, 1.24), 1.15),
            ],
            bevel_depth=0.038,
            res_u=24,
        )
        add_displace(shaft, 0.010, 0.15)
        parts_list.append((shaft, 0))

    # 0: SM_GnarledStaff — Hollow Magus Continuous Curved Reaper Scythe-Staff!
    clear_scene()
    parts = []
    make_root_shaft(parts)
    blade = create_tapered_curve(
        "ReaperHook",
        [
            ((0.02, 0.0, 1.22), 1.0),
            ((-0.06, 0.0, 1.48), 1.15),
            ((0.18, 0.0, 1.62), 0.92),
            ((0.40, 0.0, 1.46), 0.45),
            ((0.36, 0.0, 1.26), 0.06),
        ],
        bevel_depth=0.052,
        res_u=24,
    )
    add_displace(blade, 0.008, 0.14)
    parts.append((blade, 1))
    # Jagged under-barbs
    for bx, bz in [(0.14, 1.54), (0.28, 1.48), (0.35, 1.36)]:
        barb = create_tapered_curve(
            "ScytheBarb",
            [((bx, 0, bz), 0.9), ((bx - 0.03, 0, bz - 0.10), 0.06)],
            bevel_depth=0.018,
        )
        parts.append((barb, 1))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=14000), "SM_GnarledStaff.fbx")

    # 1: Corrupted Horned Skull Staff
    clear_scene()
    parts = []
    make_root_shaft(parts)
    build_anatomical_skull(parts, origin=(0.04, 0.0, 1.40), scale=1.25, mat_slot=1, eye_slot=2)
    for sy in (-1, 1):
        hr = create_tapered_curve(
            f"StaffHorn_{sy}",
            [((0.0, sy * 0.10, 1.48), 1.0), ((0.06, sy * 0.24, 1.68), 0.10)],
            bevel_depth=0.032,
        )
        parts.append((hr, 1))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=14000), "SM_Staff_SkullCorrupted.fbx")

    # 2: Chrono Crystal Scepter
    clear_scene()
    parts = []
    make_root_shaft(parts)
    cage = create_tapered_curve(
        "ScepterCage",
        [((0.0, math.sin(a) * 0.15, 1.46 + math.cos(a) * 0.16), 1.0) for a in [k * (2.0 * math.pi / 12.0) for k in range(12)]],
        bevel_depth=0.018,
        cyclic=True,
    )
    parts.append((cage, 1))
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=0.075, location=(0, 0, 1.46))
    gem = bpy.context.active_object
    gem.scale = (0.7, 0.7, 1.85)
    parts.append((gem, 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=12000), "SM_Staff_CrystalScepter.fbx")

    # 3: Ornate War Mace Battle-Staff
    clear_scene()
    parts = []
    make_root_shaft(parts)
    for i in range(6):
        a = (2.0 * math.pi * i) / 6.0
        flange = create_tapered_curve(
            f"Flange_{i}",
            [
                ((math.cos(a) * 0.04, math.sin(a) * 0.04, 1.24), 0.6),
                ((math.cos(a) * 0.16, math.sin(a) * 0.16, 1.44), 1.0),
                ((math.cos(a) * 0.05, math.sin(a) * 0.05, 1.62), 0.4),
            ],
            bevel_depth=0.024,
        )
        parts.append((flange, 1))
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.07, location=(0, 0, 1.44))
    parts.append((bpy.context.active_object, 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=12000), "SM_Staff_WarMace.fbx")


def export_familiar_and_back_cosmetics():
    # Familiar 0: Floating Spectral Skull wreathed in curved turquoise spirit flames (Hollow Magus left shoulder!)
    clear_scene()
    parts = []
    build_anatomical_skull(parts, origin=(0.0, 0.0, 0.0), scale=1.18, mat_slot=2, eye_slot=2)
    for i in range(5):
        a = (2.0 * math.pi * i) / 5.0
        wisp = create_tapered_curve(
            f"SpiritWisp_{i}",
            [
                ((math.cos(a) * 0.09, math.sin(a) * 0.09, 0.08), 1.0),
                ((math.cos(a + 0.5) * 0.14, math.sin(a + 0.5) * 0.14, 0.24), 0.55),
                ((math.cos(a + 1.0) * 0.06, math.sin(a + 1.0) * 0.06, 0.42), 0.05),
            ],
            bevel_depth=0.028,
        )
        parts.append((wisp, 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=12000), "SM_Familiar_SpectralSkull.fbx")

    # Familiar 1: Detailed Open Leather Grimoire (cambered pages!)
    clear_scene()
    parts = []
    for side in (-1, 1):
        bpy.ops.mesh.primitive_cube_add(size=0.34, location=(0.0, side * 0.15, 0.0))
        cov = bpy.context.active_object
        cov.scale = (1.28, 0.85, 0.05)
        cov.rotation_euler = (side * 0.36, 0, 0)
        parts.append((cov, 0))
        bpy.ops.mesh.primitive_cube_add(size=0.30, location=(0.0, side * 0.14, 0.024))
        pg = bpy.context.active_object
        pg.scale = (1.20, 0.80, 0.10)
        pg.rotation_euler = (side * 0.30, 0, 0)
        add_subdiv(pg, 1)
        add_displace(pg, 0.008, 0.15)
        parts.append((pg, 1))
    bpy.ops.mesh.primitive_cylinder_add(vertices=12, radius=0.028, depth=0.44, location=(0.0, 0.0, -0.02))
    sp = bpy.context.active_object
    sp.rotation_euler = (0, math.pi * 0.5, 0)
    parts.append((sp, 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=8000), "SM_Familiar_OpenGrimoire.fbx")

    # Familiar 2: Astrolabe
    clear_scene()
    parts = []
    for r, rot in [(0.19, (0.3, 0.4, 0)), (0.14, (-0.5, 0.2, 0.8)), (0.09, (0.8, -0.3, 0.4))]:
        bpy.ops.mesh.primitive_torus_add(major_radius=r, minor_radius=0.014, major_segments=28, minor_segments=10)
        ring = bpy.context.active_object
        ring.rotation_euler = rot
        parts.append((ring, 1))
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.045)
    parts.append((bpy.context.active_object, 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=8000), "SM_Familiar_Astrolabe.fbx")

    # Back 0: Flowing Tattered Cape (continuous cambered drape!)
    clear_scene()
    parts = []
    bm = bmesh.new()
    cols, rows = 14, 18
    grid = []
    for r in range(rows):
        vt = r / float(rows - 1)  # 0 (shoulders) -> 1 (ragged trailing bottom)
        row = []
        w = 0.28 + vt * 0.32
        x = -0.04 - vt * 0.42 - 0.05 * math.sin(vt * math.pi)
        z = 0.02 - vt * 1.18
        for c in range(cols):
            ut = (c / float(cols - 1)) * 2.0 - 1.0
            ripple = 0.045 * math.sin(ut * 4.5 + vt * 2.5) * vt
            tatter = 0.07 * math.sin(ut * 9.0) * (vt ** 2)
            row.append(bm.verts.new((x + ripple, ut * w, z + tatter)))
        grid.append(row)
    for r in range(rows - 1):
        for c in range(cols - 1):
            bm.faces.new([grid[r][c], grid[r][c + 1], grid[r + 1][c + 1], grid[r + 1][c]])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    cm = bpy.data.meshes.new("CapeMesh")
    bm.to_mesh(cm)
    bm.free()
    cobj = bpy.data.objects.new("CapeObj", cm)
    bpy.context.collection.objects.link(cobj)
    sol = cobj.modifiers.new("Solidify", "SOLIDIFY")
    sol.thickness = 0.018
    add_subdiv(cobj, 1)
    parts.append((cobj, 0))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=12000), "SM_Back_TatteredCape.fbx")

    # Back 1: Sculpted Demon Bat Wings (curved struts + continuous membrane fan!)
    clear_scene()
    parts = []
    for sy in (-1, 1):
        # Main wing spar
        spar = create_tapered_curve(
            f"WingSpar_{sy}",
            [
                ((0.0, sy * 0.08, 0.0), 1.1),
                ((-0.14, sy * 0.48, 0.38), 0.9),
                ((-0.24, sy * 1.05, 0.56), 0.45),
                ((-0.18, sy * 1.32, 0.12), 0.12),
            ],
            bevel_depth=0.038,
        )
        parts.append((spar, 1))
        # Continuous BMesh wing web fan with scalloped bottom edges
        bm = bmesh.new()
        roots = [(-0.04, sy * 0.12, -0.05), (-0.16, sy * 0.50, 0.34), (-0.24, sy * 1.02, 0.52)]
        tips = [(-0.18, sy * 0.38, -0.58), (-0.28, sy * 0.78, -0.52), (-0.30, sy * 1.24, -0.22)]
        r_verts = [bm.verts.new(p) for p in roots]
        t_verts = [bm.verts.new(p) for p in tips]
        for k in range(2):
            bm.faces.new([r_verts[k], r_verts[k + 1], t_verts[k + 1], t_verts[k]])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        wm = bpy.data.meshes.new(f"WingWeb_{sy}")
        bm.to_mesh(wm)
        bm.free()
        wobj = bpy.data.objects.new(f"WingWeb_{sy}", wm)
        bpy.context.collection.objects.link(wobj)
        sol = wobj.modifiers.new("Solidify", "SOLIDIFY")
        sol.thickness = 0.015
        add_subdiv(wobj, 2)
        parts.append((wobj, 0))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=12000), "SM_Back_DemonWings.fbx")

    # Back 2: Archon Halo Shards
    clear_scene()
    parts = []
    for i in range(7):
        a = -math.pi * 0.46 + (math.pi * 0.92 * i / 6.0)
        sh = create_tapered_curve(
            f"ArchonShard_{i}",
            [((-0.16, math.sin(a) * 0.46, 0.05 + math.cos(a) * 0.42), 1.0), ((-0.26, math.sin(a) * 0.82, 0.24 + math.cos(a) * 0.74), 0.08)],
            bevel_depth=0.042,
        )
        parts.append((sh, 1 if i % 2 == 0 else 2))
    export_fbx(join_and_prepare(parts, origin_bottom=False, max_tris=8000), "SM_Back_ArchonHalo.fbx")


# ---------------------------------------------------------------------------
# 3. High-Detail Grimdark Monsters & Bosses (with 3 Material Slots!)
# ---------------------------------------------------------------------------
def export_enemies():
    # 1) SM_VoidFiend: Bone Knight / Horned Revenant
    clear_scene()
    parts = []
    build_hollow_magus_body(parts, "lich_king")
    export_fbx(join_and_prepare(parts, origin_bottom=True, normalize_height=1.78, max_tris=14000), "SM_VoidFiend.fbx")

    # 2) SM_StoneColossus: Volcanic Magma Golem
    clear_scene()
    parts = []
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, radius=0.68, location=(0, 0, 1.35))
    chest = bpy.context.active_object
    chest.scale = (0.95, 1.25, 1.15)
    add_displace(chest, 0.12, 0.32)
    parts.append((chest, 0))
    bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=2, radius=0.55, location=(0.05, 0, 1.35))
    parts.append((bpy.context.active_object, 2))
    for sy in (-1, 1):
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=0.38, location=(0.08, sy * 0.95, 1.65))
        sh = bpy.context.active_object
        add_displace(sh, 0.09, 0.28)
        parts.append((sh, 0))
        bpy.ops.mesh.primitive_cube_add(size=0.52, location=(0.25, sy * 1.02, 0.85))
        fist = bpy.context.active_object
        fist.scale = (1.1, 0.95, 1.65)
        add_subdiv(fist, 2)
        add_displace(fist, 0.08, 0.30)
        parts.append((fist, 0))
    build_anatomical_skull(parts, origin=(0.24, 0.0, 1.98), scale=1.65, mat_slot=1, eye_slot=2)
    export_fbx(join_and_prepare(parts, origin_bottom=True, normalize_height=2.35, max_tris=15000), "SM_StoneColossus.fbx")

    # 3) SM_ArchonBoss: Demonic Horned Horror Knight (massive curved horns + winged armor!)
    clear_scene()
    parts = []
    build_hollow_magus_body(parts, "lich_king")
    for sy in (-1, 1):
        horn = create_tapered_curve(
            f"BossHorn_{sy}",
            [
                ((0.02, sy * 0.14, 1.64), 1.0),
                ((-0.04, sy * 0.32, 1.84), 0.78),
                ((0.12, sy * 0.42, 2.08), 0.45),
                ((0.26, sy * 0.28, 2.26), 0.06),
            ],
            bevel_depth=0.065,
            res_u=22,
        )
        add_displace(horn, 0.012, 0.14)
        parts.append((horn, 1))
        spar = create_tapered_curve(
            f"BossWingSpar_{sy}",
            [
                ((-0.14, sy * 0.16, 1.40), 1.1),
                ((-0.30, sy * 0.78, 1.92), 0.85),
                ((-0.44, sy * 1.55, 2.22), 0.45),
                ((-0.32, sy * 1.88, 1.42), 0.12),
            ],
            bevel_depth=0.055,
        )
        parts.append((spar, 1))
        bm = bmesh.new()
        roots = [(-0.18, sy * 0.18, 1.35), (-0.32, sy * 0.78, 1.88), (-0.44, sy * 1.52, 2.15)]
        tips = [(-0.32, sy * 0.55, 0.55), (-0.46, sy * 1.12, 0.68), (-0.48, sy * 1.72, 1.05)]
        rv = [bm.verts.new(p) for p in roots]
        tv = [bm.verts.new(p) for p in tips]
        for k in range(2):
            bm.faces.new([rv[k], rv[k + 1], tv[k + 1], tv[k]])
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
        wm = bpy.data.meshes.new(f"BossWingWeb_{sy}")
        bm.to_mesh(wm)
        bm.free()
        wo = bpy.data.objects.new(f"BossWingWeb_{sy}", wm)
        bpy.context.collection.objects.link(wo)
        sol = wo.modifiers.new("Solidify", "SOLIDIFY")
        sol.thickness = 0.022
        add_subdiv(wo, 2)
        parts.append((wo, 0))
    export_fbx(join_and_prepare(parts, origin_bottom=True, normalize_height=2.95, max_tris=28000), "SM_ArchonBoss.fbx")


def find_base_color_image(mesh_obj):
    def trace_base_color(socket, depth=0):
        if not socket or depth > 3:
            return None
        for link in socket.links:
            node = link.from_node
            if node.type == "TEX_IMAGE" and getattr(node, "image", None) and node.image.size[0] > 0:
                return node.image
            for inp in getattr(node, "inputs", []):
                if inp.is_linked:
                    found = trace_base_color(inp, depth + 1)
                    if found:
                        return found
        return None

    mats = list(mesh_obj.data.materials) if mesh_obj and mesh_obj.data else []
    for mat in mats:
        if not mat or not mat.use_nodes or not mat.node_tree:
            continue
        for node in mat.node_tree.nodes:
            if node.type == "BSDF_PRINCIPLED":
                img = trace_base_color(node.inputs.get("Base Color"))
                if img:
                    return img

    skip_tokens = ("normal", "nrm", "_nor", "rough", "metal", "orm", "occlusion", "_ao", "height", "bump")
    for mat in mats:
        if not mat or not mat.use_nodes or not mat.node_tree:
            continue
        for node in mat.node_tree.nodes:
            if node.type == "TEX_IMAGE" and getattr(node, "image", None) and node.image.size[0] > 0:
                nm = node.image.name.lower()
                cs = node.image.colorspace_settings.name.lower()
                if ("srgb" in cs) and not any(tok in nm for tok in skip_tokens):
                    return node.image

    for img in bpy.data.images:
        if img.size[0] > 0 and img.size[1] > 0 and not img.name.startswith("Render Result"):
            nm = img.name.lower()
            cs = img.colorspace_settings.name.lower()
            if ("srgb" in cs) and not any(tok in nm for tok in skip_tokens):
                return img

    for img in bpy.data.images:
        if img.size[0] > 0 and img.size[1] > 0 and not img.name.startswith("Render Result"):
            if not any(tok in img.name.lower() for tok in skip_tokens):
                return img
    return None


def _has_backdrop_ancestor(obj) -> bool:
    tokens = ("fondo", "backdrop", "background", "turntable", "pedestal", "floor_plane")
    cur = obj
    while cur is not None:
        nm = cur.name.lower()
        if any(t in nm for t in tokens):
            return True
        cur = cur.parent
    return False


def process_sketchfab_slots(manifest: dict):
    sketchfab_slots = manifest.get("sketchfab", [])
    if not sketchfab_slots:
        return

    CACHE.mkdir(parents=True, exist_ok=True)
    cache_file = CACHE / "sketchfab_cache.json"
    cache_data = {}
    if cache_file.exists():
        try:
            cache_data = json.loads(cache_file.read_text(encoding="utf-8"))
        except Exception:
            cache_data = {}

    for entry in sketchfab_slots:
        if entry.get("is_polyhaven"):
            continue
        slot_name = entry["slot_name"]
        src_path_str = entry.get("source_path")
        target_fbx = Path(entry["target_fbx"])
        target_h = entry.get("target_h", 1.85)
        max_tris = entry.get("max_tris", 24000)
        tex_res = entry.get("tex_res", 1024)
        duplicates = [Path(p) for p in entry.get("duplicates", [])]

        if not src_path_str:
            continue
        src_path = Path(src_path_str)
        if not src_path.exists():
            continue

        try:
            st = src_path.stat()
            src_mtime = st.st_mtime
            src_size = st.st_size
        except Exception:
            continue

        # Check cache: if fbx exists and cache matches -> skip conversion
        cached_info = cache_data.get(slot_name, {})
        if (
            target_fbx.exists()
            and target_fbx.stat().st_size > 1000
            and cached_info.get("src_path") == str(src_path)
            and cached_info.get("fbx_size") == target_fbx.stat().st_size
            and cached_info.get("src_mtime") == src_mtime
            and cached_info.get("src_size") == src_size
            and cached_info.get("target_h") == target_h
            and cached_info.get("max_tris") == max_tris
        ):
            fbx_bytes = target_fbx.read_bytes()
            for dup_p in duplicates:
                if not dup_p.exists() or dup_p.stat().st_size != len(fbx_bytes):
                    try:
                        dup_p.write_bytes(fbx_bytes)
                    except Exception:
                        pass
            continue

        print(f"Converting Sketchfab {src_path.name} -> {slot_name}.fbx ...")
        clear_scene()

        # Import glTF or FBX
        try:
            if src_path.suffix.lower() in (".glb", ".gltf"):
                bpy.ops.import_scene.gltf(filepath=str(src_path))
            else:
                bpy.ops.import_scene.fbx(filepath=str(src_path))
        except Exception as e:
            print(f"Warning: Failed to import {src_path}: {e}")
            continue

        raw_meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        if not raw_meshes:
            print(f"Warning: No mesh objects found in {src_path}")
            continue

        # Strip studio backdrop planes by mesh or parent hierarchy name
        non_backdrop = [o for o in raw_meshes if not _has_backdrop_ancestor(o)]
        if non_backdrop and len(non_backdrop) < len(raw_meshes):
            for o in raw_meshes:
                if o not in non_backdrop:
                    bpy.data.objects.remove(o, do_unlink=True)
            raw_meshes = non_backdrop

        # Bake hierarchical parent transforms into meshes before removing non-mesh empties
        bpy.ops.object.select_all(action="DESELECT")
        for m in raw_meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = raw_meshes[0]
        bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")

        # Remove non-mesh objects (empties, cameras, lights)
        for o in list(bpy.context.scene.objects):
            if o.type != "MESH":
                bpy.data.objects.remove(o, do_unlink=True)

        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        if not meshes:
            print(f"Warning: No mesh objects found in {src_path}")
            continue

        # Isolate a single hero staff when importing multi-staff collections (e.g. fantasy_magic_staffs_collection)
        if entry.get("category") == "Staff" and len(meshes) > 1:
            bbox_stats = []
            for m in meshes:
                wb = [m.matrix_world @ Vector(c) for c in m.bound_box]
                xs = [v.x for v in wb]
                ys = [v.y for v in wb]
                zs = [v.z for v in wb]
                z_span = max(zs) - min(zs)
                horiz = max(max(xs) - min(xs), max(ys) - min(ys))
                bbox_stats.append({
                    "obj": m,
                    "min_x": min(xs), "max_x": max(xs),
                    "min_y": min(ys), "max_y": max(ys),
                    "cx": 0.5 * (min(xs) + max(xs)),
                    "cy": 0.5 * (min(ys) + max(ys)),
                    "z_span": z_span,
                    "horiz": horiz,
                })
            # Drop flat planes whose horizontal span exceeds 3x vertical height
            non_flat = [s for s in bbox_stats if s["horiz"] <= 3.0 * max(s["z_span"], 1e-4)]
            if non_flat:
                for s in bbox_stats:
                    if s not in non_flat:
                        bpy.data.objects.remove(s["obj"], do_unlink=True)
                bbox_stats = non_flat

            if len(bbox_stats) > 1:
                ref = max(bbox_stats, key=lambda s: s["z_span"])
                ref_x, ref_y, ref_h = ref["cx"], ref["cy"], max(ref["z_span"], 1e-4)
                overall_x = max(s["max_x"] for s in bbox_stats) - min(s["min_x"] for s in bbox_stats)
                overall_y = max(s["max_y"] for s in bbox_stats) - min(s["min_y"] for s in bbox_stats)
                if max(overall_x, overall_y) > 0.85 * ref_h:
                    kept = [s for s in bbox_stats if math.hypot(s["cx"] - ref_x, s["cy"] - ref_y) <= 0.35 * ref_h]
                    if kept:
                        for s in bbox_stats:
                            if s not in kept:
                                bpy.data.objects.remove(s["obj"], do_unlink=True)
                        meshes = [s["obj"] for s in kept]
                    else:
                        meshes = [s["obj"] for s in bbox_stats]
                else:
                    meshes = [s["obj"] for s in bbox_stats]
            else:
                meshes = [s["obj"] for s in bbox_stats]

        bpy.ops.object.select_all(action="DESELECT")
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1:
            bpy.ops.object.join()
        obj = bpy.context.view_layer.objects.active
        bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)

        # Rotate +90 degrees around Z (Sketchfab glTF looks along -Y, UE5 expects +X)
        obj.rotation_euler.z = math.radians(90.0)
        bpy.ops.object.transform_apply(location=False, rotation=True, scale=False)

        # Center X/Y to origin, set bottom to Z=0, scale to target height
        bbox = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
        min_x = min(v.x for v in bbox)
        max_x = max(v.x for v in bbox)
        min_y = min(v.y for v in bbox)
        max_y = max(v.y for v in bbox)
        min_z = min(v.z for v in bbox)
        max_z = max(v.z for v in bbox)

        cx = 0.5 * (min_x + max_x)
        cy = 0.5 * (min_y + max_y)
        cz = min_z

        for v in obj.data.vertices:
            v.co.x -= cx
            v.co.y -= cy
            v.co.z -= cz

        cur_h = max(0.001, max_z - min_z)
        if target_h:
            sc = target_h / cur_h
            for v in obj.data.vertices:
                v.co *= sc

        # CRITICAL: Stitch chunk seams with remove_doubles
        bm = bmesh.new()
        bm.from_mesh(obj.data)
        bmesh.ops.remove_doubles(bm, verts=bm.verts[:], dist=0.0001)
        bm.to_mesh(obj.data)
        bm.free()
        obj.data.update()

        # Decimate object to budget
        decimate_object(obj, max_tris)
        for p in obj.data.polygons:
            p.use_smooth = True

        if not obj.data.uv_layers:
            obj.data.uv_layers.new(name="UVMap")
            try:
                bpy.ops.object.mode_set(mode="EDIT")
                bpy.ops.mesh.select_all(action="SELECT")
                bpy.ops.uv.smart_project(angle_limit=1.15, island_margin=0.015)
                bpy.ops.object.mode_set(mode="OBJECT")
            except Exception:
                if bpy.context.mode != "OBJECT":
                    bpy.ops.object.mode_set(mode="OBJECT")

        export_fbx(obj, f"{slot_name}.fbx")

        # Duplicate FBX to legacy / alias destinations
        for dup_p in duplicates:
            try:
                dup_p.write_bytes(target_fbx.read_bytes())
                print(f"Duplicated {slot_name}.fbx to {dup_p.name}")
            except Exception as e:
                print(f"Warning: Failed to duplicate {slot_name}: {e}")

        # Extract authentic sRGB Base Color (Albedo) texture and invalidate stale derived maps
        tex_prefix = f"T_{slot_name.replace('SM_', '')}"
        TEX_DIR.mkdir(parents=True, exist_ok=True)
        albedo_target = TEX_DIR / f"{tex_prefix}_Albedo.png"
        normal_target = TEX_DIR / f"{tex_prefix}_Normal.png"
        rough_target = TEX_DIR / f"{tex_prefix}_Roughness.png"
        base_img = find_base_color_image(obj)
        if base_img:
            try:
                for old_p in (albedo_target, normal_target, rough_target):
                    if old_p.exists():
                        old_p.unlink()
                base_img.filepath_raw = str(albedo_target)
                base_img.file_format = "PNG"
                base_img.save()
            except Exception as e:
                print(f"Warning: Failed to extract Base Color image for {slot_name}: {e}")

        # Update cache with src_path and fbx_size
        cache_data[slot_name] = {
            "src_path": str(src_path),
            "fbx_size": target_fbx.stat().st_size if target_fbx.exists() else 0,
            "src_mtime": src_mtime,
            "src_size": src_size,
            "target_h": target_h,
            "max_tris": max_tris,
        }
        try:
            cache_file.write_text(json.dumps(cache_data, indent=2), encoding="utf-8")
        except Exception:
            pass


def process_external_inbox(manifest: dict):
    for fpath_str in manifest.get("external_inbox", []):
        p = Path(fpath_str)
        if not p.exists():
            continue
        clear_scene()
        if p.suffix.lower() in (".glb", ".gltf"):
            bpy.ops.import_scene.gltf(filepath=str(p))
        else:
            bpy.ops.import_scene.fbx(filepath=str(p))
        meshes = [o for o in bpy.context.scene.objects if o.type == "MESH"]
        if not meshes:
            continue
        bpy.ops.object.select_all(action="DESELECT")
        for m in meshes:
            m.select_set(True)
        bpy.context.view_layer.objects.active = meshes[0]
        if len(meshes) > 1:
            bpy.ops.object.join()
        merged = bpy.context.view_layer.objects.active
        decimate_object(merged, 28000)
        export_fbx(merged, f"{p.stem}.fbx")


def render_lookdev_preview():
    clear_scene()
    scene = bpy.context.scene
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "CYCLES"):
        try:
            scene.render.engine = eng
            break
        except Exception:
            continue

    scene.render.resolution_x = 1920
    scene.render.resolution_y = 1080
    scene.render.filepath = str(RAW / "LookDev_HollowMagus_3070.png")

    world = bpy.data.worlds.new("NightWorld")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.015, 0.020, 0.032, 1.0)
        bg.inputs["Strength"].default_value = 0.65

    bpy.ops.mesh.primitive_plane_add(size=26.0, location=(0, 0, 0))
    fl = bpy.context.active_object
    fl.data.materials.append(ensure_material("FloorMat", (0.045, 0.050, 0.068, 1.0), 0.15, 0.52))

    def load_fbx(name, loc, rot_deg=(0, 0, 0), tex_name=None):
        fp = MESH_DIR / name
        if not fp.exists():
            return []
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(fp))
        new_objs = [o for o in (set(bpy.data.objects) - before) if o.type == "MESH"]
        for o in new_objs:
            o.location = Vector(loc)
            o.rotation_euler = Euler(tuple(math.radians(a) for a in rot_deg))

            # Bind texture if available in Textures directory
            tex_file = None
            if tex_name:
                cand = TEX_DIR / f"{tex_name}_Albedo.png"
                if cand.exists():
                    tex_file = cand
            else:
                base_slot = name.replace(".fbx", "").replace("SM_", "")
                cand = TEX_DIR / f"T_{base_slot}_Albedo.png"
                if cand.exists():
                    tex_file = cand

            if tex_file and tex_file.exists():
                mat = bpy.data.materials.new(name=f"Mat_{o.name}")
                mat.use_nodes = True
                bsdf = mat.node_tree.nodes.get("Principled BSDF")
                if bsdf:
                    img_node = mat.node_tree.nodes.new("ShaderNodeTexImage")
                    try:
                        img = bpy.data.images.load(str(tex_file))
                        img_node.image = img
                        mat.node_tree.links.new(img_node.outputs["Color"], bsdf.inputs["Base Color"])
                        bsdf.inputs["Roughness"].default_value = 0.55
                    except Exception:
                        pass
                o.data.materials.clear()
                o.data.materials.append(mat)
        return new_objs

    # Left: Clean Hollow Magus (Hero 00) - No overlapping hood/cape/staff
    hero_mesh = "SM_Hero_00.fbx" if (MESH_DIR / "SM_Hero_00.fbx").exists() else "SM_ArchmageRobed.fbx"
    load_fbx(hero_mesh, (-1.35, 0.0, 0.0), (0, 0, 75), tex_name="T_Hero_00")
    # Floating Grimoire companion slightly above floor
    load_fbx("SM_Familiar_OpenGrimoire.fbx", (-0.82, 0.45, 1.05), (0, 0, 55))
    load_fbx("SM_Familiar_SpectralSkull.fbx", (-0.72, 0.15, 1.62), (0, 0, 90))

    # Center Background: Scanned CC0 Photogrammetry Gothic Statue + Treasure Chest
    load_fbx("SM_GothicStatue.fbx", (0.0, -2.60, 0.0), (0, 0, 0), tex_name="T_GothicStatue")
    load_fbx("SM_TreasureChest.fbx", (0.0, -0.40, 0.0), (0, 0, 180), tex_name="T_TreasureChest")

    # Right: Demonic Horned Horror Knight Boss (Loc09) + Bruiser 00
    boss_mesh = "SM_Boss_Loc09.fbx" if (MESH_DIR / "SM_Boss_Loc09.fbx").exists() else "SM_ArchonBoss.fbx"
    load_fbx(boss_mesh, (1.30, -0.20, 0.0), (0, 0, 108), tex_name="T_Boss_Loc09")

    bruiser_mesh = "SM_Mob_Bruiser_00.fbx" if (MESH_DIR / "SM_Mob_Bruiser_00.fbx").exists() else "SM_StoneColossus.fbx"
    load_fbx(bruiser_mesh, (2.65, -0.80, 0.0), (0, 0, 120), tex_name="T_Mob_Bruiser_00")

    def add_light(kind, name, loc, aim, color, power):
        ld = bpy.data.lights.new(name=name, type=kind)
        ld.color = color
        ld.energy = power
        lo = bpy.data.objects.new(name=name, object_data=ld)
        scene.collection.objects.link(lo)
        lo.location = Vector(loc)
        d = Vector(aim) - lo.location
        lo.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()

    # Front-key + turquoise magic fill + amber bronze rim lighting
    add_light("AREA", "FrontKey", (0.8, 5.5, 4.2), (0.0, -0.5, 1.2), (0.78, 0.86, 1.0), 860.0)
    add_light("AREA", "WarmRim", (-3.8, -3.4, 3.8), (0.0, -0.5, 1.4), (1.0, 0.62, 0.28), 680.0)
    add_light("POINT", "NecroGlow", (-0.72, 0.25, 1.62), (0, 0, 0), (0.08, 0.98, 0.74), 95.0)

    cam_data = bpy.data.cameras.new("Cam")
    cam_data.lens = 36.0
    cam = bpy.data.objects.new("Cam", cam_data)
    scene.collection.objects.link(cam)
    cam.location = Vector((0.0, 6.2, 2.15))
    cam.rotation_euler = (Vector((0.0, -0.4, 1.35)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam

    bpy.ops.render.render(write_still=True)


def main():
    argv = sys.argv
    manifest = {}
    if "--" in argv:
        mp = Path(argv[argv.index("--") + 1])
        if mp.exists():
            manifest = json.loads(mp.read_text(encoding="utf-8"))

    # 1. Procedural fallbacks FIRST (and never overwrite existing real converted assets!)
    global PROCEDURAL_FALLBACK_PASS
    PROCEDURAL_FALLBACK_PASS = True
    export_hero_bodies()
    export_headgear_cosmetics()
    export_staff_cosmetics()
    export_familiar_and_back_cosmetics()
    export_enemies()
    PROCEDURAL_FALLBACK_PASS = False

    # 2. Scanned PolyHaven CC0 photogrammetry models & Citadel Fortress
    process_polyhaven_assets(manifest)

    # 3. Downloaded AAA Sketchfab models (36 slots with remove_doubles & cache)
    process_sketchfab_slots(manifest)

    # 4. External inbox if any
    process_external_inbox(manifest)

    # 5. LookDev preview render
    render_lookdev_preview()


if __name__ == "__main__":
    main()
