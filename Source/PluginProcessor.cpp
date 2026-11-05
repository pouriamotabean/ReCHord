#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using PID = juce::ParameterID;

static juce::String msText(float v, int) { return v >= 1000.f ? juce::String(v / 1000.f, 2) + " s" : juce::String(juce::roundToInt(v)) + " ms"; }

juce::AudioProcessorValueTreeState::ParameterLayout ReChordProcessor::layout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto fl = [&](const char* id, const char* name, float lo, float hi, float def, float skewMid,
                  std::function<juce::String(float, int)> txt)
    {
        juce::NormalisableRange<float> r(lo, hi);
        if (skewMid > 0) r.setSkewForCentre(skewMid);
        p.push_back(std::make_unique<APF>(PID { id, 1 }, name, r, def,
                                          juce::AudioParameterFloatAttributes().withStringFromValueFunction(txt)));
    };
    p.push_back(std::make_unique<juce::AudioParameterChoice>(PID { "osc", 1 }, "Sound", juce::StringArray { "FM", "Analog", "Strings", "Voice" }, 0));
    fl("character", "Character", 0.f, 1.f, 0.45f, 0, [](float v, int) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
    fl("attack", "Attack", 2.f, 1500.f, 35.f, 120.f, msText);
    fl("bright", "Bright", 0.f, 1.f, 0.45f, 0, [](float v, int) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
    fl("vibrato", "Vibrato", 0.f, 50.f, 16.f, 0, [](float v, int) { return juce::String(juce::roundToInt(v)) + " ct"; });
    fl("release", "Release", 30.f, 4000.f, 380.f, 600.f, msText);
    fl("warmth", "Warmth", 0.f, 1.f, 0.2f, 0, [](float v, int) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
    fl("space", "Space", 0.f, 1.f, 0.2f, 0, [](float v, int) { return v < 0.005f ? juce::String("dry") : juce::String(juce::roundToInt(v * 100)) + "%"; });
    p.push_back(std::make_unique<juce::AudioParameterChoice>(PID { "mode", 1 }, "Glide mode", juce::StringArray { "Smooth", "Steps", "Robot" }, 0));
    fl("spread", "Spread", 0.f, 60.f, 12.f, 0, [](float v, int) { return juce::String(juce::roundToInt(v)) + " ms"; });
    fl("double", "Double", 0.f, 1.f, 0.f, 0, [](float v, int) { return v < 0.005f ? juce::String("off") : juce::String(juce::roundToInt(v * 100)) + "%"; });
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "transpose", 1 }, "Transpose", -12, 12, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return (v > 0 ? "+" : "") + juce::String(v); })));
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "octave", 1 }, "Register", -2, 2, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 0 ? juce::String("mid") : (v > 0 ? "+" : "") + juce::String(v) + " oct"; })));
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "voices", 1 }, "Voices", 3, 4, 3));
    fl("volume", "Volume", -30.f, 6.f, rc::kPresets[0].volumeDb, 0, [](float v, int) { return juce::String(v, 1) + " dB"; });
    p.push_back(std::make_unique<juce::AudioParameterBool>(PID { "listen", 1 }, "Listen", false));
    return { p.begin(), p.end() };
}

ReChordProcessor::ReChordProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "params", layout())
{
    smart();
    startTimerHz(15);
}

rc::Params ReChordProcessor::currentParams() const
{
    auto v = [this](const char* id) { return apvts.getRawParameterValue(id)->load(); };
    rc::Params p;
    p.osc = (int) v("osc");
    p.character = v("character");
    p.warmth = v("warmth");
    p.space = v("space");
    p.attackMs = v("attack");
    p.releaseMs = v("release");
    p.bright = v("bright");
    p.vibratoCents = v("vibrato");
    p.mode = (int) v("mode");
    p.doubleAmt = v("double");
    p.spreadMs = v("spread");
    p.transpose = (int) v("transpose");
    p.volumeDb = v("volume");
    return p;
}

// ---------------- presets: they set only the sound ----------------
static const char* const kSoundIds[] = { "osc", "character", "attack", "bright", "vibrato", "release", "warmth", "space", "double", "volume" };

static float presetValue(const rc::Preset& s, int i)
{
    const float v[] = { (float) s.osc, s.character, s.attackMs, s.bright, s.vibratoCents, s.releaseMs, s.warmth, s.space, s.doubleAmt, s.volumeDb };
    return v[i];
}

