# -*- coding: utf-8 -*-
from pathlib import Path

root = Path(r"C:\Users\tagir\OneDrive\Desktop\Aetheria")
anim_path = root / "Source" / "Aetheria" / "Private" / "AetheriaAnimInstance.cpp"
snippet_path = root / "Scripts" / "FillLocomotion.cpp.txt"
anim = anim_path.read_text(encoding="utf-8")
if "int32 UAetheriaAnimInstance::FillLocomotionBlendSpaces" in anim:
    print("FillLocomotionBlendSpaces already present")
else:
    if '#include "AssetRegistry/AssetRegistryModule.h"' not in anim:
        anim = anim.replace(
            '#include "AetheriaAnimInstance.h"',
            '#include "AetheriaAnimInstance.h"\n#include "AssetRegistry/AssetRegistryModule.h"\n#include "Animation/BlendSpace.h"\n#include "UObject/SavePackage.h"',
            1,
        )
    body = snippet_path.read_text(encoding="utf-8")
    if not anim.endswith("\n"):
        anim += "\n"
    anim += "\n" + body
    raw = anim_path.read_bytes()
    nl = "\r\n" if b"\r\n" in raw else "\n"
    anim_path.write_bytes(anim.replace("\n", nl).encode("utf-8"))
    print("appended FillLocomotionBlendSpaces")

# Light DefaultEngine exposure nudge (AE stays off; PPV manual bias is the main lever)
ini = root / "Config" / "DefaultEngine.ini"
text = ini.read_text(encoding="utf-8")
orig = text
# Keep AutoExposure off (manual PPV). If Bias line exists, raise it for any path that still reads it.
text = text.replace("r.DefaultFeature.AutoExposure.Bias=0.0", "r.DefaultFeature.AutoExposure.Bias=1.0")
if text != orig:
    if "\r\n" in ini.read_bytes().decode("utf-8", "replace") or b"\r\n" in ini.read_bytes():
        text = text.replace("\n", "\r\n") if "\r\n" not in text else text
    ini.write_text(text, encoding="utf-8")
    print("DefaultEngine AutoExposure.Bias updated")
else:
    print("DefaultEngine bias line not patched or already set")
