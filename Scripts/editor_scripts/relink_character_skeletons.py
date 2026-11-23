# Run: UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script="<this>"
# Godot-imported character anims reference a missing skeleton (property=None),
# which hard-fails the cook ("no target USkeleton"). Relink every AnimSequence
# under /Game/Characters/<X>/Anims to a Skeleton asset found anywhere under
# /Game/Characters/<X>/ and save.
import unreal, os, json

ROOT = "/Game/Characters"
LOG = os.path.join(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()), "Saved", "grok_skel_relink.json")
AR = unreal.AssetRegistryHelpers.get_asset_registry()

all_ads = AR.get_assets_by_path(ROOT, True) or []
chars = {}
for ad in all_ads:
    parts = str(ad.package_name).split("/")
    if len(parts) >= 5:
        chars.setdefault(parts[3], None)

fixed, errors = [], []
for char_name in chars:
    char_root = f"{ROOT}/{char_name}"
    skel = None
    for ad in AR.get_assets_by_path(char_root, True) or []:
        if str(ad.asset_class_path.asset_name) == "Skeleton":
            skel = unreal.load_asset(ad.package_name)
            if skel:
                break
    if not skel:
        continue
    for ad in AR.get_assets_by_path(f"{char_root}/Anims", True) or []:
        if str(ad.asset_class_path.asset_name) != "AnimSequence":
            continue
        seq = unreal.load_asset(ad.package_name)
        if not seq:
            continue
        try:
            if seq.get_editor_property("skeleton"):
                continue
            seq.set_editor_property("skeleton", skel)
            seq.modify()
            unreal.EditorAssetLibrary.save_loaded_asset(seq, only_if_is_dirty=False)
            fixed.append(str(ad.package_name))
        except Exception as e:
            errors.append(f"{ad.package_name}: {e}")

json.dump({"fixed": fixed, "errors": errors}, open(LOG, "w"), indent=1)
unreal.log_warning(f"[SkelRelink] fixed={len(fixed)} errors={len(errors)} -> {LOG}")
