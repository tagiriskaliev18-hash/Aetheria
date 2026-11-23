#!/usr/bin/env python3
"""
================================================================================
AETHERIA — Diablo IV Grade Physical Foley, Spell Impact & Sanctuary Audio
================================================================================
Multi-layered, organic audio synthesis engine matching Diablo IV & Yuki's
sound design (@blooodyuki). Completely eradicates cartoonish sine sweeps,
8-bit tonal chirps, and narrow resonator spikes.

Strict Physical Foley Architecture:
  * Layer 1: Sub-bass visceral thud (40-65 Hz resonant transient, kick/boom curve)
  * Layer 2: Real acoustic body transient (cracking stone, splintering wood,
             metallic clang, wet flesh impact) using shaped noise, decaying comb
             filters, and physical resonator modeling
  * Layer 3: High-frequency sizzle / debris (crackle, spark bursts, electrical arcs,
             glass/ice shards, blood mist spray)

Delivery Specifications:
  * Sample Rate: 48,000 Hz
  * Bit Depth: 16-bit PCM Stereo WAV
  * Peak Level: Normalized strictly to -1.0 dBFS (0.89125 peak amplitude)
  * DC Offset: 0.0000 (mean-subtracted + 26 Hz sub-audible highpass)
  * Spectral Purity: Zero pure sine concentrations (< 15% top-1 FFT bin peak)
  * Low-Frequency Rumble: Rich visceral energy in 40-80 Hz band

Outputs identically to BOTH:
  1) c:\\Users\\tagir\\OneDrive\\Desktop\\Aetheria\\GodotAetheria\\assets\\audio
  2) C:\\Games\\AETHERIA\\AetheriaData\\assets\\audio
================================================================================
"""

from __future__ import annotations

import os
import shutil
import time
from typing import Dict, List, Sequence, Tuple

import numpy as np
import scipy.io.wavfile as wavfile
import scipy.signal as signal

SR = 48000
PEAK_NEG_1DB = 10.0 ** (-1.0 / 20.0)  # ~0.8912509 (exact -1.0 dBFS)

TARGET_DIRS = [
    r"c:\Users\tagir\OneDrive\Desktop\Aetheria\GodotAetheria\assets\audio",
    r"C:\Games\AETHERIA\AetheriaData\assets\audio",
]


# ==============================================================================
# 1. COLORED NOISE, FILTERS & ACOUSTIC CONVOLUTION ENGINE
# ==============================================================================

class DSP:
    """High-definition physical DSP & acoustic space engine."""

    _irs: Dict[str, Tuple[np.ndarray, np.ndarray]] = {}

    @staticmethod
    def pink_noise(n: int, rng: np.random.Generator) -> np.ndarray:
        """Warm organic -3 dB/oct noise balanced cleanly above 50 Hz."""
        if n <= 16:
            return rng.standard_normal(n)
        white = rng.standard_normal(n)
        spec = np.fft.rfft(white)
        freqs = np.fft.rfftfreq(n, 1.0 / SR)
        scale = 1.0 / np.maximum(freqs, 55.0) ** 0.36
        scale[0] = 0.0
        out = np.fft.irfft(spec * scale, n=n)
        std = float(np.std(out))
        return out / max(std, 1e-9)

    @staticmethod
    def brown_noise(n: int, rng: np.random.Generator) -> np.ndarray:
        """Deep dark rumble noise without overwhelming sub pileup."""
        if n <= 16:
            return rng.standard_normal(n)
        white = rng.standard_normal(n)
        spec = np.fft.rfft(white)
        freqs = np.fft.rfftfreq(n, 1.0 / SR)
        scale = 1.0 / np.maximum(freqs, 50.0) ** 0.65
        scale[0] = 0.0
        out = np.fft.irfft(spec * scale, n=n)
        std = float(np.std(out))
        return out / max(std, 1e-9)

    @staticmethod
    def lowpass(x: np.ndarray, cutoff: float, order: int = 3) -> np.ndarray:
        c = max(22.0, min(cutoff, SR * 0.48))
        sos = signal.butter(order, c / (SR * 0.5), btype="low", output="sos")
        if x.ndim == 1:
            return signal.sosfilt(sos, x)
        return np.vstack([signal.sosfilt(sos, x[0]), signal.sosfilt(sos, x[1])])

    @staticmethod
    def highpass(x: np.ndarray, cutoff: float, order: int = 3) -> np.ndarray:
        c = max(16.0, min(cutoff, SR * 0.48))
        sos = signal.butter(order, c / (SR * 0.5), btype="high", output="sos")
        if x.ndim == 1:
            return signal.sosfilt(sos, x)
        return np.vstack([signal.sosfilt(sos, x[0]), signal.sosfilt(sos, x[1])])

    @staticmethod
    def bandpass(x: np.ndarray, low: float, high: float, order: int = 2) -> np.ndarray:
        lo = max(18.0, min(low, SR * 0.46))
        hi = max(lo + 20.0, min(high, SR * 0.48))
        sos = signal.butter(order, [lo / (SR * 0.5), hi / (SR * 0.5)], btype="band", output="sos")
        if x.ndim == 1:
            return signal.sosfilt(sos, x)
        return np.vstack([signal.sosfilt(sos, x[0]), signal.sosfilt(sos, x[1])])

    @staticmethod
    def band_noise(n: int, rng: np.random.Generator, low: float, high: float, order: int = 2) -> np.ndarray:
        """RMS-calibrated filtered pink noise with balanced spectral energy."""
        raw = DSP.bandpass(DSP.pink_noise(n, rng), low, high, order=order)
        rms = float(np.sqrt(np.mean(raw ** 2)))
        return raw / max(rms, 1e-6) * 0.35

    @staticmethod
    def resonant_bank(exc: np.ndarray, modes: Sequence[Tuple[float, float, float]]) -> np.ndarray:
        """Parallel second-order modal resonators with bandwidth-compensated transient energy."""
        out = np.zeros_like(exc, dtype=np.float64)
        for freq, bw, gain in modes:
            if freq <= 18.0 or freq >= SR * 0.46:
                continue
            bw_c = max(24.0, bw)
            r = np.exp(-np.pi * bw_c / SR)
            theta = 2.0 * np.pi * freq / SR
            a1 = -2.0 * r * np.cos(theta)
            a2 = r * r
            b0 = (1.0 - r * r) * 0.5 * gain * np.sqrt(110.0 / bw_c) * 1.85
            out += signal.lfilter([b0, 0.0, -b0], [1.0, a1, a2], exc)
        return out

    @staticmethod
    def soft_sat(x: np.ndarray, drive: float = 1.25) -> np.ndarray:
        if drive <= 0.05:
            return x
        return np.tanh(x * drive) / np.tanh(drive)

    @staticmethod
    def pan_stereo(mono: np.ndarray, pan: float = 0.0) -> np.ndarray:
        ang = (np.clip(pan, -1.0, 1.0) + 1.0) * (np.pi * 0.25)
        return np.vstack([mono * np.cos(ang), mono * np.sin(ang)])

    @staticmethod
    def stereo_decorrelate(mono: np.ndarray, width_ms: float = 0.45, seed: int = 1) -> np.ndarray:
        """Natural Haas/phase room spread without hollow comb filtering."""
        d = max(1, int(width_ms * 0.001 * SR))
        rng = np.random.default_rng(seed)
        r_shift = np.concatenate([np.zeros(d), mono[:-d]]) if d < len(mono) else mono.copy()
        micro_air = DSP.bandpass(DSP.pink_noise(len(mono), rng), 250.0, 3200.0) * (np.abs(mono) * 0.06)
        return np.vstack([mono + micro_air, r_shift - micro_air])

    @classmethod
    def get_ir(cls, space: str) -> Tuple[np.ndarray, np.ndarray]:
        if space in cls._irs:
            return cls._irs[space]

        rng = np.random.default_rng({"close": 7101, "hall": 7202, "cathedral": 7303}[space])
        if space == "close":
            dur = 0.20
            n = int(dur * SR)
            t = np.linspace(0.0, dur, n, endpoint=False)
            ir_l = cls.bandpass(cls.pink_noise(n, rng), 110.0, 3400.0) * np.exp(-t / 0.038) * 0.35
            ir_r = cls.bandpass(cls.pink_noise(n, rng), 110.0, 3400.0) * np.exp(-t / 0.038) * 0.35
            taps = [(3.4, 0.72, 0.28), (6.7, -0.36, 0.68), (11.2, 0.42, -0.34), (17.5, -0.22, 0.26)]
        elif space == "hall":
            dur = 0.95
            n = int(dur * SR)
            t = np.linspace(0.0, dur, n, endpoint=False)
            nl = rng.standard_normal(n)
            nr = rng.standard_normal(n)
            ir_l = (
                cls.bandpass(nl, 55.0, 420.0) * np.exp(-t / 0.28) * 0.48
                + cls.bandpass(nl, 420.0, 2600.0) * np.exp(-t / 0.21) * 0.72
                + cls.bandpass(nl, 2600.0, 5400.0) * np.exp(-t / 0.10) * 0.32
            )
            ir_r = (
                cls.bandpass(nr, 55.0, 420.0) * np.exp(-t / 0.28) * 0.48
                + cls.bandpass(nr, 420.0, 2600.0) * np.exp(-t / 0.21) * 0.72
                + cls.bandpass(nr, 2600.0, 5400.0) * np.exp(-t / 0.10) * 0.32
            )
            taps = [(8.5, 0.68, -0.38), (15.0, -0.45, 0.62), (24.0, 0.35, -0.28), (38.0, -0.24, 0.30)]
        else:  # cathedral
            dur = 2.20
            n = int(dur * SR)
            t = np.linspace(0.0, dur, n, endpoint=False)
            nl = rng.standard_normal(n)
            nr = rng.standard_normal(n)
            ir_l = (
                cls.bandpass(nl, 55.0, 380.0) * np.exp(-t / 0.62) * 0.48
                + cls.bandpass(nl, 380.0, 2400.0) * np.exp(-t / 0.45) * 0.76
                + cls.bandpass(nl, 2400.0, 5200.0) * np.exp(-t / 0.18) * 0.34
            )
            ir_r = (
                cls.bandpass(nr, 55.0, 380.0) * np.exp(-t / 0.62) * 0.48
                + cls.bandpass(nr, 380.0, 2400.0) * np.exp(-t / 0.45) * 0.76
                + cls.bandpass(nr, 2400.0, 5200.0) * np.exp(-t / 0.18) * 0.34
            )
            taps = [
                (13.0, 0.65, -0.45), (22.0, -0.46, 0.60), (34.0, 0.38, -0.32),
                (48.0, -0.28, 0.34), (65.0, 0.20, -0.22), (86.0, -0.14, 0.16),
            ]

        for d_ms, gl, gr in taps:
            idx = int(d_ms * 0.001 * SR)
            if idx < len(ir_l):
                ir_l[idx] += gl
                ir_r[idx] += gr

        mx = max(float(np.max(np.abs(ir_l))), float(np.max(np.abs(ir_r))), 1e-6)
        ir_l /= mx
        ir_r /= mx
        cls._irs[space] = (ir_l, ir_r)
        return ir_l, ir_r

    @classmethod
    def convolve(cls, audio: np.ndarray, space: str = "hall", wet: float = 0.18, dry: float = 0.90) -> np.ndarray:
        if audio.ndim == 1:
            audio = np.vstack([audio, audio])
        if space == "dry" or wet <= 0.001:
            return audio
        ir_l, ir_r = cls.get_ir(space)
        rl = signal.fftconvolve(audio[0], ir_l, mode="full")[: audio.shape[1]]
        rr = signal.fftconvolve(audio[1], ir_r, mode="full")[: audio.shape[1]]
        return np.vstack([dry * audio[0] + wet * rl, dry * audio[1] + wet * rr])

    @staticmethod
    def make_seamless_loop(audio: np.ndarray, crossfade_sec: float = 0.65) -> np.ndarray:
        xf = int(crossfade_sec * SR)
        n = audio.shape[1]
        if n <= xf * 2:
            return audio
        u = np.linspace(0.0, 1.0, xf)
        fade_out = np.cos(u * np.pi * 0.5)
        fade_in = np.sin(u * np.pi * 0.5)
        loop_len = n - xf
        out = np.copy(audio[:, :loop_len])
        tail = audio[:, loop_len:]
        out[:, :xf] = out[:, :xf] * fade_in + tail[:, :xf] * fade_out
        return out


# ==============================================================================
# 2. PHYSICAL FOLEY & DARK-FANTASY MULTI-LAYERED SYNTHESIS ENGINE
# ==============================================================================

