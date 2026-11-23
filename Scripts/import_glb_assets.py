#!/usr/bin/env python3
"""Import all GLB models from RawAssets into UE5 Content browser via UnrealEditor commandlet."""
import subprocess, os, json

UE_EDITOR = r"C:\Users\tagir\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
PROJECT = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Aetheria.uproject"
RAW_MODELS = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\RawAssets\Models"

# Build import JSON for each GLB file
imports = []
for f in sorted(os.listdir(RAW_MODELS)):
    if f.endswith('.glb') or f.endswith('.fbx'):
        source = os.path.join(RAW_MODELS, f)
        dest_path = "/Game/Meshes/" + os.path.splitext(f)[0]
        imports.append({
            "SourceFile": source,
            "DestinationPath": dest_path,
            "bNaniteEnabled": True,
            "bGenerateLightmapUVs": True,
        })

json_path = r"C:\Users\tagir\OneDrive\Desktop\Aetheria\Scripts\import_manifest.json"
with open(json_path, 'w', encoding='utf-8') as fp:
    json.dump(imports, fp, indent=2, ensure_ascii=False)

print(f"[OK] Manifest written: {len(imports)} assets")
print(f"[INFO] Run the commandlet manually in UE5 Editor: Misc > Import using this manifest")
print(json_path)
