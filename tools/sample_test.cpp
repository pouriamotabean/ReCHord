// load sustained-note WAVs into SAMPLE, print the zones, render the pattern, look for clicks at the loop seams
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#define private public
#include "../Source/Presets.h"
#define private public
#include "../Source/PluginProcessor.h"
#undef private
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    ReChordProcessor proc;
    proc.setRateAndBufferSizeDetails(48000, 512); proc.prepareToPlay(48000, 512);
    juce::StringArray files; for (int i = 1; i < argc; ++i) files.add(argv[i]);
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    const int n = proc.loadSamples(files);
    std::printf("loaded %d zones in %.0f ms, osc %.0f, label '%s' / '%s'\n", n, juce::Time::getMillisecondCounterHiRes() - t0,
                proc.apvts.getRawParameterValue("osc")->load(), proc.sampleLabel().toRawUTF8(), proc.sampleDetail().toRawUTF8());
    for (auto& z : proc.sampleSet->zones) std::printf("  %-16s root %.2f loop %d samples = %.2f s\n", z.name.c_str(), z.root, z.len, z.len / z.rate);
    for (auto& s : proc.samples) std::printf("  %-16s detected %.2f name pc %d oct %d  part %.2f-%.2f (auto %.2f-%.2f) loop %.2f-%.2f\n", s.name.toRawUTF8(), s.info.detected, s.info.namePc, s.info.nameOct, s.info.a, s.info.b, s.info.autoA, s.info.autoB, s.info.loopA, s.info.loopB);
    // stretch x2 and cycle on the first file
    if (argc > 1 && std::getenv("STRETCH")) { proc.samples[0].ed.stretch = (float) atof(std::getenv("STRETCH")); }
    if (argc > 1 && std::getenv("CYCLE")) { proc.samples[0].ed.mode = 1; }
    proc.rebuildSamples();
    std::printf("  after edits: loop %.3f s root %.2f\n", proc.sampleSet->zones[0].len / proc.sampleSet->zones[0].rate, proc.sampleSet->zones[0].root);
    {
        // the loop itself, played 3 times: level per 20 ms, and the worst jump at the seams
        const auto& z = proc.sampleSet->zones[0];
        const int n = z.len, w = (int) (z.rate * 0.02);
        float mn = 1e9f, mx = -1e9f;
        for (int s0 = 0; s0 + w <= 3 * n; s0 += w / 2)
        {
            double e = 0; for (int j = 0; j < w; ++j) { const float v = z.lv[0][0][(size_t) ((s0 + j) % n)]; e += v * v; }
            const float d = (float) (10 * std::log10(e / w + 1e-12)); mn = std::min(mn, d); mx = std::max(mx, d);
        }
        std::printf("  loop level over 3 rounds: swing %.2f dB\n", mx - mn);
    }
    {
        // one held note (the PLAY of the page), dry: the level over 4 s, 50 ms steps
        proc.apvts.getParameter("space")->setValueNotifyingHost(0.f);
        proc.apvts.getParameter("listen")->setValueNotifyingHost(0.f);
        proc.apvts.getParameter("warmth")->setValueNotifyingHost(0.f);
        proc.audition = (int) std::lround(proc.sampleSet->zones[0].root);
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer mb;
        std::vector<float> lv; double acc = 0; int cnt = 0;
        for (int blk = 0; blk < 48000 * 5 / 512; ++blk)
        {
            proc.processBlock(buf, mb);
            for (int i = 0; i < 512; ++i) { { const float m = 0.5f * (buf.getSample(0, i) + buf.getSample(1, i)); acc += m * m; } if (++cnt == 2400) { lv.push_back((float) (10 * std::log10(acc / cnt + 1e-12))); acc = 0; cnt = 0; } }
        }
        float mn = 1e9f, mx = -1e9f; for (size_t i = 20; i < lv.size(); ++i) { mn = std::min(mn, lv[i]); mx = std::max(mx, lv[i]); }
        for (size_t i = 0; i < lv.size(); i += 4) std::printf("%.0f ", lv[i]); std::printf("\n");
        std::printf("  engine zone len %d, osc %d\n", proc.engine.sample ? proc.engine.sample->zones[0].len : -1, proc.engine.prm.osc);
        std::printf("  held note 1-5 s: level %.1f .. %.1f dB (swing %.1f dB)\n", mn, mx, mx - mn);
        proc.audition = -1;
    }
    // seam check on level 0: jump across the wrap vs typical step
    for (auto& z : proc.sampleSet->zones)
    {
        const auto& v = z.lv[0][0]; double avg = 0; for (int i = 1; i < z.len; ++i) avg += std::abs(v[(size_t) i] - v[(size_t) i - 1]); avg /= z.len;
        std::printf("  seam %s: step %.4f vs mean %.4f\n", z.name.c_str(), std::abs(v[0] - v[(size_t) z.len - 1]), avg);
    }
    if (std::getenv("STRESS"))
    {
        // the edits a user makes, one after another, with a note playing through the audio thread
        proc.apvts.getParameter("listen")->setValueNotifyingHost(1.f);
        juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer mb;
        auto play = [&](int blocks) { for (int b = 0; b < blocks; ++b) proc.processBlock(buf, mb); };
        auto& ed = proc.samples[0].ed;
        for (int mode = 0; mode < 2; ++mode)
            for (float st = 1.f; st <= 4.01f; st += 0.25f)
                for (float a : { -1.f, 0.05f, 0.3f, 0.7f })
                {
                    ed.mode = mode; ed.stretch = st; ed.a = a; ed.b = a < 0 ? -1 : std::min(1.1, a + 0.12 + st * 0.05);
                    const double t0 = juce::Time::getMillisecondCounterHiRes();
                    proc.rebuildSamples();
                    const double t1 = juce::Time::getMillisecondCounterHiRes();
                    proc.audition = 60 + (int) (st * 3);
                    play(20);
                    const double t2 = juce::Time::getMillisecondCounterHiRes();
                    if (t1 - t0 > 50 || t2 - t1 > 100) std::printf("  slow: mode %d st %.2f a %.2f  rebuild %.0f ms, audio %.0f ms\n", mode, st, a, t1 - t0, t2 - t1);
                    if (proc.sampleSet == nullptr) std::printf("  no set: mode %d st %.2f a %.2f\n", mode, st, a);
                }
        std::printf("stress done\n");
        proc.audition = -1;
    }
    {
        proc.samples[0].ed.stretch = 1.75f; proc.samples[0].ed.mode = 1; proc.rebuildSamples();
        const bool ok = proc.saveUserPreset("Test Zen");
        ReChordProcessor p3; p3.setRateAndBufferSizeDetails(48000, 512);
        const bool ok2 = p3.loadUserPreset(ReChordProcessor::presetDir().getChildFile("Test Zen.rcpreset"));
        std::printf("user preset: saved %d loaded %d name '%s' osc %.0f samples %d stretch %.2f mode %d zones %d\n", (int) ok, (int) ok2, p3.userPreset.toRawUTF8(),
                    p3.apvts.getRawParameterValue("osc")->load(), (int) p3.samples.size(), p3.samples.empty() ? 0.f : p3.samples[0].ed.stretch,
                    p3.samples.empty() ? -1 : p3.samples[0].ed.mode, p3.sampleSet ? (int) p3.sampleSet->zones.size() : 0);
        proc.samples[0].ed = rc::SampleEdit(); proc.rebuildSamples();
    }
    // the state round trip
    juce::MemoryBlock mb; proc.getStateInformation(mb);
    ReChordProcessor p2; p2.setRateAndBufferSizeDetails(48000, 512); p2.setStateInformation(mb.getData(), (int) mb.getSize());
    std::printf("state: %d KB, reloaded %d zones, osc %.0f\n", (int) (mb.getSize() / 1024), p2.sampleSet ? (int) p2.sampleSet->zones.size() : 0, p2.apvts.getRawParameterValue("osc")->load());
    // the loop knob
    proc.apvts.getParameter("character")->setValueNotifyingHost(1.f); proc.rebuildSamples();
    std::printf("LOOP long: %.2f s;  ", proc.sampleSet->zones[0].len / proc.sampleSet->zones[0].rate);
    proc.apvts.getParameter("character")->setValueNotifyingHost(0.f); proc.rebuildSamples();
    std::printf("short: %.2f s\n", proc.sampleSet->zones[0].len / proc.sampleSet->zones[0].rate);
    proc.apvts.getParameter("character")->setValueNotifyingHost(0.5f); proc.rebuildSamples();
    auto f = proc.renderWav();
    std::printf("render: %s\n", f.getFullPathName().toRawUTF8());
    // octave +2 render too (aliasing check by ear)
    proc.apvts.getParameter("octave")->setValueNotifyingHost(proc.apvts.getParameter("octave")->convertTo0to1(2.f));
    auto f2 = proc.renderWav();
    std::printf("render +2 oct: %s\n", f2.getFullPathName().toRawUTF8());
}