class FoleySynth:
    """Organic, multi-layered physical audio generators matching Diablo IV."""

    @staticmethod
    def visceral_sub_thud(
        n: int,
        f_low: float = 40.0,
        f_high: float = 65.0,
        decay_s: float = 0.08,
        punch: float = 1.0,
        seed: int = 1,
    ) -> np.ndarray:
        """
        Layer 1 Sub-Bass Visceral Thud (40-65 Hz resonant transient).
        Uses coupled sub resonators and shaped brownian sub rumble with soft tanh saturation.
        Disperses energy naturally across 40-75 Hz so Top-1 FFT bin stays well below 10%.
        """
        rng = np.random.default_rng(seed)
        t = np.linspace(0.0, n / SR, n, endpoint=False)
        env = np.exp(-t / max(0.015, decay_s))

        # Coupled sub modal cluster (wide bandwidths prevent high-Q sine ringing)
        modes = [
            (f_low, 24.0, 1.45),
            (f_low * 1.25, 28.0, 1.15),
            (f_low * 1.50, 34.0, 0.85),
            (f_high, 40.0, 0.60),
        ]
        exc = DSP.pink_noise(n, rng) * env
        res = DSP.resonant_bank(exc, modes)

        # Brownian sub rumble foundation
        brown = DSP.brown_noise(n, rng)
        sub_rumble = DSP.bandpass(brown, max(25.0, f_low * 0.88), min(110.0, f_high * 1.5)) * env * 0.95

        # Transient pressure shockwave
        shock = (1.0 - 2.8 * (t / 0.008)) * np.exp(-t / 0.007) * (t < 0.025)
        shock = DSP.bandpass(shock, 45.0, 480.0) * 1.2

        sig = DSP.soft_sat((res * 0.75 + sub_rumble + shock * 0.6) * punch * 1.35, drive=1.35)
        return sig

    @staticmethod
    def footstep_sub(n: int, f_low: float = 48.0, f_high: float = 85.0, decay_s: float = 0.026, punch: float = 0.95, seed: int = 1) -> np.ndarray:
        """Clean broadband footstep sub-bass punch (evenly spread, zero pure sine peak)."""
        rng = np.random.default_rng(seed)
        t = np.linspace(0.0, n / SR, n, endpoint=False)
        env = np.exp(-t / max(0.012, decay_s))
        sub = DSP.band_noise(n, rng, f_low, f_high) * env * punch
        return sub

    @staticmethod
    def flesh_impact(dur: float = 0.32, weight: float = 1.0, wetness: float = 1.0, seed: int = 100) -> np.ndarray:
        """
        Diablo IV Visceral Flesh Impact:
          - Layer 1: Sub-bass visceral thud (42-60 Hz chest-cavity thump)
          - Layer 2: Wet visceral squelch, muscle tear, cartilage snap & tissue resonance
          - Layer 3: Blood splatter droplets & high-frequency mist spray (2.8k-7kHz)
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        # Layer 1: Sub-bass thud
        sub = FoleySynth.visceral_sub_thud(n, f_low=42.0, f_high=62.0, decay_s=0.045 * weight, punch=1.25 * weight, seed=seed + 1)

        # Layer 2: Real acoustic body transient (visceral tissue deformation & cartilage fracture)
        tissue_modes = [
            (135.0, 65.0, 1.35),
            (280.0, 110.0, 1.15),
            (540.0, 180.0, 0.95),
            (920.0, 260.0, 0.65),
            (1450.0, 400.0, 0.40),
        ]
        exc = DSP.pink_noise(n, rng) * np.exp(-t / 0.038)
        tissue_body = DSP.resonant_bank(exc, tissue_modes) * 1.35

        # Viscous fluid displacement squelch bursts
        squelch = np.zeros(n)
        squelch += DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.018), 180.0, 850.0) * 1.4
        t_off1 = int(0.011 * SR)
        squelch[t_off1:] += DSP.bandpass(DSP.pink_noise(n - t_off1, rng) * np.exp(-t[:n - t_off1] / 0.014), 320.0, 1400.0) * 1.15
        t_off2 = int(0.026 * SR)
        squelch[t_off2:] += DSP.bandpass(DSP.pink_noise(n - t_off2, rng) * np.exp(-t[:n - t_off2] / 0.012), 480.0, 2200.0) * 0.95

        # Cartilage crack
        snap = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.007), 850.0, 2800.0) * 1.25

        # Layer 3: High-frequency blood mist & wet spray sizzle
        sizzle = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.016), 2800.0, 6800.0) * (0.85 * wetness)
        for _ in range(6):
            di = int(rng.uniform(0.012, min(dur * 0.65, 0.18)) * SR)
            dl = min(n - di, int(0.015 * SR))
            if dl > 12:
                dt = np.linspace(0.0, dl / SR, dl, endpoint=False)
                drop = DSP.bandpass(DSP.pink_noise(dl, rng) * np.exp(-dt / 0.0025), 3200.0, 7500.0)
                sizzle[di:di + dl] += drop * rng.uniform(0.3, 0.75) * wetness

        mixed = DSP.soft_sat(sub * 0.9 + (tissue_body + squelch + snap) * 1.1 + sizzle * 0.8, 1.28)
        return DSP.highpass(mixed, 32.0)

    @staticmethod
    def armor_impact(dur: float = 0.38, hardness: float = 0.85, damp: float = 0.09, seed: int = 200) -> np.ndarray:
        """
        Diablo IV Heavy Forged Armor Impact:
          - Layer 1: Sub-bass blunt shockwave transferring through the chest behind armor (42-75 Hz)
          - Layer 2: Multi-mode forged iron plate resonance + chainmail rattle
          - Layer 3: Steel sparks and glancing deflection scrape transients (3.2k-8kHz)
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        # Layer 1: Sub-bass blunt shockwave transferring through cuirass
        sub = FoleySynth.visceral_sub_thud(n, f_low=42.0, f_high=68.0, decay_s=0.065, punch=1.35, seed=seed + 1)
        sub_transfer = DSP.band_noise(n, rng, 42.0, 80.0) * np.exp(-t / 0.055) * 1.45

        # Layer 2: 14 dense plate modes (noise-excited, broad bandwidth to prevent sine spikes)
        ratios = [1.0, 1.32, 1.68, 2.11, 2.52, 2.98, 3.48, 4.02, 4.61, 5.24, 5.92, 6.65, 7.45, 8.35]
        modes = []
        f0 = 260.0
        for idx, r in enumerate(ratios):
            mf = f0 * r
            if mf < SR * 0.42:
                bw = (45.0 + 25.0 * idx) / max(0.35, damp * 3.5)
                gain = (1.1 / (1.0 + 0.22 * idx)) * (0.5 + 0.5 * hardness if idx > 2 else 1.0)
                modes.append((mf, min(520.0, bw), gain))

        exc = DSP.pink_noise(n, rng) * np.exp(-t / max(0.025, damp))
        plate = DSP.resonant_bank(exc, modes) * 1.35

        # Gambeson backing thud + chainmail chink
        gambeson = DSP.band_noise(n, rng, 140.0, 520.0) * np.exp(-t / 0.035) * 1.15
        chainmail = np.zeros(n)
        for _ in range(5):
            ci = int(rng.uniform(0.008, 0.075) * SR)
            cl = min(n - ci, int(0.020 * SR))
            if cl > 12:
                ct = np.linspace(0.0, cl / SR, cl, endpoint=False)
                c_chink = DSP.bandpass(DSP.pink_noise(cl, rng) * np.exp(-ct / 0.004), 1800.0, 4800.0)
                chainmail[ci:ci + cl] += c_chink * rng.uniform(0.4, 0.85)

        # Layer 3: Steel sparks & gouging scrape transient
        scrape = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.014), 3200.0, 7800.0) * 1.15 * hardness

        mixed = DSP.soft_sat(sub + sub_transfer + plate + gambeson + chainmail + scrape, 1.25)
        return DSP.highpass(mixed, 35.0)

    @staticmethod
    def melee_swing(dur: float = 0.34, blade_weight: float = 1.0, seed: int = 300) -> np.ndarray:
        """
        Diablo IV Melee Attack Swing:
          - Layer 1: Air displacement pressure wave (45-75 Hz whoosh)
          - Layer 2: Aerodynamic vortex slip & cleave velocity + leather hilt creak
          - Layer 3: Razor edge whistling slice transient (2.5k-6.5kHz)
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)
        u = t / dur
        vel = np.sin(np.pi * np.clip(u / 0.75, 0.0, 1.0)) ** 1.3

        # Layer 1: Sub-bass displacement whoosh
        sub = FoleySynth.visceral_sub_thud(n, f_low=45.0, f_high=75.0, decay_s=0.075, punch=0.85 * blade_weight, seed=seed + 1)
        sub_whoosh = DSP.band_noise(n, rng, 45.0, 95.0) * vel * 0.85 * blade_weight

        # Layer 2: Aerodynamic vortex body & blade resonance
        mid_whoosh = DSP.band_noise(n, rng, 180.0, 1150.0) * vel * 1.35
        hilt_creak = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.045), 450.0, 2400.0) * 0.65

        # Steel blade resonance (broad modes)
        blade_modes = [(320.0, 75.0, 1.1), (580.0, 110.0, 0.85), (1150.0, 200.0, 0.55)]
        blade_sing = DSP.resonant_bank(DSP.pink_noise(n, rng) * vel * np.exp(-t / 0.12), blade_modes) * 0.75

        # Layer 3: Razor edge whistling slice
        slice_burst = DSP.bandpass(DSP.pink_noise(n, rng) * (vel ** 2.0), 2600.0, 6500.0) * 1.15

        mixed = DSP.soft_sat(sub * 0.75 + sub_whoosh + mid_whoosh + hilt_creak + blade_sing + slice_burst, 1.2)
        return DSP.highpass(mixed, 35.0)

    @staticmethod
    def attack_slam(dur: float = 0.72, weight: float = 1.35, grit: float = 0.85, seed: int = 400) -> np.ndarray:
        """
        Diablo IV Heavy Ground Attack Slam:
          - Layer 1: Seismic 40-58 Hz sub-bass ground thump (260ms decaying rumble)
          - Layer 2: Fracturing stone slab body + multi-point granite fracture impulses
          - Layer 3: Flying gravel, mineral shards & pebble scatter (1.2k-5kHz)
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        # Layer 1: Sub-bass seismic boom
        sub = FoleySynth.visceral_sub_thud(n, f_low=40.0, f_high=58.0, decay_s=0.24 * weight, punch=1.45 * weight, seed=seed + 1)
        sub_rumble = DSP.band_noise(n, rng, 38.0, 75.0) * np.exp(-t / 0.28) * 1.25 * weight

        # Layer 2: Granite fracture modal body
        exc = DSP.pink_noise(n, rng) * np.exp(-t / 0.055)
        stone_modes = [
            (110.0, 55.0, 1.3),
            (185.0, 85.0, 1.2),
            (320.0, 125.0, 1.0),
            (560.0, 180.0, 0.8),
            (950.0, 260.0, 0.55),
        ]
        rock_body = DSP.resonant_bank(exc, stone_modes) * 1.55

        # Triple shock fracture impulses
        fracture = DSP.band_noise(n, rng, 450.0, 2800.0) * np.exp(-t / 0.022) * 1.45
        t1 = int(0.016 * SR)
        fracture[t1:] += DSP.band_noise(n - t1, rng, 650.0, 3200.0) * np.exp(-t[:n - t1] / 0.018) * 1.15
        t2 = int(0.038 * SR)
        fracture[t2:] += DSP.band_noise(n - t2, rng, 850.0, 3600.0) * np.exp(-t[:n - t2] / 0.015) * 0.95

        # Layer 3: Mineral crunch & pebble scatter
        debris = DSP.band_noise(n, rng, 1200.0, 4800.0) * np.exp(-t / 0.16) * (0.85 * grit)
        for _ in range(int(14 * grit)):
            pi = int(rng.uniform(0.025, min(dur * 0.8, 0.50)) * SR)
            pl = min(n - pi, int(0.022 * SR))
            if pl > 12:
                pt = np.linspace(0.0, pl / SR, pl, endpoint=False)
                pf = rng.uniform(750.0, 2800.0)
                peb = DSP.resonant_bank(DSP.pink_noise(pl, rng) * np.exp(-pt / 0.005), [(pf, 140.0, 1.0)])
                debris[pi:pi + pl] += peb * rng.uniform(0.35, 0.9)

        mixed = DSP.soft_sat(sub + sub_rumble + rock_body + fracture + debris, 1.28)
        return DSP.highpass(mixed, 32.0)

    @staticmethod
    def fireball_cast(dur: float = 0.56, seed: int = 500) -> np.ndarray:
        """
        Diablo IV Fireball Cast:
          - Layer 1: Sub-bass combustion ignition thud (45-65 Hz)
          - Layer 2: Roaring furnace vortex intake & gas expansion
          - Layer 3: Flint ignition spark snap & ember crackles
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub = FoleySynth.visceral_sub_thud(n, f_low=45.0, f_high=65.0, decay_s=0.075, punch=1.15, seed=seed + 1)
        swell = np.sin(np.pi * np.clip(t / dur, 0.0, 1.0)) ** 0.8
        roar = FoleySynth.fire_roar(dur, intensity=1.25, low_hz=110.0, high_hz=1450.0, crackle=1.5, seed=seed + 2) * swell
        draft = DSP.band_noise(n, rng, 160.0, 850.0) * np.exp(-t / 0.18) * 1.25
        flint = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.009), 2200.0, 6200.0) * 1.45

        mixed = DSP.soft_sat(sub * 0.9 + roar * 1.1 + draft + flint, 1.22)
        return DSP.highpass(mixed, 36.0)

    @staticmethod
    def fireball_impact(dur: float = 1.45, seed: int = 600) -> np.ndarray:
        """
        Diablo IV Fireball Boom & Impact:
          - Layer 1: Massive 40-55 Hz atmospheric explosion shockwave (350ms visceral boom)
          - Layer 2: High-pressure thermal detonation blast + shattered masonry
          - Layer 3: Flying cinder scatter & sizzling resin ember crackles
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub = FoleySynth.visceral_sub_thud(n, f_low=40.0, f_high=55.0, decay_s=0.34, punch=1.55, seed=seed + 1)
        sub_boom = DSP.band_noise(n, rng, 38.0, 80.0) * np.exp(-t / 0.32) * 1.45
        blast = DSP.band_noise(n, rng, 110.0, 2400.0) * np.exp(-t / 0.22) * 1.55
        fire_tail = FoleySynth.fire_roar(dur, intensity=1.35, low_hz=120.0, high_hz=2200.0, crackle=2.2, seed=seed + 2) * np.exp(-t / 0.52)
        masonry = FoleySynth.stone_impact(min(dur, 0.85), weight=1.4, grit=0.9, seed=seed + 3)
        l_mas = min(len(masonry), n)
        cinders = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.25), 2400.0, 7200.0) * 0.95

        sig = sub + sub_boom + blast + fire_tail + cinders
        sig[:l_mas] += masonry[:l_mas] * 0.85
        return DSP.highpass(DSP.soft_sat(sig, 1.28), 32.0)

    @staticmethod
    def lightning_bolt(dur: float = 1.15, seed: int = 700) -> np.ndarray:
        """
        Diablo IV Lightning Bolt / Arc Discharge:
          - Layer 1: Heavy sub-bass displacement thunder (42-65 Hz)
          - Layer 2: Dielectric plasma breakdown (4 asymmetric N-wave shocks + ozone rip)
          - Layer 3: Corona arc discharges & high-voltage spark showers (3.5k-9kHz)
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub = FoleySynth.visceral_sub_thud(n, f_low=42.0, f_high=65.0, decay_s=0.26, punch=1.25, seed=seed + 1)
        thunder_roll = DSP.band_noise(n, rng, 42.0, 110.0) * np.exp(-t / 0.30) * 1.15
        plasma = FoleySynth.plasma_lightning(dur, hops=4, thunder=0.85, seed=seed + 2)
        corona = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.18), 3500.0, 8800.0) * 1.25

        mixed = DSP.soft_sat(sub * 0.9 + thunder_roll + plasma * 1.2 + corona, 1.26)
        return DSP.highpass(mixed, 34.0)

    @staticmethod
    def blackhole_loop(dur: float = 3.2, crossfade_s: float = 0.75, seed: int = 800) -> np.ndarray:
        """
        Diablo IV Black Hole / Singularity Ambience Loop:
          - Layer 1: Continuous 40-60 Hz gravitational vortex sub (ZERO pure sine tones!)
          - Layer 2: Spacetime accretion drag & churning void winds
          - Layer 3: Dark matter ionization & particle tearing sizzle
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub_l = DSP.band_noise(n, rng, 38.0, 68.0) * 1.35
        sub_r = DSP.band_noise(n, rng, 38.0, 68.0) * 1.35
        swirl_l = DSP.band_noise(n, rng, 110.0, 680.0) * 1.15
        swirl_r = DSP.band_noise(n, rng, 110.0, 680.0) * 1.15

        lfo = 0.85 + 0.25 * np.sin(2.0 * np.pi * 0.75 * t)
        sub_l *= lfo
        sub_r *= (0.85 + 0.25 * np.cos(2.0 * np.pi * 0.75 * t))
        wisp = DSP.bandpass(DSP.pink_noise(n, rng), 1800.0, 5200.0) * 0.55

        stereo = np.vstack([
            DSP.soft_sat(sub_l + swirl_l + wisp, 1.18),
            DSP.soft_sat(sub_r + swirl_r + wisp, 1.18),
        ])
        loop = DSP.make_seamless_loop(stereo, crossfade_sec=crossfade_s)
        return loop

    @staticmethod
    def cataclysm_nova(dur: float = 1.60, seed: int = 900) -> np.ndarray:
        """
        Diablo IV Cataclysm Nova:
          - Layer 1: Apocalyptic 40-52 Hz tectonic sub-rumble (420ms decay)
          - Layer 2: Continental crust bedrock rupture & obsidian upheaval
          - Layer 3: Flying quartz crystal shards & electrical discharge sparks
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub = FoleySynth.visceral_sub_thud(n, f_low=40.0, f_high=52.0, decay_s=0.38, punch=1.65, seed=seed + 1)
        sub_wave = DSP.band_noise(n, rng, 35.0, 75.0) * np.exp(-t / 0.42) * 1.35

        eruption = np.zeros(n)
        for k in range(4):
            off = int(k * 0.055 * SR)
            sp = FoleySynth.stone_impact(dur - (off / SR), weight=1.6 - k * 0.15, grit=1.1, seed=seed + 10 + k)
            ll = min(len(sp), n - off)
            eruption[off:off + ll] += sp[:ll] * (1.1 - 0.12 * k)

        shards = FoleySynth.crystal_shard_cluster(dur, f0=380.0, count=9, spread=0.024, seed=seed + 20) * 0.95
        arcs = FoleySynth.plasma_lightning(dur, hops=3, thunder=0.3, seed=seed + 30) * 0.75

        mixed = DSP.soft_sat(sub + sub_wave + eruption * 1.15 + shards + arcs, 1.28)
        return DSP.highpass(mixed, 30.0)

    @staticmethod
    def macropyre(dur: float = 1.70, seed: int = 1000) -> np.ndarray:
        """
        Diablo IV Macropyre (Volcanic Column):
          - Layer 1: Continuous roaring magma lake sub-bass (45-68 Hz)
          - Layer 2: Convective flame vortex & boiling basalt stone churn
          - Layer 3: Continuous resin ember detonation shower & ash sizzle
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub = FoleySynth.visceral_sub_thud(n, f_low=45.0, f_high=68.0, decay_s=0.45, punch=1.35, seed=seed + 1)
        sub_rumble = DSP.band_noise(n, rng, 42.0, 78.0) * np.exp(-t / 0.55) * 1.35
        fire_column = FoleySynth.fire_roar(dur, intensity=1.55, low_hz=95.0, high_hz=1850.0, crackle=2.4, seed=seed + 2) * np.exp(-t / 0.65)
        rock_fracture = FoleySynth.stone_impact(dur, weight=1.25, grit=0.75, seed=seed + 3) * 0.85
        embers = DSP.bandpass(DSP.pink_noise(n, rng) * np.exp(-t / 0.40), 2200.0, 7200.0) * 0.95

        sig = sub + sub_rumble + fire_column + rock_fracture + embers
        return DSP.highpass(DSP.soft_sat(sig, 1.26), 34.0)

    @staticmethod
    def footstep_stone(variation: int = 1, seed: int = 1100) -> np.ndarray:
        """
        Diablo IV Tactical Stone Footstep:
          - Layer 1: 48-85 Hz boot heel ground transfer (22ms decay)
          - Layer 2: Granite acoustic slap & leather sole friction (240-950 Hz)
          - Layer 3: Natural stone dust and pebble micro-crunch (1.2k-4.4kHz)
        """
        rng = np.random.default_rng(seed + variation * 37)
        dur = 0.20
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        # Layer 1: Broadband boot heel punch (no narrow IIR mode -> Top-1 < 5%)
        heel = FoleySynth.footstep_sub(n, f_low=48.0, f_high=85.0, decay_s=0.026, punch=0.95, seed=seed + 10 + variation)

        # Layer 2: Flagstone slab slap + leather sole roll
        t_roll = np.maximum(0.0, t - 0.024)
        slab = DSP.band_noise(n, rng, 240.0, 950.0) * np.exp(-t / 0.018) * 1.15
        sole = DSP.band_noise(n, rng, 420.0, 2200.0) * np.exp(-t_roll / 0.018) * (t >= 0.022) * 0.95

        # Layer 3: Micro-pebble scatter & granite grit
        grit = DSP.band_noise(n, rng, 1200.0, 4400.0) * np.exp(-t / 0.035) * 0.65

        mixed = DSP.soft_sat(heel + slab + sole + grit, 1.15)
        return DSP.highpass(mixed, 40.0)

    @staticmethod
    def stone_impact(dur: float = 0.45, weight: float = 1.0, grit: float = 0.75, seed: int = 2) -> np.ndarray:
        """Heavy basalt/granite impact + fractured mineral crunch + pebble trickle."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub = FoleySynth.visceral_sub_thud(n, 45.0, 70.0, decay_s=0.065 * min(1.5, weight), punch=min(1.1, 0.85 * weight), seed=seed + 1)
        exc = DSP.pink_noise(n, rng) * np.exp(-t / (0.040 * max(0.6, min(1.5, weight))))
        stone_modes = [
            (115.0, 60.0, 0.95), (195.0, 95.0, 1.10), (340.0, 140.0, 1.05),
            (580.0, 200.0, 0.88), (980.0, 300.0, 0.65), (1650.0, 480.0, 0.42),
        ]
        crunch = DSP.resonant_bank(exc, stone_modes) * 1.25
        crack = DSP.band_noise(n, rng, 580.0, 3600.0) * np.exp(-t / 0.012) * (0.85 + 0.35 * weight)

        debris = np.zeros(n)
        if grit > 0.05:
            dust_env = np.clip((t - 0.014) / 0.028, 0.0, 1.0) * np.exp(-t / (0.14 + 0.12 * weight))
            debris += DSP.band_noise(n, rng, 900.0, 4200.0) * dust_env * 0.72 * grit
            for _ in range(int(9 * grit)):
                p_idx = int(rng.uniform(0.025, min(dur * 0.75, 0.45)) * SR)
                plen = min(n - p_idx, int(0.022 * SR))
                if plen > 16:
                    pt = np.linspace(0.0, plen / SR, plen, endpoint=False)
                    pe = DSP.pink_noise(plen, rng) * np.exp(-pt / 0.005)
                    pf = rng.uniform(620.0, 2400.0)
                    peb = DSP.resonant_bank(pe, [(pf, 140.0, 1.0), (pf * 1.65, 200.0, 0.6)])
                    debris[p_idx:p_idx + plen] += peb * rng.uniform(0.35, 0.85)

        return DSP.highpass(DSP.soft_sat(sub + crunch + crack + debris, 1.2), 38.0)

    @staticmethod
    def wood_oak_impact(dur: float = 0.42, f0: float = 140.0, splinter: float = 0.7, seed: int = 3) -> np.ndarray:
        """Seasoned oak impact + fibrous splinter snaps."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        thud = FoleySynth.visceral_sub_thud(n, 52.0, 85.0, decay_s=0.045, punch=0.85, seed=seed + 1)
        exc = DSP.pink_noise(n, rng) * np.exp(-t / 0.026)
        modes = [
            (f0, 60.0, 1.3), (f0 * 1.48, 85.0, 1.0), (f0 * 2.25, 130.0, 0.8),
            (f0 * 3.15, 180.0, 0.55), (f0 * 4.5, 250.0, 0.35),
        ]
        body = DSP.resonant_bank(exc, modes) * 1.55

        snaps = np.zeros(n)
        if splinter > 0.05:
            for k in range(int(7 + 14 * splinter)):
                s_idx = int((0.004 if k == 0 else rng.uniform(0.01, min(dur * 0.65, 0.32))) * SR)
                slen = min(n - s_idx, int(0.018 * SR))
                if slen > 12:
                    st = np.linspace(0.0, slen / SR, slen, endpoint=False)
                    cr = DSP.bandpass(DSP.pink_noise(slen, rng) * np.exp(-st / 0.0035), 850.0, 4600.0)
                    snaps[s_idx:s_idx + slen] += cr * rng.uniform(0.45, 1.15) * splinter

        return DSP.highpass(DSP.soft_sat(thud + body + snaps, 1.15), 42.0)

    @staticmethod
    def wrought_iron_impact(dur: float = 0.42, f0: float = 290.0, hardness: float = 0.75, damp: float = 0.14, seed: int = 4) -> np.ndarray:
        """Heavy wrought-iron plate clank without narrow high-Q sine ring."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        backing = FoleySynth.visceral_sub_thud(n, 55.0, 85.0, decay_s=0.040, punch=0.82, seed=seed + 1)
        clack = DSP.bandpass(DSP.pink_noise(n, rng), 550.0, 4200.0 * (0.5 + 0.5 * hardness)) * np.exp(-t / 0.012) * 1.35

        exc = DSP.pink_noise(n, rng) * np.exp(-t / max(0.025, damp))
        ratios = [1.0, 1.29, 1.63, 2.04, 2.51, 3.08, 3.74, 4.49, 5.33, 6.27, 7.35, 8.60]
        modes = []
        for idx, r in enumerate(ratios):
            mf = f0 * r
            if mf < SR * 0.42:
                bw = (45.0 + 25.0 * idx) / max(0.35, damp * 3.5)
                gain = (1.15 / (1.0 + 0.25 * idx)) * (0.45 + 0.55 * hardness if idx > 2 else 1.0)
                modes.append((mf, min(520.0, bw), gain))
        plate_noise = DSP.resonant_bank(exc, modes) * 1.85

        return DSP.highpass(DSP.soft_sat(backing + clack + plate_noise, 1.2), 42.0)

    @staticmethod
    def bone_impact(dur: float = 0.34, count: int = 5, pitch_scale: float = 1.0, seed: int = 5) -> np.ndarray:
        """Dry hollow calcium bone crack & rattle."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        out = FoleySynth.visceral_sub_thud(n, 65.0, 95.0, decay_s=0.032, punch=0.65, seed=seed + 1)
        for i in range(count):
            d_t = 0.0 if i == 0 else rng.uniform(0.015, dur * 0.58)
            idx = int(d_t * SR)
            blen = min(n - idx, int(0.075 * SR))
            if blen < 16:
                continue
            bt = np.linspace(0.0, blen / SR, blen, endpoint=False)
            exc = DSP.pink_noise(blen, rng) * np.exp(-bt / 0.005)
            fb = rng.uniform(460.0, 1150.0) * pitch_scale
            clack = DSP.resonant_bank(exc, [(fb, 120.0, 1.25), (fb * 1.68, 160.0, 0.95), (fb * 2.55, 220.0, 0.6)])
            amp = 1.35 if i == 0 else rng.uniform(0.35, 0.85) * np.exp(-d_t / 0.22)
            out[idx:idx + blen] += clack * amp
        return DSP.highpass(DSP.soft_sat(out, 1.15), 55.0)

    @staticmethod
    def crystal_shard_cluster(dur: float = 0.75, f0: float = 620.0, count: int = 5, spread: float = 0.035, seed: int = 6) -> np.ndarray:
        """Dark-fantasy quartz/ice shards (broad impact + modal noise)."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        out = FoleySynth.visceral_sub_thud(n, 58.0, 95.0, decay_s=0.045, punch=0.55, seed=seed + 1)
        for i in range(count):
            idx = int(min(dur * 0.45, i * spread) * SR)
            rlen = n - idx
            if rlen < 32:
                continue
            rt = np.linspace(0.0, rlen / SR, rlen, endpoint=False)
            freq = f0 * (1.0 + 0.16 * i + rng.uniform(-0.05, 0.05))
            exc = DSP.pink_noise(rlen, rng) * np.exp(-rt / 0.012)
            modes = []
            for m_idx, ratio in enumerate([1.0, 2.38, 3.85, 5.40]):
                mf = freq * ratio
                if mf < SR * 0.42:
                    modes.append((mf, 75.0 + 45.0 * m_idx, 1.0 / (1.0 + 0.5 * m_idx)))
            ring = DSP.resonant_bank(exc, modes) * np.exp(-rt / (0.18 + 0.06 * (i % 3)))
            grit = DSP.bandpass(DSP.pink_noise(rlen, rng), freq * 0.55, min(6800.0, freq * 3.0)) * np.exp(-rt / 0.06) * 0.55
            out[idx:] += (ring * 1.45 + grit) * (0.9 / (1.0 + 0.18 * i))
        return DSP.highpass(out, 50.0)

    @staticmethod
    def fire_roar(dur: float = 0.85, intensity: float = 1.0, low_hz: float = 110.0, high_hz: float = 1650.0, crackle: float = 1.2, seed: int = 7) -> np.ndarray:
        """Turbulent furnace draft + organic flame roar + resin ember crackles."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        lfo_raw = DSP.lowpass(rng.standard_normal(n), 15.0, order=2)
        lfo = np.clip(0.74 + 0.42 * (lfo_raw / max(float(np.std(lfo_raw)), 1e-6)), 0.28, 1.45)

        draft = DSP.band_noise(n, rng, 65.0, max(180.0, low_hz * 1.4)) * lfo * 0.55 * intensity
        roar = DSP.band_noise(n, rng, low_hz, high_hz) * lfo * 1.25 * intensity

        pops = np.zeros(n)
        for _ in range(int(dur * 32 * crackle)):
            p_idx = int(rng.uniform(0.0, max(0.02, dur - 0.015)) * SR)
            plen = min(n - p_idx, int(0.012 * SR))
            if plen > 12:
                pt = np.linspace(0.0, plen / SR, plen, endpoint=False)
                pop = DSP.bandpass(DSP.pink_noise(plen, rng) * np.exp(-pt / 0.0020), 1400.0, 6200.0)
                pops[p_idx:p_idx + plen] += pop * rng.uniform(0.65, 1.55) * intensity

        return DSP.soft_sat(draft + roar + pops * 1.15, 1.15)

    @staticmethod
    def plasma_lightning(dur: float = 1.0, hops: int = 4, thunder: float = 0.85, seed: int = 8) -> np.ndarray:
        """Asymmetric N-wave pressure shock impulses + ozone micro-arcs + rolling thunder."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)
        arcs = np.zeros(n)

        for h in range(hops):
            ht = 0.0 if h == 0 else h * 0.045 + rng.uniform(0.0, 0.028)
            idx = int(ht * SR)
            alen = min(n - idx, int(0.085 * SR))
            if alen < 24:
                continue
            at = np.linspace(0.0, alen / SR, alen, endpoint=False)
            nwave = (1.0 - 2.2 * (at / 0.0045)) * np.exp(-at / 0.0038) * (at < 0.018)
            nwave = DSP.bandpass(nwave, 140.0, 5200.0) * 1.4
            snap = DSP.bandpass(DSP.pink_noise(alen, rng) * np.exp(-at / 0.007), 900.0, 6800.0) * 1.65
            sparks = np.zeros(alen)
            for _ in range(7):
                si = int(rng.uniform(0.0, max(0.005, alen / SR - 0.008)) * SR)
                sl = min(alen - si, int(0.007 * SR))
                if sl > 8:
                    sparks[si:si + sl] += DSP.bandpass(rng.standard_normal(sl), 1800.0, 7400.0) * np.exp(-np.linspace(0, 1, sl) / 0.25) * 0.9
            arcs[idx:idx + alen] += (nwave + snap + sparks) * (0.90 ** h)

        roll = np.zeros(n)
        if thunder > 0.05:
            swell = DSP.lowpass(rng.standard_normal(n), 9.0, order=2)
            swell = 0.65 + 0.45 * np.tanh(swell * 1.8)
            th_env = np.exp(-t / (0.22 + 0.22 * thunder)) * swell
            roll = DSP.band_noise(n, rng, 45.0, 280.0) * th_env * 0.85 * thunder
            roll += DSP.band_noise(n, rng, 240.0, 1100.0) * np.exp(-t / 0.14) * 0.68 * thunder

        return DSP.soft_sat(arcs * 1.45 + roll, 1.25)

    @staticmethod
    def helmholtz_creak(dur: float = 0.65, rate_start: float = 24.0, rate_end: float = 44.0, body_freqs: Tuple[float, ...] = (135.0, 295.0, 620.0), seed: int = 9) -> np.ndarray:
        """Realistic stick-slip wood/iron hinge friction creak."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)
        u = t / max(dur, 1e-6)
        exc = np.zeros(n)
        cur_t = 0.01
        while cur_t < dur - 0.02:
            rate = rate_start + (rate_end - rate_start) * u[min(n - 1, int(cur_t * SR))] + rng.uniform(-5.0, 6.0)
            rate = max(12.0, rate)
            dt = (1.0 / rate) * rng.uniform(0.75, 1.28)
            idx = int(cur_t * SR)
            plen = min(n - idx, int(0.012 * SR))
            if plen > 6:
                pt = np.linspace(0.0, plen / SR, plen, endpoint=False)
                exc[idx:idx + plen] += DSP.pink_noise(plen, rng) * np.exp(-pt / 0.0028) * rng.uniform(0.5, 1.2)
            cur_t += dt
        modes = [(f, f * 0.20 + 35.0, 1.2 / (i + 1)) for i, f in enumerate(body_freqs)]
        creak = DSP.resonant_bank(exc, modes)
        env = np.sin(np.pi * u) ** 0.7
        return DSP.soft_sat(creak * env * 1.8, 1.15)

    @staticmethod
    def sanctuary_bronze_bell(f0: float = 110.0, dur: float = 2.4, dark_weight: float = 1.0, shimmer: float = 0.65, seed: int = 10) -> np.ndarray:
        """
        Diablo IV Legendary Sanctuary Bronze Bell-Gong:
        Sub-bass chest bloom + iron clapper thud + 24 chorus-detuned bronze partials + cathedral mist.
        """
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        sub_boom = FoleySynth.visceral_sub_thud(n, 40.0, 60.0, decay_s=min(0.35, dur * 0.18), punch=1.2 * dark_weight, seed=seed + 1)
        sub_pad = DSP.band_noise(n, rng, 42.0, 95.0) * np.exp(-t / (dur * 0.25)) * 0.85 * dark_weight
        clapper = DSP.band_noise(n, rng, 420.0, 3800.0) * np.exp(-t / 0.015) * 1.25

        ratios = [
            0.50, 0.75, 1.00, 1.189, 1.335, 1.502, 1.73, 2.00,
            2.24, 2.51, 2.756, 3.01, 3.38, 3.74, 4.07, 4.52,
            4.98, 5.45, 6.02, 6.68, 7.35, 8.12, 9.04, 10.10,
        ]
        weights = [
            0.14, 0.22, 0.48, 0.56, 0.55, 0.78, 0.64, 0.84,
            0.68, 0.62, 0.70, 0.58, 0.48, 0.42, 0.44, 0.36,
            0.30, 0.25, 0.21, 0.18, 0.15, 0.12, 0.10, 0.08,
        ]
        bell = np.zeros(n)
        for idx, (r, w) in enumerate(zip(ratios, weights)):
            freq = f0 * r
            if freq >= SR * 0.40:
                continue
            tau = dur * (0.54 / (1.0 + 0.20 * idx))
            env = (1.0 - np.exp(-t / 0.004)) * np.exp(-t / max(0.04, tau))
            for detune in (-0.0055, 0.0, 0.0058):
                ph = 2.0 * np.pi * (freq * (1.0 + detune)) * t + rng.uniform(0.0, 2.0 * np.pi)
                bell += np.sin(ph) * env * (w * 0.33)

        bell_noise_modes = [(f0 * r, max(45.0, f0 * r * 0.15), 1.15 / (i + 1)) for i, r in enumerate(ratios[2:14])]
        bronze_air = DSP.resonant_bank(DSP.pink_noise(n, rng) * np.exp(-t / (dur * 0.34)), bell_noise_modes) * 1.45

        mist = np.zeros(n)
        if shimmer > 0.05:
            mist_env = np.clip((t - 0.03) / 0.14, 0.0, 1.0) * np.exp(-t / (dur * 0.44))
            mist = DSP.band_noise(n, rng, 420.0, 3400.0) * mist_env * 0.95 * shimmer

        mono = DSP.soft_sat(sub_boom + sub_pad + clapper + bell * 1.65 + bronze_air + mist, drive=1.12)
        return DSP.convolve(DSP.pan_stereo(mono, -0.05), space="cathedral", wet=0.24, dry=0.88)

    @staticmethod
    def organic_choir(chord_freqs: Sequence[float], dur: float = 2.1, vowel: str = "ooh", seed: int = 11) -> np.ndarray:
        """Dark-fantasy choral ensemble using glottal pulses and vocal tract formants."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        formants = {
            "ooh": [(350.0, 120.0, 0.55), (720.0, 140.0, 0.85), (1280.0, 180.0, 0.65), (2350.0, 240.0, 0.38), (3100.0, 300.0, 0.18)],
            "aah": [(620.0, 130.0, 0.95), (1080.0, 150.0, 0.85), (1650.0, 190.0, 0.55), (2450.0, 240.0, 0.32), (3200.0, 300.0, 0.16)],
            "mmh": [(280.0, 110.0, 0.70), (620.0, 135.0, 0.75), (1180.0, 175.0, 0.45), (2100.0, 230.0, 0.22)],
        }[vowel]

        swell = (np.sin(np.pi * (t / dur)) ** 1.1) * np.clip(t / 0.18, 0.0, 1.0)
        mix_l = np.zeros(n)
        mix_r = np.zeros(n)

        for f in chord_freqs:
            for s_idx in range(6):
                drift = DSP.lowpass(rng.standard_normal(n), 3.4, order=2)
                drift = drift / max(float(np.std(drift)), 1e-6) * 0.0055
                vib_rate = rng.uniform(4.2, 5.7)
                vib = 0.0065 * np.clip((t - 0.15) / 0.32, 0.0, 1.0) * np.sin(2.0 * np.pi * vib_rate * t + rng.uniform(0, 6.28))
                inst_f = f * (1.0 + drift + vib + rng.uniform(-0.0045, 0.0045))
                phase = 2.0 * np.pi * np.cumsum(inst_f) / SR + rng.uniform(0, 6.28)

                glottal = np.zeros(n)
                for k in range(1, 12):
                    if f * k < 4200.0:
                        glottal += (1.0 / (k ** 1.12)) * np.sin(k * phase)
                breath = DSP.bandpass(DSP.pink_noise(n, rng), 260.0, 3400.0) * 0.65
                voice = DSP.resonant_bank((glottal * 0.55 + breath) * swell, formants)
                pan = (s_idx / 5.0) * 1.4 - 0.7
                ang = (pan + 1.0) * (np.pi * 0.25)
                mix_l += voice * np.cos(ang)
                mix_r += voice * np.sin(ang)

        out = DSP.soft_sat(np.vstack([mix_l, mix_r]) * 0.28, drive=1.05)
        return DSP.convolve(out, space="cathedral", wet=0.25, dry=0.86)

    @staticmethod
    def anvil_strike(dur: float = 0.92, f0: float = 310.0, quench: bool = True, seed: int = 12) -> np.ndarray:
        """Blacksmith hammer strike on anvil + hot steam quench."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        t = np.linspace(0.0, dur, n, endpoint=False)

        thud = FoleySynth.visceral_sub_thud(n, 48.0, 85.0, decay_s=0.06, punch=1.1, seed=seed + 1)
        spark = DSP.bandpass(DSP.pink_noise(n, rng), 900.0, 5800.0) * np.exp(-t / 0.011) * 1.45

        ratios = [1.0, 1.31, 1.67, 2.09, 2.48, 2.93, 3.41, 3.96, 4.55, 5.18, 5.88, 6.62, 7.45, 8.35]
        modes = [(f0 * r, 45.0 + 28.0 * i, 1.15 / (1.0 + 0.22 * i)) for i, r in enumerate(ratios) if f0 * r < SR * 0.42]
        iron_body = DSP.resonant_bank(DSP.pink_noise(n, rng) * np.exp(-t / 0.22), modes) * 1.65

        steam = np.zeros(n)
        if quench:
            q_env = np.clip((t - 0.10) / 0.05, 0.0, 1.0) * np.exp(-np.maximum(0.0, t - 0.10) / 0.24)
            steam = DSP.bandpass(DSP.pink_noise(n, rng) * q_env, 1400.0, 5600.0) * 0.95

        mono = DSP.soft_sat(thud + spark + iron_body + steam, 1.2)
        return DSP.convolve(DSP.pan_stereo(mono, 0.0), space="close", wet=0.12, dry=0.93)

    @staticmethod
    def liquid_gulp(times: Sequence[float], dur: float = 0.65, seed: int = 13) -> np.ndarray:
        """Fluid turbulence and soft throat gulp pops."""
        rng = np.random.default_rng(seed)
        n = int(dur * SR)
        out = np.zeros(n)
        for idx_g, t0 in enumerate(times):
            idx = int(t0 * SR)
            glen = min(n - idx, int(0.085 * SR))
            if glen < 24:
                continue
            gt = np.linspace(0.0, glen / SR, glen, endpoint=False)
            u = gt / (glen / SR)
            env = np.sin(np.pi * np.minimum(1.0, u / 0.25) * 0.5) * np.exp(-gt / 0.024)
            f_center = 340.0 + idx_g * 55.0
            glug = DSP.resonant_bank(
                DSP.pink_noise(glen, rng) * env,
                [(f_center, 120.0, 1.3), (f_center * 1.75, 180.0, 0.9), (f_center * 2.8, 280.0, 0.45)],
            )
            out[idx:idx + glen] += glug * 1.4
        return out


