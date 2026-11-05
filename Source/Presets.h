// ReChord - built-in sounds. Warm, vintage, a little lo-fi: leads with vibrato, string machines, voices, pads.
// A preset sets only the sound (not the chords, the links or the glide).
#pragma once
#include "Engine.h"

namespace rc
{
struct Preset
{
    const char* name;
    const char* group;
    int osc;
    float character, attackMs, bright, vibratoCents, releaseMs, warmth, space, doubleAmt, volumeDb;
};

//                      name               group      osc         char  attack bright vib  release warm space dbl  vol
inline const Preset kPresets[] = {
    { "Warm FM Lead",     "LEADS",   OscFM,      0.45f,   35, 0.45f, 16,   380, 0.20f, 0.20f, 0.0f, -4.9f },
    { "Velvet Lead",      "LEADS",   OscAnalog,  0.35f,   40, 0.32f, 18,   420, 0.35f, 0.25f, 0.0f, -3.0f },
    { "Tape Lead",        "LEADS",   OscAnalog,  0.55f,   25, 0.42f, 22,   350, 0.85f, 0.30f, 0.0f, -1.5f },
    { "Soft Bell Lead",   "LEADS",   OscFM,      0.75f,   12, 0.55f, 10,   700, 0.30f, 0.35f, 0.0f, -3.8f },
    { "Vintage Strings",  "STRINGS", OscStrings, 0.55f,  320, 0.45f,  4,  1200, 0.40f, 0.40f, 0.0f, -1.7f },
    { "Dark Strings",     "STRINGS", OscStrings, 0.40f,  450, 0.22f,  3,  1600, 0.60f, 0.50f, 0.3f, 0.2f },
    { "Solina Glow",      "STRINGS", OscStrings, 0.90f,  220, 0.58f,  0,  1000, 0.30f, 0.35f, 0.0f, -3.0f },
    { "Choir Ah",         "VOICES",  OscVoice,   0.10f,  220, 0.40f, 20,   900, 0.25f, 0.50f, 0.0f, -6.4f },
    { "Choir Oo",         "VOICES",  OscVoice,   0.90f,  260, 0.30f, 18,  1100, 0.35f, 0.55f, 0.0f, -6.1f },
    { "Whisper Oh",       "VOICES",  OscVoice,   0.50f,  140, 0.55f, 26,   700, 0.55f, 0.40f, 0.0f, -4.9f },
    { "Bedroom Pad",      "PADS",    OscAnalog,  0.80f,  600, 0.20f,  6,  2200, 0.70f, 0.65f, 0.4f, -2.0f },
    { "Night Organ",      "PADS",    OscFM,      0.15f,   60, 0.30f,  8,   600, 0.55f, 0.45f, 0.5f, -3.2f },
};
constexpr int kNumPresets = (int) (sizeof(kPresets) / sizeof(kPresets[0]));

inline Params applyPreset(Params p, const Preset& s)
{
    p.osc = s.osc; p.character = s.character; p.attackMs = s.attackMs; p.bright = s.bright; p.vibratoCents = s.vibratoCents;
    p.releaseMs = s.releaseMs; p.warmth = s.warmth; p.space = s.space; p.doubleAmt = s.doubleAmt; p.volumeDb = s.volumeDb;
    return p;
}
} // namespace rc
