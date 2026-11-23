#!/usr/bin/env python3
"""Batch cinematic asset generation via smartapi gpt-image-2.6.
Generates sky backplates, tileable PBR albedo sets per biome, menu key art.
Outputs PNGs to RawAssets/Cinematic/. Retry each prompt up to 3 times.
"""
import base64, json, os, pathlib, sys, time, urllib.request

ENDPOINT = "https://api.smartapi.pro/v1/images/generations"
HERE = pathlib.Path(__file__).parent
KEY = (HERE / "_secrets" / "smartapi.key").read_text().strip()
OUT = pathlib.Path(HERE.parent / "RawAssets" / "Cinematic")
OUT.mkdir(parents=True, exist_ok=True)

STYLE = ("Dark fantasy AAA game art, Diablo IV and Black Myth Wukong style, "
         "moody volumetric atmosphere, cinematic color grading, no text, no watermark, no characters")

JOBS = [
    # --- Sky backplates (16:9) per world ---
    ("sky_bastion", "Sky vista backdrop plate for a game skybox: ash-laden dusk sky over a gothic fortress bastion, ember-orange horizon under charcoal storm clouds, drifting embers", "1536x1024"),
    ("sky_ruins", "Sky vista backdrop plate for a game skybox: overgrown ancient temple ruins at gloomy noon, heavy moss-green clouds parting with pale sunbeams, distant crumbled spires", "1536x1024"),
    ("sky_sanctum", "Sky vista backdrop plate for a game skybox: celestial marble sanctum interior opening to a cosmic dusk, floating crystal shards, warm gold and violet god rays", "1536x1024"),
    ("sky_void", "Sky vista backdrop plate for a game skybox: obsidian void realm, deep purple-black nebula with electric violet lightning veins, floating black monoliths", "1536x1024"),
    # --- Tileable PBR albedo, Bastion ---
    ("pbr_bastion_floor", "Seamless tileable PBR albedo texture, top-down flat light: wet basalt flagstone fortress floor, ash-grey volcanic stone with ember-lit cracks, game texture", "1024x1024"),
    ("pbr_bastion_wall", "Seamless tileable PBR albedo texture, top-down flat light: dark gothic fortress wall, soot-stained granite blocks with iron reinforcements, game texture", "1024x1024"),
    # --- Ruins ---
    ("pbr_ruins_floor", "Seamless tileable PBR albedo texture, top-down flat light: ancient temple floor, cracked sandstone slabs overgrown with moss and creeping vines, game texture", "1024x1024"),
    ("pbr_ruins_wall", "Seamless tileable PBR albedo texture, top-down flat light: ruined temple wall, weathered carved sandstone with root damage and lichen, game texture", "1024x1024"),
    # --- Sanctum ---
    ("pbr_sanctum_floor", "Seamless tileable PBR albedo texture, top-down flat light: polished white marble sanctum floor with gold inlay veins and faint crystal glow, game texture", "1024x1024"),
    ("pbr_sanctum_wall", "Seamless tileable PBR albedo texture, top-down flat light: carved marble sanctum wall with celestial reliefs and amethyst crystals, game texture", "1024x1024"),
    # --- Void ---
    ("pbr_void_floor", "Seamless tileable PBR albedo texture, top-down flat light: obsidian glass floor with violet energy fissures and floating ash, game texture", "1024x1024"),
    ("pbr_void_wall", "Seamless tileable PBR albedo texture, top-down flat light: black basalt monolith wall with glowing purple runes carved deep, game texture", "1024x1024"),
    # --- Menu key art ---
    ("menu_keyart", "Main menu key art: lone archmage with glowing staff standing on a fortress rampart overlooking an ash storm, three elemental orbs orbiting him (fire, lightning, void)", "1536x1024"),
]

def gen(prompt, size):
    body = json.dumps({"model": "gpt-image-2.6", "prompt": prompt, "size": size, "n": 1}).encode()
    req = urllib.request.Request(ENDPOINT, data=body, headers={
        "Authorization": "Bearer " + KEY, "Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=300) as r:
        return json.load(r)["data"][0]["b64_json"]

done = 0
for name, prompt, size in JOBS:
    out_file = OUT / f"{name}.png"
    if out_file.exists() and out_file.stat().st_size > 10000:
        print(f"[gen] skip {name} (exists)", flush=True)
        done += 1
        continue
    full_prompt = f"{prompt}. {STYLE}"
    for attempt in range(3):
        try:
            t0 = time.time()
            b64 = gen(full_prompt, size)
            out_file.write_bytes(base64.b64decode(b64))
            print(f"[gen] {name} <- {size} {out_file.stat().st_size//1024}KB {time.time()-t0:.0f}s", flush=True)
            done += 1
            break
        except Exception as e:
            print(f"[gen] {name} attempt {attempt+1} failed: {e}", flush=True)
            time.sleep(5)
print(f"[gen] DONE {done}/{len(JOBS)}", flush=True)