# ==============================================================================
# 3. COMPLETE SFX GENERATION SUITE WITH ZERO-CLIPPING NORMALIZATION
# ==============================================================================

def build_all_sfx() -> Dict[str, Tuple[np.ndarray, float]]:
    assets: Dict[str, Tuple[np.ndarray, float]] = {}

    # --------------------------------------------------------------------------
    # 1. COMBAT FOLEY (VISCERAL, HEAVY, BONE-CRUNCHING)
    # --------------------------------------------------------------------------
    # A. Flesh Impact (SFX_Hit_Flesh.wav)
    flesh_raw = FoleySynth.flesh_impact(dur=0.32, weight=1.15, wetness=1.1, seed=3100)
    flesh_stereo = DSP.convolve(DSP.pan_stereo(flesh_raw, 0.0), space="close", wet=0.08, dry=0.95)
    assets["SFX_Hit_Flesh.wav"] = (flesh_stereo, PEAK_NEG_1DB)

    # B. Armor Impact (SFX_Hit_Armor.wav)
    armor_raw = FoleySynth.armor_impact(dur=0.38, hardness=0.88, damp=0.085, seed=3120)
    armor_stereo = DSP.convolve(DSP.pan_stereo(armor_raw, 0.04), space="close", wet=0.09, dry=0.95)
    assets["SFX_Hit_Armor.wav"] = (armor_stereo, PEAK_NEG_1DB)

    # C. Melee Attack (SFX_Attack_Melee.wav + SFX_Boss_Melee.wav + SFX_Swarm_Slash.wav)
    melee_raw = FoleySynth.melee_swing(dur=0.34, blade_weight=1.15, seed=3300)
    melee_stereo = DSP.convolve(DSP.pan_stereo(melee_raw, -0.06), space="close", wet=0.09, dry=0.95)
    assets["SFX_Attack_Melee.wav"] = (melee_stereo, PEAK_NEG_1DB)
    assets["SFX_Boss_Melee.wav"] = (melee_stereo.copy(), PEAK_NEG_1DB)

    swarm_raw = FoleySynth.melee_swing(dur=0.28, blade_weight=0.85, seed=6001)
    assets["SFX_Swarm_Slash.wav"] = (DSP.convolve(DSP.pan_stereo(swarm_raw, 0.0), space="close", wet=0.08, dry=0.95), 0.76)

    # D. Attack Slam (SFX_Attack_Slam.wav + SFX_Bruiser_Slam.wav + SFX_Boss_Slam.wav + SFX_Wall_Slam.wav)
    slam_raw = FoleySynth.attack_slam(dur=0.75, weight=1.35, grit=0.95, seed=3400)
    slam_stereo = DSP.convolve(DSP.pan_stereo(slam_raw, 0.0), space="hall", wet=0.18, dry=0.90)
    assets["SFX_Attack_Slam.wav"] = (slam_stereo, PEAK_NEG_1DB)
    assets["SFX_Bruiser_Slam.wav"] = (slam_stereo.copy(), PEAK_NEG_1DB)

    boss_slam_raw = FoleySynth.attack_slam(dur=1.55, weight=1.85, grit=1.2, seed=6130)
    assets["SFX_Boss_Slam.wav"] = (DSP.convolve(DSP.pan_stereo(boss_slam_raw, 0.0), space="cathedral", wet=0.26, dry=0.86), PEAK_NEG_1DB)

    wall_slam_raw = FoleySynth.attack_slam(dur=0.54, weight=1.15, grit=0.85, seed=5020)
    assets["SFX_Wall_Slam.wav"] = (DSP.convolve(DSP.pan_stereo(wall_slam_raw, 0.0), space="hall", wet=0.18, dry=0.90), 0.88)

    # Hit Bone, Stone, Shield, HitImpact
    bone_raw = FoleySynth.bone_impact(0.32, count=5, pitch_scale=1.05, seed=3110)
    assets["SFX_Hit_Bone.wav"] = (DSP.convolve(DSP.pan_stereo(bone_raw, 0.0), space="close", wet=0.08, dry=0.95), PEAK_NEG_1DB)

    stone_hit_raw = FoleySynth.stone_impact(0.34, weight=0.85, grit=0.9, seed=3130)
    assets["SFX_Hit_Stone.wav"] = (DSP.convolve(DSP.pan_stereo(stone_hit_raw, 0.0), space="close", wet=0.09, dry=0.95), PEAK_NEG_1DB)

    shield_thump = FoleySynth.visceral_sub_thud(int(0.36 * SR), 52.0, 75.0, 0.055, 1.0, 3141)
    shield_hum = DSP.bandpass(DSP.pink_noise(int(0.36 * SR), np.random.default_rng(3140)) * np.exp(-np.linspace(0, 0.36, int(0.36 * SR)) / 0.09), 240.0, 1450.0) * 1.1
    shield_crys = FoleySynth.crystal_shard_cluster(0.36, f0=620.0, count=3, spread=0.015, seed=3142) * 0.55
    assets["SFX_Hit_Shield_Absorb.wav"] = (DSP.convolve(DSP.pan_stereo(shield_thump + shield_hum + shield_crys, 0.0), space="close", wet=0.12, dry=0.93), PEAK_NEG_1DB)

    hi_stone = FoleySynth.stone_impact(0.34, weight=1.1, grit=0.5, seed=3150)
    hi_iron = FoleySynth.wrought_iron_impact(0.34, f0=220.0, hardness=0.75, damp=0.08, seed=3151) * 0.65
    assets["SFX_HitImpact.wav"] = (DSP.convolve(DSP.pan_stereo(hi_stone + hi_iron, 0.0), space="close", wet=0.10, dry=0.94), PEAK_NEG_1DB)

    # --------------------------------------------------------------------------
    # 2. SPELLS & ARCANUM (DIABLO IV GRIMDARK WEIGHT)
    # --------------------------------------------------------------------------
    # A. Fireball Cast (SFX_Spell_Fireball_Cast.wav + SFX_Fireball_Cast.wav)
    fb_cast_raw = FoleySynth.fireball_cast(dur=0.56, seed=4001)
    fb_cast_stereo = DSP.convolve(DSP.pan_stereo(fb_cast_raw, 0.06), space="hall", wet=0.16, dry=0.91)
    assets["SFX_Spell_Fireball_Cast.wav"] = (fb_cast_stereo, PEAK_NEG_1DB)
    assets["SFX_Fireball_Cast.wav"] = (fb_cast_stereo.copy(), PEAK_NEG_1DB)

    # B. Fireball Impact & Boom (SFX_Spell_Fireball_Impact.wav + SFX_Fireball_Boom.wav + SFX_Fireball.wav)
    fb_imp_raw = FoleySynth.fireball_impact(dur=1.45, seed=4010)
    fb_imp_stereo = DSP.convolve(DSP.pan_stereo(fb_imp_raw, 0.0), space="hall", wet=0.22, dry=0.88)
    assets["SFX_Spell_Fireball_Impact.wav"] = (fb_imp_stereo, PEAK_NEG_1DB)
    assets["SFX_Fireball_Boom.wav"] = (fb_imp_stereo.copy(), PEAK_NEG_1DB)
    assets["SFX_Fireball.wav"] = (fb_imp_stereo.copy(), PEAK_NEG_1DB)

    # C. Lightning Bolt (SFX_Spell_Lightning_Bolt.wav + SFX_ChainLightning.wav)
    lt_bolt_raw = FoleySynth.lightning_bolt(dur=1.15, seed=4050)
    lt_bolt_stereo = DSP.convolve(DSP.pan_stereo(lt_bolt_raw, 0.0), space="hall", wet=0.20, dry=0.89)
    assets["SFX_Spell_Lightning_Bolt.wav"] = (lt_bolt_stereo, PEAK_NEG_1DB)
    assets["SFX_ChainLightning.wav"] = (lt_bolt_stereo.copy(), PEAK_NEG_1DB)
    assets["SFX_Lightning_Hop.wav"] = (DSP.convolve(DSP.pan_stereo(FoleySynth.plasma_lightning(0.24, 2, 0.18, 4055), 0.0), space="close", wet=0.10, dry=0.94), 0.78)

    # D. Black Hole Loop & Collapse (SFX_Spell_Blackhole_Loop.wav + SFX_BlackHole.wav)
    bh_loop_stereo = FoleySynth.blackhole_loop(dur=3.2, crossfade_s=0.75, seed=4020)
    assets["SFX_Spell_Blackhole_Loop.wav"] = (bh_loop_stereo, PEAK_NEG_1DB)
    assets["SFX_BlackHole.wav"] = (bh_loop_stereo.copy(), PEAK_NEG_1DB)

    # Black Hole Collapse
    rng_bhc = np.random.default_rng(4025)
    d_bhc = 1.35
    n_bhc = int(d_bhc * SR)
    sp_i = int(0.24 * SR)
    suck = DSP.band_noise(sp_i, rng_bhc, 180.0, 2400.0) * (np.linspace(0.0, 1.0, sp_i) ** 2.4) * 1.20
    imp_nb = n_bhc - sp_i
    imp_tb = np.linspace(0.0, imp_nb / SR, imp_nb, endpoint=False)
    imp_bb = FoleySynth.visceral_sub_thud(imp_nb, 36.0, 55.0, 0.25, 1.45, 4026)
    imp_crack = DSP.band_noise(imp_nb, rng_bhc, 160.0, 3100.0) * np.exp(-imp_tb / 0.14) * 1.30
    bh_col = np.zeros(n_bhc)
    bh_col[:sp_i] = suck
    bh_col[sp_i:] = DSP.soft_sat(imp_bb + imp_crack, 1.28)
    assets["SFX_BlackHole_Collapse.wav"] = (DSP.convolve(DSP.pan_stereo(bh_col, 0.0), space="cathedral", wet=0.22, dry=0.88), PEAK_NEG_1DB)

    # Black Hole Solar Ignite
    d_si = 1.45
    t_si = np.linspace(0.0, d_si, int(d_si * SR), endpoint=False)
    si_fire = FoleySynth.fire_roar(d_si, 1.4, 115.0, 2300.0, 1.9, 4028) * np.exp(-t_si / 0.60)
    si_sub = FoleySynth.visceral_sub_thud(int(d_si * SR), 38.0, 58.0, 0.28, 1.35, 4029)
    si_plate = FoleySynth.wrought_iron_impact(d_si, 210.0, 0.72, 0.18, seed=4027) * 0.50
    assets["SFX_BlackHole_SolarIgnite.wav"] = (DSP.convolve(DSP.pan_stereo(si_fire + si_sub + si_plate, 0.0), space="cathedral", wet=0.22, dry=0.88), PEAK_NEG_1DB)

    # E. Cataclysm Nova (SFX_Spell_Cataclysm_Nova.wav + SFX_CataclysmNova.wav)
    cn_raw = FoleySynth.cataclysm_nova(dur=1.60, seed=4130)
    cn_stereo = DSP.convolve(DSP.pan_stereo(cn_raw, 0.0), space="cathedral", wet=0.25, dry=0.86)
    assets["SFX_Spell_Cataclysm_Nova.wav"] = (cn_stereo, PEAK_NEG_1DB)
    assets["SFX_CataclysmNova.wav"] = (cn_stereo.copy(), PEAK_NEG_1DB)

    # F. Macropyre (SFX_Spell_Macropyre.wav + SFX_Macropyre.wav)
    mp_raw = FoleySynth.macropyre(dur=1.70, seed=4111)
    mp_stereo = DSP.convolve(DSP.pan_stereo(mp_raw, 0.0), space="cathedral", wet=0.24, dry=0.87)
    assets["SFX_Spell_Macropyre.wav"] = (mp_stereo, PEAK_NEG_1DB)
    assets["SFX_Macropyre.wav"] = (mp_stereo.copy(), PEAK_NEG_1DB)

    # Remaining Spells & Synergies
    rng_cm = np.random.default_rng(4040)
    d_cm = 0.92
    t_cm = np.linspace(0.0, d_cm, int(d_cm * SR), endpoint=False)
    sky_tear = DSP.bandpass(DSP.pink_noise(len(t_cm), rng_cm) * np.exp(-t_cm / 0.13), 260.0, 3200.0) * 1.15
    desc = DSP.bandpass(DSP.pink_noise(len(t_cm), rng_cm) * np.clip(t_cm / 0.62, 0.0, 1.0) * np.exp(-np.maximum(0.0, t_cm - 0.70) / 0.08), 160.0, 1450.0) * 1.5
    fire_c = FoleySynth.fire_roar(d_cm, 1.05, 140.0, 1650.0, 1.3, 4041) * 0.68
    assets["SFX_ChaosMeteor.wav"] = (DSP.convolve(DSP.pan_stereo(sky_tear + desc + fire_c, 0.0), space="hall", wet=0.20, dry=0.89), 0.88)

    m_rock = FoleySynth.stone_impact(1.60, weight=1.85, grit=1.2, seed=4045)
    m_fire = FoleySynth.fire_roar(1.60, 1.35, 85.0, 1350.0, 1.8, 4046) * np.exp(-np.linspace(0, 1.6, int(1.6 * SR)) / 0.68)
    assets["SFX_ChaosMeteor_Impact.wav"] = (DSP.convolve(DSP.pan_stereo(m_rock * 1.25 + m_fire * 0.75, 0.0), space="cathedral", wet=0.26, dry=0.86), PEAK_NEG_1DB)

    # Chronosphere Cast & Expire
    rng_cs = np.random.default_rng(4030)
    d_cs = 2.25
    n_cs = int(d_cs * SR)
    t_cs = np.linspace(0.0, d_cs, n_cs, endpoint=False)
    vac_n = int(0.28 * SR)
    vac = DSP.band_noise(vac_n, rng_cs, 200.0, 2500.0) * (np.linspace(0.0, 1.0, vac_n) ** 2.6) * 1.25
    lock_n = n_cs - vac_n
    ch_sub = FoleySynth.visceral_sub_thud(lock_n, 38.0, 60.0, decay_s=0.28, punch=1.25, seed=4031)
    ch_glass = FoleySynth.crystal_shard_cluster(d_cs, f0=540.0, count=6, spread=0.045, seed=4032) * np.clip((t_cs - 0.18) / 0.10, 0.0, 1.0) * 1.25
    ch_wind = DSP.band_noise(n_cs, rng_cs, 110.0, 680.0) * np.exp(-t_cs / 0.85) * 0.85
    ch_sig = ch_glass + ch_wind
    ch_sig[:vac_n] += vac
    ch_sig[vac_n:] += ch_sub
    assets["SFX_Chronosphere.wav"] = (DSP.convolve(DSP.pan_stereo(ch_sig, 0.0), space="cathedral", wet=0.24, dry=0.87), PEAK_NEG_1DB)

    ex_rev = DSP.band_noise(int(0.85 * SR), np.random.default_rng(4035), 180.0, 2600.0) * np.sin(np.pi * np.linspace(0, 1, int(0.85 * SR))) * 1.1
    ex_shards = FoleySynth.crystal_shard_cluster(0.85, f0=680.0, count=7, spread=0.025, seed=4036)
    ex_sub = FoleySynth.visceral_sub_thud(int(0.85 * SR), 38.0, 62.0, 0.18, 1.25, 4037)
    assets["SFX_Chronosphere_Expire.wav"] = (DSP.convolve(DSP.pan_stereo(ex_rev + ex_shards + ex_sub, 0.0), space="hall", wet=0.22, dry=0.88), 0.88)

    # Supernova Beam & Crossing Boom
    rng_sb = np.random.default_rng(4060)
    d_sb = 1.35
    t_sb = np.linspace(0.0, d_sb, int(d_sb * SR), endpoint=False)
    sb_env = np.sin(np.pi * (t_sb / d_sb)) ** 0.7
    p_core = DSP.resonant_bank(DSP.pink_noise(len(t_sb), rng_sb) * sb_env, [(125.0, 48.0, 1.3), (260.0, 80.0, 1.0), (580.0, 140.0, 0.7), (1450.0, 320.0, 0.4)])
    s_burn = FoleySynth.fire_roar(d_sb, 1.2, 130.0, 2400.0, 1.4, 4061) * sb_env * 0.8
    assets["SFX_SupernovaBeam.wav"] = (DSP.convolve(DSP.pan_stereo(p_core * 1.3 + s_burn + FoleySynth.visceral_sub_thud(len(t_sb), 45.0, 65.0, 0.14, 1.1, 4062), 0.0), space="hall", wet=0.18, dry=0.90), 0.88)

    cr_sub = FoleySynth.visceral_sub_thud(int(1.35 * SR), 40.0, 60.0, 0.35, 1.35, 4064)
    cr_arc = FoleySynth.plasma_lightning(1.35, 4, 1.05, 4065)
    cr_fire = FoleySynth.fire_roar(1.35, 1.4, 90.0, 2100.0, 1.8, 4066) * np.exp(-np.linspace(0, 1.35, int(1.35 * SR)) / 0.42)
    assets["SFX_Beam_Crossing_Boom.wav"] = (DSP.convolve(DSP.pan_stereo(cr_sub + cr_arc + cr_fire, 0.0), space="cathedral", wet=0.26, dry=0.86), PEAK_NEG_1DB)

    # ChronoBlink & Decoy Shatter
    rng_cb = np.random.default_rng(4070)
    d_cb = 0.42
    n_cb = int(d_cb * SR)
    t_cb = np.linspace(0.0, d_cb, n_cb, endpoint=False)
    bl_air = DSP.bandpass(DSP.pink_noise(n_cb, rng_cb) * np.exp(-t_cb / 0.06), 280.0, 2900.0) * 1.3
    bl_pop = FoleySynth.visceral_sub_thud(n_cb, 50.0, 75.0, 0.040, 1.15, 4071)
    bl_shim = FoleySynth.crystal_shard_cluster(d_cb, f0=680.0, count=3, spread=0.018, seed=4072) * 0.55
    assets["SFX_ChronoBlink.wav"] = (DSP.convolve(DSP.pan_stereo(bl_air + bl_pop + bl_shim, -0.08), space="hall", wet=0.16, dry=0.91), 0.84)

    dc_glass = FoleySynth.crystal_shard_cluster(0.70, f0=580.0, count=6, spread=0.02, seed=4075)
    dc_pop = FoleySynth.stone_impact(0.70, weight=0.65, grit=0.4, seed=4076) * 0.75
    assets["SFX_ChronoDecoy_Shatter.wav"] = (DSP.convolve(DSP.pan_stereo(dc_glass + dc_pop, 0.0), space="hall", wet=0.18, dry=0.90), 0.86)

    # Gravity Cataclysm
    gc_sig = np.zeros(int(1.25 * SR))
    for k in range(5):
        off = int(k * 0.062 * SR)
        sp = FoleySynth.stone_impact(0.55, weight=1.15 - k * 0.1, grit=0.95, seed=4080 + k)
        ll = min(len(sp), len(gc_sig) - off)
        gc_sig[off:off + ll] += sp[:ll] * (1.0 - 0.1 * k)
    assets["SFX_GravityCataclysm.wav"] = (DSP.convolve(DSP.pan_stereo(gc_sig, 0.0), space="hall", wet=0.22, dry=0.88), PEAK_NEG_1DB)

    # Prism Wall (Wall, Split, Absorb, Shatter)
    pw_c = FoleySynth.crystal_shard_cluster(1.25, f0=460.0, count=7, spread=0.035, seed=4090)
    pw_s = FoleySynth.stone_impact(1.25, weight=0.85, grit=0.55, seed=4091) * 0.68
    assets["SFX_PrismWall.wav"] = (DSP.convolve(DSP.pan_stereo(pw_c + pw_s, 0.0), space="hall", wet=0.22, dry=0.88), 0.88)

    split_shards = FoleySynth.crystal_shard_cluster(0.42, 680.0, 4, 0.018, 4092)
    split_air = DSP.bandpass(DSP.pink_noise(int(0.42 * SR), np.random.default_rng(4098)), 420.0, 2800.0) * 0.55
    assets["SFX_Prism_Split.wav"] = (DSP.convolve(DSP.pan_stereo(split_shards + split_air, 0.0), space="close", wet=0.11, dry=0.94), 0.78)

    rng_ab = np.random.default_rng(4093)
    d_ab = 0.32
    n_ab = int(d_ab * SR)
    t_ab = np.linspace(0.0, d_ab, n_ab, endpoint=False)
    ab_d = DSP.band_noise(n_ab, rng_ab, 180.0, 1600.0) * np.exp(-t_ab / 0.04) * 1.15 + FoleySynth.visceral_sub_thud(n_ab, 55.0, 75.0, 0.035, 0.75, 4094)
    ab_i = FoleySynth.crystal_shard_cluster(d_ab, 440.0, 3, 0.015, 4095) * 0.72
    assets["SFX_Prism_Absorb.wav"] = (DSP.convolve(DSP.pan_stereo(ab_d + ab_i, 0.0), space="close", wet=0.10, dry=0.95), 0.76)

    sh_i = FoleySynth.crystal_shard_cluster(0.92, 620.0, 9, 0.025, 4096)
    sh_c = FoleySynth.stone_impact(0.92, 0.55, 0.9, 4097) * 0.52
    assets["SFX_PrismWall_Shatter.wav"] = (DSP.convolve(DSP.pan_stereo(sh_i + sh_c, 0.0), space="hall", wet=0.20, dry=0.89), 0.88)

    # Primal Overload
    po_fire = FoleySynth.fire_roar(1.4, 1.25, 95.0, 1850.0, 1.4, 4100) * np.exp(-np.linspace(0, 1.4, int(1.4 * SR)) / 0.40)
    po_arc = FoleySynth.plasma_lightning(1.4, 3, 0.7, 4101) * 0.82
    po_bell = FoleySynth.sanctuary_bronze_bell(220.0, 1.4, 0.8, 0.45, 4102)
    assets["SFX_PrimalOverload.wav"] = (DSP.convolve(DSP.pan_stereo(po_fire + po_arc, 0.0), space="hall", wet=0.20, dry=0.88) + po_bell * 0.45, PEAK_NEG_1DB)

    # Plasma Field
    d_pf = 1.35
    t_pf = np.linspace(0.0, d_pf, int(d_pf * SR), endpoint=False)
    pf_arc = FoleySynth.plasma_lightning(d_pf, 5, 0.8, 4120)
    pf_w = DSP.bandpass(DSP.pink_noise(len(t_pf), np.random.default_rng(4121)) * np.sin(np.pi * np.minimum(1.0, t_pf / 0.42)), 200.0, 2400.0) * 0.95
    assets["SFX_PlasmaField.wav"] = (DSP.convolve(DSP.pan_stereo(pf_arc + pf_w, 0.0), space="hall", wet=0.20, dry=0.89), PEAK_NEG_1DB)

    # Sun Strike
    d_ss = 1.40
    t_ss = np.linspace(0.0, d_ss, int(d_ss * SR), endpoint=False)
    ss_f = FoleySynth.fire_roar(d_ss, 1.5, 140.0, 2400.0, 1.8, 4140) * np.exp(-t_ss / 0.44)
    ss_b = DSP.band_noise(len(t_ss), np.random.default_rng(4141), 520.0, 4200.0) * np.exp(-t_ss / 0.22) * 1.15
    ss_s = FoleySynth.visceral_sub_thud(len(t_ss), 42.0, 62.0, 0.20, 1.25, 4142)
    ss_bell = FoleySynth.sanctuary_bronze_bell(196.0, d_ss, 0.65, 0.55, 4143) * 0.45
    assets["SFX_SunStrike.wav"] = (DSP.convolve(DSP.pan_stereo(ss_f + ss_b + ss_s, 0.0), space="cathedral", wet=0.22, dry=0.88) + ss_bell, PEAK_NEG_1DB)

    # Eye Of The Storm
    d_es = 1.55
    t_es = np.linspace(0.0, d_es, int(d_es * SR), endpoint=False)
    es_w = DSP.bandpass(DSP.pink_noise(len(t_es), np.random.default_rng(4150)) * (0.4 + 0.6 * np.sin(np.pi * (t_es / d_es))), 180.0, 2300.0) * 1.35
    es_a = FoleySynth.plasma_lightning(d_es, 6, 0.95, 4151) * 0.95
    assets["SFX_EyeOfTheStorm.wav"] = (DSP.convolve(DSP.pan_stereo(es_w + es_a, 0.0), space="cathedral", wet=0.24, dry=0.87), PEAK_NEG_1DB)

    # Requiem Blast
    d_rb = 1.70
    rb_bell = FoleySynth.sanctuary_bronze_bell(98.0, d_rb, 1.25, 0.65, 4160) * 0.68
    rb_choir = FoleySynth.organic_choir([196.00, 233.08, 293.66], d_rb, "aah", 4161) * 0.52
    rb_stone = DSP.convolve(DSP.pan_stereo(FoleySynth.stone_impact(d_rb, 1.45, 0.75, 4162), 0.0), space="cathedral", wet=0.24, dry=0.87) * 0.95
    assets["SFX_RequiemBlast.wav"] = (rb_bell + rb_choir + rb_stone, PEAK_NEG_1DB)

    # Synergies
    rng_ts = np.random.default_rng(5001)
    d_ts = 0.82
    n_ts = int(d_ts * SR)
    t_ts = np.linspace(0.0, d_ts, n_ts, endpoint=False)
    steam_ts = DSP.bandpass(DSP.pink_noise(n_ts, rng_ts) * np.exp(-t_ts / 0.22), 750.0, 5200.0) * 1.35
    cr_ice = FoleySynth.crystal_shard_cluster(d_ts, 520.0, 5, 0.02, 5002) * 0.65
    th_st = FoleySynth.stone_impact(d_ts, 0.95, 0.4, 5003) * 0.85
    assets["SFX_Synergy_ThermalShock.wav"] = (DSP.convolve(DSP.pan_stereo(steam_ts + cr_ice + th_st, 0.0), space="hall", wet=0.20, dry=0.89), 0.88)

    assets["SFX_Synergy_CryoShatter.wav"] = (DSP.convolve(DSP.pan_stereo(FoleySynth.crystal_shard_cluster(0.80, 520.0, 8, 0.022, 5010) + FoleySynth.bone_impact(0.80, 5, 1.15, 5011) * 0.85, 0.0), space="hall", wet=0.20, dry=0.89), 0.88)

    # Basic Bolts
    def make_staff_bolt(elem: str, seed_b: int) -> Tuple[np.ndarray, float]:
        rng_b = np.random.default_rng(seed_b)
        d = 0.34
        nb = int(d * SR)
        tb = np.linspace(0.0, d, nb, endpoint=False)
        punch = FoleySynth.footstep_sub(nb, f_low=52.0, f_high=85.0, decay_s=0.035, punch=0.85, seed=seed_b + 1)
        wood = FoleySynth.wood_oak_impact(0.18, f0=195.0, splinter=0.1, seed=seed_b + 2) * 0.55
        whoosh = DSP.bandpass(DSP.pink_noise(nb, rng_b) * np.exp(-tb / 0.052), 240.0, 1850.0) * 1.25
        air_disp = DSP.band_noise(nb, rng_b, 420.0, 3200.0) * np.exp(-tb / 0.065) * 0.75
        sig = punch + whoosh + air_disp
        sig[:len(wood)] += wood
        if elem == "ignis":
            sig += FoleySynth.fire_roar(d, 1.0, 140.0, 1450.0, 1.3, seed_b + 3) * np.exp(-tb / 0.13) * 0.9
        elif elem == "tempest":
            sig += FoleySynth.plasma_lightning(d, 2, 0.15, seed_b + 3) * 0.78
        elif elem == "graviton":
            sig += FoleySynth.stone_impact(d, weight=0.95, grit=0.55, seed=seed_b + 3) * 0.75 + DSP.band_noise(nb, rng_b, 220.0, 1450.0) * np.exp(-tb / 0.065) * 0.75
        else:
            sig += DSP.band_noise(nb, rng_b, 380.0, 2600.0) * np.exp(-tb / 0.075) * 0.85
        return DSP.convolve(DSP.pan_stereo(sig, 0.0), space="close", wet=0.09, dry=0.95), 0.74

    assets["SFX_BasicBolt.wav"] = make_staff_bolt("neutral", 3001)
    assets["SFX_BasicBolt_Ignis.wav"] = make_staff_bolt("ignis", 3011)
    assets["SFX_BasicBolt_Tempest.wav"] = make_staff_bolt("tempest", 3021)
    assets["SFX_BasicBolt_Graviton.wav"] = make_staff_bolt("graviton", 3031)

    # Self-Casts
    assets["SFX_SelfCast_Ignis.wav"] = (
        DSP.convolve(DSP.pan_stereo(FoleySynth.fire_roar(0.72, 1.35, 85.0, 1450.0, 1.4, 2050) * np.exp(-np.linspace(0, 0.72, int(0.72 * SR)) / 0.32) + FoleySynth.visceral_sub_thud(int(0.72 * SR), 45.0, 68.0, 0.09, 1.15, 2051), 0.0), space="hall", wet=0.18, dry=0.90),
        0.84,
    )
    rng_sct = np.random.default_rng(2052)
    d_sct = 0.72
    n_sct = int(d_sct * SR)
    t_sct = np.linspace(0.0, d_sct, n_sct, endpoint=False)
    assets["SFX_SelfCast_Tempest.wav"] = (
        DSP.convolve(DSP.pan_stereo(FoleySynth.plasma_lightning(d_sct, 3, 0.45, 2053) + DSP.bandpass(DSP.pink_noise(n_sct, rng_sct) * np.exp(-t_sct / 0.24), 320.0, 2600.0) * 0.9, 0.0), space="hall", wet=0.18, dry=0.90),
        0.84,
    )
    assets["SFX_SelfCast_Graviton.wav"] = (
        DSP.convolve(DSP.pan_stereo(FoleySynth.stone_impact(0.72, 1.2, 0.5, 2054) + FoleySynth.wrought_iron_impact(0.72, 175.0, 0.65, 0.14, 2055) * 0.6, 0.0), space="hall", wet=0.20, dry=0.89),
        0.84,
    )

    # Enemy Ranged Cast
    rng_er = np.random.default_rng(3041)
    d_er = 0.34
    n_er = int(d_er * SR)
    t_er = np.linspace(0.0, d_er, n_er, endpoint=False)
    necro_bone = DSP.band_noise(n_er, rng_er, 280.0, 1400.0) * np.exp(-t_er / 0.045) * 1.15
    necro_hiss = DSP.bandpass(DSP.pink_noise(n_er, rng_er) * np.exp(-t_er / 0.095), 550.0, 2900.0) * 0.75
    assets["SFX_Enemy_Ranged_Cast.wav"] = (DSP.convolve(DSP.pan_stereo(necro_bone + necro_hiss, 0.0), space="close", wet=0.10, dry=0.94), 0.72)

    # --------------------------------------------------------------------------
    # 3. FOOTSTEPS & MOVEMENT (4 GRANITE FLAGSTONE VARIATIONS + SPECIAL SURFACES)
    # --------------------------------------------------------------------------
    # A. Footstep Stone 1..4 (Both naming conventions: SFX_Footstep_Stone_X & SFX_Step_Stone_0X)
    for v in (1, 2, 3, 4):
        st_raw = FoleySynth.footstep_stone(variation=v, seed=1000 + v * 33)
        st_stereo = DSP.convolve(DSP.pan_stereo(st_raw, (v - 2.5) * 0.06), space="close", wet=0.04, dry=0.98)
        assets[f"SFX_Footstep_Stone_{v}.wav"] = (st_stereo, 0.55)
        assets[f"SFX_Step_Stone_{v:02d}.wav"] = (st_stereo.copy(), 0.55)

        # Basalt steps
        rng_bs = np.random.default_rng(1100 + v)
        d_bs = 0.19
        n_bs = int(d_bs * SR)
        t_bs = np.linspace(0.0, d_bs, n_bs, endpoint=False)
        heel_b = FoleySynth.footstep_sub(n_bs, f_low=45.0, f_high=80.0, decay_s=0.024, punch=0.85, seed=1110 + v)
        ash = DSP.band_noise(n_bs, rng_bs, 280.0, 1800.0) * np.exp(-t_bs / 0.038) * 1.15
        crunch = DSP.band_noise(n_bs, rng_bs, 950.0, 3800.0) * np.exp(-t_bs / 0.035) * 0.65
        assets[f"SFX_Step_Basalt_{v:02d}.wav"] = (
            DSP.convolve(DSP.pan_stereo(heel_b + ash + crunch, (v - 2.5) * 0.06), space="close", wet=0.04, dry=0.98),
            0.52,
        )

        # Ice steps
        rng_is = np.random.default_rng(1200 + v)
        d_is = 0.19
        n_is = int(d_is * SR)
        t_is = np.linspace(0.0, d_is, n_is, endpoint=False)
        heel_i = FoleySynth.footstep_sub(n_is, f_low=52.0, f_high=95.0, decay_s=0.022, punch=0.8, seed=1210 + v)
        crust = DSP.band_noise(n_is, rng_is, 950.0, 3800.0) * np.exp(-t_is / 0.030) * 0.95
        slide = DSP.band_noise(n_is, rng_is, 550.0, 2400.0) * np.exp(-t_is / 0.042) * 0.78
        assets[f"SFX_Step_Ice_{v:02d}.wav"] = (
            DSP.convolve(DSP.pan_stereo(heel_i + crust + slide, (v - 2.5) * 0.06), space="close", wet=0.07, dry=0.96),
            0.52,
        )

    # Bruiser & Titan steps
    n_hb = int(0.32 * SR)
    t_hb = np.linspace(0.0, 0.32, n_hb, endpoint=False)
    hb_sub = FoleySynth.footstep_sub(n_hb, 45.0, 85.0, decay_s=0.040, punch=1.1, seed=1302)
    hb_slab = DSP.band_noise(n_hb, np.random.default_rng(1303), 140.0, 1600.0) * np.exp(-t_hb / 0.045) * 1.05
    hb_crunch = DSP.band_noise(n_hb, np.random.default_rng(1304), 950.0, 3400.0) * np.exp(-t_hb / 0.025) * 0.65
    hb_iron = DSP.band_noise(n_hb, np.random.default_rng(1305), 320.0, 950.0) * np.exp(-t_hb / 0.060) * 0.55
    assets["SFX_Step_Heavy_Bruiser.wav"] = (
        DSP.convolve(DSP.pan_stereo(hb_sub + hb_slab + hb_crunch + hb_iron, 0.0), space="close", wet=0.10, dry=0.94),
        0.74,
    )
    assets["SFX_Step_Boss_Titan.wav"] = (
        DSP.convolve(DSP.pan_stereo(FoleySynth.stone_impact(0.48, 1.65, 0.95, 1307) + FoleySynth.wrought_iron_impact(0.48, 185.0, 0.80, 0.11, 1308) * 0.45, 0.0), space="hall", wet=0.16, dry=0.92),
        0.86,
    )

    # MoveClick & Dash
    rng_mc = np.random.default_rng(1319)
    d_mc = 0.088
    t_mc = np.linspace(0.0, d_mc, int(d_mc * SR), endpoint=False)
    mc = DSP.band_noise(len(t_mc), rng_mc, 170.0, 680.0) * np.exp(-t_mc / 0.014) * 0.78 + DSP.band_noise(len(t_mc), rng_mc, 620.0, 2200.0) * np.exp(-t_mc / 0.008) * 0.92
    assets["SFX_MoveClick.wav"] = (DSP.stereo_decorrelate(mc, 0.25, seed=1312), 0.48)

    d_dash = 0.36
    t_dash = np.linspace(0.0, d_dash, int(d_dash * SR), endpoint=False)
    push_d = FoleySynth.visceral_sub_thud(len(t_dash), 48.0, 72.0, 0.045, 0.85, 1321)
    cloak_d = DSP.bandpass(DSP.pink_noise(len(t_dash), np.random.default_rng(1320)) * np.exp(-t_dash / 0.11), 220.0, 1450.0) * 1.45
    assets["SFX_Dash.wav"] = (DSP.convolve(DSP.pan_stereo(push_d + cloak_d, -0.1), space="close", wet=0.09, dry=0.95), 0.80)

    # --------------------------------------------------------------------------
    # 4. SANCTUARY LOOT & EQUIPMENT FOLEY
    # --------------------------------------------------------------------------
    # A. Legendary Loot Drop (SFX_Loot_Drop_Legendary.wav)
    leg_bell = FoleySynth.sanctuary_bronze_bell(f0=110.0, dur=2.45, dark_weight=1.35, shimmer=0.85, seed=9301)
    leg_bell2 = FoleySynth.sanctuary_bronze_bell(f0=164.81, dur=2.45, dark_weight=0.75, shimmer=0.55, seed=9302)
    off_l = int(0.055 * SR)
    leg_mix = leg_bell.copy()
    leg_mix[:, off_l:] += leg_bell2[:, :-off_l] * 0.42
    slam_loot = FoleySynth.stone_impact(dur=0.75, weight=1.15, grit=0.45, seed=9303)
    leg_mix[:, :len(slam_loot)] += DSP.convolve(DSP.pan_stereo(slam_loot, 0.0), space="cathedral", wet=0.22, dry=0.88) * 0.62
    assets["SFX_Loot_Drop_Legendary.wav"] = (leg_mix, PEAK_NEG_1DB)

    # B. Rare Loot Drop (SFX_Loot_Drop_Rare.wav)
    rng_lr = np.random.default_rng(9201)
    d_lr = 0.85
    t_lr = np.linspace(0.0, d_lr, int(d_lr * SR), endpoint=False)
    rare_thud = FoleySynth.visceral_sub_thud(len(t_lr), 52.0, 75.0, 0.05, 0.9, 9202)
    steel_ring = FoleySynth.wrought_iron_impact(d_lr, f0=420.0, hardness=0.90, damp=0.22, seed=9203)
    arcane_chime = FoleySynth.crystal_shard_cluster(d_lr, f0=560.0, count=5, spread=0.028, seed=9204) * 0.72
    air_bloom = DSP.bandpass(DSP.pink_noise(len(t_lr), rng_lr) * np.clip(t_lr / 0.05, 0.0, 1.0) * np.exp(-t_lr / 0.24), 480.0, 3200.0) * 0.65
    assets["SFX_Loot_Drop_Rare.wav"] = (
        DSP.convolve(DSP.pan_stereo(rare_thud + steel_ring + arcane_chime + air_bloom, -0.04), space="hall", wet=0.20, dry=0.90),
        0.86,
    )

    # C. Common Loot Drop (SFX_Loot_Drop_Common.wav)
    rng_lc = np.random.default_rng(9101)
    d_lc = 0.42
    n_lc = int(d_lc * SR)
    t_lc = np.linspace(0.0, d_lc, n_lc, endpoint=False)
    drop_thud = FoleySynth.footstep_sub(n_lc, 55.0, 110.0, decay_s=0.030, punch=0.72, seed=9102)
    coins_c = np.zeros(n_lc)
    for k, dt in enumerate([0.008, 0.068, 0.145]):
        idx = int(dt * SR)
        cl = min(n_lc - idx, int(0.14 * SR))
        if cl > 20:
            c = DSP.band_noise(cl, rng_lc, 650.0 + k * 180.0, 3600.0) * np.exp(-np.linspace(0, 0.14, cl) / 0.025)
            coins_c[idx:idx + cl] += c * (0.80 * (0.82 ** k))
    leather_c = DSP.band_noise(n_lc, rng_lc, 280.0, 1950.0) * np.exp(-t_lc / 0.065) * 0.65
    common_air = DSP.band_noise(n_lc, rng_lc, 200.0, 4200.0) * np.exp(-t_lc / 0.09) * 0.45
    assets["SFX_Loot_Drop_Common.wav"] = (
        DSP.convolve(DSP.pan_stereo(drop_thud + coins_c + leather_c + common_air, 0.04), space="close", wet=0.09, dry=0.95),
        0.74,
    )

    # D. Loot Pickup (SFX_Loot_Pickup.wav)
    rng_lp = np.random.default_rng(9401)
    d_lp = 0.28
    n_lp = int(d_lp * SR)
    t_lp = np.linspace(0.0, d_lp, n_lp, endpoint=False)
    grab_p = DSP.band_noise(n_lp, rng_lp, 240.0, 1400.0) * np.exp(-t_lp / 0.035) * 1.15
    suede_p = DSP.band_noise(n_lp, rng_lp, 650.0, 2800.0) * np.exp(-t_lp / 0.07) * 0.85
    click_p = DSP.band_noise(n_lp, rng_lp, 950.0, 4800.0) * np.exp(-np.maximum(0, t_lp - 0.04) / 0.02) * (t_lp >= 0.035) * 0.95
    thump_p = DSP.band_noise(n_lp, rng_lp, 65.0, 160.0) * np.exp(-t_lp / 0.025) * 0.75
    assets["SFX_Loot_Pickup.wav"] = (DSP.stereo_decorrelate(grab_p + suede_p + click_p + thump_p, 0.3, seed=9404), 0.68)

    # E. Equip Weapon & Armor & Salvage
    rng_ew = np.random.default_rng(9501)
    d_ew = 0.58
    n_ew = int(d_ew * SR)
    t_ew = np.linspace(0.0, d_ew, n_ew, endpoint=False)
    grip_w = FoleySynth.wood_oak_impact(0.24, f0=135.0, splinter=0.1, seed=9502)
    sc_low = DSP.bandpass(DSP.pink_noise(n_ew, rng_ew) * np.exp(-t_ew / 0.20), 850.0, 2400.0) * 1.1
    lock_w = FoleySynth.wrought_iron_impact(0.24, f0=460.0, hardness=0.88, damp=0.08, seed=9503)
    wep = sc_low
    wep[:len(grip_w)] += grip_w * 0.85
    lo_idx = int(0.19 * SR)
    wep[lo_idx:lo_idx + len(lock_w)] += lock_w[:n_ew - lo_idx] * 0.8
    assets["SFX_Equip_Weapon.wav"] = (DSP.convolve(DSP.pan_stereo(wep, -0.04), space="close", wet=0.10, dry=0.94), 0.78)

    rng_ea = np.random.default_rng(9601)
    d_ea = 0.48
    n_ea = int(d_ea * SR)
    t_ea = np.linspace(0.0, d_ea, n_ea, endpoint=False)
    gambeson_a = FoleySynth.visceral_sub_thud(n_ea, 55.0, 80.0, 0.05, 1.05, 9602)
    plate_a = FoleySynth.wrought_iron_impact(0.30, f0=250.0, hardness=0.78, damp=0.07, seed=9603)
    strap_a = DSP.bandpass(DSP.pink_noise(n_ea, rng_ea) * np.exp(-t_ea / 0.14), 380.0, 2200.0) * 0.9
    buckle_a = FoleySynth.wrought_iron_impact(0.16, f0=540.0, hardness=0.88, damp=0.035, seed=9604)
    arm = gambeson_a + strap_a
    arm[int(0.018 * SR):int(0.018 * SR) + len(plate_a)] += plate_a[:n_ea - int(0.018 * SR)] * 0.85
    arm[int(0.175 * SR):int(0.175 * SR) + len(buckle_a)] += buckle_a[:n_ea - int(0.175 * SR)] * 0.75
    assets["SFX_Equip_Armor.wav"] = (DSP.convolve(DSP.pan_stereo(arm, 0.04), space="close", wet=0.07, dry=0.96), 0.76)

    assets["SFX_Salvage_Anvil.wav"] = (FoleySynth.anvil_strike(dur=0.95, f0=310.0, quench=True, seed=9701), 0.86)

    # Relic Drop
    r_bell = FoleySynth.sanctuary_bronze_bell(164.81, dur=1.65, dark_weight=1.05, shimmer=0.70, seed=7030)
    r_qtz = DSP.convolve(DSP.pan_stereo(FoleySynth.crystal_shard_cluster(1.65, 580.0, 5, 0.04, 7031), 0.0), space="cathedral", wet=0.25, dry=0.86) * 0.45
    assets["SFX_Relic_Drop.wav"] = (r_bell + r_qtz, 0.88)
    assets["SFX_RelicDrop.wav"] = ((r_bell + r_qtz).copy(), 0.88)

    # --------------------------------------------------------------------------
    # 5. ORBS & INVOCATION
    # --------------------------------------------------------------------------
    d_q = 0.44
    flint_q = DSP.bandpass(DSP.pink_noise(int(d_q * SR), np.random.default_rng(2001)) * np.exp(-np.linspace(0, d_q, int(d_q * SR)) / 0.009), 1800.0, 5600.0) * 1.25
    fire_q = FoleySynth.fire_roar(d_q, 1.0, 120.0, 1150.0, 1.4, 2002) * np.exp(-np.linspace(0, d_q, int(d_q * SR)) / 0.18)
    assets["SFX_OrbQ.wav"] = (DSP.convolve(DSP.pan_stereo(flint_q + fire_q + FoleySynth.visceral_sub_thud(int(d_q * SR), 55.0, 75.0, 0.038, 0.75, 2003), 0.06), space="close", wet=0.10, dry=0.94), 0.74)

    d_w = 0.46
    gust_w = DSP.bandpass(DSP.pink_noise(int(d_w * SR), np.random.default_rng(2010)) * np.exp(-np.linspace(0, d_w, int(d_w * SR)) / 0.22), 340.0, 2800.0) * 1.45
    assets["SFX_OrbW.wav"] = (DSP.convolve(DSP.pan_stereo(gust_w + FoleySynth.plasma_lightning(d_w, 2, 0.15, 2011) * 0.68 + FoleySynth.visceral_sub_thud(int(d_w * SR), 58.0, 80.0, 0.032, 0.7, 2012), -0.06), space="close", wet=0.11, dry=0.94), 0.74)

    d_e = 0.48
    n_e = int(d_e * SR)
    pull_i = int(0.09 * SR)
    pull_e = DSP.bandpass(DSP.pink_noise(pull_i, np.random.default_rng(2020)) * (np.linspace(0.0, 1.0, pull_i) ** 2.0), 140.0, 1400.0) * 1.1
    imp_e = FoleySynth.stone_impact((n_e - pull_i) / SR, weight=1.1, grit=0.45, seed=2021)[:n_e - pull_i]
    e_sig = np.zeros(n_e)
    e_sig[:pull_i] = pull_e
    e_sig[pull_i:] = imp_e
    assets["SFX_OrbE.wav"] = (DSP.convolve(DSP.pan_stereo(e_sig, 0.0), space="close", wet=0.12, dry=0.93), 0.76)

    d_inv = 0.62
    tome_inv = FoleySynth.wood_oak_impact(0.32, f0=115.0, splinter=0.1, seed=2030)
    inv_sig = np.zeros(int(d_inv * SR))
    inv_sig[:len(tome_inv)] += tome_inv * 1.15
    for k, dt in enumerate([0.045, 0.115, 0.195]):
        idx = int(dt * SR)
        latch = FoleySynth.wrought_iron_impact(0.20, f0=360.0 + k * 75.0, hardness=0.84, damp=0.055, seed=2031 + k)
        ll = min(len(latch), len(inv_sig) - idx)
        inv_sig[idx:idx + ll] += latch[:ll] * 0.65
    assets["SFX_Invoke.wav"] = (DSP.convolve(DSP.pan_stereo(inv_sig, 0.0), space="hall", wet=0.16, dry=0.92), 0.82)

    l1_arm = FoleySynth.wrought_iron_impact(0.10, f0=440.0, hardness=0.85, damp=0.032, seed=2040)
    l2_arm = FoleySynth.wrought_iron_impact(0.11, f0=590.0, hardness=0.88, damp=0.038, seed=2041)
    arm_sig = np.zeros(int(0.16 * SR))
    arm_sig[:len(l1_arm)] += l1_arm * 0.8
    arm_sig[int(0.048 * SR):int(0.048 * SR) + len(l2_arm)] += l2_arm[:len(arm_sig) - int(0.048 * SR)] * 0.9
    assets["SFX_Slot_Arm.wav"] = (DSP.stereo_decorrelate(arm_sig, 0.3, seed=2042), 0.62)

    # Deny sounds (broadband, no narrow sine modes)
    rng_cd = np.random.default_rng(2060)
    t_cd = np.linspace(0.0, 0.15, int(0.15 * SR), endpoint=False)
    cd1 = DSP.band_noise(len(t_cd), rng_cd, 160.0, 820.0) * np.exp(-t_cd / 0.012) * 0.95
    assets["SFX_Skill_Cooldown_Deny.wav"] = (DSP.stereo_decorrelate(cd1, 0.25, seed=2062), 0.56)

    rng_md = np.random.default_rng(2065)
    t_md = np.linspace(0.0, 0.17, int(0.17 * SR), endpoint=False)
    md_thump = DSP.band_noise(len(t_md), rng_md, 85.0, 260.0) * np.exp(-t_md / 0.025) * 0.95
    md_sand = DSP.bandpass(DSP.pink_noise(len(t_md), rng_md) * np.exp(-t_md / 0.032), 420.0, 2100.0) * 0.95
    assets["SFX_Mana_Empty_Deny.wav"] = (DSP.stereo_decorrelate(md_thump + md_sand, 0.25, seed=2067), 0.56)

    # --------------------------------------------------------------------------
    # 6. MONSTERS, BOSSES & COMBAT VOICES
    # --------------------------------------------------------------------------
    sw_st = DSP.convolve(DSP.pan_stereo(FoleySynth.bone_impact(0.65, 7, 1.05, 6021), 0.0), space="close", wet=0.12, dry=0.92)
    assets["SFX_MobDeath_Swarm.wav"] = (sw_st, 0.80)
    assets["SFX_MobDeath.wav"] = (sw_st.copy(), 0.80)

    lich_g = FoleySynth.crystal_shard_cluster(0.76, 620.0, 4, 0.025, 6025) * 0.65
    lich_b = FoleySynth.bone_impact(0.76, 5, 1.0, 6026)
    assets["SFX_MobDeath_Ranged.wav"] = (DSP.convolve(DSP.pan_stereo(lich_g + lich_b, 0.0), space="hall", wet=0.16, dry=0.91), 0.82)

    br_d = np.zeros(int(0.90 * SR))
    p1_d = FoleySynth.wrought_iron_impact(0.52, 185.0, 0.75, 0.12, 6030)
    p2_d = FoleySynth.stone_impact(0.52, 1.1, 0.7, 6031)
    br_d[:len(p1_d)] += p1_d * 0.75 + p2_d * 0.85
    assets["SFX_MobDeath_Bruiser.wav"] = (DSP.convolve(DSP.pan_stereo(br_d, 0.0), space="hall", wet=0.18, dry=0.90), 0.84)

    exp_f = FoleySynth.fire_roar(0.85, 1.4, 85.0, 1750.0, 1.7, 6035) * np.exp(-np.linspace(0, 0.85, int(0.85 * SR)) / 0.28)
    exp_b = FoleySynth.bone_impact(0.85, 6, 1.1, 6036) * 0.72
    assets["SFX_MobDeath_Explode.wav"] = (DSP.convolve(DSP.pan_stereo(exp_f + exp_b, 0.0), space="hall", wet=0.20, dry=0.89), 0.86)

    # Boss Roar
    rng_br = np.random.default_rng(6100)
    d_br = 1.95
    n_br = int(d_br * SR)
    tt_br = np.linspace(0.0, d_br, n_br, endpoint=False)
    roar_env = (np.sin(np.pi * (tt_br / d_br)) ** 0.85) * np.clip(tt_br / 0.12, 0.0, 1.0)
    vc_exc = np.zeros(n_br)
    ct = 0.02
    while ct < d_br - 0.08:
        u_c = ct / d_br
        f_vc = 68.0 - 22.0 * u_c + rng_br.uniform(-7.0, 7.0)
        idx = int(ct * SR)
        pl = min(n_br - idx, int(0.016 * SR))
        if pl > 8:
            vc_exc[idx:idx + pl] += DSP.pink_noise(pl, rng_br) * np.exp(-np.linspace(0, 1, pl) / 0.25) * rng_br.uniform(0.6, 1.3)
        ct += 1.0 / max(32.0, f_vc)
    jaw = DSP.resonant_bank((vc_exc + DSP.pink_noise(n_br, rng_br) * 0.35) * roar_env, [(185.0, 65.0, 1.35), (490.0, 110.0, 1.1), (1080.0, 180.0, 0.7), (2100.0, 280.0, 0.35)])
    sub_r = FoleySynth.visceral_sub_thud(n_br, 34.0, 60.0, decay_s=0.45, punch=1.35, seed=6101)
    roar_st = DSP.convolve(DSP.pan_stereo(DSP.soft_sat(jaw * 1.45 + sub_r, 1.35), 0.0), space="cathedral", wet=0.28, dry=0.85)
    assets["SFX_Boss_Roar.wav"] = (roar_st, PEAK_NEG_1DB)
    assets["SFX_BossRoar.wav"] = (roar_st.copy(), PEAK_NEG_1DB)

    # Boss Telegraph, Stagger Break, Phase Transition, Death
    horn_air = DSP.resonant_bank(
        DSP.pink_noise(int(1.05 * SR), np.random.default_rng(6110)) * np.exp(-np.linspace(0, 1.05, int(1.05 * SR)) / 0.52),
        [(110.0, 45.0, 0.65), (165.0, 58.0, 0.95), (247.0, 75.0, 1.05)],
    ) * 1.35
    scrape_bt = FoleySynth.wrought_iron_impact(1.05, 310.0, 0.88, 0.14, 6111) * 0.88
    assets["SFX_Boss_Telegraph.wav"] = (DSP.convolve(DSP.pan_stereo(horn_air + scrape_bt, 0.0), space="hall", wet=0.18, dry=0.90), 0.90)

    sb_armor = FoleySynth.wrought_iron_impact(1.30, 260.0, 0.90, 0.22, 6140)
    sb_stone = FoleySynth.stone_impact(1.30, 1.35, 0.9, 6141)
    sb_gong = FoleySynth.sanctuary_bronze_bell(130.81, 1.30, 1.15, 0.45, 6142) * 0.58
    assets["SFX_Boss_Stagger_Break.wav"] = (DSP.convolve(DSP.pan_stereo(sb_armor + sb_stone, 0.0), space="cathedral", wet=0.24, dry=0.87) + sb_gong, PEAK_NEG_1DB)

    pt_choir = FoleySynth.organic_choir([146.83, 207.65, 293.66], 1.85, "aah", 6150) * 0.65
    pt_bell = FoleySynth.sanctuary_bronze_bell(98.0, 1.85, 1.2, 0.55, 6151) * 0.68
    assets["SFX_Boss_Phase_Transition.wav"] = (pt_choir + pt_bell, PEAK_NEG_1DB)

    bd_bell = FoleySynth.sanctuary_bronze_bell(82.41, 2.35, 1.25, 0.60, 6160) * 0.72
    bd_stone = FoleySynth.stone_impact(2.35, 1.7, 1.0, 6161) * 0.88
    assets["SFX_Boss_Death.wav"] = (bd_bell + DSP.convolve(DSP.pan_stereo(bd_stone, 0.0), space="cathedral", wet=0.28, dry=0.85), PEAK_NEG_1DB)

    # Boss Signatures (Magma, Cryo, Void)
    sig_mag = FoleySynth.fire_roar(1.30, 1.4, 85.0, 1550.0, 1.6, 6170) * np.exp(-np.linspace(0, 1.30, int(1.30 * SR)) / 0.48) + FoleySynth.wrought_iron_impact(1.30, 210.0, 0.85, 0.16, 6171) * 0.58
    assets["SFX_Boss_Sig_Magma.wav"] = (DSP.convolve(DSP.pan_stereo(sig_mag, 0.0), space="cathedral", wet=0.24, dry=0.87), PEAK_NEG_1DB)

    sig_cryo = FoleySynth.crystal_shard_cluster(1.25, 540.0, 7, 0.03, 6172)
    assets["SFX_Boss_Sig_CryoTide.wav"] = (DSP.convolve(DSP.pan_stereo(sig_cryo, 0.0), space="cathedral", wet=0.24, dry=0.87), PEAK_NEG_1DB)

    sig_void = FoleySynth.plasma_lightning(1.25, 4, 0.95, 6174) + FoleySynth.stone_impact(1.25, 1.1, 0.45, 6175) * 0.65
    assets["SFX_Boss_Sig_VoidStorm.wav"] = (DSP.convolve(DSP.pan_stereo(sig_void, 0.0), space="cathedral", wet=0.24, dry=0.87), PEAK_NEG_1DB)

    # --------------------------------------------------------------------------
    # 7. PROPS, OBJECTIVES, SHRINES & NOTIFICATIONS
    # --------------------------------------------------------------------------
    urn_thump = FoleySynth.visceral_sub_thud(int(0.68 * SR), 65.0, 95.0, 0.025, 0.65, 7002)
    urn_shards = FoleySynth.crystal_shard_cluster(0.68, 780.0, 8, 0.02, 7001)
    assets["SFX_Urn_Shatter.wav"] = (DSP.convolve(DSP.pan_stereo(urn_thump + urn_shards, 0.0), space="close", wet=0.12, dry=0.93), 0.78)

    b_w = FoleySynth.wood_oak_impact(0.65, f0=140.0, splinter=1.1, seed=7010)
    b_h1 = FoleySynth.wrought_iron_impact(0.45, f0=360.0, hardness=0.78, damp=0.09, seed=7011)
    barrel = b_w.copy()
    barrel[int(0.04 * SR):int(0.04 * SR) + len(b_h1)] += b_h1[:len(barrel) - int(0.04 * SR)] * 0.45
    assets["SFX_Barrel_Break.wav"] = (DSP.convolve(DSP.pan_stereo(barrel, 0.0), space="close", wet=0.12, dry=0.93), 0.82)

    latch_ch = FoleySynth.wrought_iron_impact(0.16, 440.0, 0.88, 0.05, 7021)
    creak_ch = FoleySynth.helmholtz_creak(0.68, 22.0, 42.0, (145.0, 310.0, 680.0), seed=7022)
    chest = np.zeros(int(1.05 * SR))
    chest[:len(latch_ch)] += latch_ch
    chest[int(0.07 * SR):int(0.07 * SR) + len(creak_ch)] += creak_ch[:len(chest) - int(0.07 * SR)] * 1.15
    assets["SFX_Chest_Open.wav"] = (DSP.convolve(DSP.pan_stereo(chest, -0.05), space="hall", wet=0.18, dry=0.90), 0.84)

    hb1 = FoleySynth.visceral_sub_thud(int(0.44 * SR), 42.0, 65.0, 0.045, 1.05, 7041)
    assets["SFX_Orb_Heal_Pickup.wav"] = (DSP.convolve(DSP.pan_stereo(hb1, 0.0), space="close", wet=0.11, dry=0.94), 0.72)

    siphon_m = DSP.bandpass(DSP.pink_noise(int(0.42 * SR), np.random.default_rng(7045)) * np.exp(-np.linspace(0, 0.42, int(0.42 * SR)) / 0.11), 380.0, 2500.0) * 0.9
    m_qtz = FoleySynth.crystal_shard_cluster(0.42, 560.0, 3, 0.025, 7046) * 0.72
    assets["SFX_Orb_Mana_Pickup.wav"] = (DSP.convolve(DSP.pan_stereo(siphon_m + m_qtz, 0.0), space="close", wet=0.12, dry=0.94), 0.72)

    cork = FoleySynth.wood_oak_impact(0.06, f0=310.0, splinter=0.0, seed=7051) * 1.1
    glugs = FoleySynth.liquid_gulp([0.09, 0.22, 0.36], 0.66, seed=7053)
    fl_sig = glugs.copy()
    fl_sig[:len(cork)] += cork
    assets["SFX_Flask_Drink.wav"] = (DSP.convolve(DSP.pan_stereo(fl_sig, 0.05), space="close", wet=0.10, dry=0.94), 0.76)

    # Shrines
    sh_e = FoleySynth.stone_impact(1.30, 1.1, 0.48, 7060) * 0.85 + FoleySynth.crystal_shard_cluster(1.30, 440.0, 5, 0.045, 7061) * 0.68
    assets["SFX_Shrine_Essence.wav"] = (DSP.convolve(DSP.pan_stereo(sh_e, 0.0), space="cathedral", wet=0.24, dry=0.87), 0.86)

    sh_choir = FoleySynth.organic_choir([220.0, 277.18, 329.63, 440.0], 1.40, "aah", seed=7063) * 0.55
    assets["SFX_Shrine_Healing.wav"] = (sh_choir, 0.86)

    sh_c = FoleySynth.plasma_lightning(1.25, 3, 0.70, 7064) * 0.78 + FoleySynth.wrought_iron_impact(1.25, 310.0, 0.85, 0.18, 7065) * 0.65
    assets["SFX_Shrine_ChronoFury.wav"] = (DSP.convolve(DSP.pan_stereo(sh_c, 0.0), space="hall", wet=0.22, dry=0.88), 0.86)

    # Objectives & World Gates
    bc_bell = FoleySynth.sanctuary_bronze_bell(196.0, 1.40, 1.0, 0.55, 7070)
    assets["SFX_Beacon_Capture.wav"] = (bc_bell * 0.85, 0.88)
    assets["SFX_BeaconCapture.wav"] = ((bc_bell * 0.85).copy(), 0.88)

    rng_ob = np.random.default_rng(7075)
    t_ob = np.linspace(0.0, 0.42, int(0.42 * SR), endpoint=False)
    ob_air = DSP.band_noise(len(t_ob), rng_ob, 180.0, 3400.0) * np.exp(-t_ob / 0.075) * 0.55
    ob_tick = FoleySynth.stone_impact(0.42, 0.65, 0.3, 7075) * 0.70 + FoleySynth.crystal_shard_cluster(0.42, 390.0, 4, 0.04, 7076) * 0.42 + ob_air
    assets["SFX_Obelisk_Charge_Tick.wav"] = (DSP.convolve(DSP.pan_stereo(ob_tick, 0.0), space="hall", wet=0.18, dry=0.90), 0.76)

    assets["SFX_Obelisk_Complete.wav"] = (
        FoleySynth.sanctuary_bronze_bell(146.83, 1.80, 1.15, 0.65, 7077) * 0.75
        + FoleySynth.organic_choir([146.83, 220.0, 293.66], 1.80, "aah", 7078) * 0.52,
        0.90,
    )

    py_s = FoleySynth.stone_impact(0.95, 1.25, 1.0, 7080)
    py_g = FoleySynth.crystal_shard_cluster(0.95, 510.0, 7, 0.02, 7081) * 0.78
    assets["SFX_Pylon_Shatter.wav"] = (DSP.convolve(DSP.pan_stereo(py_s + py_g, 0.0), space="hall", wet=0.22, dry=0.88), 0.88)

    bolt_gate = FoleySynth.wrought_iron_impact(0.55, 220.0, 0.9, 0.14, 7091)
    groan_gate = FoleySynth.helmholtz_creak(1.30, 16.0, 28.0, (78.0, 165.0, 340.0), seed=7099)
    gate = np.zeros(int(1.65 * SR))
    gate[:len(bolt_gate)] += bolt_gate
    gate[int(0.18 * SR):int(0.18 * SR) + len(groan_gate)] += groan_gate[:len(gate) - int(0.18 * SR)] * 1.1
    assets["SFX_Boss_Gate_Open.wav"] = (DSP.convolve(DSP.pan_stereo(gate, 0.0), space="cathedral", wet=0.24, dry=0.87), 0.90)

    # Portal Open & Enter
    rng_po = np.random.default_rng(7100)
    d_po = 1.75
    n_po = int(d_po * SR)
    tear_po = DSP.bandpass(DSP.pink_noise(n_po, rng_po) * np.sin(np.pi * np.linspace(0, 1, n_po)), 180.0, 3200.0) * 1.45
    sub_po = FoleySynth.visceral_sub_thud(n_po, 40.0, 65.0, 0.5, 1.25, 7101)
    assets["SFX_PortalOpen.wav"] = (DSP.convolve(DSP.pan_stereo(tear_po + sub_po, 0.0), space="cathedral", wet=0.28, dry=0.85), 0.88)

    pe_n = int(0.85 * SR)
    pe_s = DSP.bandpass(DSP.pink_noise(pe_n, rng_po) * np.sin(np.pi * np.linspace(0, 1, pe_n)), 260.0, 3400.0) * 1.2 + FoleySynth.visceral_sub_thud(pe_n, 45.0, 70.0, 0.12, 1.1, 7105)
    assets["SFX_Portal_Enter.wav"] = (DSP.convolve(DSP.pan_stereo(pe_s, 0.0), space="cathedral", wet=0.25, dry=0.87), 0.84)

    # Ambient Brazier Loop
    b_main = FoleySynth.fire_roar(4.8, 0.65, 160.0, 2200.0, 2.2, seed=7110)
    b_loop = DSP.make_seamless_loop(DSP.stereo_decorrelate(b_main, 0.6, seed=7111), crossfade_sec=0.8)
    assets["SFX_Amb_Brazier_Loop.wav"] = (b_loop[:, :int(4.0 * SR)], 0.65)

    # Co-op Events
    ad_bell = FoleySynth.sanctuary_bronze_bell(110.0, 2.2, 1.15, 0.45, 7120)
    assets["SFX_Ally_Down.wav"] = (ad_bell * 0.85, 0.90)

    rc_choir = FoleySynth.organic_choir([196.0, 246.94, 293.66, 329.63, 440.0], 2.4, "aah", 7125) * 0.75
    assets["SFX_Revive_Channel.wav"] = (rc_choir, 0.76)

    rv_bell = FoleySynth.sanctuary_bronze_bell(293.66, 1.9, 0.85, 0.75, 7130)
    assets["SFX_Revive_Complete.wav"] = (rv_bell * 0.85, 0.88)

    pw_bell = FoleySynth.sanctuary_bronze_bell(73.42, 2.45, 1.35, 0.5, 7136)
    pw_thud = DSP.convolve(DSP.pan_stereo(FoleySynth.visceral_sub_thud(pw_bell.shape[1], 32.0, 58.0, 0.42, 1.55, 7135), 0.0), space="cathedral", wet=0.26, dry=0.86)
    assets["SFX_Party_Wipe.wav"] = (pw_bell * 0.72 + pw_thud * 0.68, PEAK_NEG_1DB)

    lu_bell = FoleySynth.sanctuary_bronze_bell(146.83, 2.25, 1.15, 0.75, 7141)
    lu_choir = FoleySynth.organic_choir([146.83, 220.0, 293.66, 349.23], 2.25, "aah", 7140)
    assets["SFX_LevelUp.wav"] = (lu_bell * 0.68 + lu_choir * 0.58, PEAK_NEG_1DB)

    bn_st = FoleySynth.stone_impact(0.62, 0.62, 0.2, 7145) * 0.78
    bn_ir = FoleySynth.wrought_iron_impact(0.62, 260.0, 0.68, 0.10, 7146) * 0.55
    assets["SFX_Banner_Notify.wav"] = (DSP.convolve(DSP.pan_stereo(bn_st + bn_ir, 0.0), space="hall", wet=0.16, dry=0.92), 0.68)

    # --------------------------------------------------------------------------
    # 8. DRY TACTILE UI FOLEY (ZERO REVERB WASH, ZERO BEEP)
    # --------------------------------------------------------------------------
    rng_ui = np.random.default_rng(8001)
    d_ui = 0.092
    n_ui = int(d_ui * SR)
    t_ui = np.linspace(0.0, d_ui, n_ui, endpoint=False)
    ui_thump = DSP.band_noise(n_ui, rng_ui, 120.0, 480.0) * np.exp(-t_ui / 0.015) * 0.95
    ui_mid = DSP.band_noise(n_ui, rng_ui, 520.0, 1800.0) * np.exp(-t_ui / 0.010) * 1.15
    ui_snap = DSP.band_noise(n_ui, rng_ui, 1900.0, 5200.0) * np.exp(-t_ui / 0.006) * 0.95
    assets["SFX_UIClick.wav"] = (DSP.stereo_decorrelate(ui_thump + ui_mid + ui_snap, 0.22, seed=8004), 0.58)

    h_n = int(0.038 * SR)
    h_t = np.linspace(0.0, 0.038, h_n, endpoint=False)
    hover = DSP.bandpass(DSP.pink_noise(h_n, np.random.default_rng(8010)) * np.exp(-h_t / 0.0075), 380.0, 1950.0) * 1.15
    assets["SFX_UI_Hover.wav"] = (DSP.stereo_decorrelate(hover, 0.18, seed=8011), 0.36)

    # Grimoire Open / Close & Page Turn
    rng_gr = np.random.default_rng(8020)
    g_n = int(0.38 * SR)
    clasp_g = FoleySynth.wrought_iron_impact(0.14, 480.0, 0.82, 0.035, 8021) * 0.55
    leather_g = FoleySynth.wood_oak_impact(0.20, 118.0, 0.05, 8022) * 0.85
    vellum_g = DSP.bandpass(DSP.pink_noise(g_n, rng_gr) * np.exp(-np.linspace(0, 0.38, g_n) / 0.11), 650.0, 3400.0) * 1.05
    g_op = vellum_g.copy()
    g_op[:len(clasp_g)] += clasp_g
    g_op[int(0.04 * SR):int(0.04 * SR) + len(leather_g)] += leather_g[:g_n - int(0.04 * SR)]
    assets["SFX_UI_Grimoire_Open.wav"] = (DSP.stereo_decorrelate(g_op, 0.35, seed=8023), 0.68)

    gc_n = int(0.28 * SR)
    thump_bk = FoleySynth.wood_oak_impact(0.28, 95.0, 0.0, 8025) * 1.15
    cl_lock = FoleySynth.wrought_iron_impact(0.14, 410.0, 0.82, 0.032, 8026) * 0.45
    g_cl = thump_bk.copy()
    g_cl[int(0.07 * SR):int(0.07 * SR) + len(cl_lock)] += cl_lock[:gc_n - int(0.07 * SR)]
    assets["SFX_UI_Grimoire_Close.wav"] = (DSP.stereo_decorrelate(g_cl, 0.30, seed=8027), 0.68)

    pt_n = int(0.22 * SR)
    pt_env = np.sin(np.pi * np.linspace(0, 1, pt_n)) ** 1.05
    page = DSP.bandpass(DSP.pink_noise(pt_n, np.random.default_rng(8030)) * pt_env, 680.0, 3400.0) * 1.15
    assets["SFX_PageTurn.wav"] = (DSP.stereo_decorrelate(page, 0.28, seed=8031), 0.56)

    at_n = int(0.40 * SR)
    scroll_at = DSP.bandpass(DSP.pink_noise(at_n, np.random.default_rng(8040)) * np.sin(np.pi * np.linspace(0, 1, at_n)), 480.0, 2900.0) * 1.15
    rod_at = FoleySynth.wood_oak_impact(0.18, 175.0, 0.05, 8041) * 0.65
    at_sig = scroll_at.copy()
    at_sig[:len(rod_at)] += rod_at
    assets["SFX_UI_Atlas_Open.wav"] = (DSP.stereo_decorrelate(at_sig, 0.30, seed=8043), 0.66)
    assets["SFX_UI_Forge_Anvil.wav"] = (FoleySynth.anvil_strike(dur=0.88, f0=340.0, quench=True, seed=8050), 0.82)

    return assets


