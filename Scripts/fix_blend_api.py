# -*- coding: utf-8 -*-
from pathlib import Path
p = Path(r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Source\Aetheria\Private\AetheriaAnimInstance.cpp")
raw = p.read_bytes()
t = raw.decode("utf-8").replace("\r\n", "\n")
if '#include "UObject/SavePackage.h"' not in t:
    t = t.replace('#include "Animation/BlendSpace.h"\n', '#include "Animation/BlendSpace.h"\n#include "UObject/SavePackage.h"\n', 1)
    print("added SavePackage.h")
old = """\t\tFBlendParameter ParamX = BS->GetBlendParameter(0);
\t\tParamX.DisplayName = TEXT("Strafe");
\t\tParamX.Min = -1.0f;
\t\tParamX.Max = 1.0f;
\t\tParamX.GridNum = 4;
\t\tFBlendParameter ParamY = BS->GetBlendParameter(1);
\t\tParamY.DisplayName = TEXT("Forward");
\t\tParamY.Min = -1.0f;
\t\tParamY.Max = 1.0f;
\t\tParamY.GridNum = 4;
\t\tBS->UpdateBlendParameter(0, ParamX);
\t\tBS->UpdateBlendParameter(1, ParamY);
"""
new = """\t\tFBlendParameter& AxisX = const_cast<FBlendParameter&>(BS->GetBlendParameter(0));
\t\tAxisX.DisplayName = TEXT("Strafe");
\t\tAxisX.Min = -1.0f;
\t\tAxisX.Max = 1.0f;
\t\tAxisX.GridNum = 4;
\t\tFBlendParameter& AxisY = const_cast<FBlendParameter&>(BS->GetBlendParameter(1));
\t\tAxisY.DisplayName = TEXT("Forward");
\t\tAxisY.Min = -1.0f;
\t\tAxisY.Max = 1.0f;
\t\tAxisY.GridNum = 4;
"""
if old not in t:
    print("PATTERN MISSING blend param block")
else:
    t = t.replace(old, new, 1)
    print("replaced UpdateBlendParameter block")
if "BS->ResampleData();" not in t:
    t = t.replace("\t\t\tBS->PostEditChange();\n", "\t\t\tBS->ValidateSampleData();\n\t\t\tBS->ResampleData();\n\t\t\tBS->PostEditChange();\n", 1)
    print("added ResampleData")
nl = "\r\n" if b"\r\n" in raw else "\n"
p.write_bytes(t.replace("\n", nl).encode("utf-8"))
print("done")
