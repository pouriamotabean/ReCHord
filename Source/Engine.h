// ReChord - sustained FM lead + the glide sequencer (no JUCE: also used by the test tool)
#pragma once
#include "Theory.h"
#include <cmath>
#include <cstring>
#include <vector>

namespace rc
{
struct Params
{
    float attackMs = 35.f, releaseMs = 380.f;
    float bright = 0.45f;         // 0..1: FM index + filter
    float vibratoCents = 16.f;    // depth of the delayed vibrato
    int mode = Smooth;            // smooth / steps / robot
    float doubleAmt = 0.f;        // 0..1: octave below the lowest voice + octave above the top voice
    float spreadMs = 12.f;        // each lower voice starts a little after the one above
    int transpose = 0;
    float volumeDb = -6.f;
};

class Engine
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v = Voice();
        for (auto& l : lanes) l = Lane();
        chorusLen = (int) (sr * 0.03) + 4;
        chorusBuf[0].assign((size_t) chorusLen, 0.f);
        chorusBuf[1].assign((size_t) chorusLen, 0.f);
        chorusPos = 0;
        chorusPh = 0;
        chorusPh2 = 0;
    }
    void setScore(const Score& s) { score = s; }
    void setParams(const Params& p) { prm = p; }

    // release everything (transport stopped)
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
                        ln.voice = startVoice(a.pitch + (float) prm.transpose, a.continuing, k == 0, k == N - 1);
                        ln.noteId = a.noteId;
                    }
                    auto& v = voices[ln.voice];
                    const float pa = a.pitch + (float) prm.transpose;
                    float pb = b.pitch + (float) prm.transpose;
                    if (b.noteId != a.noteId) pb = pa;        // a new note starts in the next sub block
                    v.pitch = pa;
                    v.pitchStep = (pb - pa) / (float) len;
                }
                for (int k = N; k < kMaxVoices; ++k)
                    if (lanes[k].voice >= 0) { voices[lanes[k].voice].releasing = true; lanes[k].voice = -1; }
            }
            for (auto& v : voices)
                if (v.active) renderVoice(v, L + done, R + done, len);
            done += len;
        }

        // light stereo chorus + gentle saturation
        const float g = std::pow(10.f, prm.volumeDb / 20.f) * 0.55f / std::sqrt((float) std::max(1, N));
        for (int i = 0; i < n; ++i)
        {
            const float inL = L[i], inR = R[i];
            chorusBuf[0][(size_t) chorusPos] = inL;
            chorusBuf[1][(size_t) chorusPos] = inR;
            chorusPh += 0.37 / sr;  if (chorusPh >= 1) chorusPh -= 1;
            chorusPh2 += 0.43 / sr; if (chorusPh2 >= 1) chorusPh2 -= 1;
            const double m1 = std::sin(chorusPh * 6.283185307179586), m2 = std::cos(chorusPh2 * 6.283185307179586);
            const float cL = tap(0, sr * (0.0105 + 0.0022 * m1)), cR = tap(1, sr * (0.0125 + 0.0022 * m2));
            chorusPos = (chorusPos + 1) % chorusLen;
            const float oL = (inL * 0.82f + cR * 0.36f) * g, oR = (inR * 0.82f + cL * 0.36f) * g;
            L[i] = std::tanh(oL * 1.1f) / 1.1f;
            R[i] = std::tanh(oR * 1.1f) / 1.1f;
        }
    }

    bool anyActive() const
    {
        for (auto& v : voices) if (v.active) return true;
        return false;
    }

