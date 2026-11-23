"""
Blender 5.2 Offline 3D AAA Grimoire Tome & Spell Diorama Plate Renderer.
Replaces flat 2D icons with genuine 3D sculpted obsidian/gold dioramas rendered
with procedural volumetric materials, Chiaroscuro lighting, and real 3D meshes
into RawAssets/Grimoire/ (T_GrimoireBookSpread.png + SpellPlate_01..10.png).
"""

import math
import random
from pathlib import Path

import bmesh
import bpy
from mathutils import Euler, Vector

ROOT = Path(r"c:\Users\user\Desktop\WizardGame")
GRIMOIRE_DIR = ROOT / "RawAssets" / "Grimoire"
MESH_DIR = ROOT / "RawAssets" / "Meshes"
TEX_DIR = ROOT / "RawAssets" / "Textures"


def clear_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)


def configure_render(res_x: int, res_y: int, out_path: Path):
    scene = bpy.context.scene
    for eng in ("BLENDER_EEVEE_NEXT", "BLENDER_EEVEE", "CYCLES"):
        try:
            scene.render.engine = eng
            break
        except Exception:
            continue
    scene.render.resolution_x = res_x
    scene.render.resolution_y = res_y
    scene.render.resolution_percentage = 100
    scene.render.filepath = str(out_path)
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"

    # Deep obsidian world
    world = bpy.data.worlds.new("GrimoireWorld")
    scene.world = world
    world.use_nodes = True
    bg = world.node_tree.nodes.get("Background")
    if bg:
        bg.inputs["Color"].default_value = (0.012, 0.012, 0.018, 1.0)
        bg.inputs["Strength"].default_value = 0.55

    # AgX look contrast for rich Chiaroscuro
    try:
        scene.view_settings.view_transform = "AgX"
        scene.view_settings.look = "AgX - Medium High Contrast"
    except Exception:
        pass


def make_pbr_mat(
    name: str,
    base_rgba=(0.15, 0.15, 0.18, 1.0),
    metallic=0.0,
    roughness=0.5,
    emission_rgba=(0.0, 0.0, 0.0, 1.0),
    emission_strength=0.0,
    alpha=1.0,
    noise_scale=0.0,
    noise_bump=0.0,
    fresnel_rim_rgba=None,
    fresnel_strength=0.0,
    voronoi_cracks_rgba=None,
    crack_strength=0.0,
):
    mat = bpy.data.materials.new(name=name)
    mat.use_nodes = True
    nt = mat.node_tree
    nodes = nt.nodes
    links = nt.links

    bsdf = nodes.get("Principled BSDF")
    if not bsdf:
        return mat

    bsdf.inputs["Base Color"].default_value = base_rgba
    bsdf.inputs["Metallic"].default_value = metallic
    bsdf.inputs["Roughness"].default_value = roughness
    if "Alpha" in bsdf.inputs:
        bsdf.inputs["Alpha"].default_value = alpha
    if alpha < 0.999:
        for attr, val in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
            try:
                setattr(mat, attr, val)
            except Exception:
                pass
    if "Emission Color" in bsdf.inputs:
        bsdf.inputs["Emission Color"].default_value = emission_rgba
    if "Emission Strength" in bsdf.inputs:
        bsdf.inputs["Emission Strength"].default_value = emission_strength

    # Optional noise bump & roughness variation
    tex_coord = nodes.new("ShaderNodeTexCoord")
    if noise_scale > 0.0:
        noise = nodes.new("ShaderNodeTexNoise")
        noise.inputs["Scale"].default_value = noise_scale
        noise.inputs["Detail"].default_value = 6.0
        noise.inputs["Roughness"].default_value = 0.65
        links.new(tex_coord.outputs["Object"], noise.inputs["Vector"])

        if noise_bump > 0.0:
            bump = nodes.new("ShaderNodeBump")
            bump.inputs["Strength"].default_value = noise_bump
            bump.inputs["Distance"].default_value = 0.02
            links.new(noise.outputs["Fac"], bump.inputs["Height"])
            links.new(bump.outputs["Normal"], bsdf.inputs["Normal"])

    # Optional Fresnel rim glow & translucent holographic mix
    if fresnel_rim_rgba is not None and fresnel_strength > 0.0:
        lw = nodes.new("ShaderNodeLayerWeight")
        lw.inputs["Blend"].default_value = 0.35
        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.25
        ramp.color_ramp.elements[0].color = (0.0, 0.0, 0.0, 1.0)
        ramp.color_ramp.elements[1].position = 0.85
        ramp.color_ramp.elements[1].color = (1.0, 1.0, 1.0, 1.0)
        links.new(lw.outputs["Facing"], ramp.inputs["Fac"])

        em = nodes.new("ShaderNodeEmission")
        em.inputs["Color"].default_value = fresnel_rim_rgba
        em.inputs["Strength"].default_value = fresnel_strength

        mult = nodes.new("ShaderNodeMath")
        mult.operation = "MULTIPLY"
        links.new(ramp.outputs["Color"], mult.inputs[0])
        mult.inputs[1].default_value = fresnel_strength
        if "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = fresnel_rim_rgba
        if "Emission Strength" in bsdf.inputs:
            links.new(mult.outputs["Value"], bsdf.inputs["Emission Strength"])

    # Optional Voronoi magma/rune fissures
    if voronoi_cracks_rgba is not None and crack_strength > 0.0:
        vor = nodes.new("ShaderNodeTexVoronoi")
        vor.feature = "DISTANCE_TO_EDGE"
        vor.inputs["Scale"].default_value = 6.5
        links.new(tex_coord.outputs["Object"], vor.inputs["Vector"])

        ramp = nodes.new("ShaderNodeValToRGB")
        ramp.color_ramp.elements[0].position = 0.0
        ramp.color_ramp.elements[0].color = (1.0, 1.0, 1.0, 1.0)
        ramp.color_ramp.elements[1].position = 0.09
        ramp.color_ramp.elements[1].color = (0.0, 0.0, 0.0, 1.0)
        links.new(vor.outputs["Distance"], ramp.inputs["Fac"])

        mult = nodes.new("ShaderNodeMath")
        mult.operation = "MULTIPLY"
        links.new(ramp.outputs["Color"], mult.inputs[0])
        mult.inputs[1].default_value = crack_strength
        if "Emission Color" in bsdf.inputs:
            bsdf.inputs["Emission Color"].default_value = voronoi_cracks_rgba
        if "Emission Strength" in bsdf.inputs:
            links.new(mult.outputs["Value"], bsdf.inputs["Emission Strength"])

    return mat


