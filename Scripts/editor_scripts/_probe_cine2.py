import unreal
for wid in range(4):
    m = unreal.AetheriaMaterialLibrary.get_cinematic_biome_material(wid, False)
    unreal.log_warning(f"[ProbeCine] wid={wid} mat={m.get_path_name() if m else None}")
