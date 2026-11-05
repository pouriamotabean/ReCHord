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
    auto* box = findChild<PresetBox>(ed.get());
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
    std::printf("modified at start: %d\n", (int) proc.presetModified());
    snap("a_default.png");
    proc.loadPreset(4);
    view->setSel(1, 1);
    snap("b_strings.png");
    box->onOpen();
    snap("c_presets.png");
    box->onOpen();
    proc.loadPreset(7);
    proc.apvts.getParameter("character")->setValueNotifyingHost(0.66f);
    std::printf("modified after tweak: %d\n", (int) proc.presetModified());
    snap("d_voice_tweaked.png");

    juce::MemoryBlock mb; proc.getStateInformation(mb);
    ReChordProcessor p2; p2.setStateInformation(mb.getData(), (int) mb.getSize());
    std::printf("state: preset %d osc %.0f character %.2f\n", p2.presetIndex, p2.apvts.getRawParameterValue("osc")->load(), p2.apvts.getRawParameterValue("character")->load());
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto f = proc.renderWav();
    std::printf("wav: %s (%lld bytes) in %.0f ms\n", f.getFullPathName().toRawUTF8(), (long long) f.getSize(), juce::Time::getMillisecondCounterHiRes() - t0);
    ed.reset();
    return 0;
}
