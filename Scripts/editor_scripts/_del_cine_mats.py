import unreal
n = 0
for stem in ["pbr_bastion_floor","pbr_bastion_wall","pbr_ruins_floor","pbr_ruins_wall","pbr_sanctum_floor","pbr_sanctum_wall","pbr_void_floor","pbr_void_wall"]:
    pkg = f"/Game/Materials/M_Cine_{stem}"
    if unreal.EditorAssetLibrary.does_asset_exist(pkg):
        unreal.EditorAssetLibrary.delete_asset(pkg)
        n += 1
unreal.log_warning(f"[DelCine] {n}")
