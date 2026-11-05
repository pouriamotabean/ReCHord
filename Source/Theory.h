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
constexpr int kBars = 4;        // the pattern is 4 bars long
constexpr int kMaxSlots = 8;    // each bar can hold 1 chord or 2 half-bar chords
constexpr int kMaxVoices = 4;

enum Quality { Maj = 0, Min, Dom7, Maj7, Min7, Sus2, Sus4, Dim, kNumQualities };
enum EndMode { Loop = 0, None = 1, ToChord = 2 };   // the ends of the pattern
enum GlideMode { Smooth = 0, Steps = 1, Robot = 2 };

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
inline std::string chordName(int root, int q, int transpose = 0) { return std::string(rootName(root + transpose)) + qualitySuffix(q); }
inline std::string noteName(int midi) { return std::string(rootName(midi)) + std::to_string((int) std::floor(midi / 12.0) - 1); }

struct Chord { int root = 9, quality = Min; };

// the pitch classes a chord uses for n voices (triad + 4 voices: root doubled; seventh + 3 voices: no fifth)
inline std::vector<int> chordTones(const Chord& c, int n)
{
    static const std::vector<int> iv[] = { { 0, 4, 7 }, { 0, 3, 7 }, { 0, 4, 7, 10 }, { 0, 4, 7, 11 },
                                           { 0, 3, 7, 10 }, { 0, 2, 7 }, { 0, 5, 7 }, { 0, 3, 6 } };
    std::vector<int> t = iv[std::clamp(c.quality, 0, kNumQualities - 1)];
    if ((int) t.size() < n) t.push_back(0);
    if ((int) t.size() > n) t.erase(t.begin() + 2);
    for (auto& x : t) x = (x + c.root) % 12;
    return t;
}

// ---------------- what the user writes ----------------
struct Pattern
{
    bool split[kBars] {};                                       // bar b holds two half-bar chords
    Chord chord[kBars * 2] = { { 9, Min }, { 9, Min }, { 7, Maj }, { 7, Maj }, { 5, Maj }, { 5, Maj }, { 4, Min }, { 4, Min } };
    int entryMode = Loop, exitMode = Loop;
    Chord entry { 4, Min }, exit { 2, Min };

    // the chord slots in time order: chord index (bar*2 + half) and start in bars
    int slots(int* key, float* start) const
    {
        int n = 0;
        for (int b = 0; b < kBars; ++b)
        {
            key[n] = b * 2; start[n] = (float) b; ++n;
            if (split[b]) { key[n] = b * 2 + 1; start[n] = b + 0.5f; ++n; }
        }
        start[n] = (float) kBars;
        return n;
    }
};

// ---------------- what the audio thread plays (plain arrays, copied whole) ----------------
struct Link
{
    int to = 0;           // voice of the next chord this one goes to
    bool glide = true;    // false: the note ends at the change, the next one starts fresh
    float early = 0.5f;   // beats before the change the glide starts
    float late = 0.25f;   // beats after the change it lands
    float curve = 0.f;    // -1 moves at once and settles ... +1 waits and jumps late
};

struct Score
{
    int nVoices = 3, nSlots = 4;
    float start[kMaxSlots + 1] = { 0, 1, 2, 3, 4 };    // in bars
    int key[kMaxSlots] = { 0, 2, 4, 6 };                // which Pattern chord each slot plays
    int pitch[kMaxSlots][kMaxVoices] {};
    Link link[kMaxSlots][kMaxVoices];                   // slot i -> slot i+1 (the last one -> loop / exit)
    int entryMode = Loop, exitMode = Loop;
    int entryPitch[kMaxVoices] {}, exitPitch[kMaxVoices] {};
    Link entryLink[kMaxVoices];                         // entry chord -> slot 0
    int scaleMask = 0;                                  // pitch classes of every chord (robot glide)
    bool edited = false;                                // false = exactly the smart voicing