# ==============================================================================
# 4. MASTERING, SYNC & OBJECTIVE SPECTRAL VERIFICATION
# ==============================================================================

def master_and_write(filename: str, stereo_audio: np.ndarray, target_peak: float) -> Dict[str, float]:
    """
    Masters audio to 48kHz 16-bit stereo PCM with clean fade curves,
    mean-subtracted zero DC offset, and exact peak normalization to -1.0 dBFS max.
    """
    if stereo_audio.ndim == 1:
        stereo_audio = np.vstack([stereo_audio, stereo_audio])

    # Clean sub-audible DC highpass at 26 Hz + subtract exact channel mean
    audio = DSP.highpass(stereo_audio.astype(np.float64), 26.0, order=2)
    audio = audio - np.mean(audio, axis=1, keepdims=True)

    # Click-free linear fade-in (2ms) and fade-out (8ms)
    n = audio.shape[1]
    fi = min(n // 6, int(0.002 * SR))
    fo = min(n // 4, int(0.008 * SR))
    if fi > 1:
        audio[:, :fi] *= np.linspace(0.0, 1.0, fi)
    if fo > 1:
        audio[:, -fo:] *= np.linspace(1.0, 0.0, fo)

    # Peak normalization capped strictly at -1.0 dBFS (0.89125)
    target_norm = min(target_peak, PEAK_NEG_1DB)
    mx = float(np.max(np.abs(audio)))
    if mx > 1e-8:
        audio = audio * (target_norm / mx)

    # Final zero DC check
    audio = audio - np.mean(audio, axis=1, keepdims=True)

    pcm16 = np.clip(audio * 32767.0, -32768.0, 32767.0).astype(np.int16).T
    primary_path = os.path.join(TARGET_DIRS[0], filename)
    os.makedirs(TARGET_DIRS[0], exist_ok=True)
    wavfile.write(primary_path, SR, pcm16)

    # Mirror byte-identically to all secondary directories
    for mirror_dir in TARGET_DIRS[1:]:
        os.makedirs(mirror_dir, exist_ok=True)
        dst = os.path.join(mirror_dir, filename)
        for attempt in range(6):
            try:
                shutil.copy2(primary_path, dst)
                break
            except OSError:
                time.sleep(0.20 * (attempt + 1))

    return analyze_wav(primary_path)


def analyze_wav(path: str) -> Dict[str, float]:
    """Runs rigorous FFT spectral analysis to audit anti-beep and sub-bass energy."""
    sr, data = wavfile.read(path)
    assert sr == SR, f"{path}: SR={sr} != {SR}"
    assert data.ndim == 2 and data.shape[1] == 2, f"{path}: not stereo"
    x = data.astype(np.float32) / 32768.0
    assert np.all(np.isfinite(x)), f"{path}: NaN/Inf"

    dc = float(np.max(np.abs(np.mean(x, axis=0))))
    assert dc < 0.002, f"{path}: DC offset {dc:.5f}"

    mono = 0.5 * (x[:, 0] + x[:, 1])
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x ** 2)))
    crest_db = 20.0 * np.log10((peak + 1e-9) / (rms + 1e-9))

    win = np.hanning(len(mono))
    spec_pow = np.abs(np.fft.rfft(mono * win)) ** 2
    freqs = np.fft.rfftfreq(len(mono), 1.0 / sr)
    tot = float(np.sum(spec_pow)) + 1e-12

    top1_pct = float(np.max(spec_pow)) / tot
    top2_pct = float(np.sum(np.sort(spec_pow)[-2:])) / tot
    rumble_40_80_pct = float(np.sum(spec_pow[(freqs >= 40.0) & (freqs <= 80.0)])) / tot
    sub_pct = float(np.sum(spec_pow[(freqs >= 20.0) & (freqs <= 120.0)])) / tot
    centroid = float(np.sum(freqs * spec_pow) / tot)

    return {
        "dur": len(mono) / sr,
        "peak": peak,
        "rms": rms,
        "crest_db": crest_db,
        "centroid": centroid,
        "sub_pct": sub_pct,
        "rumble_40_80_pct": rumble_40_80_pct,
        "top1_pct": top1_pct,
        "top2_pct": top2_pct,
    }


