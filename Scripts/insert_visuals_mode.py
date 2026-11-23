# -*- coding: utf-8 -*-
from pathlib import Path
p = Path(r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Scripts\ue_port_editor.py")
raw = p.read_bytes()
t = raw.decode("utf-8").replace("\r\n", "\n")
fn = """
def do_visuals():
    log("Visuals: bake sanctum/gothic materials and fill locomotion blendspaces")
    saved = 0
    filled = 0
    try:
        saved = int(unreal.AetheriaMaterialLibrary.ensure_bastion_environment_materials())
        log("EnsureBastionEnvironmentMaterials saved=%d" % saved)
    except Exception as exc:
        log_warn("EnsureBastionEnvironmentMaterials: %s" % exc)
    try:
        filled = int(unreal.AetheriaAnimInstance.fill_locomotion_blend_spaces())
        log("FillLocomotionBlendSpaces filled=%d" % filled)
    except Exception as exc:
        log_warn("FillLocomotionBlendSpaces: %s" % exc)
    return saved, filled

"""
if "def do_visuals" not in t:
    t = t.replace("def main():", fn + "\ndef main():", 1)
old = '    if mode in ("map", "all"):\n        do_map()\n'
new = old + '    if mode in ("visuals", "all"):\n        do_visuals()\n'
if 'mode in ("visuals"' not in t:
    if old not in t:
        raise SystemExit("PATTERN MISSING main mode map branch")
    t = t.replace(old, new, 1)
nl = "\r\n" if b"\r\n" in raw else "\n"
p.write_bytes(t.replace("\n", nl).encode("utf-8"))
print("visuals mode inserted")
