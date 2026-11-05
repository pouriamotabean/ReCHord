// ReChord engine test (no JUCE): rechord_test outdir
// renders several patterns and prints voicings + the pitch path around changes
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdint>

static void writeWav(const std::string& path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen(path.c_str(), "wb");
    const uint32_t n = (uint32_t) L.size(), data = n * 4;
    auto u32 = [&](uint32_t v) { std::fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { std::fwrite(&v, 2, 1, f); };
    std::fwrite("RIFF", 1, 4, f); u32(36 + data); std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16); u16(1); u16(2); u32((uint32_t) sr); u32((uint32_t) sr * 4); u16(4); u16(16);
    std::fwrite("data", 1, 4, f); u32(data);
    for (uint32_t i = 0; i < n; ++i)
    {
        int16_t a = (int16_t) std::lround(std::clamp(L[i], -1.f, 1.f) * 32767), b = (int16_t) std::lround(std::clamp(R[i], -1.f, 1.f) * 32767);
        std::fwrite(&a, 2, 1, f); std::fwrite(&b, 2, 1, f);
    }
    std::fclose(f);
}

static void show(const rc::Score& s, const rc::Pattern& p)
{
    if (!s.loop() && s.entryMode == rc::ToChord)
    {
        std::printf("  entry %-6s", rc::chordName(p.entry.root, p.entry.quality).c_str());
        for (int v = 0; v < s.nVoices; ++v) std::printf(" %s->%d", rc::noteName(s.entryPitch[v]).c_str(), s.entryLink[v].to);
        std::printf("\n");
    }
    for (int i = 0; i < s.nSlots; ++i)
    {
        const auto& c = p.chord[s.key[i]];
        std::printf("  %.1f %-6s", s.start[i], rc::chordName(c.root, c.quality).c_str());
        for (int v = 0; v < s.nVoices; ++v) std::printf(" %s->%d%s", rc::noteName(s.pitch[i][v]).c_str(), s.link[i][v].to, s.link[i][v].glide ? "" : "x");
        std::printf("\n");
    }
    if (!s.loop() && s.exitMode == rc::ToChord)
    {
        std::printf("  exit  %-6s", rc::chordName(p.exit.root, p.exit.quality).c_str());
        for (int v = 0; v < s.nVoices; ++v) std::printf(" %s", rc::noteName(s.exitPitch[v]).c_str());
        std::printf("\n");
    }
}

int main(int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : ".";
    rc::Params prm;
    struct Case { const char* name; rc::Pattern p; int voices; int mode; float dbl; };
    std::vector<Case> cases;
    { rc::Pattern p; cases.push_back({ "loop_smooth", p, 3, rc::Smooth, 0.f }); }
    { rc::Pattern p; cases.push_back({ "loop_steps", p, 3, rc::Steps, 0.f }); }
    { rc::Pattern p; cases.push_back({ "loop_robot_double", p, 4, rc::Robot, 0.8f }); }
    { rc::Pattern p; p.split[1] = true; p.chord[3] = { 2, rc::Min }; p.split[3] = true; p.chord[7] = { 4, rc::Maj };
      cases.push_back({ "halfbars", p, 3, rc::Smooth, 0.f }); }
    { rc::Pattern p; p.exitMode = rc::ToChord; p.exit = { 2, rc::Min }; p.entryMode = rc::None; cases.push_back({ "A_to_Dm", p, 3, rc::Smooth, 0.f }); }
    { rc::Pattern p; p.exitMode = rc::None; p.entryMode = rc::ToChord; p.entry = { 4, rc::Min };
      p.chord[0] = { 2, rc::Min }; p.chord[2] = { 10, rc::Maj }; p.chord[4] = { 0, rc::Maj }; p.chord[6] = { 9, rc::Min };
      cases.push_back({ "B_from_Em", p, 3, rc::Smooth, 0.f }); }

    for (auto& cs : cases)
    {
        rc::Score s;
        rc::buildScore(s, cs.p, cs.voices, 0);
        std::printf("%s  (slots %d, voices %d)\n", cs.name, s.nSlots, s.nVoices);
        show(s, cs.p);
        prm.mode = cs.mode;
        prm.doubleAmt = cs.dbl;
        std::printf("  lane0 around beat 4:");
        for (double u = 3.3; u <= 4.45; u += 0.1) std::printf(" %.2f", rc::evalLane(s, 0, u, 4, cs.mode).pitch);
        std::printf("\n");
        std::vector<float> L, R;
        rc::renderPattern(s, prm, 48000, 100, 4, L, R);
        float peak = 0;
        for (size_t i = 0; i < L.size(); ++i) peak = std::max(peak, std::max(std::abs(L[i]), std::abs(R[i])));
        std::printf("  %.2f s, peak %.3f\n", L.size() / 48000.0, peak);
        writeWav(out + "/" + cs.name + ".wav", L, R, 48000);
    }
    return 0;
}
