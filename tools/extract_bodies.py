#!/usr/bin/env python3
# ReChord: "body" curves from reference recordings -> Source/Bodies.h
# For each section: the long-term spectrum of its steady part, the tops of its harmonic peaks joined into a curve
# (dB at 64 log-spaced points, 30 Hz .. 16 kHz), and how much noise sits between the harmonics (bow / air).
# usage: extract_bodies.py ref.wav out.h
import sys
import numpy as np
import scipy.io.wavfile as wavfile
from scipy.signal import welch, find_peaks, medfilt

# sections of the reference, named by the user. KEYS: CHARACTER morphs felt piano -> felt piano 2 -> Rhodes.
# STRINGS, dark to bright: winter, dreamy (Olafur Arnalds), texture, soft, eternal, sharp
KEYS = [("Felt piano", 0.05, 1.2), ("Felt piano 2", 6.6, 7.8), ("Rhodes", 20.0, 25.0)]
STRINGS = [("Winter", 13.5, 19.0), ("Dreamy", 52.5, 58.8), ("Texture", 26.0, 32.3), ("Soft", 33.5, 38.3),
           ("Eternal", 39.2, 44.5), ("Sharp", 45.2, 51.2)]
GRID = np.geomspace(30.0, 16000.0, 64)


def curve(x, sr, a, b):
    s = x[int(a * sr):int(b * sr)].mean(axis=1)
    f, P = welch(s, sr, nperseg=16384, noverlap=12288)
    d = 10 * np.log10(P + 1e-20)
    base = medfilt(d, 301)                               # the floor under the peaks
    pk, _ = find_peaks(d, height=base + 6.0, distance=8)
    pk = pk[(f[pk] > 40) & (f[pk] < 16000)]
    pf, pd = f[pk], d[pk]
    keep = pd > pd.max() - 75
    pf, pd = pf[keep], pd[keep]
    env = np.full(len(GRID), np.nan)
    for i, g in enumerate(GRID):
        w = (pf > g / 2 ** (1 / 6)) & (pf < g * 2 ** (1 / 6))
        if w.any():
            env[i] = pd[w].max()
    ok = ~np.isnan(env)
    idx = np.arange(len(GRID))
    # the body starts at the lowest strong note (room hum / bleed under it is ignored)
    strong = ok & (np.nan_to_num(env, nan=-999) > np.nanmax(env) - 22.0)
    lo, hi = idx[strong][0], idx[ok][-1]
    ok[:lo] = False
    env[ok == False] = np.interp(idx[ok == False], idx[ok], env[ok])
    env[:lo] = env[lo]                                   # below the lowest harmonic: flat
    # above the last harmonic: keep falling with the slope of the last octave
    oct_pts = max(2, int(64 / np.log2(16000 / 30)))
    slope = (env[hi] - env[max(lo, hi - oct_pts)]) / max(1, hi - max(lo, hi - oct_pts))
    slope = min(slope, -1.0)
    for i in range(hi + 1, len(GRID)):
        env[i] = env[hi] + slope * (i - hi)
    env = np.convolve(np.pad(env, 1, mode="edge"), [0.25, 0.5, 0.25], "valid")
    env -= env.max()
    env = np.maximum(env, -90.0)
    # noise between harmonics, 800 Hz .. 6 kHz, relative to the curve there
    band = (f > 800) & (f < 6000)
    nz = np.median(base[band]) - np.interp(np.log(1500.0), np.log(GRID), env + pd.max())
    return env, float(np.clip(nz, -70, 0))


def main():
    sr, x = wavfile.read(sys.argv[1])
    x = x.astype(float) / 32768.0
    out = ["// ReChord - body curves measured from the user's reference (tools/extract_bodies.py). Do not edit by hand.",
           "#pragma once", "", "namespace rc", "{", "constexpr int kBodyPoints = 64;",
           "constexpr float kBodyLoHz = 30.f, kBodyHiHz = 16000.f;",
           "struct Body { const char* name; float noiseDb; float db[kBodyPoints]; };", ""]
    for title, items in (("kKeyBodies", KEYS), ("kStringBodies", STRINGS)):
        out.append("inline const Body %s[] = {" % title)
        for name, a, b in items:
            env, nz = curve(x, sr, a, b)
            print("%-14s noise %6.1f dB  curve@100/500/1k/2k/4k/8k: %s" % (
                name, nz, " ".join("%5.1f" % np.interp(np.log(h), np.log(GRID), env) for h in (100, 500, 1000, 2000, 4000, 8000))))
            vals = ", ".join("%.1ff" % v for v in env)
            out.append('    { "%s", %.1ff, { %s } },' % (name, nz, vals))
        out.append("};")
    out += ["constexpr int kNumKeyBodies = (int) (sizeof(kKeyBodies) / sizeof(kKeyBodies[0]));",
            "constexpr int kNumStringBodies = (int) (sizeof(kStringBodies) / sizeof(kStringBodies[0]));",
            "} // namespace rc", ""]
    open(sys.argv[2], "w").write("\n".join(out))


if __name__ == "__main__":
    main()
