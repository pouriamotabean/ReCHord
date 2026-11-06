// ReChord 2.5 - SAMPLE: a note from any synth (a WAV) becomes a gliding voice.
// Per file: the steadiest part is found (or chosen on the SAMPLE EDIT page), its level is flattened (a one-shot's
// decay would pump in a loop), it can be stretched without changing the pitch (WSOLA), the pitch is detected
// (the file name helps: "Pad C3", "lead_F"), then a loop is cut where the end really matches the start and joined
// with a linear crossfade - or, in CYCLE, a few exact periods (as clean as a wavetable). Octave-down copies (mips)
// keep high notes from aliasing. Several files = a small multisample. No JUCE here.
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
    float root = 60.f;            // midi pitch of the loop (fractional: its own tuning)
    double rate = 48000.0;        // the file's sample rate (the loop is at this rate)
    int len = 0;                  // loop length at level 0
    static constexpr int kLevels = 4;
    std::vector<float> lv[kLevels][2];   // level k: the loop low-passed and decimated by 2^k

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
        const auto& v0 = lv[k][0];
        const int n = (int) v0.size();
        const double x = pos * (double) n / (double) len;
        const int i = (int) x;
        const float t = (float) (x - i);
        auto at = [&](const std::vector<float>& v, int j) { j %= n; if (j < 0) j += n; return v[(size_t) j]; };
        auto herm = [&](const std::vector<float>& v)
        {
            const float y0 = at(v, i - 1), y1 = at(v, i), y2 = at(v, i + 1), y3 = at(v, i + 2);
            const float c1 = 0.5f * (y2 - y0), c2 = y0 - 2.5f * y1 + 2.f * y2 - 0.5f * y3, c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
            return ((c3 * t + c2) * t + c1) * t + y1;
        };
        l = herm(v0); r = herm(lv[k][1]);
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