    bool loop() const { return exitMode == Loop; }
    // transition t: 0..nSlots-1 = after slot t;  -1 = entry -> slot 0
    Link& linkAt(int t, int v) { return t < 0 ? entryLink[v] : link[t][v]; }
    const Link& linkAt(int t, int v) const { return t < 0 ? entryLink[v] : link[t][v]; }
    // column c: 0..nSlots-1 = slots, -1 = entry, nSlots = exit
    int& pitchAt(int c, int v) { return c < 0 ? entryPitch[v] : c >= nSlots ? exitPitch[v] : pitch[c][v]; }
    int pitchAt(int c, int v) const { return c < 0 ? entryPitch[v] : c >= nSlots ? (loop() ? pitch[0][v] : exitPitch[v]) : pitch[c][v]; }
    bool hasTarget(int t) const { return t < 0 ? entryMode == ToChord : (t < nSlots - 1 || exitMode != None); }
};

// ---------------- smart voicing ----------------
using Voicing = std::array<int, kMaxVoices>;

inline std::vector<Voicing> candidates(const Chord& c, int n, int centre)
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
    std::vector<Voicing> out;
    std::vector<size_t> idx((size_t) n, 0);
    for (;;)
    {
        std::vector<int> p;
        for (int i = 0; i < n; ++i) p.push_back(opts[(size_t) i][idx[(size_t) i]]);
        std::sort(p.begin(), p.end());
        bool ok = p.back() - p.front() <= 19;
        for (size_t i = 1; i < p.size(); ++i) if (p[i] == p[i - 1]) ok = false;
        if (ok)
        {
            Voicing v {};
            for (size_t i = 0; i < p.size(); ++i) v[i] = p[i];
            if (std::find(out.begin(), out.end(), v) == out.end()) out.push_back(v);
        }
        size_t k = 0;
        while (k < (size_t) n && ++idx[k] >= opts[k].size()) { idx[k] = 0; ++k; }
        if (k == (size_t) n) break;
    }
    return out;
}
inline float moveCost(const Voicing& a, const Voicing& b, int n)
{
    float c = 0;
    for (size_t i = 0; i < (size_t) n; ++i)
    {
        const int d = std::abs(a[i] - b[i]);
        c += (float) d + (d > 7 ? (float) (d - 7) * 0.6f : 0.f);
    }
    return c;
}
inline float shapeCost(const Voicing& v, int n, int centre)
{
    float mean = 0;
    for (size_t i = 0; i < (size_t) n; ++i) mean += (float) v[i];
    mean /= (float) n;
    const int span = v[(size_t) n - 1] - v[0];
    return std::abs(mean - (float) centre) * 0.35f + (float) std::max(0, span - 12) * 0.25f + (v[0] < 52 ? (float) (52 - v[0]) * 0.5f : 0.f);
}

// best voicings for a chain of chords; cyclic = the last one also leads back to the first
inline std::vector<Voicing> bestChain(const std::vector<Chord>& ch, int n, int centre, bool cyclic)
{
    const size_t m = ch.size();
    std::vector<std::vector<Voicing>> cand(m);
    for (size_t i = 0; i < m; ++i) cand[i] = candidates(ch[i], n, centre);
    float best = 1e30f;
    std::vector<size_t> bestPick(m, 0);
    const size_t firstChoices = cyclic ? cand[0].size() : 1;
    for (size_t c0 = 0; c0 < firstChoices; ++c0)
    {
        std::vector<std::vector<float>> cost(m);
        std::vector<std::vector<size_t>> from(m);
        cost[0].assign(cand[0].size(), 1e30f);
        if (cyclic) cost[0][c0] = shapeCost(cand[0][c0], n, centre);
        else for (size_t j = 0; j < cand[0].size(); ++j) cost[0][j] = shapeCost(cand[0][j], n, centre);
        for (size_t b = 1; b < m; ++b)
        {
            cost[b].assign(cand[b].size(), 1e30f);
            from[b].assign(cand[b].size(), 0);
            for (size_t j = 0; j < cand[b].size(); ++j)
                for (size_t i = 0; i < cand[b - 1].size(); ++i)
                {
                    if (cost[b - 1][i] >= 1e29f) continue;
                    const float c = cost[b - 1][i] + moveCost(cand[b - 1][i], cand[b][j], n) + shapeCost(cand[b][j], n, centre);
                    if (c < cost[b][j]) { cost[b][j] = c; from[b][j] = i; }
                }
        }
        for (size_t j = 0; j < cand[m - 1].size(); ++j)
        {
            if (cost[m - 1][j] >= 1e29f) continue;
            const float c = cost[m - 1][j] + (cyclic ? moveCost(cand[m - 1][j], cand[0][c0], n) : 0.f);
            if (c < best)
            {
                best = c;
                bestPick[m - 1] = j;
                for (size_t b = m - 1; b > 0; --b) bestPick[b - 1] = from[b][bestPick[b]];
            }
        }
    }
    std::vector<Voicing> out(m);
    for (size_t i = 0; i < m; ++i) out[i] = cand[i][bestPick[i]];
    return out;
}