private:
    static constexpr int kSub = 32;
    static constexpr int kPool = 16;

    struct Voice
    {
        bool active = false, releasing = false;
        double ph1 = 0, ph2 = 0, phm = 0, lfo = 0;
        float fb = 0, env = 0, age = 0, pitch = 60, pitchStep = 0, lfoRate = 5.2f;
        float lpL1 = 0, lpL2 = 0, lpR1 = 0, lpR2 = 0;
        double phLo = 0, phHi = 0;
        bool low = false, high = false;       // this voice also plays its octave copy (DOUBLE)
        unsigned order = 0;
    };
    struct Lane { int voice = -1; long long noteId = -1; };

    int startVoice(float pitch, bool continuing, bool low, bool high)
    {
        int best = -1;
        for (int i = 0; i < kPool; ++i) if (!voices[i].active) { best = i; break; }
        if (best < 0)   // steal the quietest releasing voice, else the oldest
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
        rng = rng * 1664525u + 1013904223u;
        v.lfoRate = 4.9f + (float) (rng >> 8) / 16777216.f * 0.7f;       // each voice its own vibrato speed
        v.ph2 = (double) ((rng >> 4) & 0xffff) / 65536.0;
        v.lfo = 0.25;
        v.low = low; v.high = high;
        if (continuing) { v.env = 1.f; v.age = 1.5f; }   // a note that was already sounding before the pattern
        return best;
    }

    void renderVoice(Voice& v, float* L, float* R, int len)
    {
        const double twoPi = 6.283185307179586;
        const float att = std::max(0.001f, prm.attackMs * 0.001f);
        const float relCoef = std::exp(-1.f / (float) (std::max(0.02f, prm.releaseMs * 0.001f) / 4.6f * (float) sr));
        const float index = 0.3f + 2.1f * prm.bright, fbAmt = 0.1f + 0.45f * prm.bright;
        const float dt = 1.f / (float) sr;
        // filter follows the pitch: keeps the top soft so it never sounds like a beep
        const float f0 = 440.f * std::pow(2.f, (v.pitch - 69.f) / 12.f);
        const float fc = std::clamp(f0 * (2.5f + 11.f * prm.bright), 250.f, 15000.f);
        const float a = 1.f - std::exp(-2.f * 3.14159265f * fc / (float) sr);

        for (int i = 0; i < len; ++i)
        {
            // envelope
            if (v.releasing)
            {
                v.env *= relCoef;
                if (v.env < 0.0001f) { v.active = false; return; }
            }
            else if (v.env < 1.f) v.env = std::min(1.f, v.env + dt / att);

            // delayed vibrato
            const float vf = std::clamp((v.age - 0.22f) / 0.6f, 0.f, 1.f);
            const float vib = (float) std::sin(v.lfo * twoPi) * prm.vibratoCents * 0.01f * vf * vf;
            v.lfo += v.lfoRate * dt;
            if (v.lfo >= 1) v.lfo -= 1;

            const float p = v.pitch + vib;
            const double f = 440.0 * std::exp2((p - 69.0) / 12.0);
            const float bite = 0.72f + 0.28f * std::exp(-v.age / 0.16f);

            const float mod = (float) std::sin(v.phm * twoPi + fbAmt * v.fb);
            v.fb = 0.5f * (v.fb + mod);
            const float I = index * bite;
            const float c1 = (float) std::sin(v.ph1 * twoPi + I * mod);
            const float c2 = (float) std::sin(v.ph2 * twoPi + I * 0.85f * mod);
            // octave copies: same modulator, carriers an octave down / up
            float dbl = 0.f;
            if (prm.doubleAmt > 0.001f)
            {
                if (v.low)  { dbl += (float) std::sin(v.phLo * twoPi + I * 0.7f * mod) * prm.doubleAmt * 0.8f; v.phLo += f * 0.5 / sr; if (v.phLo >= 1) v.phLo -= 1; }
                if (v.high) { dbl += (float) std::sin(v.phHi * twoPi + I * 0.5f * mod) * prm.doubleAmt * 0.45f; v.phHi += f * 2.0 / sr; if (v.phHi >= 1) v.phHi -= 1; }
            }
            v.ph1 += f / sr;            if (v.ph1 >= 1) v.ph1 -= 1;
            v.ph2 += f * 1.0038 / sr;   if (v.ph2 >= 1) v.ph2 -= 1;
            v.phm += f / sr;            if (v.phm >= 1) v.phm -= 1;

            float l = (c1 * 0.62f + c2 * 0.38f + dbl) * v.env, r = (c1 * 0.38f + c2 * 0.62f + dbl) * v.env;
            v.lpL1 += a * (l - v.lpL1); v.lpL2 += a * (v.lpL1 - v.lpL2);
            v.lpR1 += a * (r - v.lpR1); v.lpR2 += a * (v.lpR1 - v.lpR2);
            L[i] += v.lpL2;
            R[i] += v.lpR2;

            v.pitch += v.pitchStep;
            v.age += dt;
        }
        v.pitchStep = 0;
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
    double chorusPh = 0, chorusPh2 = 0;
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
    const long long tail = s.exitMode == None ? (long long) (sr * (p.releaseMs * 0.001 * 1.2 + 0.05)) : 0;
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
