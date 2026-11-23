# Run: UnrealEditor-Cmd.exe Aetheria.uproject -run=pythonscript -script="<this>"
# (Re)creates /Game/AetheriaCookRefs and fills it with hard references to
# every asset in the roots the game loads dynamically at runtime. The
# GameMode constructor loads this asset, so the cooker follows the chain.
import unreal, os, time

ROOTS = [
    "/Game/Characters",
    "/Game/FreeStylizedLaserBeamVFX",
    "/Game/ParagonAurora",
    "/Game/ParagonGideon",
    "/Game/ParagonGrux",
    "/Game/ParagonKhaimera",
    "/Game/ParagonMinions",
    "/Game/ParagonSevarog",
    "/Game/Portals",
    "/Game/RPG_Magic_Staff_Pack",
    "/Game/Stylish_Fire_VFX",
    "/Game/Vefects",
    "/Game/_SplineVFX",
]
PKG = "/Game/AetheriaCookRefs"

AR = unreal.AssetRegistryHelpers.get_asset_registry()
i = 0
while AR.is_loading_assets() and i < 1200:
    time.sleep(0.05)
    i += 1

if unreal.EditorAssetLibrary.does_asset_exist(PKG):
    unreal.EditorAssetLibrary.delete_asset(PKG)

factory = unreal.DataAssetFactory()
factory.set_editor_property("data_asset_class", unreal.AetheriaCookRefs.static_class())
asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset("AetheriaCookRefs", "/Game", unreal.AetheriaCookRefs.static_class(), factory)
ALLOWED = {
    "SkeletalMesh", "Skeleton", "PhysicsAsset", "AnimSequence", "AnimMontage",
    "BlendSpace", "BlendSpace1D", "Material", "MaterialInstance",
    "MaterialInstanceConstant", "MaterialFunction", "Texture2D", "Texture",
    "StaticMesh", "SoundWave", "SoundCue", "SoundBase", "NiagaraSystem",
    "NiagaraEmitter", "ParticleSystem", "MorphTarget", "CurveFloat",
}
refs = []
skipped = 0
for root in ROOTS:
    for ad in AR.get_assets_by_path(root, True) or []:
        cls = str(ad.asset_class_path.asset_name)
        if cls not in ALLOWED:
            skipped += 1
            continue
        try:
            obj = ad.get_asset()
        except Exception:
            obj = None
        if obj is None:
            skipped += 1
            continue
        refs.append(obj)

asset.set_editor_property("refs", refs)

# NOTE: never reference Maps/Worlds/Levels from a DataAsset — loading them at
# CDO time triggers engine ensures and kills the cook (Paragon packs ship
# demo .umaps inside Characters/Maps/). Whitelist only asset classes the
# runtime bind/spell code actually LoadObject()s.
asset.modify()
unreal.EditorAssetLibrary.save_loaded_asset(asset, only_if_is_dirty=False)
unreal.log_warning(f"[CookRefs] {len(refs)} refs (skipped {skipped}) -> {PKG}")
