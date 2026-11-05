// ReChord - the sound (FM / Analog / Strings / Voice) + the glide sequencer (no JUCE: also used by the test tool)
#pragma once
#include "Theory.h"
#include "Bodies.h"
#include <cmath>
#include <memory>
#include <mutex>
#include <cstring>
#include <vector>

namespace rc
{
enum OscType { OscFM = 0, OscAnalog = 1, OscStrings = 2, OscVoice = 3, OscTine = 4, OscSine = 5, OscKeys = 6, OscBowed = 7, Osc808 = 8, kNumOsc };

struct Params
{
    int osc = OscFM;
    float character = 0.45f;      // FM: bite  Analog: detune  Strings: ensemble  Voice: vowel ah..oo  Tine: bell  Sine: harmonics
                                  // Keys / Bowed: which measured body (a morph from the first to the last)
    float attackMs = 35.f, releaseMs = 380.f;
    float bright = 0.45f;         // the filter (follows the pitch)
    float vibratoCents = 16.f;    // depth of the delayed vibrato (Tine: tremolo depth, 0..50 = 0..100%)
    float warmth = 0.2f;          // tape: wow / flutter, drift, saturation, soft top
    float space = 0.2f;           // reverb + a dotted-1/8 echo
    int mode = Smooth;            // glide: smooth / steps / robot
    float doubleAmt = 0.f;        // octave below the lowest voice + octave above the top voice
    float spreadMs = 12.f;        // each lower voice starts a little after the one above
    int transpose = 0;            // pattern: transpose + octave (semitones)
    int liveShift = 0;            // keyboard: octave (semitones)
    float liveGlideMs = 250.f;    // keyboard: time to glide from one chord to the next
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


// ---------------- measured bodies -> wavetables ----------------
// One band-limited single cycle per body every 3 semitones (MIDI 21..120). Harmonic k of a note at f0 gets the
// level the measured body curve has at k*f0, so every note keeps the instrument's colour where it sits.
// Built once per sample rate and shared by every Engine.
class BodyBank
{
public:
    static constexpr int kSize = 2048, kLo = 21, kStep = 3, kNotes = 34;
    static constexpr int kBodies = kNumKeyBodies + kNumStringBodies;

    static std::shared_ptr<const BodyBank> get(double sr)
    {
        static std::mutex m;
        static std::shared_ptr<const BodyBank> cached;
        std::lock_guard<std::mutex> lock(m);
        if (cached == nullptr || std::abs(cached->rate - sr) > 1.0) cached = std::make_shared<const BodyBank>(sr);
        return cached;
    }
    explicit BodyBank(double sr) : rate(sr), data((size_t) kBodies * kNotes * kSize, 0.f)
    {
        std::vector<double> phase(200);
        unsigned seed = 7u;
        for (auto& p : phase) { seed = seed * 1664525u + 1013904223u; p = (double) (seed >> 8) / 16777216.0 * 6.283185307179586; }
        std::vector<double> amp(200);
        for (int b = 0; b < kBodies; ++b)
        {
            const Body& body = b < kNumKeyBodies ? kKeyBodies[b] : kStringBodies[b - kNumKeyBodies];
            for (int j = 0; j < kNotes; ++j)
            {
                const double f0 = 440.0 * std::exp2((kLo + j * kStep - 69) / 12.0);
                const double top = 440.0 * std::exp2((kLo + j * kStep + kStep - 69) / 12.0);
                const int K = std::max(1, std::min(160, (int) (0.45 * sr / top)));
                for (int k = 1; k <= K; ++k) amp[(size_t) k] = std::pow(10.0, curveDb(body, f0 * k) / 20.0);
                float* t = table(b, j);
                for (int k = 1; k <= K; ++k)
                {
                    const double a = amp[(size_t) k];
                    if (a < 1e-4) continue;
                    // a sine by rotation (cheap and exact enough for a table)
                    const double w = 6.283185307179586 * k / kSize;
                    double c = std::cos(phase[(size_t) k]), sn = std::sin(phase[(size_t) k]);
                    const double cw = std::cos(w), sw = std::sin(w);
                    for (int i = 0; i < kSize; ++i)
                    {
                        t[i] += (float) (a * sn);
                        const double nc = c * cw - sn * sw;
                        sn = sn * cw + c * sw; c = nc;
                    }
                }
                double e = 0;
                for (int i = 0; i < kSize; ++i) e += (double) t[i] * t[i];
                const float g = (float) (0.5 / std::sqrt(e / kSize + 1e-12));   // every note at the same loudness
                for (int i = 0; i < kSize; ++i) t[i] *= g;
            }
        }
    }
    float* table(int b, int j) { return data.data() + ((size_t) b * kNotes + (size_t) j) * kSize; }
    const float* table(int b, int j) const { return data.data() + ((size_t) b * kNotes + (size_t) j) * kSize; }