def add_light(kind: str, name: str, loc, aim, color, energy, size=1.2):
    ld = bpy.data.lights.new(name=name, type=kind)
    ld.color = color
    ld.energy = energy
    if kind == "AREA":
        ld.size = size
    elif kind == "POINT":
        ld.shadow_soft_size = size * 0.25
    lo = bpy.data.objects.new(name=name, object_data=ld)
    bpy.context.scene.collection.objects.link(lo)
    lo.location = Vector(loc)
    if aim is not None:
        d = Vector(aim) - lo.location
        if d.length > 0.001:
            lo.rotation_euler = d.to_track_quat("-Z", "Y").to_euler()
    return lo


def add_curve_tube(name: str, points, bevel_depth=0.04, mat=None, cyclic=False):
    cdata = bpy.data.curves.new(name=name, type="CURVE")
    cdata.dimensions = "3D"
    cdata.resolution_u = 20
    cdata.bevel_depth = bevel_depth
    cdata.bevel_resolution = 5
    cdata.use_fill_caps = True
    is_jagged = "Bolt" in name
    spline = cdata.splines.new("POLY" if is_jagged else "NURBS")
    spline.points.add(len(points) - 1)
    for i, p in enumerate(points):
        spline.points[i].co = (p[0], p[1], p[2], 1.0)
    if not is_jagged:
        spline.use_endpoint_u = True
        spline.order_u = min(4, len(points))
    spline.use_cyclic_u = cyclic
    obj = bpy.data.objects.new(name, cdata)
    bpy.context.scene.collection.objects.link(obj)
    if mat:
        obj.data.materials.append(mat)
    return obj