// what the SAMPLE EDIT page sets, per file
struct SampleEdit
{
    double a = -1, b = -1;        // the part used, in seconds (< 0 = automatic: the steadiest part)
    float stretch = 1.f;          // 1..4: longer, without changing the pitch
    float xfade = 0.3f;           // the seam's crossfade, as a part of the loop
    int mode = 0;                 // 0 LOOP (keeps its movement), 1 CYCLE (a few exact periods, like a wavetable)
    int fix = 0;                  // semitones added to the detected root (when the detection is wrong)
};
// what the analysis found, for the page
struct SampleInfo
{
    double autoA = 0, autoB = 0, a = 0, b = 0, loopA = 0, loopB = 0;   // seconds in the file (the loop: before the stretch)
    float detected = 0, root = 60;
    int namePc = -1, nameOct = -99;
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

// a note name in a file name: "Pad C3", "lead_F#2", "Bb4", "mint_F" (no octave: only the pitch class)
inline void nameNote(const std::string& s, int& pc, int& oct)
{
    pc = -1; oct = -99;
    for (int i = (int) s.size() - 1; i >= 0; --i)
    {
        const char c = (char) std::toupper((unsigned char) s[(size_t) i]);
        if (c < 'A' || c > 'G') continue;
        if (i > 0 && std::isalpha((unsigned char) s[(size_t) i - 1])) continue;          // inside a word
        static const int base[7] = { 9, 11, 0, 2, 4, 5, 7 };
        int p = base[c - 'A'], j = i + 1;
        if (j < (int) s.size() && s[(size_t) j] == '#') { ++p; ++j; }
        else if (j < (int) s.size() && (s[(size_t) j] == 'b' || s[(size_t) j] == 's')
                 && (j + 1 >= (int) s.size() || !std::isalpha((unsigned char) s[(size_t) j + 1]))) { p += s[(size_t) j] == 'b' ? -1 : 1; ++j; }
        bool neg = false;
        if (j < (int) s.size() && s[(size_t) j] == '-' && j + 1 < (int) s.size() && std::isdigit((unsigned char) s[(size_t) j + 1])) { neg = true; ++j; }
        if (j < (int) s.size() && std::isdigit((unsigned char) s[(size_t) j]))
        {
            if (j + 1 < (int) s.size() && std::isalnum((unsigned char) s[(size_t) j + 1])) continue;   // "C12", "B4x"
            pc = (p + 12) % 12; oct = (s[(size_t) j] - '0') * (neg ? -1 : 1); return;
        }
        if (j < (int) s.size() && std::isalnum((unsigned char) s[(size_t) j])) continue;           // "Cat", "Bass"
        pc = (p + 12) % 12; return;                                                             // "_F", " Eb"
    }
}

// level per 10 ms frame, in dB
inline std::vector<float> framesDb(const std::vector<float>& m, double rate, int& fr)
{
    fr = std::max(64, (int) (rate * 0.01));
    const int nf = (int) m.size() / fr;
    std::vector<float> d((size_t) std::max(nf, 1), -120.f);
    for (int f = 0; f < nf; ++f)
    {
        double s = 0; for (int j = 0; j < fr; ++j) { const float v = m[(size_t) (f * fr + j)]; s += v * v; }
        d[(size_t) f] = (float) (10.0 * std::log10(s / fr + 1e-12));
    }
    return d;
}

// the steadiest part: after the attack, before the release, where level and brightness change least
inline void steadiest(const std::vector<float>& m, double rate, double& a, double& b)
{
    int fr; const auto db = framesDb(m, rate, fr);
    const int nf = (int) db.size();
    a = 0; b = (double) m.size() / rate;
    if (nf < 8) return;
    // brightness proxy per frame: zero crossings
    std::vector<float> zc((size_t) nf, 0.f);
    for (int f = 0; f < nf; ++f) { int z = 0; for (int j = 1; j < fr; ++j) z += (m[(size_t) (f * fr + j)] >= 0) != (m[(size_t) (f * fr + j - 1)] >= 0); zc[(size_t) f] = (float) z / fr; }
    float peak = -120; for (auto v : db) peak = std::max(peak, v);
    int f0 = 0; while (f0 < nf - 1 && db[(size_t) f0] < peak - 3.f) ++f0;
    f0 = std::min(nf - 1, f0 + 3);
    int f1 = nf - 1; while (f1 > f0 && db[(size_t) f1] < peak - 20.f) --f1;
    const int zone = f1 - f0;
    if (zone < 6) { a = f0 * fr / rate; b = (f1 + 1) * fr / rate; return; }
    const int W = std::clamp((int) (zone * 0.45), std::min(zone, 25), 300);
    double best = 1e30; int bf = f0;
    for (int s = f0; s + W <= f1; ++s)
    {
        double mu = 0, mz = 0; for (int k = s; k < s + W; ++k) { mu += db[(size_t) k]; mz += zc[(size_t) k]; }
        mu /= W; mz /= W;
        double v = 0, vz = 0; for (int k = s; k < s + W; ++k) { v += (db[(size_t) k] - mu) * (db[(size_t) k] - mu); vz += (zc[(size_t) k] - mz) * (zc[(size_t) k] - mz); }
        const double cost = v / W + 400.0 * vz / W / std::max(1e-4, mz * mz) * 0.01 - 0.02 * mu;   // steady, and not the quiet tail
        if (cost < best) { best = cost; bf = s; }
    }
    a = bf * fr / rate; b = (bf + W) * fr / rate;
}

// make the level flat (a one-shot's decay, a swell) so the loop doesn't pump
inline void flatten(std::vector<float>& L, std::vector<float>& R, double rate)
{
    const int n = (int) L.size();
    const int hop = std::max(32, (int) (rate * 0.01)), nf = n / hop + 1;
    std::vector<float> env((size_t) nf, 0.f);
    for (int f = 0; f < nf; ++f)
    {
        const int c = f * hop, w = (int) (rate * 0.03);
        double s = 0; int cnt = 0;
        for (int j = std::max(0, c - w); j < std::min(n, c + w); ++j) { s += 0.5 * (L[(size_t) j] * L[(size_t) j] + R[(size_t) j] * R[(size_t) j]); ++cnt; }
        env[(size_t) f] = (float) std::sqrt(s / std::max(1, cnt));
    }
    double mean = 0; for (auto e : env) mean += e; mean /= nf;
    if (mean < 1e-7) return;
    for (int i = 0; i < n; ++i)
    {
        const float x = (float) i / hop; const int f = std::min((int) x, nf - 2); const float t = x - f;
        const float e = env[(size_t) f] + (env[(size_t) f + 1] - env[(size_t) f]) * t;
        const float g = std::clamp((float) mean / std::max(e, 1e-7f), 0.25f, 4.f);
        L[(size_t) i] *= g; R[(size_t) i] *= g;
    }
}

// longer without changing the pitch: WSOLA (overlapping grains, each placed where it lines up with the last)
inline void stretchWsola(std::vector<float>& L, std::vector<float>& R, double factor, double period)
{
    if (factor < 1.01) return;
    const int n = (int) L.size();
    int win = (int) std::max(1024.0, std::ceil(period * 4.0)); win += win & 1;
    if (n < win * 2) return;
    const int ha = win / 2;
    const int tol = (int) std::clamp(period > 1 ? period : 200.0, 64.0, 600.0);
    const int m = (int) (n * factor);
    std::vector<float> oL((size_t) m + (size_t) win, 0.f), oR((size_t) m + (size_t) win, 0.f), mono((size_t) n);
    for (int i = 0; i < n; ++i) mono[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]);
    std::vector<float> w((size_t) win);
    for (int i = 0; i < win; ++i) w[(size_t) i] = 0.5f - 0.5f * std::cos(6.283185307f * i / win);
    int prev = 0;
    for (int k = 0; k * ha < m; ++k)
    {
        const int nominal = (int) (k * ha / factor);
        int pos = std::clamp(nominal, 0, n - win);
        if (k > 0)
        {
            const int target = std::clamp(prev + ha, 0, n - win);       // where the last grain would naturally go on
            auto corr = [&](int p, int step)
            {
                double s = 0; for (int j = 0; j < ha; j += step) s += mono[(size_t) (p + j)] * mono[(size_t) (target + j)];
                return s;
            };
            int bestP = pos; double best = -1e30;
            for (int d = -tol; d <= tol; d += 4) { const int p = std::clamp(nominal + d, 0, n - win); const double c = corr(p, 4); if (c > best) { best = c; bestP = p; } }
            const int c0 = bestP;
            for (int d = -4; d <= 4; ++d) { const int p = std::clamp(c0 + d, 0, n - win); const double c = corr(p, 1); if (c > best || d == -4) { if (c > best) { best = c; bestP = p; } } }
            pos = bestP;
        }
        const int o = k * ha;
        for (int j = 0; j < win && o + j < (int) oL.size(); ++j) { oL[(size_t) (o + j)] += L[(size_t) (pos + j)] * w[(size_t) j]; oR[(size_t) (o + j)] += R[(size_t) (pos + j)] * w[(size_t) j]; }
        prev = pos;
    }
    // the first half-window has only one grain: keep it at full level
    for (int j = 0; j < ha; ++j) { const float g = 1.f / std::max(0.05f, w[(size_t) j]); oL[(size_t) j] *= g; oR[(size_t) j] *= g; }
    oL.resize((size_t) m); oR.resize((size_t) m);
    L.swap(oL); R.swap(oR);
}