def verify_requirements(fname: str, m: Dict[str, float]) -> List[str]:
    issues: List[str] = []
    # Zero clipping check (strictly <= -1.0 dBFS)
    if m["peak"] > 0.895:
        issues.append(f"peak {m['peak']:.4f} exceeds -1.0 dBFS (0.89125)")
    if m["rms"] < 0.012:
        issues.append(f"rms {m['rms']:.4f} < 0.012")
    if m["crest_db"] < 9.0:
        issues.append(f"crest {m['crest_db']:.1f}dB < 9.0dB")

    # Anti-cartoon / anti-beep check (Top-1 FFT bin peak < 15%)
    if m["top1_pct"] >= 0.15:
        issues.append(f"top-1 FFT bin {m['top1_pct']*100:.1f}% >= 15% (pure tone concentration)")
    if m["top2_pct"] >= 0.22:
        issues.append(f"top-2 FFT bins {m['top2_pct']*100:.1f}% >= 22%")

    # Sub-bass rumble check (40-80 Hz) for heavy impacts, slams, spells, legendary loot
    heavy_keys = ("Hit_Flesh", "Hit_Armor", "Attack_Slam", "Slam", "Legendary", "Fireball_Impact", "Boom", "Cataclysm", "Macropyre", "Blackhole", "BlackHole", "Meteor_Impact", "Boss_Death", "Party_Wipe")
    if any(k in fname for k in heavy_keys) and m["rumble_40_80_pct"] < 0.08:
        issues.append(f"heavy rumble (40-80Hz) {m['rumble_40_80_pct']*100:.1f}% < 8%")

    # Mirror check
    p0 = os.path.join(TARGET_DIRS[0], fname)
    p1 = os.path.join(TARGET_DIRS[1], fname)
    if not os.path.exists(p1) or os.path.getsize(p0) != os.path.getsize(p1):
        issues.append("mirror sync mismatch")

    return issues


