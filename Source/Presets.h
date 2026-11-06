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
    { "Warm FM Lead",     "LEADS",   OscFM,      0.45f,   35, 0.45f, 16,   380, 0.20f, 0.20f, 0.0f, -4.4f },
    { "Velvet Lead",      "LEADS",   OscAnalog,  0.35f,   40, 0.32f, 18,   420, 0.35f, 0.25f, 0.0f, -2.8f },
    { "Tape Lead",        "LEADS",   OscAnalog,  0.55f,   25, 0.42f, 22,   350, 0.85f, 0.30f, 0.0f, -1.2f },
    { "Soft Bell Lead",   "LEADS",   OscFM,      0.75f,   12, 0.55f, 10,   700, 0.30f, 0.35f, 0.0f, -3.2f },
    { "Vintage Strings",  "STRINGS", OscStrings, 0.55f,  320, 0.45f,  4,  1200, 0.40f, 0.40f, 0.0f, -1.0f },
    { "Dark Strings",     "STRINGS", OscStrings, 0.40f,  450, 0.22f,  3,  1600, 0.60f, 0.50f, 0.3f, 1.0f },
    { "Solina Glow",      "STRINGS", OscStrings, 0.90f,  220, 0.58f,  0,  1000, 0.30f, 0.35f, 0.0f, -2.0f },
    { "Choir Ah",         "VOICES",  OscVoice,   0.10f,  220, 0.40f, 20,   900, 0.25f, 0.50f, 0.0f, -6.5f },
    { "Choir Oo",         "VOICES",  OscVoice,   0.90f,  260, 0.30f, 18,  1100, 0.35f, 0.55f, 0.0f, -5.9f },
    { "Whisper Oh",       "VOICES",  OscVoice,   0.50f,  140, 0.55f, 26,   700, 0.55f, 0.40f, 0.0f, -4.7f },
    { "Bedroom Pad",      "PADS",    OscAnalog,  0.80f,  600, 0.20f,  6,  2200, 0.70f, 0.65f, 0.4f, -1.4f },
    { "Night Organ",      "PADS",    OscFM,      0.15f,   60, 0.30f,  8,   600, 0.55f, 0.45f, 0.5f, -2.9f },
    // 1.3: the soft ones. KEYS: Rhodes colour, held (VIBRATO = tremolo). DREAM: sine and soft FM
    { "Velvet Tines",     "EP",      OscTine,    0.35f,    8, 0.32f, 15,   900, 0.30f, 0.35f, 0.0f, -3.3f },
    { "Suitcase Dream",   "EP",      OscTine,    0.45f,    8, 0.40f, 28,  1200, 0.25f, 0.55f, 0.0f, -2.2f },
    { "Dusty Keys",       "EP",      OscTine,    0.30f,   10, 0.28f, 12,   900, 0.85f, 0.40f, 0.0f, -1.1f },
    { "Glass Tines",      "EP",      OscTine,    0.75f,    5, 0.52f, 18,  1400, 0.20f, 0.50f, 0.0f, -3.2f },
    { "Felt Keys",        "EP",      OscTine,    0.12f,   25, 0.18f,  8,   800, 0.50f, 0.35f, 0.0f, -2.7f },
    { "Lullaby Keys",     "EP",      OscTine,    0.50f,   10, 0.36f, 22,  1600, 0.35f, 0.62f, 0.4f, -2.3f },
    { "Pure Sine",        "DREAM",   OscSine,    0.00f,   45, 0.50f, 14,   500, 0.15f, 0.35f, 0.0f, -4.7f },
    { "Dream Sine",       "DREAM",   OscSine,    0.30f,  180, 0.45f, 18,   900, 0.30f, 0.60f, 0.0f, -3.5f },
    { "Soft FM Flute",    "DREAM",   OscFM,      0.04f,   70, 0.35f, 20,   600, 0.30f, 0.45f, 0.0f, -3.8f },
    { "Cloud Lead",       "DREAM",   OscSine,    0.20f,  250, 0.40f, 16,  1200, 0.35f, 0.70f, 0.5f, -3.4f },
    { "Ocean Pad",        "DREAM",   OscSine,    0.45f,  700, 0.35f,  8,  2600, 0.50f, 0.75f, 0.3f, -2.4f },
    { "Hum Lead",         "DREAM",   OscFM,      0.18f,   60, 0.22f, 20,   700, 0.60f, 0.40f, 0.0f, -2.5f },
    // 1.4: bodies measured from the user's reference. PIANO: CHARACTER goes felt piano -> felt piano 2 -> Rhodes.
    // BOWED: winter -> dreamy (Arnalds) -> texture -> soft -> eternal -> sharp
    { "Felt Piano",       "PIANO",   OscKeys,    0.00f,   20, 0.55f,  0,   900, 0.20f, 0.35f, 0.0f, -3.6f },
    { "Felt Piano II",    "PIANO",   OscKeys,    0.50f,   20, 0.55f,  0,   900, 0.20f, 0.35f, 0.0f, -3.6f },
    { "Rhodes Body",      "PIANO",   OscKeys,    1.00f,   20, 0.55f,  0,  1000, 0.25f, 0.40f, 0.0f, -3.3f },
    { "Felt to Rhodes",   "PIANO",   OscKeys,    0.75f,   20, 0.55f,  0,   950, 0.25f, 0.40f, 0.0f, -3.2f },
    { "Upright Haze",     "PIANO",   OscKeys,    0.10f,   25, 0.45f,  0,   900, 0.70f, 0.45f, 0.0f, -1.6f },
    { "Dream Keys",       "PIANO",   OscKeys,    0.85f,   40, 0.55f,  0,  1400, 0.30f, 0.62f, 0.3f, -2.7f },
    { "Winter Strings",   "BOWED",   OscBowed,   0.00f,  350, 0.62f,  8,  1600, 0.25f, 0.50f, 0.0f, -2.9f },
    { "Dreamy Strings",   "BOWED",   OscBowed,   0.20f,  300, 0.62f, 12,  1600, 0.20f, 0.55f, 0.0f, -3.0f },
    { "Texture Strings",  "BOWED",   OscBowed,   0.40f,  250, 0.62f, 10,  1400, 0.20f, 0.45f, 0.0f, -3.2f },
    { "Soft Strings",     "BOWED",   OscBowed,   0.60f,  300, 0.62f, 10,  1400, 0.20f, 0.45f, 0.0f, -3.1f },
    { "Eternal Strings",  "BOWED",   OscBowed,   0.80f,  350, 0.62f, 12,  1800, 0.15f, 0.55f, 0.0f, -3.3f },
    { "Sharp Strings",    "BOWED",   OscBowed,   1.00f,  150, 0.62f, 18,  1000, 0.15f, 0.40f, 0.0f, -3.3f },
    // 1.7: 808 bass, each fitted to one of the user's 808 samples (harmonics + the pitch punch). Mono, louder than the rest.
    // set VOICES to 1 for a line on the chord roots, or play / write single notes. VIBRATO = PUNCH, CHARACTER = DRIVE, WARMTH = GRIT
    { "808 Caliente",     "BASS",    Osc808,     0.10f,    3, 0.40f,  8,   250, 0.30f, 0.00f, 0.0f, -6.1f },
    { "808 Deep",         "BASS",    Osc808,     0.30f,    3, 0.40f,  0,   250, 0.10f, 0.00f, 0.0f, -7.8f },
    { "808 Grit",         "BASS",    Osc808,     0.45f,    3, 1.00f, 17,   250, 1.00f, 0.00f, 0.0f, -3.7f },
    { "808 Noisey",       "BASS",    Osc808,     0.70f,    3, 1.00f, 32,   250, 1.00f, 0.00f, 0.0f, -4.3f },
    { "808 Lo-Fi",        "BASS",    Osc808,     0.15f,    3, 1.00f,  8,   250, 0.00f, 0.00f, 0.0f, -6.7f },
    { "808 Long Glide",   "BASS",    Osc808,     0.25f,    3, 0.50f, 12,   300, 0.20f, 0.00f, 0.0f, -7.6f },
};
constexpr int kNumPresets = (int) (sizeof(kPresets) / sizeof(kPresets[0]));

inline Params applyPreset(Params p, const Preset& s)
{
    p.osc = s.osc; p.character = s.character; p.attackMs = s.attackMs; p.bright = s.bright; p.vibratoCents = s.vibratoCents;
    p.releaseMs = s.releaseMs; p.warmth = s.warmth; p.space = s.space; p.doubleAmt = s.doubleAmt; p.volumeDb = s.volumeDb;
    return p;
}
} // namespace rc
