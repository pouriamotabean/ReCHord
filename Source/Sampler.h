// ReChord 2.4 - SAMPLE: a sustained note from any synth (a WAV) becomes a gliding voice.
// From each file: the sustain is found, the pitch is detected (or read from the file name), a loop is cut on
// whole periods and crossfaded so it joins itself, then octave-down copies (mips) are made so a note played
// high doesn't alias. Several files = a small multisample: each voice takes the nearest one when it starts.
// No JUCE here (the processor reads the files).
#pragma once
#include <vector>
#include <string>
#include <cmath>
#include <algorithm>
#include <memory>
#include <cctype>

namespace rc
{
struct SampleZone
{
    std::string name;
    float root = 60.f;            // midi pitch of the loop as recorded (fractional: its own tuning)
    bool rootFromName = false;
    double rate = 48000.0;        // the file's sample rate
    int len = 0;                  // loop length at level 0 (a multiple of 8)
    static constexpr int kLevels = 4;
    std::vector<float> lv[kLevels][2];   // level k: the loop low-passed and decimated by 2^k

    // read one stereo sample at pos (level-0 samples) for a playback ratio; blends two levels when playing high
    inline void read(double pos, double ratio, float& outL, float& outR) const
    {
        double oct = ratio > 1.0 ? std::log2(ratio) : 0.0;
        if (oct > kLevels - 1) oct = kLevels - 1;
        const int k = (int) oct;
        const float fr = (float) (oct - k);
        float l0, r0; readLevel(k, pos, l0, r0);
        if (fr > 0.001f && k + 1 < kLevels)
        {
            float l1, r1; readLevel(k + 1, pos, l1, r1);
            outL = l0 + (l1 - l0) * fr; outR = r0 + (r1 - r0) * fr;
        }
        else { outL = l0; outR = r0; }
    }
private:
    inline void readLevel(int k, double pos, float& l, float& r) const
    {
        const int n = len >> k;
        const double x = pos / (double) (1 << k);
        const int i = (int) x;
        const float t = (float) (x - i);
        auto at = [&](const std::vector<float>& v, int j) { j %= n; if (j < 0) j += n; return v[(size_t) j]; };
        auto herm = [&](const std::vector<float>& v)
        {
            const float y0 = at(v, i - 1), y1 = at(v, i), y2 = at(v, i + 1), y3 = at(v, i + 2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        };
        l = herm(lv[k][0]); r = herm(lv[k][1]);
    }
};

struct SampleSet
{
    std::vector<SampleZone> zones;
    int nearest(float pitch) const
    {
        int best = 0; float d = 1e9f;
        for (int i = 0; i < (int) zones.size(); ++i) { const float e = std::abs(zones[(size_t) i].root - pitch); if (e < d) { d = e; best = i; } }
        return best;
    }
};

// ---- analysis ----
// pitch: YIN on a few windows of the sustain, median. Returns 0 when there is no clear pitch.
inline float detectPitch(const std::vector<float>& x, int a, int b, double rate)
{
    const int W = std::min(4096, std::max(512, (int) (rate * 0.06)));
    const int tauMin = std::max(2, (int) (rate / 2000.0)), tauMax = std::min(W - 2, (int) (rate / 30.0));
    if (b - a < W + tauMax || tauMax <= tauMin) return 0.f;
    std::vector<float> found;
    std::vector<double> d((size_t) tauMax + 1), cm((size_t) tauMax + 1);
    for (int w = 0; w < 5; ++w)
    {
        const int s = a + (int) ((double) (b - a - W - tauMax) * (0.15 + 0.175 * w));
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0;
            for (int j = 0; j < W; ++j) { const double e = x[(size_t) (s + j)] - x[(size_t) (s + j + tau)]; sum += e * e; }
            d[(size_t) tau] = sum;
        }
        double run = 0;
        cm[0] = 1;
        for (int tau = 1; tau <= tauMax; ++tau) { run += d[(size_t) tau]; cm[(size_t) tau] = run > 0 ? d[(size_t) tau] * tau / run : 1; }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (cm[(size_t) tau] < 0.15)
            {
                while (tau + 1 < tauMax && cm[(size_t) tau + 1] < cm[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0)
        {
            double m = 1e9;
            for (int tau = tauMin; tau < tauMax; ++tau) if (cm[(size_t) tau] < m) { m = cm[(size_t) tau]; best = tau; }
            if (m > 0.45) continue;   // nothing periodic here
        }
        // parabolic refinement
        double t = best;
        if (best > 1 && best < tauMax - 1)
        {
            const double y0 = cm[(size_t) best - 1], y1 = cm[(size_t) best], y2 = cm[(size_t) best + 1], den = y0 - 2 * y1 + y2;
            if (std::abs(den) > 1e-12) t = best + 0.5 * (y0 - y2) / den;
        }
        found.push_back((float) (69.0 + 12.0 * std::log2(rate / t / 440.0)));
    }
    if (found.empty()) return 0.f;
    std::sort(found.begin(), found.end());
    return found[found.size() / 2];
}

// a note name in a file name ("Pad C3.wav", "lead_F#2", "Bb4") -> midi pitch (C3 = 48, Cubase / Yamaha would say 60:
// we use the common C4 = 60). Returns -1 when there is none.
inline int pitchFromName(const std::string& s)
{
    for (int i = (int) s.size() - 1; i >= 0; --i)
    {
        const char c = (char) std::toupper((unsigned char) s[(size_t) i]);
        if (c < 'A' || c > 'G') continue;
        if (i > 0 && std::isalpha((unsigned char) s[(size_t) i - 1])) continue;       // part of a word
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };
        int pc = base[c - 'A'], j = i + 1;
        if (j < (int) s.size() && (s[(size_t) j] == '#' || s[(size_t) j] == 's')) { ++pc; ++j; }
        else if (j < (int) s.size() && s[(size_t) j] == 'b') { --pc; ++j; }
        bool neg = false;
        if (j < (int) s.size() && s[(size_t) j] == '-') { neg = true; ++j; }
        if (j >= (int) s.size() || !std::isdigit((unsigned char) s[(size_t) j])) continue;
        const int oct = (s[(size_t) j] - '0') * (neg ? -1 : 1);
        if (j + 1 < (int) s.size() && std::isalnum((unsigned char) s[(size_t) j + 1])) continue;   // "C12", "B4x"
        return (oct + 1) * 12 + pc;
    }
    return -1;
}

// one file -> one zone.  loopAmt 0 = a short loop (steady), 1 = the whole sustain (all its movement)
inline bool buildZone(SampleZone& z, const std::vector<float>& inL, const std::vector<float>& inR, double rate, const std::string& name, float loopAmt)
{
    const int N = (int) inL.size();
    if (N < (int) (rate * 0.15)) return false;
    std::vector<float> mono((size_t) N);
    for (int i = 0; i < N; ++i) mono[(size_t) i] = 0.5f * (inL[(size_t) i] + inR[(size_t) i]);
    // the sustain: from just after the attack peak to where it starts to fall away
    const int fr = std::max(64, (int) (rate * 0.01));
    const int nf = N / fr;
    std::vector<float> rms((size_t) nf);
    float peak = 0; int peakF = 0;
    for (int f = 0; f < nf; ++f)
    {
        double s = 0; for (int j = 0; j < fr; ++j) { const float v = mono[(size_t) (f * fr + j)]; s += v * v; }
        rms[(size_t) f] = (float) std::sqrt(s / fr);
        if (rms[(size_t) f] > peak) { peak = rms[(size_t) f]; peakF = f; }
    }
    if (peak < 1e-5f) return false;
    int attackF = 0;                              // the attack is over when it first gets near the peak (not at the peak:
    while (attackF < peakF && rms[(size_t) attackF] < peak * 0.7f) ++attackF;   //  a beating pad can peak late)
    int a = std::max(attackF * fr, (int) (N * 0.03)) + (int) (rate * 0.05);
    int lastF = nf - 1;
    while (lastF > peakF && rms[(size_t) lastF] < peak * 0.35f) --lastF;
    int b = lastF * fr - (int) (rate * 0.03);
    if (b - a < (int) (rate * 0.25)) { a = (int) (N * 0.2); b = (int) (N * 0.85); }
    a = std::clamp(a, 0, N - 2); b = std::clamp(b, a + 2, N);

    // the root: a note name in the file name wins (the detected pitch keeps its fine tuning if it agrees)
    const float det = detectPitch(mono, a, b, rate);
    const int named = pitchFromName(name);
    z.rootFromName = named >= 0;
    if (named >= 0) z.root = (det > 0 && std::abs(det - named) < 0.6f) ? det : (float) named;
    else z.root = det > 0 ? det : 60.f;
    const double period = rate / (440.0 * std::exp2((z.root - 69.0) / 12.0));

    // the loop: [s, s + L) with its last X samples crossfaded into the X before s, so the end flows into the start
    const int region = b - a;
    const int Lmin = (int) std::min<double>(region * 0.6, rate * 0.25);
    int L = (int) (Lmin + (std::min<double>(region * 0.75, rate * 6.0) - Lmin) * std::clamp(loopAmt, 0.f, 1.f));
    int X = std::min((int) (L * 0.35), (int) (rate * 0.25));
    if (period > 4 && period < L) { const int np = std::max(1, (int) std::lround(L / period)); L = (int) std::lround(np * period); }
    L = std::max(64, (L / 8) * 8);
    X = std::min(X, L / 2);
    const int s = std::clamp(a + X + (region - X - L) / 3, X, std::max(X, N - L));
    if (s + L > N) return false;
    z.name = name; z.rate = rate; z.len = L;
    for (int c = 0; c < 2; ++c)
    {
        const auto& src = c == 0 ? inL : inR;
        auto& o = z.lv[0][c];
        o.assign((size_t) L, 0.f);
        for (int i = 0; i < L; ++i) o[(size_t) i] = src[(size_t) (s + i)];
        for (int j = 0; j < X; ++j)
        {
            const float t = (float) (j + 0.5f) / (float) X;
            const float fo = std::cos(t * 1.5707963f), fi = std::sin(t * 1.5707963f);
            o[(size_t) (L - X + j)] = src[(size_t) (s + L - X + j)] * fo + src[(size_t) (s - X + j)] * fi;
        }
    }
    // level: the same loudness for every file
    double e = 0; for (int i = 0; i < L; ++i) e += 0.5 * (z.lv[0][0][(size_t) i] * z.lv[0][0][(size_t) i] + z.lv[0][1][(size_t) i] * z.lv[0][1][(size_t) i]);
    const float g = e > 0 ? (float) (0.42 / std::sqrt(e / L)) : 1.f;
    for (int c = 0; c < 2; ++c) for (auto& v : z.lv[0][c]) v *= g;
    // mips: a circular half-band low-pass, then every 2nd sample
    static const int T = 15;
    float h[2 * T + 1]; float hs = 0;
    for (int k = -T; k <= T; ++k)
    {
        const double x = k * 0.5 * 0.92;      // cutoff 0.46 of the rate (just under the new Nyquist)
        const double sinc = k == 0 ? 1.0 : std::sin(3.141592653589793 * x) / (3.141592653589793 * x);
        const double win = 0.42 + 0.5 * std::cos(3.141592653589793 * k / (T + 1)) + 0.08 * std::cos(2 * 3.141592653589793 * k / (T + 1));
        h[k + T] = (float) (sinc * win); hs += h[k + T];
    }
    for (auto& v : h) v /= hs;
    for (int lvl = 1; lvl < SampleZone::kLevels; ++lvl)
        for (int c = 0; c < 2; ++c)
        {
            const auto& src = z.lv[lvl - 1][c];
            const int n = (int) src.size(), m = n / 2;
            auto& o = z.lv[lvl][c];
            o.assign((size_t) m, 0.f);
            for (int i = 0; i < m; ++i)
            {
                float acc = 0;
                for (int k = -T; k <= T; ++k) { int j = 2 * i + k; j %= n; if (j < 0) j += n; acc += src[(size_t) j] * h[k + T]; }
                o[(size_t) i] = acc;
            }
        }
    return true;
}
} // namespace rc
