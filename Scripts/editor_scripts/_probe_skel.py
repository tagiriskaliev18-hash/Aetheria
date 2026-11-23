import unreal
seq = unreal.load_asset("/Game/Characters/BruiserJuggernaut/Anims/Cast_Primary")
skel = seq.get_editor_property("skeleton") if seq else None
unreal.log_warning(f"[ProbeSkel] seq={seq} skel_prop={skel}")
if skel:
    p = str(skel.get_path_name())
    unreal.log_warning(f"[ProbeSkel] skel path={p}")
    loaded = unreal.load_asset(p.split(".")[0])
    unreal.log_warning(f"[ProbeSkel] reload={loaded} valid={bool(loaded)}")