// one file -> one zone.  loopAmt (the LOOP knob) = how much of the chosen part the loop uses
inline bool buildZone(SampleZone& z, SampleInfo& info, const std::vector<float>& inL, const std::vector<float>& inR, double rate,
                      const std::string& name, const SampleEdit& ed, float loopAmt)
{
    const int N = (int) inL.size();
    if (N < (int) (rate * 0.1)) return false;
    std::vector<float> mono((size_t) N);
    for (int i = 0; i < N; ++i) mono[(size_t) i] = 0.5f * (inL[(size_t) i] + inR[(size_t) i]);
    steadiest(mono, rate, info.autoA, info.autoB);
    double a = ed.a >= 0 ? ed.a : info.autoA, b = ed.b >= 0 ? ed.b : info.autoB;
    a = std::clamp(a, 0.0, N / rate - 0.03); b = std::clamp(b, a + 0.03, N / rate);
    info.a = a; info.b = b;
    const int ia = (int) (a * rate), ib = (int) (b * rate);

    // the root: detected on the chosen part; a name with an octave decides, a name without one picks the pitch class
    info.detected = detectPitch(mono, ia, ib, rate);
    if (info.detected <= 0) info.detected = detectPitch(mono, 0, N, rate);
    nameNote(name, info.namePc, info.nameOct);
    float root = info.detected > 0 ? info.detected : 60.f;
    if (info.namePc >= 0)
    {
        float nominal;
        if (info.nameOct > -99) nominal = (float) ((info.nameOct + 1) * 12 + info.namePc);
        else nominal = (float) info.namePc + 12.f * std::round((root - (float) info.namePc) / 12.f);
        root = (info.detected > 0 && std::abs(info.detected - nominal) < 0.6f) ? info.detected : nominal;
    }
    root += (float) ed.fix;
    info.root = root;
    const double period = rate / (440.0 * std::exp2((root - 69.0) / 12.0));

    // the part: flattened, then stretched
    std::vector<float> L(inL.begin() + ia, inL.begin() + ib), R(inR.begin() + ia, inR.begin() + ib);
    flatten(L, R, rate);
    float st = std::clamp(ed.stretch, 1.f, 4.f);
    if ((double) L.size() * st > rate * 24.0) st = (float) std::max(1.0, rate * 24.0 / (double) L.size());   // keep the work small
    stretchWsola(L, R, st, period);
    const int M = (int) L.size();
    std::vector<float> m2((size_t) M);
    for (int i = 0; i < M; ++i) m2[(size_t) i] = 0.5f * (L[(size_t) i] + R[(size_t) i]);
    auto ncorr = [&](int p, int q, int w)
    {
        double s = 0, e1 = 0, e2 = 0;
        for (int j = 0; j < w; ++j) { const double x = m2[(size_t) (p + j)], y = m2[(size_t) (q + j)]; s += x * y; e1 += x * x; e2 += y * y; }
        return s / std::sqrt(e1 * e2 + 1e-12);
    };

    int s0, Lp, X;
    if (ed.mode == 1 && period > 2 && period * 4 < M)
    {
        // CYCLE: a few exact periods from the middle
        const int k = std::max(1, (int) std::lround(2048.0 / period));
        Lp = (int) std::lround(k * period);
        if (Lp * 2 + 8 > M) Lp = (int) std::lround(period * std::max(1, (int) ((M / 2 - 8) / period)));
        X = Lp;                                   // the loop crossfades fully with the periods just before it
        s0 = std::max(X, std::min((M - Lp) / 2, M - Lp));
        // the exact length: where one more period lines up best
        int best = Lp; double bc = -2;
        for (int d = -3; d <= 3; ++d)
        {
            const int l2 = Lp + d, w = std::min(l2, 64 + (int) period);
            if (l2 < 32 || s0 + l2 + w > M) continue;
            const double c = ncorr(s0, s0 + l2, w);
            if (c > bc) { bc = c; best = l2; }
        }
        Lp = best;
    }
    else
    {
        // LOOP: as long as the LOOP knob says; the end is moved to where it really matches the start
        const double lmin = std::min(rate * 0.25, M * 0.4), lmax = std::min(M * 0.82, rate * 8.0);   // 8 s of loop is plenty
        Lp = (int) (lmin + (lmax - lmin) * std::clamp(loopAmt, 0.f, 1.f));
        X = (int) std::clamp((double) Lp * std::clamp(ed.xfade, 0.05f, 0.5f), 64.0, (double) (M - Lp) - 2);
        if (X < 32) { X = std::min(Lp / 3, 32); Lp = std::max(64, M - X - 2); }
        s0 = X + std::max(0, (M - X - Lp) / 2);
        if (s0 + Lp > M) Lp = M - s0;
        const int w = std::min(X, 4096);
        const int range = (int) std::clamp(period > 2 ? period * 1.5 : 300.0, 32.0, 2000.0);
        int best = Lp; double bc = -2;
        for (int d = -range; d <= range; ++d)
        {
            const int l2 = Lp + d;
            if (l2 < 64 || s0 + l2 > M || l2 <= X) continue;
            const double c = ncorr(s0 + l2 - w, s0 - w, w);
            if (c > bc) { bc = c; best = l2; }
        }
        Lp = best;
    }
    X = std::min(X, Lp);                         // the seam never longer than the loop (CYCLE moves Lp after X was set)
    if (Lp < 32 || X < 1 || s0 - X < 0 || s0 + Lp > M) return false;
    info.loopA = a + (double) s0 / st / rate; info.loopB = a + (double) (s0 + Lp) / st / rate;

    z.name = name; z.rate = rate; z.len = Lp; z.root = root;
    for (int c = 0; c < 2; ++c)
    {
        const auto& src = c == 0 ? L : R;
        auto& o = z.lv[0][c];
        o.assign((size_t) Lp, 0.f);
        for (int i = 0; i < Lp; ++i) o[(size_t) i] = src[(size_t) (s0 + i)];
        for (int j = 0; j < X; ++j)                                   // linear: both sides are the same sound, in phase
        {
            const float t = (float) (j + 0.5f) / (float) X;
            const int i = Lp - X + j;
            o[(size_t) i] = src[(size_t) (s0 + i)] * (1.f - t) + src[(size_t) (s0 - X + j)] * t;
        }
    }
    double e = 0; for (int i = 0; i < Lp; ++i) e += 0.5 * (z.lv[0][0][(size_t) i] * z.lv[0][0][(size_t) i] + z.lv[0][1][(size_t) i] * z.lv[0][1][(size_t) i]);
    const float g = e > 0 ? (float) (0.42 / std::sqrt(e / Lp)) : 1.f;
    for (int c = 0; c < 2; ++c) for (auto& v : z.lv[0][c]) v *= g;
    // mips
    static const int T = 15;
    float h[2 * T + 1]; float hs = 0;
    for (int k = -T; k <= T; ++k)
    {
        const double x = k * 0.5 * 0.92;
        const double sinc = k == 0 ? 1.0 : std::sin(3.141592653589793 * x) / (3.141592653589793 * x);
        const double win = 0.42 + 0.5 * std::cos(3.141592653589793 * k / (T + 1)) + 0.08 * std::cos(2 * 3.141592653589793 * k / (T + 1));
        h[k + T] = (float) (sinc * win); hs += h[k + T];
    }
    for (auto& v : h) v /= hs;
    for (int lvl = 1; lvl < SampleZone::kLevels; ++lvl)
        for (int c = 0; c < 2; ++c)
        {
            const auto& src = z.lv[lvl - 1][c];
            const int n = (int) src.size(), mm = std::max(4, n / 2);
            auto& o = z.lv[lvl][c];
            o.assign((size_t) mm, 0.f);
            for (int i = 0; i < mm; ++i)
            {
                float acc = 0;
                for (int k = -T; k <= T; ++k) { int j = 2 * i + k; j %= n; if (j < 0) j += n; acc += src[(size_t) j] * h[k + T]; }
                o[(size_t) i] = acc;
            }
        }
    return true;
}
} // namespace rc
