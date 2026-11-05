// Linux only: open the editor offscreen, simulate a few edits, save PNGs + a rendered WAV
#include "../Source/PluginEditor.h"
int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    ReChordProcessor proc;
    proc.prepareToPlay(48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(960, 600);
    auto snap = [&](const juce::String& name)
    {
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.5f);
        juce::File f(outDir + "/" + name);
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat().writeImageToStream(img, os);
    };
    // fake playhead position for the screenshot
    proc.uiBeat = 5.2;
    snap("ui_default.png");
    // edits: swap two links in bar 2, set one link to END, move a note an octave
    proc.linkTo(1, 0, 1);
    proc.toggleMode(2, 2);
    proc.moveOctave(3, 0, -1);
    snap("ui_edited.png");
    // chord change + 4 voices
    proc.setChord(0, 0, rc::Maj7);
    proc.apvts.getParameter("voices")->setValueNotifyingHost(1.f);
    juce::Thread::sleep(200);
    proc.smart();
    snap("ui_4voices.png");
    // state round trip
    juce::MemoryBlock mb; proc.getStateInformation(mb);
    ReChordProcessor p2; p2.setStateInformation(mb.getData(), (int) mb.getSize());
    std::printf("state ok: %d voices, chord1 %s, glide[2][2]=%d\n", p2.score.nVoices,
                rc::chordName(p2.chords[0].root, p2.chords[0].quality).c_str(), (int) p2.score.glide[2][2]);
    auto f = proc.renderWav();
    std::printf("wav: %s (%lld bytes)\n", f.getFullPathName().toRawUTF8(), (long long) f.getSize());
    ed.reset();
    return 0;
}