# ---------------------------------------------------------------------------
# 1. Open Grimoire Book Spread 3D Render (T_GrimoireBookSpread.png)
# ---------------------------------------------------------------------------
def render_grimoire_book_spread():
    clear_scene()
    out_path = GRIMOIRE_DIR / "T_GrimoireBookSpread.png"
    configure_render(1440, 920, out_path)

    # Dark carved slate altar underneath
    bpy.ops.mesh.primitive_plane_add(size=12.0, location=(0, 0, -0.18))
    table = bpy.context.active_object
    table.data.materials.append(make_pbr_mat("SlateTable", (0.018, 0.018, 0.024, 1.0), metallic=0.1, roughness=0.72, noise_scale=8.0, noise_bump=0.25))

    # Heavy dark-leather hard cover + brass corner clasps
    leather_mat = make_pbr_mat("AncientLeather", (0.045, 0.026, 0.018, 1.0), metallic=0.0, roughness=0.58, noise_scale=18.0, noise_bump=0.28)
    bronze_mat = make_pbr_mat("AntiqueBronze", (0.72, 0.54, 0.28, 1.0), metallic=0.92, roughness=0.28, noise_scale=24.0, noise_bump=0.12)

    bpy.ops.mesh.primitive_cube_add(size=1.0, location=(0.0, 0.0, -0.06))
    cover = bpy.context.active_object
    cover.scale = (3.44, 2.22, 0.08)
    bpy.ops.object.transform_apply(scale=True)
    bev = cover.modifiers.new("Bevel", "BEVEL")
    bev.width = 0.02
    bev.segments = 3
    cover.data.materials.append(leather_mat)

    # Central narrow binding spine ridge (strictly <= 3.5% width!)
    bpy.ops.mesh.primitive_cylinder_add(vertices=20, radius=0.052, depth=2.24, location=(0, 0, -0.025))
    spine = bpy.context.active_object
    spine.rotation_euler.x = math.radians(90.0)
    spine.data.materials.append(bronze_mat)

    # Forged bronze corner clasps & rivets
    for sx in (-1.0, 1.0):
        for sy in (-1.0, 1.0):
            bpy.ops.mesh.primitive_cube_add(size=1.0, location=(sx * 1.66, sy * 1.05, -0.04))
            clasp = bpy.context.active_object
            clasp.scale = (0.16, 0.16, 0.11)
            clasp.data.materials.append(bronze_mat)
            bpy.ops.mesh.primitive_uv_sphere_add(radius=0.022, location=(sx * 1.65, sy * 1.04, 0.022))
            rivet = bpy.context.active_object
            rivet.data.materials.append(bronze_mat)

    # Curved 3D Vellum Parchment Pages (left & right with narrow clean seam!)
    parch_mat = make_pbr_mat("AgedVellum", (0.76, 0.69, 0.56, 1.0), metallic=0.0, roughness=0.82, noise_scale=14.0, noise_bump=0.06)
    bm = bmesh.new()
    for side in (-1, 1):
        nx, ny = 28, 20
        verts = []
        for iy in range(ny + 1):
            row = []
            v = iy / float(ny)
            y = -1.03 + 2.06 * v
            for ix in range(nx + 1):
                u = ix / float(nx)
                # Keep spine gap very narrow so right & left text stay clean!
                x = side * (0.032 + 1.56 * u)
                # Gentle realistic page arc flattening fast after spine
                arch = math.sin(min(u * 1.35, 1.0) * math.pi) * 0.042 + 0.028 * (1.0 - math.exp(-u * 6.0))
                z = 0.005 + arch
                row.append(bm.verts.new((x, y, z)))
            verts.append(row)
        for iy in range(ny):
            for ix in range(nx):
                v0, v1, v2, v3 = verts[iy][ix], verts[iy][ix + 1], verts[iy + 1][ix + 1], verts[iy + 1][ix]
                bm.faces.new([v0, v1, v2, v3] if side > 0 else [v1, v0, v3, v2])
    pmesh = bpy.data.meshes.new("ParchmentSpread")
    bm.to_mesh(pmesh)
    bm.free()
    pobj = bpy.data.objects.new("ParchmentSpread", pmesh)
    bpy.context.scene.collection.objects.link(pobj)
    for p in pobj.data.polygons:
        p.use_smooth = True
    sol = pobj.modifiers.new("Thick", "SOLIDIFY")
    sol.thickness = 0.045
    pobj.data.materials.append(parch_mat)

    # Thin silk bookmark ribbon
    ribbon_mat = make_pbr_mat("CrimsonSilk", (0.45, 0.05, 0.06, 1.0), metallic=0.0, roughness=0.65)
    add_curve_tube("BookmarkRibbon", [(0.0, -1.04, 0.02), (0.04, -1.16, -0.04), (0.02, -1.28, -0.11)], bevel_depth=0.018, mat=ribbon_mat)

    # Dramatic museum chiaroscuro lighting (warm candle + cool moonlight fill)
    add_light("AREA", "WarmKey", (-1.1, -0.4, 3.2), (0.0, 0.0, 0.0), (1.0, 0.88, 0.72), 460.0, size=2.4)
    add_light("AREA", "CoolFill", (1.4, 0.9, 2.8), (0.0, 0.0, 0.0), (0.68, 0.78, 1.0), 160.0, size=2.2)

    # Orthographic top-down framing tightly filling 1440x920
    cam_d = bpy.data.cameras.new("OrthoCam")
    cam_d.type = "ORTHO"
    cam_d.ortho_scale = 3.55
    cam = bpy.data.objects.new("OrthoCam", cam_d)
    bpy.context.scene.collection.objects.link(cam)
    cam.location = Vector((0.0, 0.0, 4.0))
    cam.rotation_euler = Euler((0.0, 0.0, 0.0))
    bpy.context.scene.camera = cam

    bpy.ops.render.render(write_still=True)
    print(f"Rendered {out_path.name}")


