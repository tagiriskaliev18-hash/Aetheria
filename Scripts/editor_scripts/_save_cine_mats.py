import unreal
saved = []
for wid in range(4):
    for wall in [False, True]:
        m = unreal.AetheriaMaterialLibrary.get_cinematic_biome_material(wid, wall)
        if m and str(m.get_path_name()).startswith("/Game/Materials/M_Cine"):
            ok = unreal.EditorAssetLibrary.save_loaded_asset(m, only_if_is_dirty=False)
            saved.append((str(m.get_name()), ok))
unreal.log_warning(f"[SaveCine] {saved}")
