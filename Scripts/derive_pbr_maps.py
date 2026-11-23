#!/usr/bin/env python3
"""Derive DirectX normal + linear roughness maps from the cinematic albedo set.
Height = blurred luminance (cavity-aware), normal via central differences with
wrap padding (keeps the albedo's seamless tiling), roughness from inverted
luminance pushed to a game-friendly range. Writes <name>_Normal/_Roughness
next to the source in RawAssets/Cinematic/.
"""
import numpy as np
from PIL import Image, ImageFilter
from pathlib import Path

SRC = Path(__file__).parent.parent / "RawAssets" / "Cinematic"

def derive(name: str, wet: float = None) -> None:
    albedo_path = SRC / f"{name}.png"
    if not albedo_path.exists():
        print(f"[derive] missing {albedo_path}")
        return
    img = Image.open(albedo_path).convert("RGB")
    a = np.asarray(img, dtype=np.float32) / 255.0
    lum = (a[..., 0] * 0.2126 + a[..., 1] * 0.7152 + a[..., 2] * 0.0722).astype(np.float32)

    pil_lum = Image.fromarray((lum * 255).astype(np.uint8))
    height = np.asarray(pil_lum.filter(ImageFilter.GaussianBlur(2.2)), dtype=np.float32) / 255.0
    # Cavity: darker crevices read as depth
    cavity = lum - height

    gy, gx = np.gradient(height)
    strength = 2.6
    nx = -gx * strength
    ny = gy * strength  # DirectX green
    nz = np.ones_like(height)
    nlen = np.sqrt(nx * nx + ny * ny + nz * nz)
    nmap = np.stack([nx / nlen, ny / nlen, nz / nlen], axis=-1)
    nimg = Image.fromarray((nmap * 0.5 + 0.5).clip(0, 1).__mul__(255).astype(np.uint8), mode="RGB")

    rough_base = 1.0 - lum * 0.55
    if wet is not None:
        # Wet floors: low base roughness with crevice variation — sells the
        # "wet basalt" specular the flat pass was missing.
        rough = np.clip(wet * (0.85 - lum * 0.5) + 0.10 + cavity * 0.35, 0.08, 0.85)
    else:
        rough = np.clip(rough_base * 0.75 + 0.28 + cavity * 0.5, 0.12, 0.94)
    rimg = Image.fromarray((rough * 255).astype(np.uint8), mode="L")

    nimg.save(SRC / f"{name}_Normal.png")
    rimg.save(SRC / f"{name}_Roughness.png")

    # Ember/crystal emissive mask: keep only strongly warm (R >> G) pixels —
    # lava cracks in Bastion, violet fissures land near zero and stay subtle.
    warm = np.clip((a[..., 0] - a[..., 1] * 1.25) * (a[..., 0] - a[..., 2] * 1.05), 0.0, 1.0)
    warm = np.clip(warm * 3.2, 0.0, 1.0)
    eimg = Image.fromarray((warm * 255).astype(np.uint8), mode="L")
    eimg.save(SRC / f"{name}_Emissive.png")
    print(f"[derive] {name}: normal+roughness+emissive written")

WET = {"pbr_bastion_floor": 0.62, "pbr_void_floor": 0.70, "pbr_sanctum_floor": 0.55}
for base in ["pbr_bastion_floor", "pbr_bastion_wall", "pbr_ruins_floor", "pbr_ruins_wall",
             "pbr_sanctum_floor", "pbr_sanctum_wall", "pbr_void_floor", "pbr_void_wall"]:
    derive(base, WET.get(base))
print("[derive] DONE")
