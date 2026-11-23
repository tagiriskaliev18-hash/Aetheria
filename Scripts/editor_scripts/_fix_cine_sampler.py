import unreal
fixed = 0
sets = ["pbr_bastion_floor","pbr_bastion_wall","pbr_ruins_floor","pbr_ruins_wall","pbr_sanctum_floor","pbr_sanctum_wall","pbr_void_floor","pbr_void_wall"]
for stem in sets:
    for suffix in ["_Roughness", "_Emissive"]:
        pkg = f"/Game/Textures/T_Cine_{stem}{suffix}"
        if not unreal.EditorAssetLibrary.does_asset_exist(pkg):
            continue
        tex = unreal.load_asset(pkg)
        if not tex:
            continue
        try:
            tex.set_editor_property("s_rgb", True)
            tex.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
            fixed += 1
        except Exception as e:
            unreal.log_warning(f"[SamplerFix] {pkg}: {e}")
unreal.log_warning(f"[SamplerFix] fixed {fixed}")
