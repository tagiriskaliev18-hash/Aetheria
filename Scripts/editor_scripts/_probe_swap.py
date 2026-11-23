import unreal
les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
els = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
ok = les.load_level("/Game/Maps/AetheriaWorlds/World_Bastion")
actors = els.get_all_level_actors()
comps = 0
meshy = 0
big = []
for a in actors:
    cs = a.get_components_by_class(unreal.StaticMeshComponent)
    comps += len(cs)
    for c in cs:
        m = c.get_editor_property("static_mesh")
        if m:
            meshy += 1
            b = m.get_bounds().box_extent
            if max(b.x, b.y) > 250 and b.z < 300 * 3:
                big.append((str(a.get_class().get_name()), str(m.get_name()), round(b.x), round(b.y), round(b.z)))
mat = unreal.AetheriaMaterialLibrary.get_cinematic_biome_material(0, False)
unreal.log_warning("[Probe] load=%s actors=%d smcomps=%d withmesh=%d mat=%s bigsample=%s" % (ok, len(actors), comps, meshy, mat, big[:5]))
