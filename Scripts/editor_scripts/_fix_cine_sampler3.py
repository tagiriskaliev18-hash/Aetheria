import unreal
sets = ["pbr_bastion_floor","pbr_bastion_wall","pbr_ruins_floor","pbr_ruins_wall","pbr_sanctum_floor","pbr_sanctum_wall","pbr_void_floor","pbr_void_wall"]
fixed = 0
for stem in sets:
    for suffix in ["_Roughness", "_Emissive"]:
        pkg = f"/Game/Textures/T_Cine_{stem}{suffix}"
        tex = unreal.load_asset(pkg)
        if not tex:
            continue
        try:
            cs = tex.get_editor_property("compression_settings")
            tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_DEFAULT)
            tex.set_editor_property("srgb", True)
            tex.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(tex, only_if_is_dirty=False)
            fixed += 1
        except Exception as e:
            unreal.log_warning(f"[SF3] {pkg}: {e}")
unreal.log_warning(f"[SF3] fixed {fixed}")
