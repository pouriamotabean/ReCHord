// ReChord - the sound (FM / Analog / Strings / Voice) + the glide sequencer (no JUCE: also used by the test tool)
#pragma once
#include "Theory.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace rc
{
enum OscType { OscFM = 0, OscAnalog = 1, OscStrings = 2, OscVoice = 3, OscTine = 4, OscSine = 5, kNumOsc };

struct Params
{
    int osc = OscFM;
    float character = 0.45f;      // FM: bite  Analog: detune  Strings: ensemble  Voice: vowel ah..oo  Tine: bell  Sine: harmonics
    float attackMs = 35.f, releaseMs = 380.f;
    float bright = 0.45f;         // the filter (follows the pitch)
    float vibratoCents = 16.f;    // depth of the delayed vibrato (Tine: tremolo depth, 0..50 = 0..100%)
    float warmth = 0.2f;          // tape: wow / flutter, drift, saturation, soft top
    float space = 0.2f;           // reverb + a dotted-1/8 echo
    int mode = Smooth;            // glide: smooth / steps / robot
    float doubleAmt = 0.f;        // octave below the lowest voice + octave above the top voice
    float spreadMs = 12.f;        // each lower voice starts a little after the one above
    int transpose = 0;
    float volumeDb = -6.f;
};

// ---------------- small DSP parts ----------------
inline float polyBlep(double t, double dt)
{
    if (t < dt) { t /= dt; return (float) (t + t - t * t - 1.0); }
    if (t > 1.0 - dt) { t = (t - 1.0) / dt; return (float) (t * t + t + t + 1.0); }
    return 0.f;
}
inline float sawStep(double& ph, double inc)
{
    const float v = (float) (2.0 * ph - 1.0) - polyBlep(ph, inc);
    ph += inc;
    if (ph >= 1.0) ph -= 1.0;
    return v;
}
// state variable filter (TPT): set() per block, then lp / bp per sample
struct Svf
{
    float a1 = 0, a2 = 0, a3 = 0, k = 1, ic1 = 0, ic2 = 0;
    void set(float fc, float q, double sr)
    {
        const float g = std::tan(3.14159265f * std::clamp(fc, 20.f, (float) sr * 0.45f) / (float) sr);
        k = 1.f / std::max(0.3f, q);
        a1 = 1.f / (1.f + g * (g + k)); a2 = g * a1; a3 = g * a2;
    }
    void tick(float x, float& lp, float& bp)
    {
        const float v3 = x - ic2, v1 = a1 * ic1 + a2 * v3, v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.f * v1 - ic1; ic2 = 2.f * v2 - ic2;
        lp = v2; bp = v1;
    }
};

// a small Freeverb (8 combs + 4 allpasses a side)
class Reverb
{
public:
    void prepare(double sr)
    {
        static const int combT[8] = { 1116, 1188, 1277, 1356, 1422, 1491, 1557, 1617 }, apT[4] = { 556, 441, 341, 225 };
        const double s = sr / 44100.0;
        for (int c = 0; c < 2; ++c)
        {
            for (int i = 0; i < 8; ++i) { comb[c][i].assign((size_t) ((combT[i] + c * 23) * s) + 1, 0.f); combPos[c][i] = 0; combLp[c][i] = 0; }
            for (int i = 0; i < 4; ++i) { ap[c][i].assign((size_t) ((apT[i] + c * 23) * s) + 1, 0.f); apPos[c][i] = 0; }
        }
        pre.assign((size_t) (sr * 0.025) + 1, 0.f); prePos = 0;
    }
    void process(float inL, float inR, float& outL, float& outR)
    {
        const float fb = 0.86f, damp = 0.32f;
        const float in = (inL + inR) * 0.5f * 0.03f;
        const float x = pre[(size_t) prePos];
        pre[(size_t) prePos] = in;
        prePos = (prePos + 1) % (int) pre.size();
        float o[2];
        for (int c = 0; c < 2; ++c)
        {
            float acc = 0;
            for (int i = 0; i < 8; ++i)
            {
                auto& b = comb[c][i]; int& p = combPos[c][i];
                const float y = b[(size_t) p];
                combLp[c][i] = y * (1 - damp) + combLp[c][i] * damp;
                b[(size_t) p] = x + combLp[c][i] * fb;
                p = (p + 1) % (int) b.size();
                acc += y;
            }
            for (int i = 0; i < 4; ++i)
            {
                auto& b = ap[c][i]; int& p = apPos[c][i];
                const float y = b[(size_t) p];
                b[(size_t) p] = acc + y * 0.5f;
                p = (p + 1) % (int) b.size();
                acc = y - acc;
            }
            o[c] = acc;
        }
        outL = o[0]; outR = o[1];
    }
private:
    std::vector<float> comb[2][8], ap[2][4], pre;
    int combPos[2][8] {}, apPos[2][4] {}, prePos = 0;
    float combLp[2][8] {};
};

class Engine
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v = Voice();
        for (auto& l : lanes) l = Lane();
        chorusLen = (int) (sr * 0.04) + 4;
        chorusBuf[0].assign((size_t) chorusLen, 0.f);
        chorusBuf[1].assign((size_t) chorusLen, 0.f);
        chorusPos = 0;
        for (auto& p : chorusPh) p = 0;
        wowPh = flutPh = 0;
        toneL = toneR = 0;
        reverb.prepare(sr);
        echoLen = (int) (sr * 2.2) + 4;
        echo[0].assign((size_t) echoLen, 0.f); echo[1].assign((size_t) echoLen, 0.f);
        echoPos = 0; echoLp[0] = echoLp[1] = echoHp[0] = echoHp[1] = 0;
        tremPh = 0;
    }
    void setScore(const Score& s) { score = s; }
    void setParams(const Params& p) { prm = p; }

    void allOff()
    {
        for (auto& l : lanes)
        {
            if (l.voice >= 0) voices[l.voice].releasing = true;
            l.voice = -1;
        }
    }

    // render n samples; when playing, beat = position of the first sample in beats (quarter notes)
    void process(float* L, float* R, int n, bool playing, double beat, double bpm, double barBeats)
    {
        std::memset(L, 0, sizeof(float) * (size_t) n);
        std::memset(R, 0, sizeof(float) * (size_t) n);
        if (!playing) allOff();
        const double beatsPerSample = bpm / 60.0 / sr;
        const int N = score.nVoices;
        curBpm = bpm;

        int done = 0;
        while (done < n)
        {
            const int len = std::min(kSub, n - done);
            if (playing)
            {
                for (int k = 0; k < N; ++k)
                {
                    const double off = (double) (N - 1 - k) * prm.spreadMs * 0.001 * bpm / 60.0;   // top voice first
                    const double u0 = beat + done * beatsPerSample - off;
                    const double u1 = u0 + len * beatsPerSample;
                    const auto a = evalLane(score, k, u0, barBeats, prm.mode);
                    const auto b = evalLane(score, k, u1, barBeats, prm.mode);
                    auto& ln = lanes[k];
                    if (ln.voice < 0 || ln.noteId != a.noteId)
                    {
                        if (ln.voice >= 0) voices[ln.voice].releasing = true;
                        ln.voice = startVoice(a.pitch + (float) prm.transpose, a.continuing, k == 0, k == N - 1, k, N);
                        ln.noteId = a.noteId;
                    }
                    auto& v = voices[ln.voice];
                    const float pa = a.pitch + (float) prm.transpose;
                    float pb = b.pitch + (float) prm.transpose;
                    if (b.noteId != a.noteId) pb = pa;
                    v.pitch = pa;
                    v.pitchStep = (pb - pa) / (float) len;
                }
                for (int k = N; k < kMaxVoices; ++k)
                    if (lanes[k].voice >= 0) { voices[lanes[k].voice].releasing = true; lanes[k].voice = -1; }
            }
            // tape wow + flutter: one slow wobble for every voice (warmth)
            wowPh += 0.55 * len / sr;   if (wowPh >= 1) wowPh -= 1;
            flutPh += 6.3 * len / sr;   if (flutPh >= 1) flutPh -= 1;
            const float tape = prm.warmth * ((float) std::sin(wowPh * 6.283185307179586) * 0.11f + (float) std::sin(flutPh * 6.283185307179586) * 0.025f);
            for (auto& v : voices)
                if (v.active) renderVoice(v, L + done, R + done, len, tape);
            done += len;
        }
        post(L, R, n, N);
    }