# ---------------------------------------------------------------------------
# 2. Shared Ornate 3D Astrolabe Medallion Frame for SpellPlate_01..10
# ---------------------------------------------------------------------------
def build_spellplate_stage(accent_rgba=(0.08, 0.96, 0.82, 1.0)):
    clear_scene()
    slate_mat = make_pbr_mat("ObsidianSlate", (0.028, 0.030, 0.042, 1.0), metallic=0.12, roughness=0.66, noise_scale=11.0, noise_bump=0.35)
    bronze_mat = make_pbr_mat("AstrolabeBronze", (0.76, 0.58, 0.30, 1.0), metallic=0.92, roughness=0.25, noise_scale=20.0, noise_bump=0.15)
    rune_mat = make_pbr_mat("RimRuneGold", accent_rgba, metallic=0.2, roughness=0.3, emission_rgba=accent_rgba, emission_strength=2.2)

    bpy.ops.mesh.primitive_plane_add(size=8.0, location=(0, 0, -0.35))
    fl = bpy.context.active_object
    fl.data.materials.append(slate_mat)

    # Recessed outer bronze astrolabe bezel rings
    bpy.ops.mesh.primitive_torus_add(major_radius=1.18, minor_radius=0.048, major_segments=64, minor_segments=16, location=(0, 0, 0.02))
    ring_outer = bpy.context.active_object
    ring_outer.data.materials.append(bronze_mat)

    bpy.ops.mesh.primitive_torus_add(major_radius=1.06, minor_radius=0.018, major_segments=64, minor_segments=12, location=(0, 0, -0.02))
    ring_inner = bpy.context.active_object
    ring_inner.data.materials.append(rune_mat)

    # 12 astrolabe notches + 4 cardinal corner rivets
    for i in range(12):
        ang = (2.0 * math.pi * i) / 12.0
        r = 1.12
        bpy.ops.mesh.primitive_cube_add(size=1.0, location=(math.cos(ang) * r, math.sin(ang) * r, 0.04))
        tick = bpy.context.active_object
        tick.rotation_euler.z = ang
        tick.scale = (0.09, 0.025, 0.04)
        tick.data.materials.append(bronze_mat)

    for ang_deg in (45, 135, 225, 315):
        ang = math.radians(ang_deg)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.055, location=(math.cos(ang) * 1.24, math.sin(ang) * 1.24, 0.04))
        stud = bpy.context.active_object
        stud.data.materials.append(bronze_mat)

    # Dramatic lighting + camera angled slightly down from isometric height
    add_light("AREA", "RimCold", (-2.6, 2.4, 2.8), (0, 0, 0), (0.62, 0.78, 1.0), 320.0, size=2.2)
    add_light("AREA", "WarmEdge", (2.6, -2.0, 2.0), (0, 0, 0), (1.0, 0.68, 0.34), 240.0, size=2.0)

    cam_d = bpy.data.cameras.new("PlateCam")
    cam_d.lens = 50.0
    cam = bpy.data.objects.new("PlateCam", cam_d)
    bpy.context.scene.collection.objects.link(cam)
    cam.location = Vector((0.0, -2.35, 3.05))
    cam.rotation_euler = (Vector((0.0, 0.0, 0.08)) - cam.location).to_track_quat("-Z", "Y").to_euler()
    bpy.context.scene.camera = cam