    // one sample of body b at pitch p (MIDI) and phase (0..1): the two nearest tables, crossfaded
    float read(int b, float p, double phase) const
    {
        const float x = std::clamp((p - (float) kLo) / (float) kStep, 0.f, (float) (kNotes - 1) - 0.001f);
        const int j = (int) x;
        const float fr = x - (float) j;
        const double pos = phase * kSize;
        const int i0 = (int) pos & (kSize - 1), i1 = (i0 + 1) & (kSize - 1);
        const float fi = (float) (pos - std::floor(pos));
        const float* a = table(b, j);
        const float* c = table(b, j + 1);
        const float va = a[i0] + (a[i1] - a[i0]) * fi, vc = c[i0] + (c[i1] - c[i0]) * fi;
        return va + (vc - va) * fr;
    }
    // CHARACTER 0..1 across the bodies of one family: the two nearest, crossfaded
    float morph(bool strings, float character, float p, double phase) const
    {
        const int n = strings ? kNumStringBodies : kNumKeyBodies, first = strings ? kNumKeyBodies : 0;
        const float x = std::clamp(character, 0.f, 1.f) * (float) (n - 1);
        const int b = std::min(n - 2, (int) x);
        const float fr = x - (float) b;
        const float v0 = read(first + b, p, phase);
        return fr < 0.002f ? v0 : v0 + (read(first + b + 1, p, phase) - v0) * fr;
    }
    static float noiseDb(bool strings, float character)
    {
        const int n = strings ? kNumStringBodies : kNumKeyBodies;
        const float x = std::clamp(character, 0.f, 1.f) * (float) (n - 1);
        const int b = std::min(n - 2, (int) x);
        const float fr = x - (float) b;
        const Body* B = strings ? kStringBodies : kKeyBodies;
        return B[b].noiseDb + (B[b + 1].noiseDb - B[b].noiseDb) * fr;
    }
    double rate;
private:
    std::vector<float> data;
    static double curveDb(const Body& body, double f)
    {
        const double x = std::log(std::clamp(f, (double) kBodyLoHz, (double) kBodyHiHz) / kBodyLoHz) / std::log((double) kBodyHiHz / kBodyLoHz) * (kBodyPoints - 1);
        const int i = std::min(kBodyPoints - 2, (int) x);
        const double fr = x - i;
        double d = body.db[i] + (body.db[i + 1] - body.db[i]) * fr;
        if (f > kBodyHiHz) d -= 24.0 * std::log2(f / kBodyHiHz);
        return d;
    }
};

class Engine
{
public:
    void prepare(double sampleRate)
    {
        sr = sampleRate;
        for (auto& v : voices) v = Voice();
        for (auto& l : lanes) l = Lane();
        for (auto& l : live) l = LiveLane();
        chorusLen = (int) (sr * 0.04) + 4;
        chorusBuf[0].assign((size_t) chorusLen, 0.f);
        chorusBuf[1].assign((size_t) chorusLen, 0.f);
        chorusPos = 0;
        for (auto& p : chorusPh) p = 0;
        wowPh = flutPh = 0;
        toneL = toneR = 0; hpL = hpR = sideLp = revHpL = revHpR = 0;
        reverb.prepare(sr);
        echoLen = (int) (sr * 2.2) + 4;
        echo[0].assign((size_t) echoLen, 0.f); echo[1].assign((size_t) echoLen, 0.f);
        echoPos = 0; echoLp[0] = echoLp[1] = echoHp[0] = echoHp[1] = 0;
        tremPh = 0;
        bodies = BodyBank::get(sr);
    }
    void setScore(const Score& s) { score = s; cache.build(score); }
    void setParams(const Params& p) { prm = p; }

