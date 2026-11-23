import unreal
tex = unreal.load_asset("/Game/Textures/T_Cine_pbr_ruins_floor_Roughness")
unreal.log_warning(f"[SF2] exists={tex is not None}")
if tex:
    names = [n for n in dir(tex) if "rgb" in n.lower() or "srgb" in n.lower()]
    unreal.log_warning(f"[SF2] props={names}")
    try:
        unreal.log_warning(f"[SF2] get_srgb={tex.get_editor_property('srgb')}")
    except Exception as e:
        unreal.log_warning(f"[SF2] srgb err {e}")
    try:
        unreal.log_warning(f"[SF2] get_s_rgb={tex.get_editor_property('s_rgb')}")
    except Exception as e:
        unreal.log_warning(f"[SF2] s_rgb err {e}")
