// Linux only: open the editor offscreen, make a few edits, save PNGs, check state + render
#include "../Source/PluginEditor.h"

template <class T> static T* findChild(juce::Component* c)
{
    if (auto* t = dynamic_cast<T*>(c)) return t;
    for (auto* ch : c->getChildren()) if (auto* t = findChild<T>(ch)) return t;
    return nullptr;
}

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    const juce::String outDir = argc > 1 ? argv[1] : ".";
    ReChordProcessor proc;
    proc.prepareToPlay(48000, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    ed->setSize(960, 600);
    auto* dev = findChild<Device>(ed.get());
    auto* view = findChild<LinkView>(ed.get());
    auto* strip = findChild<ChordStrip>(ed.get());
    auto snap = [&](const juce::String& name)
    {
        dev->refresh();
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.5f);
        juce::File f(outDir + "/" + name);
        f.deleteFile();
        juce::FileOutputStream os(f);
        juce::PNGImageFormat().writeImageToStream(img, os);
    };
    proc.uiBeat = 5.2;
    snap("a_default.png");

    // half bar with Dm in bar 2, one glide with its own timing, selected
    proc.setSplit(1, true);
    proc.setChord(3, 2, rc::Min);
    proc.setTiming(0, 2, 1.5f, 0.75f, -0.7f);
    proc.setTiming(2, 0, 0.25f, 0.5f, 0.8f);
    proc.toggleGlide(3, 1);
    view->setSel(0, 2);
    snap("b_halfbar_edited.png");

    // open pattern: ends on Dm, starts fresh; picker open on AFTER
    proc.chordFor(-2) = { 2, rc::Min };
    proc.setEnd(-2, rc::ToChord);
    view->setSel(-2, -1);
    strip->onClick(-2);
    snap("c_to_Dm_picker.png");
    strip->onClick(-99);

    // steps mode + 4 voices + double
    proc.apvts.getParameter("mode")->setValueNotifyingHost(0.5f);
    proc.apvts.getParameter("voices")->setValueNotifyingHost(1.f);
    proc.apvts.getParameter("double")->setValueNotifyingHost(0.6f);
    proc.smart();
    snap("d_steps_4v.png");

    juce::MemoryBlock mb; proc.getStateInformation(mb);
    ReChordProcessor p2; p2.setStateInformation(mb.getData(), (int) mb.getSize());
    std::printf("state: %d slots, %d voices, exit %d, entry %d, edited %d, mode %.0f\n", p2.score.nSlots, p2.score.nVoices,
                p2.score.exitMode, p2.score.entryMode, (int) p2.score.edited, p2.apvts.getRawParameterValue("mode")->load());
    proc.setTiming(1, 1, 1.f, 0.5f, 0.5f);
    proc.getStateInformation(mb);
    ReChordProcessor p3; p3.setStateInformation(mb.getData(), (int) mb.getSize());
    std::printf("edited link kept: early %.2f late %.2f curve %.2f edited %d\n", p3.score.link[1][1].early, p3.score.link[1][1].late, p3.score.link[1][1].curve, (int) p3.score.edited);
    auto f = proc.renderWav();
    std::printf("wav: %s (%lld bytes)\n", f.getFullPathName().toRawUTF8(), (long long) f.getSize());
    ed.reset();
    return 0;
}
