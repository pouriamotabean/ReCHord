#include "PluginProcessor.h"
#include "PluginEditor.h"

using APF = juce::AudioParameterFloat;
using PID = juce::ParameterID;

static juce::String msText(float v, int) { return v >= 1000.f ? juce::String(v / 1000.f, 2) + " s" : juce::String(juce::roundToInt(v)) + " ms"; }
static juce::String beatText(float b, int)
{
    // glide times are in beats; show the familiar note values when they land on one
    struct N { float b; const char* t; } names[] = { { 0.f, "0" }, { 0.125f, "1/32" }, { 0.25f, "1/16" }, { 0.375f, "3/32" }, { 0.5f, "1/8" },
                                                    { 0.75f, "3/16" }, { 1.f, "1/4" }, { 1.5f, "3/8" }, { 2.f, "1/2" } };
    for (auto& n : names) if (std::abs(b - n.b) < 0.012f) return n.t;
    return juce::String(b, 2) + " b";
}

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
    fl("attack", "Attack", 2.f, 600.f, 35.f, 60.f, msText);
    fl("bright", "Bright", 0.f, 1.f, 0.45f, 0, [](float v, int) { return juce::String(juce::roundToInt(v * 100)) + "%"; });
    fl("vibrato", "Vibrato", 0.f, 50.f, 16.f, 0, [](float v, int) { return juce::String(juce::roundToInt(v)) + " ct"; });
    fl("release", "Release", 30.f, 3000.f, 380.f, 400.f, msText);
    // glide: snaps to 1/32-note steps so the musical values are easy to hit
    p.push_back(std::make_unique<APF>(PID { "early", 1 }, "Glide early", juce::NormalisableRange<float>(0.f, 2.f, 0.03125f), 0.5f,
                                      juce::AudioParameterFloatAttributes().withStringFromValueFunction(beatText)));
    p.push_back(std::make_unique<APF>(PID { "late", 1 }, "Glide late", juce::NormalisableRange<float>(0.f, 1.f, 0.03125f), 0.25f,
                                      juce::AudioParameterFloatAttributes().withStringFromValueFunction(beatText)));
    fl("curve", "Glide curve", -1.f, 1.f, 0.f, 0, [](float v, int) {
        return std::abs(v) < 0.04f ? juce::String("smooth") : (v < 0 ? "early " : "late ") + juce::String(juce::roundToInt(std::abs(v) * 100)); });
    fl("spread", "Spread", 0.f, 60.f, 12.f, 0, [](float v, int) { return juce::String(juce::roundToInt(v)) + " ms"; });
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "transpose", 1 }, "Transpose", -12, 12, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return (v > 0 ? "+" : "") + juce::String(v); })));
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "octave", 1 }, "Register", -2, 2, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 0 ? juce::String("mid") : (v > 0 ? "+" : "") + juce::String(v) + " oct"; })));
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "voices", 1 }, "Voices", 3, 4, 3));
    fl("volume", "Volume", -30.f, 6.f, -4.f, 0, [](float v, int) { return juce::String(v, 1) + " dB"; });
    p.push_back(std::make_unique<juce::AudioParameterBool>(PID { "live", 1 }, "Play with Cubase", true));
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
    p.attackMs = v("attack");
    p.releaseMs = v("release");
    p.bright = v("bright");
    p.vibratoCents = v("vibrato");
    p.early = v("early");
    p.late = v("late");
    p.curve = v("curve");
    p.spreadMs = v("spread");
    p.transpose = (int) v("transpose");
    p.volumeDb = v("volume");
    return p;
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
    rc::smartVoicing(score, chords, lastVoices, lastOctave);
    publish();
}

void ReChordProcessor::setChord(int bar, int root, int quality)
{
    chords[bar] = { (root % 12 + 12) % 12, quality };
    smart();
}

bool ReChordProcessor::moveOctave(int bar, int slot, int dir)
{
    const int np = score.pitch[bar][slot] + 12 * dir;
    if (np < 28 || np > 103) return false;
    for (int v = 0; v < score.nVoices; ++v) if (v != slot && score.pitch[bar][v] == np) return false;  // no unisons
    score.pitch[bar][slot] = np;
    publish();
    return true;
}