    // ---- the keyboard: the notes now held (sorted, max 8). Each new chord glides from the last one ----
    static constexpr int kLive = 8;
    void setLive(const int* notes, int n)
    {
        n = std::min(n, kLive);
        // the voices sounding now, low to high
        int old[kLive], oldLane[kLive], no = 0;
        for (int k = 0; k < kLive; ++k) if (live[k].voice >= 0) { old[no] = k; ++no; }
        std::sort(old, old + no, [this](int a, int b) { return live[a].cur < live[b].cur; });
        for (int i = 0; i < no; ++i) oldLane[i] = old[i];
        // smallest movement, no crossing: match / a voice stops / a voice starts
        float dp[kLive + 1][kLive + 1];
        int how[kLive + 1][kLive + 1];
        const float birth = 7.f;
        for (int i = 0; i <= no; ++i)
            for (int j = 0; j <= n; ++j)
            {
                if (i == 0 && j == 0) { dp[0][0] = 0; how[0][0] = 0; continue; }
                float b = 1e9f; int h = 0;
                if (i > 0 && j > 0) { const float c = dp[i - 1][j - 1] + std::min(24.f, std::abs(live[oldLane[i - 1]].cur - (float) (notes[j - 1] + prm.liveShift))); if (c < b) { b = c; h = 1; } }
                if (i > 0) { const float c = dp[i - 1][j] + birth; if (c < b) { b = c; h = 2; } }
                if (j > 0) { const float c = dp[i][j - 1] + birth; if (c < b) { b = c; h = 3; } }
                dp[i][j] = b; how[i][j] = h;
            }
        int i = no, j = n;
        const float lowest = n > 0 ? (float) (notes[0] + prm.liveShift) : 0.f, highest = n > 0 ? (float) (notes[n - 1] + prm.liveShift) : 0.f;
        while (i > 0 || j > 0)
        {
            const int h = how[i][j];
            if (h == 1)
            {
                auto& L = live[oldLane[i - 1]];
                L.from = L.cur; L.to = (float) (notes[j - 1] + prm.liveShift); L.t = 0;
                --i; --j;
            }
            else if (h == 2)
            {
                auto& L = live[oldLane[i - 1]];
                if (L.voice >= 0) voices[L.voice].releasing = true;
                L.voice = -1;
                --i;
            }
            else
            {
                const float p = (float) (notes[j - 1] + prm.liveShift);
                for (auto& L : live)
                    if (L.voice < 0)
                    {
                        L.voice = startVoice(p, false, p == lowest && n > 1, p == highest && n > 1, j - 1, n);
                        L.from = L.to = L.cur = p; L.t = 1e9;
                        break;
                    }
                --j;
            }
        }
    }
    bool liveActive() const { for (auto& L : live) if (L.voice >= 0) return true; return false; }

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
                    const auto a = evalLane(score, k, u0, barBeats, prm.mode, &cache);
                    const auto b = evalLane(score, k, u1, barBeats, prm.mode, &cache);
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
            // the keyboard voices
            for (auto& L : live)
            {
                if (L.voice < 0) continue;
                if (!voices[L.voice].active || voices[L.voice].releasing) { L.voice = -1; continue; }
                const double dur = std::max(0.0005, prm.liveGlideMs * 0.001);
                const double t1 = L.t + len / sr;
                auto at = [&](double t) { return t >= dur ? L.to : glideValue(L.from, L.to, (float) (t / dur), 0.f, prm.mode, score.scaleMask | liveMask()); };
                const float pa = at(L.t), pb = at(t1);
                L.t = t1; L.cur = pb;
                voices[L.voice].pitch = pa;
                voices[L.voice].pitchStep = (pb - pa) / (float) len;
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
    static constexpr int kPool = 24;

    struct Voice
    {
        bool active = false, releasing = false;
        double ph[6] {};                        // oscillator phases
        double phm = 0, lfo = 0, drift = 0, pl[3] { 0.1, 0.45, 0.8 };
        float fb = 0, env = 0, age = 0, pitch = 60, pitchStep = 0, lfoRate = 5.2f, driftRate = 0.2f, pan = 0.f;
        Svf fL, fR, form[3];
        float lpL1 = 0, lpL2 = 0, lpR1 = 0, lpR2 = 0;
        bool low = false, high = false;         // plays its octave copy (DOUBLE)
        unsigned order = 0, noise = 1;
    };
    struct Lane { int voice = -1; long long noteId = -1; };
    struct LiveLane { int voice = -1; float from = 60, to = 60, cur = 60; double t = 0; };
    LiveLane live[kLive];
    int liveMask() const { int m = 0; for (auto& L : live) if (L.voice >= 0) m |= 1 << (((int) std::lround(L.to) % 12 + 12) % 12); return m; }

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
            for (auto& l : live) if (l.voice == best) l.voice = -1;
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
        v.pan = nLanes > 1 && prm.osc != Osc808 ? ((float) lane / (float) (nLanes - 1) - 0.5f) * 0.5f : 0.f;   // low voices a bit left, high a bit right (808: centre)
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
        static const float oscGain[kNumOsc] = { 1.f, 1.6f, 1.9f, 0.95f, 1.1f, 1.0f, 1.2f, 1.38f, 1.0f };   // the 4 sounds at about the same loudness
        const float og = oscGain[std::clamp(prm.osc, 0, kNumOsc - 1)];

        // per block: filters
        float fc = 0, q = 0.707f;
        switch (prm.osc)
        {
            case OscFM:      fc = f0 * (2.5f + 11.f * br); break;
            case OscAnalog:  fc = f0 * (1.2f + 16.f * br * br); q = 0.85f + 0.6f * br; break;
            case OscStrings: fc = f0 * (1.5f + 12.f * br * br); q = 0.6f; break;
            case OscTine:    fc = f0 * (2.f + 12.f * br); break;
            case OscSine:    fc = f0 * (3.f + 14.f * br); break;
            case OscKeys:
            case OscBowed:   fc = f0 * std::exp2(1.f + br * 9.f); break;
            case Osc808:     fc = f0 * (3.f + 40.f * br * br); break;    // BRIGHT 0.6..1 = the body as measured, lower = darker
            default:         fc = f0 * (3.f + 10.f * br); q = 0.6f; break;
        }
        fc = std::clamp(fc, 180.f, (prm.osc == OscKeys || prm.osc == OscBowed) ? 20000.f : 16000.f);
        if (prm.osc == OscBowed) v.fR.set(std::clamp(f0 * 3.f, 600.f, 5000.f), 0.7f, sr);    // the bow: noise around the 3rd harmonic
        const float a = 1.f - std::exp(-2.f * 3.14159265f * fc / (float) sr);
        if (prm.osc != OscBowed) { v.fL.set(fc, q, sr); v.fR.set(fc, q, sr); }
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
            const float vib = (prm.osc == OscTine || prm.osc == OscBowed || prm.osc == Osc808) ? 0.f : (float) std::sin(v.lfo * twoPi) * prm.vibratoCents * 0.01f * vf * vf;
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
                    const float index = (0.3f + 2.1f * ch) * 0.8f, fbAmt = 0.1f + 0.45f * ch;     // body only: no bite at the start
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
                    // Rhodes colour, held from the first moment: a warm 1:1 FM body + a steady shine of the tine (CHARACTER)
                    const float bark = 0.42f;                                           // held body, no struck attack
                    const float mod = (float) std::sin(v.phm * twoPi);
                    const float body = (float) std::sin(v.ph[0] * twoPi + bark * mod);
                    const float body2 = (float) std::sin(v.ph[1] * twoPi + bark * 0.8f * mod);
                    const float bell = ch * ((float) std::sin(v.ph[4] * twoPi) * 0.07f + (float) std::sin(v.ph[5] * twoPi) * 0.025f);   // a steady glassy shine
                    const float settle = 1.f;
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
                case Osc808:
                {
                    // 808: a sine that starts PUNCH semitones high and drops onto the note (only a fresh note: a glide
                    // keeps going), driven into a soft clipper (CHARACTER), off-centre for even harmonics (WARMTH). Mono.
                    const float punch = prm.vibratoCents / 50.f * 12.f * std::exp(-v.age / 0.04f);
                    const double f8 = inc * std::exp2(punch / 12.0);
                    const float g = 0.25f + 15.f * std::pow(ch, 1.5f), bias = prm.warmth * 0.9f;
                    auto shape = [&](float x) { return (std::tanh(g * (x + bias)) - std::tanh(g * bias)) / std::tanh(g); };
                    float y = shape((float) std::sin(v.ph[0] * twoPi));
                    v.ph[0] += f8; if (v.ph[0] >= 1) v.ph[0] -= 1;
                    if (dbl > 0.001f)
                    {
                        if (v.low)  { y += shape((float) std::sin(v.ph[2] * twoPi)) * dbl * 0.8f; v.ph[2] += f8 * 0.5; if (v.ph[2] >= 1) v.ph[2] -= 1; }
                        if (v.high) { y += shape((float) std::sin(v.ph[3] * twoPi)) * dbl * 0.4f; v.ph[3] += f8 * 2.0; if (v.ph[3] >= 1) v.ph[3] -= 1; }
                    }
                    l = r = y * v.env;
                    v.lpL1 += a * (l - v.lpL1); v.lpL2 += a * (v.lpL1 - v.lpL2);
                    l = r = v.lpL2;
                    break;
                }
                case OscKeys:
                case OscBowed:
                {
                    const bool strings = prm.osc == OscBowed;
                    const auto& bank = *bodies;
                    float sL = 0, sR = 0;
                    if (!strings)
                    {
                        // the body of a key, held: two "strings" a hair apart (the slow beating of a piano unison)
                        const float a0 = bank.morph(false, ch, p, v.ph[0]), a1 = bank.morph(false, ch, p, v.ph[1]);
                        v.ph[0] += inc * std::exp2(-0.014 / 12.0); if (v.ph[0] >= 1) v.ph[0] -= 1;
                        v.ph[1] += inc * std::exp2(0.014 / 12.0);  if (v.ph[1] >= 1) v.ph[1] -= 1;
                        sL = a0 * 0.62f + a1 * 0.38f; sR = a0 * 0.38f + a1 * 0.62f;
                    }
                    else
                    {
                        // a small section: 3 players, each a little out of tune with their own slow vibrato, + bow noise
                        static const float det[3] = { -0.07f, 0.0f, 0.06f }, rate[3] = { 4.7f, 5.3f, 5.9f }, pan[3] = { 0.8f, 0.5f, 0.2f };
                        for (int k = 0; k < 3; ++k)
                        {
                            v.pl[k] += rate[k] * dt; if (v.pl[k] >= 1) v.pl[k] -= 1;
                            const float vibk = (float) std::sin(v.pl[k] * twoPi) * (0.04f + prm.vibratoCents * 0.006f) * vf;
                            const float pk = p + det[k] + vibk;
                            const float x = bank.morph(true, ch, pk, v.ph[k]);
                            v.ph[k] += inc * std::exp2((det[k] + vibk) / 12.0); if (v.ph[k] >= 1) v.ph[k] -= 1;
                            sL += x * pan[k]; sR += x * (1.f - pan[k]);
                        }
                        sL *= 0.62f; sR *= 0.62f;
                        v.noise = v.noise * 1664525u + 1013904223u;
                        const float nz = ((float) (v.noise >> 8) / 8388608.f - 1.f);
                        float nlp, nbp;
                        v.fR.tick(nz, nlp, nbp);
                        const float bow = nbp * std::pow(10.f, BodyBank::noiseDb(true, ch) / 20.f) * 2.2f;
                        sL += bow; sR += bow;
                    }
                    if (dbl > 0.001f)
                    {
                        if (v.low)  { const float d = bank.morph(strings, ch, p - 12.f, v.ph[3]) * dbl * 0.8f; sL += d; sR += d; v.ph[3] += inc * 0.5; if (v.ph[3] >= 1) v.ph[3] -= 1; }
                        if (v.high) { const float d = bank.morph(strings, ch, p + 12.f, v.ph[4]) * dbl * 0.4f; sL += d; sR += d; v.ph[4] += inc * 2.0; if (v.ph[4] >= 1) v.ph[4] -= 1; }
                    }
                    l = sL * v.env; r = sR * v.env;
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
        const bool tape = prm.osc != Osc808;          // the 808 has its own clipper (WARMTH = its even harmonics)
        const bool ens = prm.osc == OscStrings;
        const float depth = ens ? 0.0018f + 0.0026f * prm.character : 0.0022f;
        const float mix = ens ? 0.45f + 0.45f * prm.character : 0.36f;
        const float wet = prm.space * 0.85f;
        const float hpA = 1.f - std::exp(-2.f * 3.14159265f * 250.f / (float) sr), monoA = 1.f - std::exp(-2.f * 3.14159265f * 150.f / (float) sr);
        const float trem = prm.osc == OscTine ? std::clamp(prm.vibratoCents / 50.f, 0.f, 1.f) * 0.7f : 0.f;
        const float echoWet = prm.space * prm.space * 0.55f;                   // the echo comes in as SPACE goes up
        const double echoDelay = std::min((double) echoLen - 2.0, 0.75 * 60.0 / std::max(20.0, curBpm) * sr);
        for (int i = 0; i < n; ++i)
        {
            toneL += toneA * (L[i] - toneL); toneR += toneA * (R[i] - toneR);
            float inL = toneL * pre, inR = toneR * pre;
            // the low end stays dry, centred and tight: the chorus and the room only get what is above ~250 Hz
            hpL += hpA * (inL - hpL); hpR += hpA * (inR - hpR);
            chorusBuf[0][(size_t) chorusPos] = inL - hpL;
            chorusBuf[1][(size_t) chorusPos] = inR - hpR;
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
            if (tape) { oL = std::tanh(oL * drive) / drive; oR = std::tanh(oR * drive) / drive; }
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
            {
                // mono below ~150 Hz
                const float mid = 0.5f * (oL + oR), side = 0.5f * (oL - oR);
                sideLp += monoA * (side - sideLp);
                oL = mid + (side - sideLp); oR = mid - (side - sideLp);
            }
            if (wet > 0.001f)
            {
                float rl, rr;
                revHpL += hpA * (oL - revHpL); revHpR += hpA * (oR - revHpR);
                reverb.process(oL - revHpL, oR - revHpR, rl, rr);
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
    LaneCache cache;
    Params prm;
    Voice voices[kPool];
    Lane lanes[kMaxVoices];
    unsigned counter = 0, rng = 12345u;
    std::vector<float> chorusBuf[2];
    int chorusLen = 1, chorusPos = 0;
    double chorusPh[3] {}, wowPh = 0, flutPh = 0;
    float toneL = 0, toneR = 0, hpL = 0, hpR = 0, sideLp = 0, revHpL = 0, revHpR = 0;
    Reverb reverb;
    std::vector<float> echo[2];
    int echoLen = 1, echoPos = 0;
    float echoLp[2] {}, echoHp[2] {};
    double tremPh = 0, curBpm = 120;
    std::shared_ptr<const BodyBank> bodies;
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
    const long long cycle = (long long) std::llround(s.nBars * barBeats * samplesPerBeat);
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