private:
    static constexpr int kSub = 32;
    static constexpr int kPool = 16;

    struct Voice
    {
        bool active = false, releasing = false;
        double ph[6] {};                        // oscillator phases
        double phm = 0, lfo = 0, drift = 0;
        float fb = 0, env = 0, age = 0, pitch = 60, pitchStep = 0, lfoRate = 5.2f, driftRate = 0.2f, pan = 0.f;
        Svf fL, fR, form[3];
        float lpL1 = 0, lpL2 = 0, lpR1 = 0, lpR2 = 0;
        bool low = false, high = false;         // plays its octave copy (DOUBLE)
        unsigned order = 0, noise = 1;
    };
    struct Lane { int voice = -1; long long noteId = -1; };

    int startVoice(float pitch, bool continuing, bool low, bool high, int lane, int nLanes)
    {
        int best = -1;
        for (int i = 0; i < kPool; ++i) if (!voices[i].active) { best = i; break; }
        if (best < 0)
        {
            float e = 1e9f;
            for (int i = 0; i < kPool; ++i) if (voices[i].releasing && voices[i].env < e) { e = voices[i].env; best = i; }
            if (best < 0) { unsigned o = ~0u; for (int i = 0; i < kPool; ++i) if (voices[i].order < o) { o = voices[i].order; best = i; } }
            for (auto& l : lanes) if (l.voice == best) l.voice = -1;
        }
        auto& v = voices[best];
        v = Voice();
        v.active = true;
        v.pitch = pitch;
        v.order = ++counter;
        auto rnd = [this] { rng = rng * 1664525u + 1013904223u; return (float) (rng >> 8) / 16777216.f; };
        v.lfoRate = 4.9f + rnd() * 0.7f;
        v.driftRate = 0.11f + rnd() * 0.17f;
        v.drift = rnd();
        for (auto& p : v.ph) p = rnd();
        v.lfo = 0.25;
        v.noise = 1u + (unsigned) (rnd() * 1e6f);
        v.low = low; v.high = high;
        v.pan = nLanes > 1 ? ((float) lane / (float) (nLanes - 1) - 0.5f) * 0.5f : 0.f;   // low voices a bit left, high a bit right
        if (continuing) { v.env = 1.f; v.age = 1.5f; }
        return best;
    }

    void renderVoice(Voice& v, float* L, float* R, int len, float tape)
    {
        const double twoPi = 6.283185307179586;
        const float att = std::max(0.001f, prm.attackMs * 0.001f);
        const float relCoef = std::exp(-1.f / (float) (std::max(0.02f, prm.releaseMs * 0.001f) / 4.6f * (float) sr));
        const float dt = 1.f / (float) sr;
        const float ch = prm.character, br = prm.bright;
        const float f0 = 440.f * std::pow(2.f, (v.pitch - 69.f) / 12.f);
        const float dbl = prm.doubleAmt;
        static const float oscGain[kNumOsc] = { 1.f, 1.6f, 1.9f, 0.95f, 1.1f, 1.0f };   // the 4 sounds at about the same loudness
        const float og = oscGain[std::clamp(prm.osc, 0, kNumOsc - 1)];

        // per block: filters
        float fc = 0, q = 0.707f;
        switch (prm.osc)
        {
            case OscFM:      fc = f0 * (2.5f + 11.f * br); break;
            case OscAnalog:  fc = f0 * (1.2f + 16.f * br * br) * (1.f + 0.5f * std::exp(-v.age / 0.25f)); q = 0.85f + 0.6f * br; break;
            case OscStrings: fc = f0 * (1.5f + 12.f * br * br); q = 0.6f; break;
            case OscTine:    fc = f0 * (2.f + 12.f * br); break;
            case OscSine:    fc = f0 * (3.f + 14.f * br); break;
            default:         fc = f0 * (3.f + 10.f * br); q = 0.6f; break;
        }
        fc = std::clamp(fc, 180.f, 16000.f);
        const float a = 1.f - std::exp(-2.f * 3.14159265f * fc / (float) sr);
        v.fL.set(fc, q, sr); v.fR.set(fc, q, sr);
        if (prm.osc == OscVoice)
        {
            // vowel: ah -> oh -> oo (formants F1 F2 F3)
            const float x = std::clamp(ch, 0.f, 1.f);
            auto lerp3 = [&](float ah, float oh, float oo) { return x < 0.5f ? ah + (oh - ah) * x * 2.f : oh + (oo - oh) * (x - 0.5f) * 2.f; };
            const float F1 = lerp3(730.f, 520.f, 330.f), F2 = lerp3(1150.f, 880.f, 760.f), F3 = lerp3(2600.f, 2450.f, 2300.f);
            v.form[0].set(F1, F1 / 90.f, sr); v.form[1].set(F2, F2 / 110.f, sr); v.form[2].set(F3, F3 / 150.f, sr);
        }

        for (int i = 0; i < len; ++i)
        {
            if (v.releasing)
            {
                v.env *= relCoef;
                if (v.env < 0.0001f) { v.active = false; return; }
            }
            else if (v.env < 1.f) v.env = std::min(1.f, v.env + dt / att);

            const float vf = std::clamp((v.age - 0.22f) / 0.6f, 0.f, 1.f);
            const float vib = prm.osc == OscTine ? 0.f : (float) std::sin(v.lfo * twoPi) * prm.vibratoCents * 0.01f * vf * vf;
            v.lfo += v.lfoRate * dt;  if (v.lfo >= 1) v.lfo -= 1;
            v.drift += v.driftRate * dt; if (v.drift >= 1) v.drift -= 1;
            const float drift = (float) std::sin(v.drift * twoPi) * prm.warmth * 0.05f;   // analog drift, up to 5 cents

            const float p = v.pitch + vib + tape + drift;
            const double f = 440.0 * std::exp2((p - 69.0) / 12.0);
            const double inc = f / sr;
            float l = 0, r = 0;

            switch (prm.osc)
            {
                case OscFM:
                {
                    const float bite = 0.72f + 0.28f * std::exp(-v.age / 0.16f);
                    const float index = (0.3f + 2.1f * ch) * bite, fbAmt = 0.1f + 0.45f * ch;
                    const float mod = (float) std::sin(v.phm * twoPi + fbAmt * v.fb);
                    v.fb = 0.5f * (v.fb + mod);
                    const float c1 = (float) std::sin(v.ph[0] * twoPi + index * mod), c2 = (float) std::sin(v.ph[1] * twoPi + index * 0.85f * mod);
                    float d = 0;
                    if (dbl > 0.001f)
                    {
                        if (v.low)  { d += (float) std::sin(v.ph[2] * twoPi + index * 0.7f * mod) * dbl * 0.8f; v.ph[2] += inc * 0.5; if (v.ph[2] >= 1) v.ph[2] -= 1; }
                        if (v.high) { d += (float) std::sin(v.ph[3] * twoPi + index * 0.5f * mod) * dbl * 0.45f; v.ph[3] += inc * 2.0; if (v.ph[3] >= 1) v.ph[3] -= 1; }
                    }
                    v.ph[0] += inc;          if (v.ph[0] >= 1) v.ph[0] -= 1;
                    v.ph[1] += inc * 1.0038; if (v.ph[1] >= 1) v.ph[1] -= 1;
                    v.phm += inc;            if (v.phm >= 1) v.phm -= 1;
                    l = c1 * 0.62f + c2 * 0.38f + d; r = c1 * 0.38f + c2 * 0.62f + d;
                    l *= v.env; r *= v.env;
                    v.lpL1 += a * (l - v.lpL1); v.lpL2 += a * (v.lpL1 - v.lpL2);
                    v.lpR1 += a * (r - v.lpR1); v.lpR2 += a * (v.lpR1 - v.lpR2);
                    l = v.lpL2; r = v.lpR2;
                    break;
                }
                case OscAnalog:
                {
                    // two saws, detuned by CHARACTER (up to 14 cents), warm resonant low pass
                    const double det = std::exp2(ch * 0.14 / 12.0);
                    const float s1 = sawStep(v.ph[0], inc / det), s2 = sawStep(v.ph[1], inc * det);
                    float d = 0;
                    if (dbl > 0.001f)
                    {
                        if (v.low)  d += sawStep(v.ph[2], inc * 0.5) * dbl * 0.7f;
                        if (v.high) d += sawStep(v.ph[3], inc * 2.0) * dbl * 0.35f;
                    }
                    float lp, bp;
                    v.fL.tick((s1 * 0.6f + s2 * 0.4f + d) * 0.6f, lp, bp); l = lp;
                    v.fR.tick((s1 * 0.4f + s2 * 0.6f + d) * 0.6f, lp, bp); r = lp;
                    l *= v.env; r *= v.env;
                    break;
                }
                case OscStrings:
                {
                    // string machine: 3 detuned saws (8') + one an octave up (4'), soft low pass; the ensemble is after the voices
                    const double d1 = std::exp2((0.04 + ch * 0.12) / 12.0);
                    const float s = sawStep(v.ph[0], inc) + sawStep(v.ph[1], inc * d1) + sawStep(v.ph[2], inc / d1);
                    const float o = sawStep(v.ph[3], inc * 2.0 * std::exp2(0.03 / 12.0));
                    float d = 0;
                    if (dbl > 0.001f && v.low) d += sawStep(v.ph[4], inc * 0.5) * dbl * 0.9f;
                    const float x = (s * 0.33f + o * 0.22f + d) * 0.55f;
                    float lp, bp;
                    v.fL.tick(x, lp, bp); l = lp;
                    v.fR.tick(x, lp, bp); r = lp;
                    l *= v.env; r *= v.env;
                    break;
                }
                case OscTine:
                {
                    // Rhodes colour, held: a warm 1:1 FM body that barks a little at the start and settles,
                    // plus the bell of the tine (CHARACTER) that rings out fast
                    const float bark = 0.42f + 0.75f * std::exp(-v.age / 0.22f);
                    const float mod = (float) std::sin(v.phm * twoPi);
                    const float body = (float) std::sin(v.ph[0] * twoPi + bark * mod);
                    const float body2 = (float) std::sin(v.ph[1] * twoPi + bark * 0.8f * mod);
                    const float bell = ch * ((float) std::sin(v.ph[4] * twoPi) * 0.22f * std::exp(-v.age / 0.11f)
                                           + (float) std::sin(v.ph[5] * twoPi) * 0.12f * std::exp(-v.age / 0.035f));
                    const float settle = 0.62f + 0.38f * std::exp(-v.age / 0.8f);     // like a struck key, then held
                    float d = 0;
                    if (dbl > 0.001f)
                    {
                        if (v.low)  { d += (float) std::sin(v.ph[2] * twoPi + bark * 0.5f * mod) * dbl * 0.8f; v.ph[2] += inc * 0.5; if (v.ph[2] >= 1) v.ph[2] -= 1; }
                        if (v.high) { d += (float) std::sin(v.ph[3] * twoPi) * dbl * 0.4f; v.ph[3] += inc * 2.0; if (v.ph[3] >= 1) v.ph[3] -= 1; }
                    }
                    v.ph[0] += inc;          if (v.ph[0] >= 1) v.ph[0] -= 1;
                    v.ph[1] += inc * 1.0025; if (v.ph[1] >= 1) v.ph[1] -= 1;
                    v.phm += inc;            if (v.phm >= 1) v.phm -= 1;
                    v.ph[4] += inc * 7.0;    if (v.ph[4] >= 1) v.ph[4] -= 1;
                    v.ph[5] += inc * 14.0;   if (v.ph[5] >= 1) v.ph[5] -= 1;
                    l = ((body * 0.6f + body2 * 0.4f) * settle + d) * v.env;
                    r = ((body * 0.4f + body2 * 0.6f) * settle + d) * v.env;
                    v.lpL1 += a * (l - v.lpL1); v.lpL2 += a * (v.lpL1 - v.lpL2);
                    v.lpR1 += a * (r - v.lpR1); v.lpR2 += a * (v.lpR1 - v.lpR2);
                    l = v.lpL2 + bell * v.env * (0.4f + br);       // the bell sits over the filter
                    r = v.lpR2 + bell * v.env * (0.4f + br);
                    break;
                }
                case OscSine:
                {
                    // the softest: a sine, CHARACTER adds a little 2nd and 3rd harmonic (glass .. flute); a second
                    // sine a few cents up makes it breathe
                    const float h2 = 0.28f * ch, h3 = 0.16f * ch * ch;
                    const double t0 = v.ph[0] * twoPi, t1 = v.ph[1] * twoPi;
                    const float s0 = (float) (std::sin(t0) + h2 * std::sin(2 * t0) + h3 * std::sin(3 * t0));
                    const float s1 = (float) (std::sin(t1) + h2 * std::sin(2 * t1));
                    float d = 0;
                    if (dbl > 0.001f)
                    {
                        if (v.low)  { d += (float) std::sin(v.ph[2] * twoPi) * dbl * 0.8f; v.ph[2] += inc * 0.5; if (v.ph[2] >= 1) v.ph[2] -= 1; }
                        if (v.high) { d += (float) std::sin(v.ph[3] * twoPi) * dbl * 0.4f; v.ph[3] += inc * 2.0; if (v.ph[3] >= 1) v.ph[3] -= 1; }
                    }
                    v.ph[0] += inc;          if (v.ph[0] >= 1) v.ph[0] -= 1;
                    v.ph[1] += inc * 1.0019; if (v.ph[1] >= 1) v.ph[1] -= 1;
                    l = (s0 * 0.65f + s1 * 0.35f + d) * v.env;
                    r = (s0 * 0.35f + s1 * 0.65f + d) * v.env;
                    v.lpL1 += a * (l - v.lpL1); v.lpL2 += a * (v.lpL1 - v.lpL2);
                    v.lpR1 += a * (r - v.lpR1); v.lpR2 += a * (v.lpR1 - v.lpR2);
                    l = v.lpL2; r = v.lpR2;
                    break;
                }
                default:
                {
                    // voice: buzz + breath through 3 formants (a choir-ish "ah" .. "oo")
                    const double dd = std::exp2(0.07 / 12.0);
                    v.noise = v.noise * 1664525u + 1013904223u;
                    const float nz = ((float) (v.noise >> 8) / 8388608.f - 1.f) * 0.18f;
                    float src = sawStep(v.ph[0], inc) * 0.55f + sawStep(v.ph[1], inc * dd) * 0.45f + nz;
                    if (dbl > 0.001f)
                    {
                        if (v.low)  src += sawStep(v.ph[2], inc * 0.5) * dbl * 0.7f;
                        if (v.high) src += sawStep(v.ph[3], inc * 2.0) * dbl * 0.3f;
                    }
                    float lp, b1, b2, b3;
                    v.form[0].tick(src, lp, b1);
                    v.form[1].tick(src, lp, b2);
                    v.form[2].tick(src, lp, b3);
                    float body;
                    v.fL.tick(src, body, lp);
                    const float x = (b1 * 1.0f + b2 * 0.55f + b3 * 0.22f) * 0.9f + body * 0.12f;
                    l = r = x * v.env;
                    break;
                }
            }
            L[i] += l * og * (1.f - v.pan);
            R[i] += r * og * (1.f + v.pan);

            v.pitch += v.pitchStep;
            v.age += dt;
        }
        v.pitchStep = 0;
    }

    // after the voices: tape tone + saturation (warmth), ensemble / chorus, reverb (space), volume
    void post(float* L, float* R, int n, int N)
    {
        const double twoPi = 6.283185307179586;
        const float pre = 0.75f / std::sqrt((float) std::max(1, N));         // the voices summed to a steady level before the tape
        const float g = std::pow(10.f, prm.volumeDb / 20.f) * 0.75f;
        const float toneA = 1.f - std::exp(-2.f * 3.14159265f * (17000.f - 10500.f * prm.warmth) / (float) sr);
        const float drive = 1.f + 2.5f * prm.warmth;
        const bool ens = prm.osc == OscStrings;
        const float depth = ens ? 0.0018f + 0.0026f * prm.character : 0.0022f;
        const float mix = ens ? 0.45f + 0.45f * prm.character : 0.36f;
        const float wet = prm.space * 0.85f;
        const float trem = prm.osc == OscTine ? std::clamp(prm.vibratoCents / 50.f, 0.f, 1.f) * 0.7f : 0.f;
        const float echoWet = prm.space * prm.space * 0.55f;                   // the echo comes in as SPACE goes up
        const double echoDelay = std::min((double) echoLen - 2.0, 0.75 * 60.0 / std::max(20.0, curBpm) * sr);
        for (int i = 0; i < n; ++i)
        {
            toneL += toneA * (L[i] - toneL); toneR += toneA * (R[i] - toneR);
            float inL = toneL * pre, inR = toneR * pre;
            chorusBuf[0][(size_t) chorusPos] = inL;
            chorusBuf[1][(size_t) chorusPos] = inR;
            // three slow lines (a string ensemble); other sounds use two of them as a light chorus
            const double r1 = ens ? 0.63 : 0.37, r2 = ens ? 0.79 : 0.43, r3 = 0.97;
            chorusPh[0] += r1 / sr; if (chorusPh[0] >= 1) chorusPh[0] -= 1;
            chorusPh[1] += r2 / sr; if (chorusPh[1] >= 1) chorusPh[1] -= 1;
            chorusPh[2] += r3 / sr; if (chorusPh[2] >= 1) chorusPh[2] -= 1;
            const double m1 = std::sin(chorusPh[0] * twoPi), m2 = std::sin(chorusPh[1] * twoPi + 2.1), m3 = std::sin(chorusPh[2] * twoPi + 4.2);
            float cL = tap(0, sr * (0.0105 + depth * m1)), cR = tap(1, sr * (0.0125 + depth * m2));
            if (ens) { const float c3 = tap(0, sr * (0.0115 + depth * m3)); cL = (cL + c3) * 0.7f; cR = (cR + c3) * 0.7f; }
            chorusPos = (chorusPos + 1) % chorusLen;
            float oL = inL * (1.f - mix * 0.4f) + cR * mix, oR = inR * (1.f - mix * 0.4f) + cL * mix;
            oL = std::tanh(oL * drive) / drive; oR = std::tanh(oR * drive) / drive;
            if (trem > 0.001f)
            {
                // the Rhodes suitcase: the sound swings left / right
                tremPh += 4.6 / sr; if (tremPh >= 1) tremPh -= 1;
                const float m = (float) std::sin(tremPh * twoPi);
                oL *= 1.f - trem * (0.5f + 0.5f * m);
                oR *= 1.f - trem * (0.5f - 0.5f * m);
            }
            if (echoWet > 0.001f)
            {
                // a dotted-1/8 ping-pong echo, darker each time round
                double rp = (double) echoPos - echoDelay;
                while (rp < 0) rp += echoLen;
                const int i0 = (int) rp, i1 = (i0 + 1) % echoLen;
                const float fr = (float) (rp - i0);
                const float eL = echo[0][(size_t) i0] * (1 - fr) + echo[0][(size_t) i1] * fr;
                const float eR = echo[1][(size_t) i0] * (1 - fr) + echo[1][(size_t) i1] * fr;
                for (int c = 0; c < 2; ++c)
                {
                    const float x = c == 0 ? eR : eL;                       // ping-pong
                    echoLp[c] += 0.32f * (x - echoLp[c]);
                    echoHp[c] += 0.02f * (echoLp[c] - echoHp[c]);
                }
                echo[0][(size_t) echoPos] = (oL + oR) * 0.5f + (echoLp[0] - echoHp[0]) * 0.42f;
                echo[1][(size_t) echoPos] = (echoLp[1] - echoHp[1]) * 0.42f;
                echoPos = (echoPos + 1) % echoLen;
                oL += eL * echoWet; oR += eR * echoWet;
            }
            if (wet > 0.001f)
            {
                float rl, rr;
                reverb.process(oL, oR, rl, rr);
                oL = oL * (1.f - wet * 0.35f) + rl * wet;
                oR = oR * (1.f - wet * 0.35f) + rr * wet;
            }
            L[i] = std::tanh(oL * g * 1.3f) / 1.3f;      // a safety soft clip only near full scale
            R[i] = std::tanh(oR * g * 1.3f) / 1.3f;
        }
    }

    float tap(int ch, double delaySamples)
    {
        double rp = (double) chorusPos - delaySamples;
        while (rp < 0) rp += chorusLen;
        const int i0 = (int) rp, i1 = (i0 + 1) % chorusLen;
        const float fr = (float) (rp - i0);
        return chorusBuf[ch][(size_t) i0] * (1 - fr) + chorusBuf[ch][(size_t) i1] * fr;
    }

    double sr = 48000;
    Score score;
    Params prm;
    Voice voices[kPool];
    Lane lanes[kMaxVoices];
    unsigned counter = 0, rng = 12345u;
    std::vector<float> chorusBuf[2];
    int chorusLen = 1, chorusPos = 0;
    double chorusPh[3] {}, wowPh = 0, flutPh = 0;
    float toneL = 0, toneR = 0;
    Reverb reverb;
    std::vector<float> echo[2];
    int echoLen = 1, echoPos = 0;
    float echoLp[2] {}, echoHp[2] {};
    double tremPh = 0, curBpm = 120;
};

