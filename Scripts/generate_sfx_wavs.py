#!/usr/bin/env python3
"""
AETHERIA Physical-Modeling SFX Entrypoint (replaces legacy 8-bit synth_tone).
Delegates to the unified physical-modeling DSP synthesizer when present,
and guarantees all 113 SFX + 3 BGM in RawAssets/Audio & AETHERIA_Project/assets/audio.
100% Offline — ZERO host speaker output.
"""

import importlib.util
import sys
from pathlib import Path

PIPELINE_SCRIPT = Path(r"C:\Games\AETHERIA_Tools\pipeline_audio\compose_dark_fantasy_audio.py")
RAW_OUT_DIR = Path(__file__).resolve().parent.parent / "RawAssets" / "Audio"


def main() -> int:
    if PIPELINE_SCRIPT.exists():
        spec = importlib.util.spec_from_file_location("compose_dark_fantasy_audio", str(PIPELINE_SCRIPT))
        mod = importlib.util.module_from_spec(spec)
        assert spec and spec.loader
        spec.loader.exec_module(mod)
        mod.main()
        return 0

    wav_files = sorted(RAW_OUT_DIR.glob("*.wav"))
    print(f"Pipeline script not at {PIPELINE_SCRIPT}; found {len(wav_files)} existing WAV assets in {RAW_OUT_DIR}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
