# Resave static meshes under given content roots so cooked collision/physics
# stop recomputing at cook time (the memory blow-up source).
import unreal, sys, os
roots = sys.argv[1:] or ["/Game/KiteDemo"]
AR = unreal.AssetRegistryHelpers.get_asset_registry()
# Registry can still be scanning at commandlet boot; wait it out or we only
# see a fraction of the meshes (37 instead of hundreds).
import time
i = 0
while AR.is_loading_assets() and i < 1200:
    time.sleep(0.05)
    i += 1
saved = failed = 0
for root in roots:
    for ad in AR.get_assets_by_path(root, True) or []:
        if str(ad.asset_class_path.asset_name) != "StaticMesh":
            continue
        try:
            mesh = unreal.load_asset(ad.package_name)
            if not mesh:
                continue
            mesh.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False)
            saved += 1
        except Exception:
            failed += 1
unreal.log_warning(f"[Resave] roots={roots} saved={saved} failed={failed}")