// a fresh score from the pattern: slots, smart voicing, default links (all edits dropped)
inline void buildScore(Score& s, const Pattern& p, int n, int octave)
{
    n = std::clamp(n, 3, kMaxVoices);
    Score fresh;
    s = fresh;
    s.nVoices = n;
    s.nSlots = p.slots(s.key, s.start);
    s.exitMode = p.exitMode;
    s.entryMode = p.exitMode == Loop ? Loop : p.entryMode;
    const int centre = 64 + 12 * octave;

    std::vector<Chord> chain;
    const bool withEntry = !s.loop() && s.entryMode == ToChord, withExit = !s.loop() && s.exitMode == ToChord;
    if (withEntry) chain.push_back(p.entry);
    for (int i = 0; i < s.nSlots; ++i) chain.push_back(p.chord[s.key[i]]);
    if (withExit) chain.push_back(p.exit);
    const auto v = bestChain(chain, n, centre, s.loop());

    size_t k = 0;
    if (withEntry) { for (int j = 0; j < n; ++j) s.entryPitch[j] = v[k][(size_t) j]; ++k; }
    for (int i = 0; i < s.nSlots; ++i, ++k) for (int j = 0; j < n; ++j) s.pitch[i][j] = v[k][(size_t) j];
    if (withExit) for (int j = 0; j < n; ++j) s.exitPitch[j] = v[k][(size_t) j];
    if (!withEntry) for (int j = 0; j < n; ++j) s.entryPitch[j] = s.pitch[0][j];
    if (!withExit && !s.loop()) for (int j = 0; j < n; ++j) s.exitPitch[j] = s.pitch[s.nSlots - 1][j];

    for (int i = 0; i < kMaxSlots; ++i) for (int j = 0; j < kMaxVoices; ++j) { s.link[i][j] = Link(); s.link[i][j].to = j; }
    for (int j = 0; j < kMaxVoices; ++j) { s.entryLink[j] = Link(); s.entryLink[j].to = j; }

    for (auto& c : chain) for (int pc : chordTones(c, 4)) s.scaleMask |= 1 << pc;
    s.edited = false;
}

// ---------------- edits ----------------
// connect voice v of transition t to voice target; whatever pointed there takes v's old target (one-to-one)
inline void setLink(Score& s, int t, int v, int target)
{
    int other = -1;
    for (int i = 0; i < s.nVoices; ++i) if (s.linkAt(t, i).to == target) other = i;
    if (other < 0 || other == v) return;
    std::swap(s.linkAt(t, v).to, s.linkAt(t, other).to);
    s.edited = true;
}

// ---------------- playing ----------------
inline float glideShape(float x, float curve)
{
    x = std::clamp(x, 0.f, 1.f);
    const float p = std::pow(2.f, 2.6f * std::clamp(curve, -1.f, 1.f));   // 0.16 .. 6: a curve you can see and hear
    const float xp = std::pow(x, p);
    return 0.25f * xp + 0.75f * xp * xp * (3.f - 2.f * xp);
}

// pitch between a and b at x (0..1) for the glide mode
inline float glideValue(float a, float b, float x, float curve, int mode, int scaleMask)
{
    const float y = glideShape(x, curve);
    const float p = a + (b - a) * y;
    if (mode == Steps || a == b) return mode == Steps ? a + std::round(p - a) : p;
    if (mode == Robot)
    {
        // the nearest note of the song's scale, never past the target
        const int lo = (int) std::min(a, b), hi = (int) std::max(a, b);
        float best = p, bd = 1e9f;
        for (int m = lo; m <= hi; ++m)
            if ((scaleMask >> (((m % 12) + 12) % 12)) & 1)
            {
                const float d = std::abs((float) m - p);
                if (d < bd) { bd = d; best = (float) m; }
            }
        return best;
    }
    return p;
}

struct LaneState { float pitch = 60; long long noteId = 0; bool continuing = false; };

// beats each slot lasts
inline double slotBeats(const Score& s, int i, double barBeats) { return (s.start[i + 1] - s.start[i]) * barBeats; }