def main() -> int:
    print("=" * 114)
    print("AETHERIA DIABLO IV DARK-FANTASY AUDIO SYNTHESIS & RIGOROUS SPECTRAL AUDIT")
    print("=" * 114)
    t0 = time.time()

    assets = build_all_sfx()
    print(f"Synthesizing & mastering {len(assets)} AAA SFX assets -> GodotAetheria + AetheriaData...")
    print(f"{'FILE':<32} | {'DUR':>5} | {'PEAK':>6} | {'RMS':>5} | {'CREST':>6} | {'40-80Hz':>7} | {'TOP1%':>5} | {'TOP2%':>5} | {'STATUS'}")
    print("-" * 114)

    failures = []
    max_top1 = 0.0
    max_top2 = 0.0
    for fname in sorted(assets.keys()):
        audio, target_peak = assets[fname]
        m = master_and_write(fname, audio, target_peak)
        issues = verify_requirements(fname, m)
        max_top1 = max(max_top1, m["top1_pct"])
        max_top2 = max(max_top2, m["top2_pct"])
        status = "PASS" if not issues else "FAIL: " + "; ".join(issues)
        if issues:
            failures.append((fname, issues))
        print(
            f"{fname:<32} | {m['dur']:5.2f} | {m['peak']:6.4f} | {m['rms']:5.3f} | "
            f"{m['crest_db']:5.1f}d | {m['rumble_40_80_pct']*100:6.1f}% | {m['top1_pct']*100:4.1f}% | {m['top2_pct']*100:4.1f}% | {status}"
        )

    print("-" * 114)
    print(f"COMPLETED {len(assets)} SFX in {time.time() - t0:.2f}s | Max Top-1 Bin = {max_top1*100:.1f}% | Max Top-2 Bin = {max_top2*100:.1f}% | Failures = {len(failures)}")
    if failures:
        for fn, iss in failures:
            print(f"  [FAIL] {fn}: {iss}")
        return 1
    print("ALL SFX PASSED ZERO-CLIPPING (-1.0 dBFS), VISCERAL 40-80Hz RUMBLE & ZERO-PURE-SINE (<15%) AUDIT!")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
