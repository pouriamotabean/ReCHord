#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include "Engine.h"

#ifndef RECHORD_VERSION
 #define RECHORD_VERSION "1.0.0"
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
    double getTailLengthSeconds() const override { return 2.5; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;

    // ---- the model (message thread) ----
    rc::Chord chords[rc::kBars] = { { 9, rc::Min }, { 7, rc::Maj }, { 5, rc::Maj }, { 4, rc::Min } };
    rc::Score score;
    void setChord(int bar, int root, int quality);
    void smart();                                  // fresh smart voicing + default links (drops edits)
    void linkTo(int bar, int slot, int target) { rc::setLink(score, bar, slot, target); publish(); }
    void toggleMode(int bar, int slot) { score.glide[bar][slot] = !score.glide[bar][slot]; publish(); }
    bool moveOctave(int bar, int slot, int dir);
    std::atomic<int> modelVersion { 0 };

    rc::Params currentParams() const;
    int transpose() const { return (int) apvts.getRawParameterValue("transpose")->load(); }

    // the 4 bars as a looping WAV at the project tempo; returns the file
    juce::File renderWav();

    // transport, for the display
    std::atomic<bool> preview { false };
    std::atomic<double> uiBeat { -1.0 };     // < 0: not playing
    std::atomic<double> hostBpm { 120.0 }, hostBarBeats { 4.0 };

private:
    void timerCallback() override;
    void publish();

    rc::Engine engine;
    juce::SpinLock scoreLock;
    rc::Score audioScore;
    bool scoreDirty = true;
    double previewBeat = 0;
    int lastVoices = 3, lastOctave = 0;

    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReChordProcessor)
};
