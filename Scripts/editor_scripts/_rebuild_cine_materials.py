import unreal
# Drop stale cinematic textures + materials so the C++ builder re-imports
# from the regenerated PNGs (wet roughness + ember emissive).
deleted = 0
for name in ["pbr_bastion_floor", "pbr_bastion_wall", "pbr_ruins_floor", "pbr_ruins_wall",
             "pbr_sanctum_floor", "pbr_sanctum_wall", "pbr_void_floor", "pbr_void_wall"]:
    for suffix in ["_Albedo", "_Normal", "_Roughness", "_Emissive"]:
        for pkg in [f"/Game/Textures/T_Cine_{name}{suffix}", f"/Game/Cinematic/T_Cine_{name}{suffix}"]:
            if unreal.EditorAssetLibrary.does_asset_exist(pkg):
                unreal.EditorAssetLibrary.delete_asset(pkg)
                deleted += 1
    for pkg in [f"/Game/Materials/M_Cine_{name}"]:
        if unreal.EditorAssetLibrary.does_asset_exist(pkg):
            unreal.EditorAssetLibrary.delete_asset(pkg)
            deleted += 1
unreal.log_warning(f"[CineRebuild] deleted {deleted} stale assets")
