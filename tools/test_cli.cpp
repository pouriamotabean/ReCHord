// ReChord engine test: rechord_test out.wav [bpm] ; prints voicings, links and the pitch path of each lane
#include "../Source/Engine.h"
#include <cstdio>
#include <cstdint>

static void writeWav(const char* path, const std::vector<float>& L, const std::vector<float>& R, int sr)
{
    FILE* f = std::fopen(path, "wb");
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

int main(int argc, char** argv)
{
    const char* out = argc > 1 ? argv[1] : "test.wav";
    const double bpm = argc > 2 ? std::atof(argv[2]) : 100;
    const int nv = argc > 3 ? std::atoi(argv[3]) : 3;
    rc::Chord ch[4] = { { 9, rc::Min }, { 7, rc::Maj }, { 5, rc::Maj }, { 4, rc::Min } };
    if (argc > 4) { ch[0] = { 0, rc::Maj7 }; ch[1] = { 9, rc::Min7 }; ch[2] = { 2, rc::Min7 }; ch[3] = { 7, rc::Dom7 }; }
    rc::Score s;
    rc::smartVoicing(s, ch, nv, 0);
    for (int b = 0; b < 4; ++b)
    {
        std::printf("%-6s", rc::chordName(ch[b].root, ch[b].quality).c_str());
        for (int v = 0; v < s.nVoices; ++v) std::printf(" %-4s->%d", rc::noteName(s.pitch[b][v]).c_str(), s.perm[b][v]);
        std::printf("\n");
    }
    rc::Params p;
    // pitch path of each lane around the bar line between bar 1 and 2 (beats 3..5)
    for (int k = 0; k < s.nVoices; ++k)
    {
        std::printf("lane %d:", k);
        for (double u = 3.0; u <= 4.6; u += 0.1)
            std::printf(" %.2f", rc::evalLane(s, k, u, 4, p.early, p.late, p.curve).pitch);
        std::printf("\n");
    }
    std::vector<float> L, R;
    rc::renderLoop(s, p, 48000, bpm, 4, L, R);
    float peak = 0;
    for (size_t i = 0; i < L.size(); ++i) peak = std::max(peak, std::max(std::abs(L[i]), std::abs(R[i])));
    std::printf("samples %zu  peak %.3f\n", L.size(), peak);
    writeWav(out, L, R, 48000);
    // a second version: one link set to END to hear a fresh note
    s.glide[1][2] = false;
    rc::renderLoop(s, p, 48000, bpm, 4, L, R);
    std::string o2 = std::string(out) + ".end.wav";
    writeWav(o2.c_str(), L, R, 48000);
    return 0;
}
