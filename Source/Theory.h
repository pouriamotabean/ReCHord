// ReChord - chord theory, smart voicing and note links (no JUCE: also used by the test tool)
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace rc
{
constexpr int kBars = 4;      // the cycle: 4 chords, one bar each
constexpr int kMaxVoices = 4;

enum Quality { Maj = 0, Min, Dom7, Maj7, Min7, Sus2, Sus4, Dim, kNumQualities };

inline const char* qualitySuffix(int q)
{
    static const char* s[] = { "", "m", "7", "maj7", "m7", "sus2", "sus4", "dim" };
    return s[std::clamp(q, 0, kNumQualities - 1)];
}
inline const char* qualityLabel(int q)
{
    static const char* s[] = { "MAJ", "MIN", "7", "MAJ7", "MIN7", "SUS2", "SUS4", "DIM" };
    return s[std::clamp(q, 0, kNumQualities - 1)];
}
inline const char* rootName(int pc)
{
    static const char* n[] = { "C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    return n[((pc % 12) + 12) % 12];
}
inline std::string chordName(int root, int q, int transpose = 0)
{
    return std::string(rootName(root + transpose)) + qualitySuffix(q);
}
inline std::string noteName(int midi)
{
    return std::string(rootName(midi)) + std::to_string(midi / 12 - 1);
}

struct Chord { int root = 9, quality = Min; };

// the pitch classes a chord uses for n voices
// triad + 4 voices: the root is doubled; seventh + 3 voices: the fifth is left out
inline std::vector<int> chordTones(const Chord& c, int n)
{
    static const std::vector<int> iv[] = { { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 },
                                           { 0, 3, 7, 10 }, { 0, 2, 7 }, { 0, 5, 7 }, { 0, 3, 6 } };
    std::vector<int> t = iv[std::clamp(c.quality, 0, kNumQualities - 1)];
    if ((int) t.size() < n) t.push_back(0);                         // doubled root
    if ((int) t.size() > n) t.erase(t.begin() + 2);                 // drop the fifth
    for (auto& x : t) x = (x + c.root) % 12;
    return t;
}

// what the audio thread plays: plain arrays, copied whole
struct Score
{
    int nVoices = 3;
    int pitch[kBars][kMaxVoices] {};   // MIDI note of each slot in each bar
    int perm[kBars][kMaxVoices] {};    // slot s of bar b goes to slot perm[b][s] of bar b+1 (bar 4 wraps to bar 1)
    bool glide[kBars][kMaxVoices] {};  // true = glide to it, false = the note ends and the next one starts fresh
};

// ---------------- smart voicing ----------------
// every voicing of a chord inside a register window; the chord tones are kept, octaves are free
inline std::vector<std::array<int, kMaxVoices>> candidates(const Chord& c, int n, int centre)
{
    const auto tones = chordTones(c, n);
    const int lo = centre - 11, hi = centre + 12;
    std::vector<std::vector<int>> opts;
    for (int pc : tones)
    {
        std::vector<int> o;
        for (int m = lo; m <= hi; ++m) if (((m % 12) + 12) % 12 == pc) o.push_back(m);
        opts.push_back(o);
    }
    std::vector<std::array<int, kMaxVoices>> out;
    std::vector<int> idx(n, 0);
    for (;;)
    {
        std::array<int, kMaxVoices> v {};
        std::vector<int> p;
        for (int i = 0; i < n; ++i) p.push_back(opts[i][idx[i]]);
        std::sort(p.begin(), p.end());
        bool ok = true;
        for (int i = 1; i < n; ++i) if (p[i] == p[i - 1]) ok = false;                 // no unisons
        if (p.back() - p.front() > 19) ok = false;                                      // not wider than an octave + 6th
        if (ok)
        {
            for (int i = 0; i < n; ++i) v[i] = p[i];
            if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
        }
        int k = 0;
        while (k < n && ++idx[k] >= (int) opts[k].size()) { idx[k] = 0; ++k; }
        if (k == n) break;
    }
    return out;
}

// sorted voicings: matching voice i to voice i is the smallest total movement
inline float moveCost(const std::array<int, kMaxVoices>& a, const std::array<int, kMaxVoices>& b, int n)
{
    float c = 0;
    for (int i = 0; i < n; ++i)
    {
        const int d = std::abs(a[i] - b[i]);
        c += (float) d + (d > 7 ? (d - 7) * 0.6f : 0.f);  // big leaps cost extra
    }
    return c;
}
inline float shapeCost(const std::array<int, kMaxVoices>& v, int n, int centre)
{
    float mean = 0;
    for (int i = 0; i < n; ++i) mean += (float) v[i];
    mean /= (float) n;
    const int span = v[n - 1] - v[0];
    return std::abs(mean - (float) centre) * 0.35f + std::max(0, span - 12) * 0.25f
         + (v[0] < 52 ? (52 - v[0]) * 0.5f : 0.f);          // low thirds get muddy
}

// the whole cycle at once, the wrap (bar 4 -> bar 1) included
inline void smartVoicing(Score& s, const Chord (&ch)[kBars], int n, int octave)
{
    n = std::clamp(n, 3, kMaxVoices);
    s.nVoices = n;
    const int centre = 64 + 12 * octave;
    std::vector<std::array<int, kMaxVoices>> cand[kBars];
    for (int b = 0; b < kBars; ++b) cand[b] = candidates(ch[b], n, centre);

    float best = 1e30f;
    std::array<int, kBars> bestPick {};
    for (int c0 = 0; c0 < (int) cand[0].size(); ++c0)
    {
        // dp over bars 1..3 with bar 0 fixed
        std::vector<float> cost[kBars];
        std::vector<int> from[kBars];
        cost[0].assign(cand[0].size(), 1e30f);
        cost[0][c0] = shapeCost(cand[0][c0], n, centre);
        for (int b = 1; b < kBars; ++b)
        {
            cost[b].assign(cand[b].size(), 1e30f);
            from[b].assign(cand[b].size(), -1);
            for (int j = 0; j < (int) cand[b].size(); ++j)
                for (int i = 0; i < (int) cand[b - 1].size(); ++i)
                {
                    if (cost[b - 1][i] >= 1e29f) continue;
                    const float c = cost[b - 1][i] + moveCost(cand[b - 1][i], cand[b][j], n) + shapeCost(cand[b][j], n, centre);
                    if (c < cost[b][j]) { cost[b][j] = c; from[b][j] = i; }
                }
        }
        for (int j = 0; j < (int) cand[kBars - 1].size(); ++j)
        {
            if (cost[kBars - 1][j] >= 1e29f) continue;
            const float c = cost[kBars - 1][j] + moveCost(cand[kBars - 1][j], cand[0][c0], n);
            if (c < best)
            {
                best = c;
                bestPick[kBars - 1] = j;
                for (int b = kBars - 1; b > 0; --b) bestPick[b - 1] = from[b][bestPick[b]];
            }
        }
    }
    for (int b = 0; b < kBars; ++b)
        for (int v = 0; v < kMaxVoices; ++v)
        {
            s.pitch[b][v] = v < n ? cand[b][bestPick[b]][v] : 0;
            s.perm[b][v] = v;           // voice i -> voice i (the smallest movement for sorted voicings)
            s.glide[b][v] = true;
        }
}

// ---------------- edits ----------------
// link slot s of bar b to slot t of bar b+1; whatever pointed at t takes s's old target (links stay one-to-one)
inline void setLink(Score& s, int b, int slot, int target)
{
    int other = -1;
    for (int i = 0; i < s.nVoices; ++i) if (s.perm[b][i] == target) other = i;
    if (other < 0 || other == slot) return;
    std::swap(s.perm[b][slot], s.perm[b][other]);
}
inline int linkInto(const Score& s, int b, int target)  // which slot of bar b-1 arrives at slot target of bar b
{
    const int pb = (b + kBars - 1) % kBars;
    for (int i = 0; i < s.nVoices; ++i) if (s.perm[pb][i] == target) return i;
    return target;
}

// ---------------- lanes: one synth voice path through the cycle ----------------
// lane k starts in slot k of bar 0 and follows the links; slot sequences repeat within 48 bars (4 x lcm(1..4))
inline int laneSlot(const Score& s, int lane, long long bar)
{
    long long m = ((bar % 48) + 48) % 48;
    int slot = lane;
    for (long long b = 0; b < m; ++b) slot = s.perm[b % kBars][slot];
    return slot;
}

// the glide's shape: curve < 0 moves early and settles, curve > 0 waits and lands late
inline float glideShape(float x, float curve)
{
    x = std::clamp(x, 0.f, 1.f);
    const float p = std::pow(2.f, 1.6f * curve);
    const float xp = std::pow(x, p);
    const float ss = xp * xp * (3.f - 2.f * xp);
    return 0.45f * xp + 0.55f * ss;
}

struct LaneState { float pitch = 60; long long noteId = 0; };

// where lane k is at beat position u (already shifted by the lane's spread delay)
inline LaneState evalLane(const Score& s, int lane, double u, double barLen, double early, double late, float curve)
{
    early = std::clamp(early, 0.0, barLen * 0.5);
    late = std::clamp(late, 0.0, barLen * 0.5);
    const long long B = (long long) std::floor(u / barLen);
    const double T0 = (double) B * barLen, T1 = T0 + barLen;
    auto bi = [](long long b) { return (int) (((b % kBars) + kBars) % kBars); };

    const int sl = laneSlot(s, lane, B), i = bi(B);
    const float cur = (float) s.pitch[i][sl];
    const int sp = laneSlot(s, lane, B - 1), ip = bi(B - 1);
    const bool gIn = s.glide[ip][sp];
    const bool gOut = s.glide[i][sl];
    const double w = early + late;

    LaneState st;
    st.pitch = cur;
    if (w > 1e-6)
    {
        if (gOut && u >= T1 - early)
        {
            const float nxt = (float) s.pitch[bi(B + 1)][s.perm[i][sl]];
            st.pitch = cur + (nxt - cur) * glideShape((float) ((u - (T1 - early)) / w), curve);
        }
        else if (gIn && u < T0 + late)
        {
            const float prv = (float) s.pitch[ip][sp];
            st.pitch = prv + (cur - prv) * glideShape((float) ((u - (T0 - early)) / w), curve);
        }
    }
    // the note began at the last bar line that was not reached by a glide
    long long start = B;
    for (int k = 0; k < 48; ++k)
    {
        const long long pb = start - 1;
        if (!s.glide[bi(pb)][laneSlot(s, lane, pb)]) break;
        start = pb;
        if (k == 47) start = -1000000;     // glides all the way round: one endless note
    }
    st.noteId = start;
    return st;
}
} // namespace rc
