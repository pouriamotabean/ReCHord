// simulate Cubase playing chords into ReChord while LEARN is on
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "../Source/Presets.h"
#define private public
#include "../Source/PluginProcessor.h"
#undef private
struct Head : juce::AudioPlayHead
{
    double ppq = 0; bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    { PositionInfo p; p.setIsPlaying(playing); p.setBpm(120.0); p.setPpqPosition(ppq); p.setTimeSignature(TimeSignature { 4, 4 }); return p; }
};
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const double startBar = argc > 1 ? atof(argv[1]) : 0.0;   // where play starts (bars)
    const int bs = argc > 2 ? atoi(argv[2]) : 512;
    ReChordProcessor proc;
    const double sr = 48000; proc.setRateAndBufferSizeDetails(sr, bs); proc.prepareToPlay(sr, bs);
    Head head; proc.setPlayHead(&head);
    // chords: C Am F G (to differ from the default), one per bar, 8 bars
    const int ch[4][3] = { { 60, 64, 67 }, { 57, 60, 64 }, { 53, 57, 60 }, { 55, 59, 62 } };
    struct Ev { double beat; int key; bool on; };
    std::vector<Ev> evs;
    for (int b = 0; b < 16; ++b) for (int k : ch[b % 4]) { evs.push_back({ b * 4.0, k, true }); evs.push_back({ b * 4.0 + 3.9, k, false }); }
    proc.startLearn();
    const double spb = sr * 60.0 / 120.0;
    head.ppq = startBar * 4.0;
    juce::AudioBuffer<float> buf(2, bs);
    for (int blk = 0; blk < 20000 && proc.learnState.load() != 3; ++blk)
    {
        juce::MidiBuffer mb;
        const double b0 = head.ppq, b1 = head.ppq + bs / spb;
        for (auto& e : evs) if (e.beat >= b0 && e.beat < b1)
            mb.addEvent(e.on ? juce::MidiMessage::noteOn(1, e.key, (juce::uint8) 100) : juce::MidiMessage::noteOff(1, e.key), (int) ((e.beat - b0) * spb));
        proc.processBlock(buf, mb);
        head.ppq = b1;
    }
    std::printf("state %d, events %d\n", proc.learnState.load(), proc.learnCount.load());
    for (int i = 0; i < std::min(8, proc.learnCount.load()); ++i) std::printf("  %.3f %d %d\n", proc.learnEvents[i].beat, proc.learnEvents[i].key, (int) proc.learnEvents[i].on);
    proc.finishLearn();
    for (int i = 0; i < proc.score.nSlots; ++i) std::printf("%s ", proc.slotName(i).toRawUTF8());
    std::printf("\n");
}
