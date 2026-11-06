// load sustained-note WAVs into SAMPLE, print the zones, render the pattern, look for clicks at the loop seams
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
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
    for (auto& z : proc.sampleSet->zones) std::printf("  %-16s root %.2f (%s) loop %d samples = %.2f s\n", z.name.c_str(), z.root, z.rootFromName ? "name" : "detected", z.len, z.len / z.rate);
    // seam check on level 0: jump across the wrap vs typical step
    for (auto& z : proc.sampleSet->zones)
    {
        const auto& v = z.lv[0][0]; double avg = 0; for (int i = 1; i < z.len; ++i) avg += std::abs(v[(size_t) i] - v[(size_t) i - 1]); avg /= z.len;
        std::printf("  seam %s: step %.4f vs mean %.4f\n", z.name.c_str(), std::abs(v[0] - v[(size_t) z.len - 1]), avg);
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