// the 4 bars as a WAV.
//  loop: render from one cycle earlier and crossfade the seam, so it joins itself without a click.
//  otherwise: the pattern from its first beat; ends that join the next / previous pattern get a 3 ms edge,
//  an end with no next chord lets the notes ring out.
inline void renderPattern(const Score& s, const Params& p, double sr, double bpm, double barBeats,
                          std::vector<float>& outL, std::vector<float>& outR)
{
    Engine e;
    e.prepare(sr);
    e.setScore(s);
    e.setParams(p);
    const double samplesPerBeat = sr * 60.0 / bpm;
    const long long cycle = (long long) std::llround(kBars * barBeats * samplesPerBeat);
    const int block = 256;
    if (s.loop())
    {
        const long long fade = (long long) (sr * 0.03);
        const long long total = 2 * cycle + fade;
        std::vector<float> L((size_t) total), R((size_t) total);
        for (long long pos = 0; pos < total; pos += block)
        {
            const int n = (int) std::min<long long>(block, total - pos);
            e.process(L.data() + pos, R.data() + pos, n, true, (double) pos / samplesPerBeat, bpm, barBeats);
        }
        outL.assign(L.begin() + cycle, L.begin() + 2 * cycle);
        outR.assign(R.begin() + cycle, R.begin() + 2 * cycle);
        for (long long i = 0; i < fade; ++i)
        {
            const float w = (float) i / (float) fade;
            const float a = 0.5f - 0.5f * std::cos(w * 3.14159265f), b = 1.f - a;
            outL[(size_t) i] = outL[(size_t) i] * a + L[(size_t) (2 * cycle + i)] * b;
            outR[(size_t) i] = outR[(size_t) i] * a + R[(size_t) (2 * cycle + i)] * b;
        }
        return;
    }
    const long long tail = s.exitMode == None ? (long long) (sr * (p.releaseMs * 0.001 * 1.2 + 0.05 + p.space * 3.0)) : 0;   // ring out (+ the room)
    const long long total = cycle + tail;
    outL.assign((size_t) total, 0.f);
    outR.assign((size_t) total, 0.f);
    for (long long pos = 0; pos < total; pos += block)
    {
        const int n = (int) std::min<long long>(block, total - pos);
        e.process(outL.data() + pos, outR.data() + pos, n, pos < cycle, (double) pos / samplesPerBeat, bpm, barBeats);
    }
    const long long edge = (long long) (sr * 0.003);
    for (long long i = 0; i < edge; ++i)
    {
        const float w = (float) i / (float) edge;
        if (s.entryMode == ToChord) { outL[(size_t) i] *= w; outR[(size_t) i] *= w; }
        if (s.exitMode == ToChord) { outL[(size_t) (total - 1 - i)] *= w; outR[(size_t) (total - 1 - i)] *= w; }
    }
}
} // namespace rc
