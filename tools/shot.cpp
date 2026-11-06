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
    ed->setSize(960, 654);
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
    proc.loadPreset(36);
    view->setSel(1, 1);
    snap("b_strings.png");
    box->onOpen();
    snap("c_presets.png");
    box->onOpen();
    proc.loadPreset(25);
    proc.apvts.getParameter("character")->setValueNotifyingHost(0.66f);
    std::printf("modified after tweak: %d\n", (int) proc.presetModified());
    snap("d_voice_tweaked.png");

    // a dropped MIDI file
    if (argc > 2)
    {
        const bool ok = proc.loadMidiFile(juce::File(argv[2]));
        std::printf("midi file: %d  slots %d voices %d edited %d\n", (int) ok, proc.score.nSlots, proc.score.nVoices, (int) proc.score.edited);
        for (int i = 0; i < proc.score.nSlots; ++i)
        {
            std::printf("  %.1f %-6s", proc.score.start[i], proc.slotName(i).toRawUTF8());
            for (int v = 0; v < proc.score.nVoices; ++v) std::printf(" %s", rc::noteName(proc.score.pitch[i][v]).c_str());
            std::printf("\n");
        }
        proc.apvts.getParameter("octave")->setValueNotifyingHost(proc.apvts.getParameter("octave")->convertTo0to1(-2.f));
        proc.learnState = 2; proc.learnPos = 1.4;
        snap("e_midi_octave.png");
        proc.learnState = 0;
    }
    // 8 bars, second page
    for (int b = 4; b < 8; ++b) proc.setBarOn(b, true);
    proc.setBarOn(6, false);   // 7 bars: bar 7 off
    proc.setChord(8, 0, rc::Maj);
    view->page = 1; if (view->onPage) view->onPage();
    view->setSel(4, 1);
    snap("f_8bars_page2.png");
    view->page = 0; if (view->onPage) view->onPage();
    snap("g_8bars_page1.png");
    if (auto* tabs = findChild<PanelTabs>(ed.get()))
    {
        tabs->current = 1; tabs->onChange(1); snap("h_tab_glide.png");
        tabs->current = 2; tabs->onChange(2); snap("i_tab_key.png");
        tabs->current = 0; tabs->onChange(0);
    }
    // a note moved a semitone: F -> F#
    proc.movePitch(4, 1, 1);
    std::printf("slot 4 label after a semitone: %s\n", proc.slotLabel(4).toRawUTF8());
    snap("j_semitone.png");
    proc.undo();
    std::printf("after undo: %s  (can redo %d)\n", proc.slotLabel(4).toRawUTF8(), (int) proc.canRedo());
    proc.redo();
    std::printf("after redo: %s\n", proc.slotLabel(4).toRawUTF8());
    proc.applyMove(1);
    std::printf("move BASS FIRST: link0 v0 early %.2f late %.2f, v2 early %.2f late %.2f\n", proc.score.linkAt(0, 0).early, proc.score.linkAt(0, 0).late, proc.score.linkAt(0, 2).early, proc.score.linkAt(0, 2).late);
    if (auto* tabs = findChild<PanelTabs>(ed.get())) { tabs->current = 1; tabs->onChange(1); snap("k_glide_move.png"); tabs->current = 0; tabs->onChange(0); }
    std::printf("8 bars: slots %d bars %d, render ", proc.score.nSlots, proc.score.nBars);
    {
        auto f8 = proc.renderWav();
        std::printf("%lld bytes\n", (long long) f8.getSize());
    }
    juce::MemoryBlock mb; proc.getStateInformation(mb);
    ReChordProcessor p2; p2.setStateInformation(mb.getData(), (int) mb.getSize());
    std::printf("state: preset %d osc %.0f character %.2f\n", p2.presetIndex, p2.apvts.getRawParameterValue("osc")->load(), p2.apvts.getRawParameterValue("character")->load());
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    auto f = proc.renderWav();
    std::printf("wav: %s (%lld bytes) in %.0f ms\n", f.getFullPathName().toRawUTF8(), (long long) f.getSize(), juce::Time::getMillisecondCounterHiRes() - t0);
    ed.reset();
    return 0;
}
