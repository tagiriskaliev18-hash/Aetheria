import unreal, os, json
AR = unreal.AssetRegistryHelpers.get_asset_registry()
out = []
for ad in AR.get_assets_by_path("/Game", True) or []:
    if str(ad.asset_class_path.asset_name) != "AnimSequence":
        continue
    seq = unreal.load_asset(ad.package_name)
    if not seq:
        continue
    try:
        if not seq.get_editor_property("skeleton"):
            out.append(str(ad.package_name))
    except Exception:
        pass
json.dump(out, open(os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "grok_null_skel_anims.json"), "w"), indent=0)
unreal.log_warning(f"[NullSkel] count={len(out)}")