// which voice of slot i lane k is in, in cycle c
inline int laneSlot(const Score& s, int lane, long long c, int i)
{
    const int n = s.nSlots;
    long long T = s.loop() ? (((c * n + i) % ((long long) n * 12)) + (long long) n * 12) % ((long long) n * 12) : i;
    int v = lane;
    for (long long t = 0; t < T; ++t) v = s.link[t % n][v].to;
    return v;
}

// where lane k is at beat position u (already shifted by the lane's spread delay)
inline LaneState evalLane(const Score& s, int lane, double u, double barBeats, int mode)
{
    const int n = s.nSlots;
    const double L = kBars * barBeats;
    const long long c = (long long) std::floor(u / L);
    const double local = u - (double) c * L;
    int i = 0;
    while (i < n - 1 && local >= s.start[i + 1] * barBeats) ++i;
    const double Ts = s.start[i] * barBeats, Te = s.start[i + 1] * barBeats;
    const double lenI = Te - Ts;

    const int v = laneSlot(s, lane, c, i);
    const float cur = (float) s.pitch[i][v];
    LaneState st;
    st.pitch = cur;

    // out of this slot
    const Link& lo = s.link[i][v];
    bool outOk = lo.glide && s.hasTarget(i);
    float nxt = cur;
    double nextLen = barBeats;
    if (outOk)
    {
        if (i < n - 1) { nxt = (float) s.pitch[i + 1][lo.to]; nextLen = slotBeats(s, i + 1, barBeats); }
        else if (s.loop()) { nxt = (float) s.pitch[0][lo.to]; nextLen = slotBeats(s, 0, barBeats); }
        else nxt = (float) s.exitPitch[lo.to];
    }
    // into this slot
    bool inOk = false;
    float prv = cur;
    const Link* li = nullptr;
    double prevLen = barBeats;
    if (i > 0)
    {
        const int pv = laneSlot(s, lane, c, i - 1);
        li = &s.link[i - 1][pv]; prv = (float) s.pitch[i - 1][pv]; prevLen = slotBeats(s, i - 1, barBeats);
        inOk = li->glide;
    }
    else if (s.loop())
    {
        const int pv = laneSlot(s, lane, c - 1, n - 1);
        li = &s.link[n - 1][pv]; prv = (float) s.pitch[n - 1][pv]; prevLen = slotBeats(s, n - 1, barBeats);
        inOk = li->glide;
    }
    else if (s.entryMode == ToChord)
    {
        for (int e = 0; e < s.nVoices; ++e) if (s.entryLink[e].to == v) { li = &s.entryLink[e]; prv = (float) s.entryPitch[e]; }
        inOk = li != nullptr && li->glide;
    }

    bool gliding = false;
    if (outOk)
    {
        const double e = std::min((double) lo.early, lenI * 0.5), l = std::min((double) lo.late, nextLen * 0.5);
        if (e + l > 1e-6 && local >= Te - e)
        {
            st.pitch = glideValue(cur, nxt, (float) ((local - (Te - e)) / (e + l)), lo.curve, mode, s.scaleMask);
            gliding = true;
        }
    }
    if (inOk && !gliding)
    {
        const double e = std::min((double) li->early, prevLen * 0.5), l = std::min((double) li->late, lenI * 0.5);
        if (e + l > 1e-6 && local < Ts + l)
            st.pitch = glideValue(prv, cur, (float) ((local - (Ts - e)) / (e + l)), li->curve, mode, s.scaleMask);
    }

    // the note began at the last change not reached by a glide
    if (s.loop())
    {
        long long T = c * n + i;       // global slot index
        for (int k = 0; k < n * 12; ++k)
        {
            const long long pT = T - 1;
            const long long pc = (long long) std::floor((double) pT / n);
            const int pi = (int) (pT - pc * n);
            if (!s.link[pi][laneSlot(s, lane, pc, pi)].glide) break;
            T = pT;
            if (k == n * 12 - 1) T = -1000000;
        }
        st.noteId = T;
    }
    else
    {
        int j = i;
        while (j > 0 && s.link[j - 1][laneSlot(s, lane, c, j - 1)].glide) --j;
        st.noteId = c * 1000 + j;
        st.continuing = j == 0 && inOk;     // came in gliding from the chord before the pattern: no attack
    }
    return st;
}
} // namespace rc
