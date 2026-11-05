#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Presets.h"

#ifndef RECHORD_VERSION
 #define RECHORD_VERSION "1.4.0"
#endif

class ReChordProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    ReChordProcessor();
    ~ReChordProcessor() override { stopTimer(); }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override
    { return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo(); }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "ReChord"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 3.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- the model (message thread) ----
    rc::Pattern pattern;
    rc::Score score;
    void smart();                                            // fresh smart voicing + default links (drops edits)
    // key: 0..7 = bar*2 + half, -1 = the chord before the pattern, -2 = the chord after it
    rc::Chord& chordFor(int key) { return key == -1 ? pattern.entry : key == -2 ? pattern.exit : pattern.chord[key]; }
    void setChord(int key, int root, int quality);
    void setSplit(int bar, bool on);
    void setEnd(int which, int mode);                        // which: -1 entry, -2 exit
    // edits of one link: t = transition (0..nSlots-1 after slot t, -1 entry -> slot 1), v = voice
    void linkTo(int t, int v, int target) { rc::setLink(score, t, v, target); publish(); }
    void toggleGlide(int t, int v) { auto& l = score.linkAt(t, v); l.glide = !l.glide; score.edited = true; publish(); }
    void setTiming(int t, int v, float early, float late, float curve);
    void resetTiming(int t, int v) { setTiming(t, v, rc::Link().early, rc::Link().late, rc::Link().curve); }
    void timingToAll(int t, int v);
    bool moveOctave(int col, int v, int dir);                // col: slot, -1 entry, nSlots exit
    std::atomic<int> modelVersion { 0 };

    rc::Params currentParams() const;
    int transpose() const { return (int) apvts.getRawParameterValue("transpose")->load(); }
    juce::String slotName(int i) const;                      // chord name of slot i (transposed)

    juce::File renderWav();

    // presets (sound only)
    int presetIndex = 0;
    void loadPreset(int i);
    bool presetModified() const;

    std::atomic<double> uiBeat { -1.0 };                    // < 0: not playing
    std::atomic<double> hostBpm { 120.0 }, hostBarBeats { 4.0 };

private:
    void timerCallback() override;
    void publish();

    rc::Engine engine;
    juce::SpinLock scoreLock;
    rc::Score audioScore;
    bool scoreDirty = true;
    double freeBeat = 0;
    int lastVoices = 3, lastOctave = 0;

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReChordProcessor)
};