def render_spell_plate(idx: int):
    rng = random.Random(100 + idx)
    out_path = GRIMOIRE_DIR / f"SpellPlate_{idx:02d}.png"

    accents = {
        1: (1.0, 0.36, 0.05, 1.0),
        2: (0.62, 0.24, 1.0, 1.0),
        3: (0.08, 0.94, 1.0, 1.0),
        4: (1.0, 0.28, 0.04, 1.0),
        5: (0.12, 0.95, 1.0, 1.0),
        6: (1.0, 0.55, 0.14, 1.0),
        7: (0.14, 0.88, 1.0, 1.0),
        8: (0.72, 0.25, 1.0, 1.0),
        9: (0.45, 0.55, 1.0, 1.0),
        10: (1.0, 0.82, 0.42, 1.0),
    }
    build_spellplate_stage(accents[idx])
    configure_render(640, 640, out_path)

    if idx == 1:
        # Fireball: Solar cataclysm plasma orb + 3 helical flame ribbons + sparks
        sun_mat = make_pbr_mat("SunCore", (1.0, 0.22, 0.02, 1.0), roughness=0.2, emission_rgba=(1.0, 0.35, 0.04, 1.0), emission_strength=9.0, voronoi_cracks_rgba=(1.0, 0.95, 0.55, 1.0), crack_strength=16.0)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, radius=0.46, location=(0, 0, 0.18))
        orb = bpy.context.active_object
        orb.data.materials.append(sun_mat)
        rib_mat = make_pbr_mat("FlameRib", (1.0, 0.45, 0.08, 1.0), emission_rgba=(1.0, 0.42, 0.06, 1.0), emission_strength=6.5)
        for k in range(3):
            pts = []
            ph = k * 2.094
            for s in range(18):
                t = s / 17.0
                a = ph + t * 5.2
                r = 0.52 + 0.28 * t
                pts.append((math.cos(a) * r, math.sin(a) * r, 0.05 + 0.42 * math.sin(t * math.pi)))
            add_curve_tube(f"SolarRibbon_{k}", pts, bevel_depth=0.026, mat=rib_mat)
        add_light("POINT", "SunLight", (0, 0, 0.3), None, (1.0, 0.42, 0.08), 550.0)

    elif idx == 2:
        # Black Hole: Pitch-black singularity + violet lensing rim + fiery accretion disc + 32 orbiting shards
        bh_mat = make_pbr_mat("EventHorizon", (0.0, 0.0, 0.0, 1.0), metallic=0.0, roughness=1.0, fresnel_rim_rgba=(0.55, 0.18, 1.0, 1.0), fresnel_strength=12.0)
        bpy.ops.mesh.primitive_uv_sphere_add(segments=48, ring_count=32, radius=0.38, location=(0, 0, 0.20))
        bh = bpy.context.active_object
        bh.data.materials.append(bh_mat)

        disc_mat = make_pbr_mat("AccretionGold", (1.0, 0.50, 0.12, 1.0), emission_rgba=(1.0, 0.42, 0.10, 1.0), emission_strength=7.5)
        for rad, thick in [(0.68, 0.025), (0.86, 0.015), (0.52, 0.032)]:
            bpy.ops.mesh.primitive_torus_add(major_radius=rad, minor_radius=thick, major_segments=64, minor_segments=12, location=(0, 0, 0.20))
            t = bpy.context.active_object
            t.rotation_euler = Euler((math.radians(22), math.radians(-16), 0))
            t.data.materials.append(disc_mat)

        shard_mat = make_pbr_mat("VoidDebris", (0.12, 0.08, 0.16, 1.0), metallic=0.6, roughness=0.4, emission_rgba=(0.55, 0.20, 1.0, 1.0), emission_strength=2.5)
        for k in range(32):
            a = (2.0 * math.pi * k) / 32.0 + rng.uniform(-0.1, 0.1)
            dist = rng.uniform(0.46, 0.96)
            z = 0.20 + rng.uniform(-0.14, 0.14)
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=rng.uniform(0.018, 0.042), location=(math.cos(a) * dist, math.sin(a) * dist, z))
            bpy.context.active_object.data.materials.append(shard_mat)
        add_light("POINT", "BHLight", (0, 0, 0.35), None, (0.65, 0.25, 1.0), 420.0)

    elif idx == 3:
        # Chronosphere: Translucent cosmic dome + 3 interlocking astrolabe rings + 56 star shards + lightning arcs
        dome_mat = make_pbr_mat("CosmicDome", (0.02, 0.06, 0.18, 1.0), metallic=0.08, roughness=0.08, alpha=0.22, fresnel_rim_rgba=(0.10, 0.90, 1.0, 1.0), fresnel_strength=6.5)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=4, radius=0.84, location=(0, 0, 0.16))
        dome = bpy.context.active_object
        dome.data.materials.append(dome_mat)

        gyro_mat = make_pbr_mat("ChronoBrass", (0.85, 0.72, 0.42, 1.0), metallic=0.92, roughness=0.2, emission_rgba=(0.12, 0.88, 1.0, 1.0), emission_strength=1.4)
        for rx, ry, rz, rad in [(28, 15, 0, 0.86), (-36, 48, 22, 0.74), (65, -24, 45, 0.60)]:
            bpy.ops.mesh.primitive_torus_add(major_radius=rad, minor_radius=0.016, major_segments=64, minor_segments=12, location=(0, 0, 0.16))
            gr = bpy.context.active_object
            gr.rotation_euler = Euler((math.radians(rx), math.radians(ry), math.radians(rz)))
            gr.data.materials.append(gyro_mat)

        star_mat = make_pbr_mat("CosmicStar", (0.8, 0.98, 1.0, 1.0), emission_rgba=(0.25, 0.96, 1.0, 1.0), emission_strength=14.0)
        for k in range(56):
            u = rng.uniform(-1, 1)
            th = rng.uniform(0, 2 * math.pi)
            rr = rng.uniform(0.18, 0.78)
            s = math.sqrt(max(0.0, 1 - u * u))
            pos = (rr * s * math.cos(th), rr * s * math.sin(th), 0.16 + rr * u * 0.7)
            bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1, radius=rng.uniform(0.010, 0.026), location=pos)
            bpy.context.active_object.data.materials.append(star_mat)

        for k in range(4):
            a0 = k * 1.57
            pts = [(0, 0, 0.16)]
            for s in range(1, 6):
                t = s / 5.0
                pts.append((math.cos(a0 + rng.uniform(-0.3, 0.3)) * t * 0.8, math.sin(a0 + rng.uniform(-0.3, 0.3)) * t * 0.8, 0.16 + rng.uniform(-0.2, 0.25)))
            add_curve_tube(f"ChronoBolt_{k}", pts, bevel_depth=0.012, mat=star_mat)
        add_light("POINT", "ChronoGlow", (0, 0, 0.35), None, (0.12, 0.92, 1.0), 480.0)

    elif idx == 4:
        # Chaos Meteor: Jagged molten cracked meteor plunging out of violet portal ring
        rift_mat = make_pbr_mat("SkyRift", (0.55, 0.15, 1.0, 1.0), emission_rgba=(0.65, 0.18, 1.0, 1.0), emission_strength=9.5)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.62, minor_radius=0.025, location=(0.28, 0.25, 0.58))
        rift = bpy.context.active_object
        rift.rotation_euler = Euler((math.radians(38), math.radians(-28), 0))
        rift.data.materials.append(rift_mat)

        met_mat = make_pbr_mat("MoltenMeteor", (0.08, 0.04, 0.03, 1.0), roughness=0.72, noise_scale=9.0, noise_bump=0.45, voronoi_cracks_rgba=(1.0, 0.32, 0.02, 1.0), crack_strength=18.0)
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=3, radius=0.42, location=(-0.05, -0.05, 0.22))
        met = bpy.context.active_object
        for v in met.data.vertices:
            v.co *= 1.0 + rng.uniform(-0.22, 0.24)
        met.data.materials.append(met_mat)
        add_light("POINT", "MeteorFire", (-0.1, -0.1, 0.28), None, (1.0, 0.36, 0.05), 520.0)

    elif idx == 5:
        # Chain Lightning: 2 Dark gothic spires + violent branching cyan lightning bolts
        spire_mat = make_pbr_mat("IronSpire", (0.10, 0.11, 0.15, 1.0), metallic=0.82, roughness=0.32, noise_scale=12.0, noise_bump=0.25)
        for sx in (-0.65, 0.65):
            bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=0.22, radius2=0.03, depth=0.95, location=(sx, 0, 0.10))
            bpy.context.active_object.data.materials.append(spire_mat)
        bolt_mat = make_pbr_mat("ArcaneBolt", (0.7, 0.98, 1.0, 1.0), emission_rgba=(0.15, 0.95, 1.0, 1.0), emission_strength=16.0)
        for b in range(4):
            pts = []
            for s in range(9):
                t = s / 8.0
                x = -0.62 + 1.24 * t
                y = math.sin(t * math.pi) * rng.uniform(-0.32, 0.32)
                z = 0.28 + 0.15 * math.sin(t * math.pi) + rng.uniform(-0.12, 0.12)
                pts.append((x, y, z))
            add_curve_tube(f"ChainBolt_{b}", pts, bevel_depth=0.018 if b == 0 else 0.010, mat=bolt_mat)
        add_light("POINT", "ArcLight", (0, 0, 0.35), None, (0.15, 0.95, 1.0), 500.0)

    elif idx == 6:
        # Supernova Beam: Intense diagonal plasma lance wrapped in twin helical coils
        core_mat = make_pbr_mat("BeamCore", (1.0, 0.92, 0.72, 1.0), emission_rgba=(1.0, 0.45, 0.08, 1.0), emission_strength=18.0)
        coil_mat = make_pbr_mat("BeamHelix", (0.2, 0.92, 1.0, 1.0), emission_rgba=(0.15, 0.92, 1.0, 1.0), emission_strength=9.5)
        add_curve_tube("SolarLance", [(-0.85, -0.45, 0.12), (0.85, 0.45, 0.32)], bevel_depth=0.085, mat=core_mat)
        for ph in (0.0, math.pi):
            pts = []
            for s in range(28):
                t = s / 27.0
                bx = -0.82 + 1.64 * t
                by = -0.42 + 0.84 * t
                bz = 0.12 + 0.20 * t
                a = ph + t * 14.0
                pts.append((bx + math.cos(a) * 0.16, by + math.sin(a) * 0.14, bz + math.sin(a * 0.8) * 0.16))
            add_curve_tube(f"BeamCoil_{int(ph)}", pts, bevel_depth=0.022, mat=coil_mat)
        add_light("POINT", "BeamGlow", (0, 0, 0.25), None, (1.0, 0.52, 0.12), 540.0)

    elif idx == 7:
        # Chrono-Blink: Holographic wizard emerging through a glowing temporal portal ring
        holo_mat = make_pbr_mat("HoloWizard", (0.05, 0.35, 0.65, 1.0), metallic=0.1, roughness=0.2, alpha=0.65, fresnel_rim_rgba=(0.10, 0.96, 1.0, 1.0), fresnel_strength=11.0)
        portal_mat = make_pbr_mat("TemporalPortal", (0.25, 0.85, 1.0, 1.0), emission_rgba=(0.10, 0.88, 1.0, 1.0), emission_strength=11.0)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.64, minor_radius=0.032, location=(-0.12, 0.15, 0.24))
        por = bpy.context.active_object
        por.rotation_euler = Euler((math.radians(72), math.radians(-22), math.radians(15)))
        por.data.materials.append(portal_mat)

        hero_fbx = MESH_DIR / "SM_Hero_00.fbx"
        if not hero_fbx.exists():
            hero_fbx = MESH_DIR / "SM_ArchmageRobed.fbx"
        if hero_fbx.exists():
            before = set(bpy.data.objects)
            bpy.ops.import_scene.fbx(filepath=str(hero_fbx))
            for o in set(bpy.data.objects) - before:
                if o.type == "MESH":
                    o.scale = Vector((0.55, 0.55, 0.55))
                    o.location = Vector((0.12, -0.08, -0.18))
                    o.rotation_euler = Euler((0, 0, math.radians(-68)))
                    o.data.materials.clear()
                    o.data.materials.append(holo_mat)
        add_light("POINT", "BlinkGlow", (0.05, 0, 0.4), None, (0.12, 0.92, 1.0), 420.0)

    elif idx == 8:
        # Gravity Cataclysm: 7 sculpted tilted jagged basalt monolith spires erupting from violet magma seal
        spire_mat = make_pbr_mat(
            "ObsidianBasaltSpire",
            (0.05, 0.05, 0.07, 1.0),
            metallic=0.28,
            roughness=0.52,
            noise_scale=10.0,
            noise_bump=0.45,
            voronoi_cracks_rgba=(1.0, 0.34, 0.05, 1.0),
            crack_strength=14.0,
        )
        seal_mat = make_pbr_mat("GravitySeal", (0.58, 0.18, 1.0, 1.0), emission_rgba=(0.62, 0.22, 1.0, 1.0), emission_strength=9.5)
        bpy.ops.mesh.primitive_torus_add(major_radius=0.72, minor_radius=0.022, location=(0, 0, -0.02))
        bpy.context.active_object.data.materials.append(seal_mat)

        spire_specs = [
            (0.0, 0.0, 0.24, 1.15, 0.0, 0.0),
            (-0.38, 0.14, 0.18, 0.86, 8.0, 14.0),
            (0.38, -0.10, 0.19, 0.90, -10.0, -12.0),
            (-0.18, -0.34, 0.16, 0.72, -14.0, 9.0),
            (0.22, 0.34, 0.16, 0.76, 12.0, -11.0),
            (-0.56, -0.18, 0.13, 0.56, -15.0, 16.0),
            (0.54, 0.20, 0.14, 0.58, 15.0, -15.0),
        ]
        for sx, sy, rad, hgt, rx, ry in spire_specs:
            bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=rad, radius2=rad * 0.14, depth=hgt, location=(sx, sy, hgt * 0.35 - 0.12))
            sp = bpy.context.active_object
            sp.rotation_euler = Euler((math.radians(rx), math.radians(ry), rng.uniform(0, 3.14)))
            for v in sp.data.vertices:
                v.co.x *= rng.uniform(0.88, 1.15)
                v.co.y *= rng.uniform(0.88, 1.15)
            sp.data.materials.append(spire_mat)
        add_light("POINT", "CataclysmUnderGlow", (0, 0, 0.18), None, (1.0, 0.38, 0.08), 460.0)
        add_light("POINT", "CataclysmVoid", (0, 0.2, 0.75), None, (0.68, 0.25, 1.0), 320.0)

    elif idx == 9:
        # Cryo-Void Prism Wall: 7 tall faceted crystal prisms splitting incoming beam into 3
        crys_mat = make_pbr_mat("VoidCrystal", (0.18, 0.22, 0.68, 1.0), metallic=0.1, roughness=0.08, alpha=0.68, fresnel_rim_rgba=(0.15, 0.94, 1.0, 1.0), fresnel_strength=10.5)
        for k in range(7):
            t = (k / 6.0) - 0.5
            x = t * 1.15
            y = -0.18 * (t * t * 4.0)
            hgt = 0.72 + (1.0 - abs(t) * 1.6) * 0.38
            bpy.ops.mesh.primitive_cone_add(vertices=6, radius1=0.12, radius2=0.02, depth=hgt, location=(x, y, hgt * 0.35 - 0.10))
            pr = bpy.context.active_object
            pr.rotation_euler = Euler((0, math.radians(t * 24.0), math.radians(k * 18.0)))
            pr.data.materials.append(crys_mat)

        in_mat = make_pbr_mat("RayIn", (1.0, 0.4, 0.1, 1.0), emission_rgba=(1.0, 0.45, 0.08, 1.0), emission_strength=14.0)
        out_mat = make_pbr_mat("RayOut", (0.15, 0.96, 1.0, 1.0), emission_rgba=(0.15, 0.96, 1.0, 1.0), emission_strength=12.0)
        add_curve_tube("IncomingRay", [(0.0, -0.82, 0.18), (0.0, -0.02, 0.22)], bevel_depth=0.028, mat=in_mat)
        for ang in (-0.42, 0.0, 0.42):
            add_curve_tube(f"SplitRay_{ang}", [(0.0, 0.02, 0.22), (math.sin(ang) * 0.85, 0.78, 0.25)], bevel_depth=0.020, mat=out_mat)
        add_light("POINT", "PrismGlow", (0, 0, 0.35), None, (0.35, 0.75, 1.0), 420.0)

    elif idx == 10:
        # Primal Triad Overload: Three elemental vortex rings + blinding white-gold core
        core_mat = make_pbr_mat("TriadCore", (1.0, 0.98, 0.94, 1.0), emission_rgba=(1.0, 0.95, 0.88, 1.0), emission_strength=18.0)
        bpy.ops.mesh.primitive_uv_sphere_add(radius=0.28, location=(0, 0, 0.22))
        bpy.context.active_object.data.materials.append(core_mat)

        triad_cols = [
            ((1.0, 0.30, 0.05, 1.0), (25, 15, 0)),
            ((0.08, 0.96, 1.0, 1.0), (-32, 42, 30)),
            ((0.68, 0.22, 1.0, 1.0), (55, -35, 70)),
        ]
        for k, (col, rot) in enumerate(triad_cols):
            rm = make_pbr_mat(f"TriadRib_{k}", col, emission_rgba=col, emission_strength=9.5)
            bpy.ops.mesh.primitive_torus_add(major_radius=0.68 + k * 0.07, minor_radius=0.022, major_segments=64, minor_segments=12, location=(0, 0, 0.22))
            tr = bpy.context.active_object
            tr.rotation_euler = Euler(tuple(math.radians(a) for a in rot))
            tr.data.materials.append(rm)
        add_light("POINT", "OverloadFlash", (0, 0, 0.35), None, (1.0, 0.92, 0.85), 580.0)

    for obj in bpy.context.scene.objects:
        if obj.type == "MESH" and len(obj.data.polygons) >= 100:
            if not any(m and m.name == "MoltenMeteor" for m in obj.data.materials):
                obj.data.polygons.foreach_set("use_smooth", [True] * len(obj.data.polygons))
                obj.data.update()

    bpy.ops.render.render(write_still=True)
    print(f"Rendered {out_path.name}")


def main():
    GRIMOIRE_DIR.mkdir(parents=True, exist_ok=True)
    render_grimoire_book_spread()
    for i in range(1, 11):
        render_spell_plate(i)
    print("All 11 AAA 3D Grimoire & SpellPlate renders complete.")


if __name__ == "__main__":
    main()