void ReChordProcessor::timerCallback()
{
    // voices / register changed (from the UI or automation): new smart voicing, here on the message thread
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

    const bool live = apvts.getRawParameterValue("live")->load() > 0.5f;
    bool playing = false;
    double beat = 0;
    if (hostPlaying && live) { playing = true; beat = ppq; preview = false; }
    else if (preview.load() && !hostPlaying)
    {
        playing = true;
        beat = previewBeat;
        previewBeat += n * bpm / 60.0 / getSampleRate();
        previewBeat = std::fmod(previewBeat, rc::kBars * barBeats * 12.0);
    }
    if (!preview.load()) previewBeat = 0;

    uiBeat = playing ? beat : -1.0;
    engine.process(buffer.getWritePointer(0), buffer.getWritePointer(1), n, playing, beat, bpm, barBeats);
}

juce::File ReChordProcessor::renderWav()
{
    const double sr = getSampleRate() > 8000 ? getSampleRate() : 48000.0;
    const double bpm = hostBpm.load(), barBeats = hostBarBeats.load();
    std::vector<float> L, R;
    rc::renderLoop(score, currentParams(), sr, bpm, barBeats, L, R);

    juce::String names;
    for (int b = 0; b < rc::kBars; ++b)
        names << (b ? "-" : "") << juce::String(rc::chordName(chords[b].root, chords[b].quality, transpose())).replace("#", "s");
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

void ReChordProcessor::getStateInformation(juce::MemoryBlock& dest)
{
    auto st = apvts.copyState();
    juce::ValueTree m("model");
    m.setProperty("version", RECHORD_VERSION, nullptr);
    m.setProperty("nVoices", score.nVoices, nullptr);
    for (int b = 0; b < rc::kBars; ++b)
    {
        juce::ValueTree bar("bar");
        bar.setProperty("root", chords[b].root, nullptr);
        bar.setProperty("quality", chords[b].quality, nullptr);
        juce::String pi, pe, gl;
        for (int v = 0; v < rc::kMaxVoices; ++v)
        {
            pi << score.pitch[b][v] << " ";
            pe << score.perm[b][v] << " ";
            gl << (score.glide[b][v] ? 1 : 0) << " ";
        }
        bar.setProperty("pitch", pi.trim(), nullptr);
        bar.setProperty("perm", pe.trim(), nullptr);
        bar.setProperty("glide", gl.trim(), nullptr);
        m.appendChild(bar, nullptr);
    }
    st.removeChild(st.getChildWithName("model"), nullptr);
    st.appendChild(m, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}

void ReChordProcessor::setStateInformation(const void* data, int size)
{
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml(*xml);
    auto m = st.getChildWithName("model");
    st.removeChild(m, nullptr);
    apvts.replaceState(st);
    if (!m.isValid() || m.getNumChildren() != rc::kBars) { smart(); return; }

    rc::Score s;
    s.nVoices = juce::jlimit(3, rc::kMaxVoices, (int) m.getProperty("nVoices", 3));
    bool ok = true;
    for (int b = 0; b < rc::kBars; ++b)
    {
        auto bar = m.getChild(b);
        chords[b] = { (int) bar.getProperty("root", 9), juce::jlimit(0, rc::kNumQualities - 1, (int) bar.getProperty("quality", 0)) };
        juce::StringArray pi, pe, gl;
        pi.addTokens(bar.getProperty("pitch").toString(), " ", "");
        pe.addTokens(bar.getProperty("perm").toString(), " ", "");
        gl.addTokens(bar.getProperty("glide").toString(), " ", "");
        if (pi.size() != rc::kMaxVoices || pe.size() != rc::kMaxVoices || gl.size() != rc::kMaxVoices) { ok = false; break; }
        bool used[rc::kMaxVoices] {};
        for (int v = 0; v < rc::kMaxVoices; ++v)
        {
            s.pitch[b][v] = pi[v].getIntValue();
            s.perm[b][v] = juce::jlimit(0, rc::kMaxVoices - 1, pe[v].getIntValue());
            s.glide[b][v] = gl[v].getIntValue() != 0;
            if (v < s.nVoices)
            {
                if (s.perm[b][v] >= s.nVoices || used[s.perm[b][v]]) ok = false;   // links must stay one-to-one
                else used[s.perm[b][v]] = true;
            }
        }
    }
    lastVoices = (int) apvts.getRawParameterValue("voices")->load();
    lastOctave = (int) apvts.getRawParameterValue("octave")->load();
    if (!ok || s.nVoices != lastVoices) { smart(); return; }
    score = s;
    publish();
}

juce::AudioProcessorEditor* ReChordProcessor::createEditor() { return new ReChordEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReChordProcessor(); }