void ReChordProcessor::loadPreset(int i)
{
    i = ((i % rc::kNumPresets) + rc::kNumPresets) % rc::kNumPresets;
    presetIndex = i;
    for (int k = 0; k < 10; ++k)
        if (auto* p = apvts.getParameter(kSoundIds[k]))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(presetValue(rc::kPresets[i], k)));
            p->endChangeGesture();
        }
    ++modelVersion;
}

bool ReChordProcessor::presetModified() const
{
    const auto& s = rc::kPresets[presetIndex];
    for (int k = 0; k < 10; ++k)
    {
        auto* p = apvts.getParameter(kSoundIds[k]);
        const float now = p->convertFrom0to1(p->getValue()), want = presetValue(s, k);
        const float range = std::max(1.f, std::abs(want)) * 0.01f;
        if (std::abs(now - want) > range) return true;
    }
    return false;
}

juce::String ReChordProcessor::slotName(int i) const
{
    const auto& c = pattern.chord[score.key[juce::jlimit(0, score.nSlots - 1, i)]];
    return rc::chordName(c.root, c.quality, transpose());
}

void ReChordProcessor::publish()
{
    {
        const juce::SpinLock::ScopedLockType sl(scoreLock);
        audioScore = score;
        scoreDirty = true;
    }
    ++modelVersion;
}

void ReChordProcessor::smart()
{
    lastVoices = (int) apvts.getRawParameterValue("voices")->load();
    lastOctave = (int) apvts.getRawParameterValue("octave")->load();
    rc::buildScore(score, pattern, lastVoices, lastOctave);
    publish();
}

void ReChordProcessor::setChord(int key, int root, int quality)
{
    chordFor(key) = { (root % 12 + 12) % 12, quality };
    if (key >= 0 && key % 2 == 0 && !pattern.split[key / 2]) pattern.chord[key + 1] = pattern.chord[key];   // a later split starts from it
    smart();
}

void ReChordProcessor::setSplit(int bar, bool on)
{
    pattern.split[bar] = on;
    if (on) pattern.chord[bar * 2 + 1] = pattern.chord[bar * 2];
    smart();
}

void ReChordProcessor::setEnd(int which, int mode)
{
    if (mode == rc::Loop) { pattern.entryMode = pattern.exitMode = rc::Loop; }
    else
    {
        if (pattern.exitMode == rc::Loop) { pattern.entryMode = pattern.exitMode = rc::None; }   // leaving the loop: both ends open
        (which == -1 ? pattern.entryMode : pattern.exitMode) = mode;
    }
    smart();
}

void ReChordProcessor::setTiming(int t, int v, float early, float late, float curve)
{
    auto& l = score.linkAt(t, v);
    l.early = juce::jlimit(0.f, 4.f, early);
    l.late = juce::jlimit(0.f, 4.f, late);
    l.curve = juce::jlimit(-1.f, 1.f, curve);
    score.edited = true;
    publish();
}

void ReChordProcessor::timingToAll(int t, int v)
{
    const auto src = score.linkAt(t, v);
    for (int i = -1; i < score.nSlots; ++i)
        for (int j = 0; j < score.nVoices; ++j)
        {
            auto& l = score.linkAt(i, j);
            l.early = src.early; l.late = src.late; l.curve = src.curve;
        }
    score.edited = true;
    publish();
}

bool ReChordProcessor::moveOctave(int col, int v, int dir)
{
    const int np = score.pitchAt(col, v) + 12 * dir;
    if (np < 28 || np > 103) return false;
    for (int j = 0; j < score.nVoices; ++j) if (j != v && score.pitchAt(col, j) == np) return false;  // no unisons
    if (col >= score.nSlots && score.loop()) col = 0;     // the faded copy of chord 1
    score.pitchAt(col, v) = np;
    score.edited = true;
    publish();
    return true;
}

void ReChordProcessor::timerCallback()
{
    const int nv = (int) apvts.getRawParameterValue("voices")->load();
    const int oc = (int) apvts.getRawParameterValue("octave")->load();
    if (nv != lastVoices || oc != lastOctave) smart();
}

void ReChordProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    scoreDirty = true;
}

void ReChordProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    midi.clear();
    const int n = buffer.getNumSamples();
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, n);
    {
        const juce::SpinLock::ScopedTryLockType sl(scoreLock);
        if (sl.isLocked() && scoreDirty) { engine.setScore(audioScore); scoreDirty = false; }
    }
    engine.setParams(currentParams());

    bool hostPlaying = false;
    double ppq = 0, bpm = hostBpm.load(), barBeats = hostBarBeats.load();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            hostPlaying = pos->getIsPlaying();
            if (auto b = pos->getBpm()) bpm = *b;
            if (auto p = pos->getPpqPosition()) ppq = *p;
            if (auto ts = pos->getTimeSignature()) if (ts->numerator > 0 && ts->denominator > 0) barBeats = ts->numerator * 4.0 / ts->denominator;
        }
    bpm = juce::jlimit(20.0, 400.0, bpm);
    hostBpm = bpm;
    hostBarBeats = barBeats;

    // LISTEN on: with Cubase when it plays, otherwise on its own. Off: silent.
    const bool listen = apvts.getRawParameterValue("listen")->load() > 0.5f;
    bool playing = false;
    double beat = 0;
    if (listen && hostPlaying) { playing = true; beat = ppq; freeBeat = 0; }
    else if (listen)
    {
        playing = true;
        beat = freeBeat;
        freeBeat = std::fmod(freeBeat + n * bpm / 60.0 / getSampleRate(), rc::kBars * barBeats * 12.0);
    }
    else freeBeat = 0;

    uiBeat = playing ? beat : -1.0;
    engine.process(buffer.getWritePointer(0), buffer.getWritePointer(1), n, playing, beat, bpm, barBeats);
}

juce::File ReChordProcessor::renderWav()
{
    const double sr = getSampleRate() > 8000 ? getSampleRate() : 48000.0;
    const double bpm = hostBpm.load(), barBeats = hostBarBeats.load();
    std::vector<float> L, R;
    rc::renderPattern(score, currentParams(), sr, bpm, barBeats, L, R);

    juce::String names;
    for (int i = 0; i < score.nSlots; ++i) names << (i ? "-" : "") << slotName(i).replace("#", "s");
    auto dir = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ReChord").getChildFile("Renders");
    dir.createDirectory();
    auto f = dir.getNonexistentChildFile("ReChord " + names + " " + juce::String(juce::roundToInt(bpm)) + "bpm", ".wav", false);

    juce::AudioBuffer<float> buf(2, (int) L.size());
    buf.copyFrom(0, 0, L.data(), (int) L.size());
    buf.copyFrom(1, 0, R.data(), (int) R.size());
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os(f.createOutputStream().release());
    if (os == nullptr) return {};
    std::unique_ptr<juce::AudioFormatWriter> w(wav.createWriterFor(os.get(), sr, 2, 24, {}, 0));
    if (w == nullptr) return {};
    os.release();
    w->writeFromAudioSampleBuffer(buf, 0, buf.getNumSamples());
    w.reset();
    return f;
}

// ---------------- state ----------------
static juce::String linkText(const rc::Link& l)
{
    return juce::String(l.to) + "," + juce::String(l.glide ? 1 : 0) + "," + juce::String(l.early, 4) + "," + juce::String(l.late, 4) + "," + juce::String(l.curve, 4);
}
static bool linkFrom(const juce::String& t, rc::Link& l)
{
    juce::StringArray a; a.addTokens(t, ",", "");
    if (a.size() != 5) return false;
    l.to = a[0].getIntValue(); l.glide = a[1].getIntValue() != 0;
    l.early = a[2].getFloatValue(); l.late = a[3].getFloatValue(); l.curve = a[4].getFloatValue();
    return true;
}

void ReChordProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    juce::ValueTree m("model2");
    m.setProperty("version", RECHORD_VERSION, nullptr);
    m.setProperty("preset", presetIndex, nullptr);
    juce::String chords, split;
    for (auto& c : pattern.chord) chords << c.root << ":" << c.quality << " ";
    for (bool b : pattern.split) split << (b ? 1 : 0) << " ";
    m.setProperty("chords", chords.trim(), nullptr);
    m.setProperty("split", split.trim(), nullptr);
    m.setProperty("ends", juce::String(pattern.entryMode) + " " + juce::String(pattern.exitMode) + " " + juce::String(pattern.entry.root) + ":" + juce::String(pattern.entry.quality)
                          + " " + juce::String(pattern.exit.root) + ":" + juce::String(pattern.exit.quality), nullptr);
    m.setProperty("nVoices", score.nVoices, nullptr);
    m.setProperty("nSlots", score.nSlots, nullptr);
    m.setProperty("edited", score.edited, nullptr);
    juce::String pitches, links;
    for (int c = -1; c <= score.nSlots; ++c)
        for (int v = 0; v < rc::kMaxVoices; ++v) pitches << (c < 0 ? score.entryPitch[v] : c >= score.nSlots ? score.exitPitch[v] : score.pitch[c][v]) << " ";
    for (int t = -1; t < score.nSlots; ++t)
        for (int v = 0; v < rc::kMaxVoices; ++v) links << linkText(score.linkAt(t, v)) << " ";
    m.setProperty("pitches", pitches.trim(), nullptr);
    m.setProperty("links", links.trim(), nullptr);
    st.removeChild(st.getChildWithName("model2"), nullptr);
    st.appendChild(m, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}

void ReChordProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml(*xml);
    auto m = st.getChildWithName("model2");
    st.removeChild(m, nullptr);
    st.removeChild(st.getChildWithName("model"), nullptr);
    apvts.replaceState(st);
    if (!m.isValid()) { smart(); return; }
    presetIndex = juce::jlimit(0, rc::kNumPresets - 1, (int) m.getProperty("preset", 0));

    juce::StringArray ch, sp, ends;
    ch.addTokens(m.getProperty("chords").toString(), " ", "");
    sp.addTokens(m.getProperty("split").toString(), " ", "");
    ends.addTokens(m.getProperty("ends").toString(), " ", "");
    auto chordOf = [](const juce::String& s) { return rc::Chord { s.upToFirstOccurrenceOf(":", false, false).getIntValue() % 12,
                                                                  juce::jlimit(0, rc::kNumQualities - 1, s.fromFirstOccurrenceOf(":", false, false).getIntValue()) }; };
    if (ch.size() == rc::kBars * 2) for (int i = 0; i < ch.size(); ++i) pattern.chord[i] = chordOf(ch[i]);
    if (sp.size() == rc::kBars) for (int i = 0; i < rc::kBars; ++i) pattern.split[i] = sp[i].getIntValue() != 0;
    if (ends.size() == 4)
    {
        pattern.entryMode = juce::jlimit(0, 2, ends[0].getIntValue());
        pattern.exitMode = juce::jlimit(0, 2, ends[1].getIntValue());
        pattern.entry = chordOf(ends[2]);
        pattern.exit = chordOf(ends[3]);
    }
    smart();   // slots, scale, modes from the pattern; then the saved voicing and links on top

    juce::StringArray pi, li;
    pi.addTokens(m.getProperty("pitches").toString(), " ", "");
    li.addTokens(m.getProperty("links").toString(), " ", "");
    const int nv = (int) m.getProperty("nVoices", 0), ns = (int) m.getProperty("nSlots", 0);
    if (nv != score.nVoices || ns != score.nSlots || pi.size() != (ns + 2) * rc::kMaxVoices || li.size() != (ns + 1) * rc::kMaxVoices) return;

    rc::Score s = score;
    int k = 0;
    for (int c = -1; c <= ns; ++c)
        for (int v = 0; v < rc::kMaxVoices; ++v, ++k)
        {
            const int p = pi[k].getIntValue();
            if (c < 0) s.entryPitch[v] = p; else if (c >= ns) s.exitPitch[v] = p; else s.pitch[c][v] = p;
        }
    k = 0;
    for (int t = -1; t < ns; ++t)
    {
        bool used[rc::kMaxVoices] {};
        for (int v = 0; v < rc::kMaxVoices; ++v, ++k)
        {
            rc::Link l;
            if (!linkFrom(li[k], l)) return;
            if (v < nv)
            {
                if (l.to < 0 || l.to >= nv || used[l.to]) return;     // links must stay one-to-one
                used[l.to] = true;
            }
            s.linkAt(t, v) = l;
        }
    }
    s.edited = (bool) m.getProperty("edited", false);
    score = s;
    publish();
}

juce::AudioProcessorEditor* ReChordProcessor::createEditor() { return new ReChordEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReChordProcessor(); }
