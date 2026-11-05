// renders every preset on the default pattern: loudness, peak, clicks; writes one WAV each + a demo of all
#include "../Source/Presets.h"
#include <cstdio>
#include <cstdint>
#include <string>

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

int main(int argc, char** argv)
{
    const std::string out = argc > 1 ? argv[1] : ".";
    const int sr = 48000;
    rc::Pattern pat;
    rc::Score s;
    rc::buildScore(s, pat, argc > 3 ? std::atoi(argv[3]) : 3, argc > 2 ? std::atoi(argv[2]) : 0);
    std::vector<float> demoL, demoR;
    for (int i = 0; i < rc::kNumPresets; ++i)
    {
        const auto& ps = rc::kPresets[i];
        rc::Params p = rc::applyPreset(rc::Params(), ps);
        std::vector<float> L, R;
        rc::renderPattern(s, p, sr, 90, 4, L, R);
        double sum = 0; float peak = 0, maxd = 0;
        for (size_t k = 0; k < L.size(); ++k)
        {
            sum += L[k] * L[k] + R[k] * R[k];
            peak = std::max(peak, std::max(std::abs(L[k]), std::abs(R[k])));
            if (k > 1) maxd = std::max(maxd, std::abs(L[k] - 2 * L[k - 1] + L[k - 2]));
        }
        const double rms = std::sqrt(sum / (2.0 * (double) L.size()));
        std::printf("%-16s rms %6.1f dB  peak %6.1f dB  max2nd %.4f\n", ps.name, 20 * std::log10(rms + 1e-9), 20 * std::log10(peak + 1e-9), maxd);
        writeWav(out + "/" + std::to_string(i + 1) + " " + ps.name + ".wav", L, R, sr);
        // demo: 2 bars of each, with a short fade
        const size_t take = (size_t) (sr * 60.0 / 90 * 8);
        for (size_t k = 0; k < take && k < L.size(); ++k)
        {
            const float f = std::min(1.f, std::min((float) k / 2400.f, (float) (take - k) / 4800.f));
            demoL.push_back(L[k] * f); demoR.push_back(R[k] * f);
        }
        for (int k = 0; k < sr / 4; ++k) { demoL.push_back(0); demoR.push_back(0); }
    }
    writeWav(out + "/ReChord presets demo.wav", demoL, demoR, sr);
    return 0;
}
