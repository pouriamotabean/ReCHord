#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <map>

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
    p.push_back(std::make_unique<juce::AudioParameterChoice>(PID { "osc", 1 }, "Sound", juce::StringArray { "FM", "Analog", "Strings", "Voice", "EP", "Sine", "Keys", "Bowed", "808", "Sample" }, 0));
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
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "octave", 1 }, "Octave", -3, 2, 0,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 0 ? juce::String("0") : (v > 0 ? "+" : "") + juce::String(v) + " oct"; })));
    fl("liveGlide", "Keyboard glide", 0.f, 2000.f, 250.f, 300.f, msText);
    p.push_back(std::make_unique<juce::AudioParameterInt>(PID { "voices", 1 }, "Voices", 1, 4, 3,
        juce::AudioParameterIntAttributes().withStringFromValueFunction([](int v, int) { return v == 1 ? juce::String("1 bass") : v == 2 ? juce::String("2 bass") : juce::String(v); })));
    fl("volume", "Volume", -30.f, 6.f, rc::kPresets[0].volumeDb, 0, [](float v, int) { return juce::String(v, 1) + " dB"; });
    p.push_back(std::make_unique<juce::AudioParameterBool>(PID { "listen", 1 }, "Listen", false));
    p.push_back(std::make_unique<juce::AudioParameterBool>(PID { "addGlide", 1 }, "Added note glides in", true));
    return { p.begin(), p.end() };
}

ReChordProcessor::ReChordProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "params", layout())
{
    smart();
    loadPreset(27);     // a new ReChord starts on a measured body: Felt to Rhodes
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
    p.transpose = (int) v("transpose") + 12 * (int) v("octave");
    p.liveShift = 12 * (int) v("octave");
    p.liveGlideMs = v("liveGlide");
    p.addGlide = v("addGlide") > 0.5f;
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
    userPreset.clear();
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
        rc::LaneCache c; c.build(score);               // the heavy part, here and not on the audio thread
        const juce::SpinLock::ScopedLockType sl(scoreLock);
        audioScore = score;
        audioCache = c;
        scoreDirty = true;
    }
    ++modelVersion;
}

void ReChordProcessor::smart()
{
    checkpoint();
    lastVoices = (int) apvts.getRawParameterValue("voices")->load();
    rc::buildScore(score, pattern, lastVoices, 0);       // OCTAVE moves the sound, not the voicing
    publish();
}

void ReChordProcessor::setChord(int key, int root, int quality)
{
    checkpoint();
    chordFor(key) = { (root % 12 + 12) % 12, quality };
    if (key >= 0 && key % 2 == 0 && !pattern.split[key / 2]) pattern.chord[key + 1] = pattern.chord[key];   // a later split starts from it
    smart();
}

void ReChordProcessor::setBarOn(int bar, bool on)
{
    if (!on && pattern.activeBars() <= 1 && pattern.on[bar]) return;     // at least one bar
    checkpoint();
    pattern.on[bar] = on;
    smart();
}

bool ReChordProcessor::movePitch(int col, int v, int semis)
{
    if (semis == 0) return false;
    checkpoint(2000 + (col + 1) * 8 + v);
    const int np = score.pitchAt(col, v) + semis;
    if (np < 21 || np > 108) return false;
    for (int j = 0; j < score.nVoices; ++j) if (j != v && score.pitchAt(col, j) == np) return false;  // no unisons
    if (col >= score.nSlots && score.loop()) col = 0;
    score.pitchAt(col, v) = np;
    score.scaleMask |= 1 << (np % 12);      // ROBOT steps through the new note too
    score.edited = true;
    publish();
    return true;
}

// what slot i really plays: the chord as written, or (a note moved off it) the chord its notes make, with * when
// they make none of the kinds
juce::String ReChordProcessor::slotLabel(int i) const
{
    i = juce::jlimit(0, score.nSlots - 1, i);
    const auto& c = pattern.chord[score.key[i]];
    int written = 0;
    for (int pc : rc::chordTones(c, 4)) written |= 1 << pc;
    for (int pc : rc::chordTones(c, 3)) written |= 1 << pc;
    int played = 0, bass = 127;
    for (int v = 0; v < score.nVoices; ++v) { played |= 1 << (score.pitch[i][v] % 12); bass = std::min(bass, score.pitch[i][v]); }
    if ((played & ~written) == 0) return rc::chordName(c.root, c.quality, transpose());
    const auto id = rc::identifyChord(played, bass % 12);
    int idMask = 0;
    for (int pc : rc::chordTones(id, 4)) idMask |= 1 << pc;
    for (int pc : rc::chordTones(id, 3)) idMask |= 1 << pc;
    return juce::String(rc::chordName(id.root, id.quality, transpose())) + ((played & ~idMask) != 0 ? "*" : "");
}

void ReChordProcessor::setSplit(int bar, bool on)
{
    checkpoint();
    pattern.split[bar] = on;
    if (on) pattern.chord[bar * 2 + 1] = pattern.chord[bar * 2];
    smart();
}

void ReChordProcessor::setEnd(int which, int mode)
{
    checkpoint();
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
    checkpoint(1000 + (t + 1) * 8 + v);
    auto& l = score.linkAt(t, v);
    l.early = juce::jlimit(0.f, 4.f, early);
    l.late = juce::jlimit(0.f, 4.f, late);
    l.curve = juce::jlimit(-1.f, 1.f, curve);
    score.edited = true;
    publish();
}

void ReChordProcessor::timingToAll(int t, int v)
{
    checkpoint();
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
    checkpoint(2000 + (col + 1) * 8 + v);
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
    if (nv != lastVoices) smart();
    if (learnState.load() == 3) finishLearn();
    // SAMPLE: CHARACTER = the loop's length; the loop is cut again a moment after the knob stops
    if ((int) apvts.getRawParameterValue("osc")->load() == rc::OscSample && !samples.empty())
    {
        const float c = apvts.getRawParameterValue("character")->load();
        const double now = juce::Time::getMillisecondCounterHiRes();
        if (std::abs(c - seenChar) > 0.0005f) { seenChar = c; charAt = now; }
        else if (std::abs(c - builtLoop) > 0.004f && now - charAt > 300.0) rebuildSamples();
    }
    if (editPending && juce::Time::getMillisecondCounterHiRes() - editAt > 120.0) { editPending = false; rebuildSamples(); }
    {
        // the set the audio thread gave back: freed here, never there
        std::shared_ptr<const rc::SampleSet> old;
        { const juce::SpinLock::ScopedTryLockType sl(scoreLock); if (sl.isLocked()) std::swap(old, retiredSample); }
    }
}

void ReChordProcessor::prepareToPlay(double sampleRate, int)
{
    engine.prepare(sampleRate);
    scoreDirty = true;
}

void ReChordProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples();
    for (int c = 2; c < buffer.getNumChannels(); ++c) buffer.clear(c, 0, n);
    {
        const juce::SpinLock::ScopedTryLockType sl(scoreLock);
        if (sl.isLocked() && scoreDirty) { engine.setScore(audioScore, audioCache); scoreDirty = false; }
        if (sl.isLocked() && sampleDirty && retiredSample == nullptr) { retiredSample = engine.swapSample(pendingSample); sampleDirty = false; }
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
            // bar 1 is not always at ppq 0 (a meter change, a pickup): count from Cubase's own bar lines
            if (auto bs = pos->getPpqPositionOfLastBarStart())
                if (auto bc = pos->getBarCount())
                    if (*bc >= 0 && *bs <= ppq + 1e-6) ppq = (double) *bc * barBeats + (ppq - *bs);
        }
    bpm = juce::jlimit(20.0, 400.0, bpm);
    hostBpm = bpm;
    hostBarBeats = barBeats;

    // ---- the keyboard: notes held (+ the sustain pedal). A chord is taken 70 ms after its first key; when you
    //      play legato, the keys still held from the last chord are left out of the new one; a gap up to 0.5 s
    //      between chords still glides ----
    const double spb = getSampleRate() * 60.0 / bpm;
    // ---- LEARN: armed -> records from the next bar line, for the bars that are on, of Cubase playback ----
    if (int armed = 1; hostPlaying && learnState.compare_exchange_strong(armed, 2))
    {
        learnStart = -1.0;                                                   // set by the first note: its bar is bar 1
        learnBarBeats = barBeats;
        learnCount = 0;
        learnLastPpq = ppq;
    }
    if (int rec = 2; hostPlaying && ppq < learnLastPpq - 0.01) learnState.compare_exchange_strong(rec, 3);   // Cubase cycled back: one pass is enough
    for (const auto m : midi)
    {
        const auto msg = m.getMessage();
        const int key = msg.getNoteNumber();
        if (msg.isNoteOn())
        {
            held[key] = true; sustained[key] = false; stale[key] = false;
            if (chordWait < 0) { chordWait = (int) (getSampleRate() * 0.07); for (auto& x : fresh) x = false; }
            fresh[key] = true;
        }
        else if (msg.isNoteOff())
        {
            held[key] = false;
            if (pedal && !stale[key]) sustained[key] = true;
            if (!sustained[key]) stale[key] = false;
            if (chordWait < 0) { chordWait = (int) (getSampleRate() * 0.07); for (auto& x : fresh) x = false; }
        }
        else if (msg.isSustainPedalOn()) pedal = true;
        else if (msg.isSustainPedalOff())
        {
            pedal = false;
            for (int k = 0; k < 128; ++k) if (sustained[k]) { sustained[k] = false; stale[k] = false; }
            if (chordWait < 0) { chordWait = (int) (getSampleRate() * 0.07); for (auto& x : fresh) x = false; }
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            for (int k = 0; k < 128; ++k) held[k] = sustained[k] = stale[k] = fresh[k] = false;
            chordWait = 0;
        }
        if (learnState.load() == 2 && hostPlaying && (msg.isNoteOn() || msg.isNoteOff()) && learnCount < (int) learnEvents.size())
        {
            const double beat = ppq + m.samplePosition / spb;
            if (learnStart < 0 && msg.isNoteOn()) learnStart = std::floor((beat + 0.1) / barBeats) * barBeats;   // a hair early = that bar
            if (learnStart >= 0 && beat >= learnStart - 0.1)
                learnEvents[(size_t) learnCount++] = { std::max(beat, learnStart), key + 128 * juce::jlimit(0, 15, msg.getChannel() - 1), msg.isNoteOn() };
        }
    }
    midi.clear();
    if (chordWait >= 0)
    {
        chordWait -= n;
        if (chordWait < 0)
        {
            bool anyFresh = false;
            int nFresh = 0, nKept = 0;
            for (int k = 0; k < 128; ++k)
            {
                anyFresh = anyFresh || fresh[k];
                nFresh += fresh[k] ? 1 : 0;
                nKept += (!fresh[k] && !stale[k] && (held[k] || sustained[k])) ? 1 : 0;
            }
            // 1-2 new keys over a chord still held = added to it (Am + G = Am7); 3+ = a new chord (legato change).
            // With VOICES 1 (a line) every new key is the next note.
            const bool adding = anyFresh && nKept > 0 && nFresh <= 2 && (int) apvts.getRawParameterValue("voices")->load() > 1;
            int notes[128], c = 0;
            for (int k = 0; k < 128; ++k)
            {
                const bool down = held[k] || sustained[k];
                if (!down) continue;
                if (anyFresh && !fresh[k] && !adding) { stale[k] = true; continue; }    // still held from the chord before
                if (stale[k]) continue;
                notes[c++] = k;
            }
            for (auto& x : fresh) x = false;
            if (c == 0)
            {
                if (engine.liveActive() && anyFresh == false) graceLeft = (int) (getSampleRate() * 0.5);
            }
            else { graceLeft = 0; engine.setLive(notes, c); }
        }
    }
    if (graceLeft > 0)
    {
        graceLeft -= n;
        if (graceLeft <= 0) engine.setLive(nullptr, 0);
    }
    if (const int au = audition.load(); au != lastAudition)
    {
        if (au >= 0) { int k = au; engine.setLive(&k, 1); } else engine.setLive(nullptr, 0);
        lastAudition = au;
    }

    // ---- LEARN: armed -> records from the next bar line for 4 bars of Cubase playback ----
    if (learnState.load() == 2)
    {
        learnLastPpq = ppq;
        if (!hostPlaying || (learnStart >= 0 && ppq + n / spb >= learnStart + rc::kMaxBars * learnBarBeats)) { int rec = 2; learnState.compare_exchange_strong(rec, 3); }
        learnPos = hostPlaying && learnStart >= 0 ? (ppq - learnStart) / learnBarBeats : -1.0;
    }

    // LISTEN on: with Cubase when it plays, otherwise on its own. Off: silent.
    const bool listen = apvts.getRawParameterValue("listen")->load() > 0.5f;
    bool playing = false;
    double beat = 0;
    if (listen && !hostPlaying) if (const double sk = seekBeat.exchange(-1.0); sk >= 0) freeBeat = sk;   // kept until LISTEN is on
    if (listen && hostPlaying) { playing = true; beat = ppq; freeBeat = 0; }
    else if (listen)
    {
        playing = true;
        beat = freeBeat;
        freeBeat = std::fmod(freeBeat + n * bpm / 60.0 / getSampleRate(), std::max(1, audioScore.nBars) * barBeats * 12.0 * 64.0);
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
    rc::renderPattern(score, currentParams(), sr, bpm, barBeats, L, R, sampleSet);

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

// ---------------- chords from notes: LEARN and a dropped MIDI file ----------------
void ReChordProcessor::startLearn() { learnCount = 0; learnPos = -1.0; learnState = learnState.load() == 0 ? 1 : 0; }

void ReChordProcessor::finishLearn()
{
    std::vector<Note> notes;
    std::map<int, double> on;
    std::vector<int> skipOff(16 * 128, 0);      // per channel + key
    const int count = learnCount;
    if (learnStart < 0 || count == 0) { learnState = 0; learnPos = -1.0; return; }    // no note came
    const double end = std::max(learnLastPpq, learnStart) - learnStart;
    for (int i = 0; i < count; ++i)
    {
        const auto& e = learnEvents[(size_t) i];
        if (e.on)
        {
            // a new chord (2+ keys within 1/10 beat) ends the notes still on from long before it - so a lost
            // note-off can't spill one chord into all the next ones
            int cluster = 0;
            for (int j = i; j < count && learnEvents[(size_t) j].beat - e.beat < 0.1; ++j) if (learnEvents[(size_t) j].on) ++cluster;
            if (cluster >= 2)
                for (auto it = on.begin(); it != on.end();)
                {
                    if (e.beat - it->second >= 0.5) { notes.push_back({ it->second - learnStart, e.beat - learnStart, it->first % 128 }); ++skipOff[(size_t) it->first]; it = on.erase(it); }
                    else ++it;
                }
            // the same key again while it is still on (legato overlap, or a lost note-off): the earlier note ends here
            if (on.count(e.key)) { if (e.beat - on[e.key] > 0.05) notes.push_back({ on[e.key] - learnStart, e.beat - learnStart, e.key % 128 }); ++skipOff[(size_t) e.key]; }
            on[e.key] = e.beat;
        }
        else if (skipOff[(size_t) e.key] > 0) --skipOff[(size_t) e.key];
        else if (on.count(e.key)) { notes.push_back({ on[e.key] - learnStart, e.beat - learnStart, e.key % 128 }); on.erase(e.key); }
    }
    // the bars that really went by (a stop or a Cubase cycle can end it early)
    const int span = juce::jlimit(1, rc::kMaxBars, (int) std::ceil(end / learnBarBeats - 0.05));
    for (auto& kv : on) notes.push_back({ kv.second - learnStart, span * learnBarBeats, kv.first % 128, true });
    learnState = 0;
    learnPos = -1.0;
    applyNotes(notes, learnBarBeats, 0, 0);          // bar 1 = the bar of the first note; as many bars as have notes (like a dropped file)
}

bool ReChordProcessor::loadMidiFile(const juce::File& f)
{
    juce::FileInputStream in(f);
    juce::MidiFile mf;
    if (!in.openedOk() || !mf.readFrom(in)) return false;
    const int tpq = mf.getTimeFormat();
    if (tpq <= 0) return false;
    double barBeats = 4.0;
    juce::MidiMessageSequence meta;
    mf.findAllTimeSigEvents(meta);
    if (meta.getNumEvents() > 0)
    {
        int num = 4, den = 4;
        meta.getEventPointer(0)->message.getTimeSignatureInfo(num, den);
        if (num > 0 && den > 0) barBeats = num * 4.0 / den;
    }
    std::vector<Note> notes;
    for (int t = 0; t < mf.getNumTracks(); ++t)
    {
        juce::MidiMessageSequence seq(*mf.getTrack(t));
        seq.updateMatchedPairs();
        for (int i = 0; i < seq.getNumEvents(); ++i)
        {
            auto* ev = seq.getEventPointer(i);
            if (!ev->message.isNoteOn()) continue;
            const double a = ev->message.getTimeStamp() / tpq;
            const double b = ev->noteOffObject != nullptr ? ev->noteOffObject->message.getTimeStamp() / tpq : a + 1.0;
            notes.push_back({ a, b, ev->message.getNoteNumber() });
        }
    }
    if (notes.empty()) return false;
    // from the bar of the first note
    double first = 1e9;
    for (auto& nt : notes) first = std::min(first, nt.a);
    const double start = std::floor(first / barBeats + 1e-6) * barBeats;
    for (auto& nt : notes) { nt.a -= start; nt.b -= start; }
    return applyNotes(notes, barBeats, 0);
}

// 4 bars of notes -> the pattern: a chord per half bar (a bar is split when its halves differ), and the voicing
// as played when every chord has the same 3 or 4 notes
bool ReChordProcessor::applyNotes(const std::vector<Note>& notes, double barBeats, int firstBar, int bars)
{
    if (notes.empty()) return false;
    checkpoint();
    struct Half { int mask = 0, bass = -1; std::vector<int> keys; };
    // as many bars as the material has (a 4-bar part = a 4-bar pattern); those bars on, the rest off
    if (bars <= 0)
    {
        double last = 0;
        for (auto& nt : notes) last = std::max(last, nt.a);
        bars = (int) std::floor((last + 0.5) / barBeats) + 1;     // a chord played a little early still starts its bar
    }
    const int NB = juce::jlimit(1, rc::kMaxBars, bars);
    for (int b = 0; b < rc::kMaxBars; ++b) pattern.on[b] = b < NB;
    Half half[rc::kMaxBars][2];
    bool any = false;
    for (int b = 0; b < NB; ++b)
        for (int h = 0; h < 2; ++h)
        {
            const double w0 = b * barBeats + h * barBeats * 0.5, w1 = w0 + barBeats * 0.5;
            auto& H = half[b][h];
            bool fresh = false;
            for (auto& nt : notes) fresh = fresh || (nt.a >= w0 - 0.25 && nt.a < w1);
            for (auto& nt : notes)
            {
                if (nt.open && fresh && nt.a < w0 - 0.25) continue;
                const double ov = std::min(w1, nt.b) - std::max(w0, nt.a);
                if (ov < (w1 - w0) * 0.3) continue;
                if (std::find(H.keys.begin(), H.keys.end(), nt.key) == H.keys.end()) H.keys.push_back(nt.key);
            }
            std::sort(H.keys.begin(), H.keys.end());
            for (int k : H.keys) H.mask |= 1 << (k % 12);
            if (!H.keys.empty()) { H.bass = H.keys.front() % 12; any = true; }
        }
    if (!any) return false;
    // empty halves keep the chord before them
    for (int b = 0; b < NB; ++b)
        for (int h = 0; h < 2; ++h)
            if (half[b][h].keys.empty())
            {
                const int pb = h == 1 ? b : (b + NB - 1) % NB, ph = h == 1 ? 0 : 1;
                if (!half[pb][ph].keys.empty()) half[b][h] = half[pb][ph];
                else if (!half[b][1 - h].keys.empty()) half[b][h] = half[b][1 - h];
            }
    // the cycle in the plugin is tied to the project: bar B of the project plays pattern bar B mod 4
    rc::Pattern p = pattern;
    p.entryMode = p.exitMode = rc::Loop;
    std::vector<std::vector<int>> keysPerSlot;
    for (int b = 0; b < NB; ++b)
    {
        const int pb = pattern.barAt(((firstBar + b) % NB + NB) % NB);
        const auto c0 = rc::identifyChord(half[b][0].mask, half[b][0].bass), c1 = rc::identifyChord(half[b][1].mask, half[b][1].bass);
        p.split[pb] = !(c0.root == c1.root && c0.quality == c1.quality) && !half[b][1].keys.empty();
        p.chord[pb * 2] = c0;
        p.chord[pb * 2 + 1] = p.split[pb] ? c1 : c0;
    }
    pattern = p;
    // the voicing as played
    int key[rc::kMaxSlots]; float st[rc::kMaxSlots + 1];
    const int ns = pattern.slots(key, st);
    std::vector<std::vector<int>> played((size_t) ns);
    int count = -1;
    bool same = true;
    for (int i = 0; i < ns; ++i)
    {
        const int pb = key[i] / 2, hh = key[i] % 2;
        const int b = ((pattern.indexOf(pb) - firstBar) % NB + NB) % NB;
        played[(size_t) i] = half[b][hh].keys;
        const int c = (int) played[(size_t) i].size();
        if (count < 0) count = c; else if (c != count) same = false;
    }
    if (same && count >= 1 && count <= 4)
    {
        auto* vp = apvts.getParameter("voices");
        vp->setValueNotifyingHost(vp->convertTo0to1((float) count));
    }
    smart();
    if (same && count == score.nVoices)
    {
        for (int i = 0; i < ns; ++i) for (int v = 0; v < count; ++v) score.pitch[i][v] = played[(size_t) i][(size_t) v];
        score.edited = true;      // "as played" is not the smart voicing
        publish();
    }
    return true;
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
    juce::String onStr;
    for (bool b : pattern.on) onStr << (b ? 1 : 0) << " ";
    m.setProperty("on", onStr.trim(), nullptr);
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
    // the samples go inside the project (16-bit WAV), so it opens the same on any computer
    st.removeChild(st.getChildWithName("samples"), nullptr);
    if (!samples.empty()) st.appendChild(samplesTree(), nullptr);
    st.setProperty("userPreset", userPreset, nullptr);
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}

void ReChordProcessor::setStateInformation(const void* data, int size)
{
    undoStack.clear(); redoStack.clear();
    auto xml = getXmlFromBinary(data, size);
    if (xml == nullptr) return;
    auto st = juce::ValueTree::fromXml(*xml);
    auto m = st.getChildWithName("model2");
    st.removeChild(m, nullptr);
    st.removeChild(st.getChildWithName("model"), nullptr);
    {
        const auto sm = st.getChildWithName("samples");
        st.removeChild(sm, nullptr);
        samplesFromTree(sm);
        userPreset = st.getProperty("userPreset").toString();
        st.removeProperty("userPreset", nullptr);
    }
    apvts.replaceState(st);
    builtLoop = -1.f;
    rebuildSamples();
    if (!m.isValid()) { smart(); return; }
    presetIndex = juce::jlimit(0, rc::kNumPresets - 1, (int) m.getProperty("preset", 0));

    juce::StringArray ch, sp, ends;
    ch.addTokens(m.getProperty("chords").toString(), " ", "");
    sp.addTokens(m.getProperty("split").toString(), " ", "");
    ends.addTokens(m.getProperty("ends").toString(), " ", "");
    auto chordOf = [](const juce::String& s) { return rc::Chord { ((s.upToFirstOccurrenceOf(":", false, false).getIntValue() % 12) + 12) % 12,
                                                                  juce::jlimit(0, rc::kNumQualities - 1, s.fromFirstOccurrenceOf(":", false, false).getIntValue()) }; };
    if (ch.size() == rc::kBars * 2 || ch.size() == rc::kMaxBars * 2) for (int i = 0; i < ch.size(); ++i) pattern.chord[i] = chordOf(ch[i]);
    if (sp.size() == rc::kBars || sp.size() == rc::kMaxBars) for (int i = 0; i < sp.size(); ++i) pattern.split[i] = sp[i].getIntValue() != 0;
    {
        juce::StringArray onA; onA.addTokens(m.getProperty("on").toString(), " ", "");
        const bool eight = (int) m.getProperty("bars", 4) == 8;     // 1.8 saved a 4 / 8 switch
        for (int b = 0; b < rc::kMaxBars; ++b) pattern.on[b] = onA.size() == rc::kMaxBars ? onA[b].getIntValue() != 0 : (b < 4 || eight);
        bool any = false; for (bool b : pattern.on) any = any || b;
        if (!any) pattern.on[0] = true;
    }
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
            const int p = juce::jlimit(0, 127, pi[k].getIntValue());
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
    for (int t = -1; t < ns; ++t)
        for (int v = 0; v < rc::kMaxVoices; ++v)
        {
            auto& l = s.linkAt(t, v);
            if (v >= nv || l.to < 0 || l.to >= rc::kMaxVoices) l.to = v;
            l.early = juce::jlimit(0.f, 4.f, l.early); l.late = juce::jlimit(0.f, 4.f, l.late); l.curve = juce::jlimit(-1.f, 1.f, l.curve);
        }
    s.edited = (bool) m.getProperty("edited", false);
    score = s;
    publish();
    undoStack.clear(); redoStack.clear();
}

juce::AudioProcessorEditor* ReChordProcessor::createEditor() { return new ReChordEditor(*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new ReChordProcessor(); }

// ---------------- 2.3: undo / redo, movement presets ----------------
void ReChordProcessor::checkpoint(int coalesce)
{
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (coalesce != 0 && coalesce == lastCoalesce && now - lastCheck < 1500.0) { lastCheck = now; return; }   // one drag = one step
    if (coalesce == 0 && lastCoalesce == 0 && now - lastCheck < 40.0) { lastCheck = now; return; }            // an edit that calls another one
    lastCoalesce = coalesce; lastCheck = now;
    undoStack.push_back({ pattern, score });
    if (undoStack.size() > 64) undoStack.erase(undoStack.begin());
    redoStack.clear();
}

bool ReChordProcessor::undo()
{
    if (undoStack.empty()) return false;
    redoStack.push_back({ pattern, score });
    pattern = undoStack.back().pattern; score = undoStack.back().score; undoStack.pop_back();
    lastCoalesce = 0;
    publish();
    return true;
}

bool ReChordProcessor::redo()
{
    if (redoStack.empty()) return false;
    undoStack.push_back({ pattern, score });
    pattern = redoStack.back().pattern; score = redoStack.back().score; redoStack.pop_back();
    lastCoalesce = 0;
    publish();
    return true;
}

const char* ReChordProcessor::moveName(int kind)
{
    static const char* n[] = { "TOGETHER", "BASS FIRST", "TOP FIRST", "BLOOM", "SNAP", "DRIFT" };
    return n[juce::jlimit(0, kNumMoves - 1, kind)];
}

void ReChordProcessor::applyMove(int kind)
{
    checkpoint();
    const int nv = score.nVoices;
    for (int t = -1; t < score.nSlots; ++t)
    {
        // the voices' order by pitch in the chord the glide leaves
        int rank[rc::kMaxVoices] {};
        for (int v = 0; v < nv; ++v)
            for (int u = 0; u < nv; ++u)
                if (score.pitchAt(t, u) < score.pitchAt(t, v) || (score.pitchAt(t, u) == score.pitchAt(t, v) && u < v)) ++rank[v];
        for (int v = 0; v < nv; ++v)
        {
            auto& l = score.linkAt(t, v);
            const float x = nv > 1 ? (float) rank[v] / (float) (nv - 1) : 0.5f;   // 0 = bass ... 1 = top
            float e = 0.5f, la = 0.25f, c = 0.f;
            switch (kind)
            {
                case 1: { const float d = (x - 0.5f) * 0.7f; e = 0.5f - d; la = 0.25f + d; break; }   // bass leaves and lands first
                case 2: { const float d = (0.5f - x) * 0.7f; e = 0.5f - d; la = 0.25f + d; break; }   // the top first
                case 3: e = 1.0f; la = 0.4f; c = 0.45f; break;                                         // waits, then opens
                case 4: e = 0.12f; la = 0.05f; c = 0.f; break;                                         // quick
                case 5: e = 1.5f; la = 1.0f; c = -0.35f; break;                                        // long, lazy
                default: break;
            }
            l.early = juce::jlimit(0.f, 4.f, e); l.late = juce::jlimit(0.f, 4.f, la); l.curve = c;
        }
    }
    score.edited = true;
    publish();
}

// ---------------- 2.4: SAMPLE ----------------
int ReChordProcessor::loadSamples(const juce::StringArray& files)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();
    std::vector<SampleSrc> got;
    for (auto& path : files)
    {
        const juce::File f(path);
        std::unique_ptr<juce::AudioFormatReader> r(fm.createReaderFor(f));
        if (r == nullptr || r->sampleRate < 8000 || r->sampleRate > 384000 || r->lengthInSamples < (juce::int64) (r->sampleRate * 0.15)) continue;
        const int n = (int) std::min<juce::int64>(r->lengthInSamples, (juce::int64) (r->sampleRate * 20.0));   // 20 s is plenty
        SampleSrc src; src.name = f.getFileNameWithoutExtension(); src.rate = r->sampleRate;
        src.audio.setSize(2, n);
        r->read(&src.audio, 0, n, 0, true, true);                       // mono files: the same on both sides
        got.push_back(std::move(src));
        if (got.size() >= 8) break;
    }
    if (got.empty()) return 0;
    samples = std::move(got);
    sampleSel = 0;
    builtLoop = -1.f;
    rebuildSamples();
    if (sampleSet == nullptr || sampleSet->zones.empty()) return 0;
    // the file as it is: full brightness, no vibrato of our own
    auto set = [this](const char* id, float v) { auto* p = apvts.getParameter(id); p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1(v)); p->endChangeGesture(); };
    set("osc", (float) rc::OscSample);
    set("bright", 1.f);
    set("vibrato", 0.f);
    return (int) sampleSet->zones.size();
}

void ReChordProcessor::clearSamples()
{
    samples.clear();
    rebuildSamples();
}

void ReChordProcessor::rebuildSamples()
{
    const float loop = apvts.getRawParameterValue("character")->load();
    auto set = std::make_shared<rc::SampleSet>();
    for (auto& src : samples)
    {
        const int n = src.audio.getNumSamples();
        std::vector<float> L((size_t) n), R((size_t) n);
        for (int i = 0; i < n; ++i) { L[(size_t) i] = src.audio.getSample(0, i); R[(size_t) i] = src.audio.getSample(1, i); }
        rc::SampleZone z;
        if (rc::buildZone(z, src.info, L, R, src.rate, src.name.toStdString(), src.ed, loop)) set->zones.push_back(std::move(z));
    }
    std::sort(set->zones.begin(), set->zones.end(), [](const rc::SampleZone& a, const rc::SampleZone& b) { return a.root < b.root; });
    builtLoop = loop; seenChar = loop;
    sampleSet = set->zones.empty() ? nullptr : std::shared_ptr<const rc::SampleSet>(set);
    const juce::SpinLock::ScopedLockType sl(scoreLock);
    pendingSample = sampleSet;
    sampleDirty = true;
}

juce::String ReChordProcessor::sampleLabel() const
{
    if (sampleSet == nullptr || sampleSet->zones.empty()) return "drop a WAV here";
    if (sampleSet->zones.size() == 1) return juce::String(sampleSet->zones[0].name);
    return juce::String((int) sampleSet->zones.size()) + " samples";
}

juce::String ReChordProcessor::sampleDetail() const
{
    if (sampleSet == nullptr) return "SAMPLE";
    juce::String s;
    for (auto& z : sampleSet->zones) s << (s.isEmpty() ? juce::String() : juce::String::fromUTF8(" \xc2\xb7 ")) << juce::String(rc::noteName((int) std::lround(z.root)));
    return "SAMPLE  " + s;
}

// ---------------- 2.5.3: samples as a tree, user presets ----------------
juce::ValueTree ReChordProcessor::samplesTree() const
{
    juce::ValueTree sm("samples");
    for (auto& src : samples)
    {
        if (src.b64.isEmpty())
        {
        juce::MemoryBlock mb;
        {
            auto os = std::make_unique<juce::MemoryOutputStream>(mb, false);
            juce::WavAudioFormat wav;
            if (auto* w = wav.createWriterFor(os.get(), src.rate, 2, 16, {}, 0))
            {
                os.release();
                std::unique_ptr<juce::AudioFormatWriter> writer(w);
                writer->writeFromAudioSampleBuffer(src.audio, 0, src.audio.getNumSamples());
            }
        }
        src.b64 = mb.toBase64Encoding();
        }
        juce::ValueTree one("sample");
        one.setProperty("name", src.name, nullptr);
        one.setProperty("a", src.ed.a, nullptr); one.setProperty("b", src.ed.b, nullptr);
        one.setProperty("stretch", src.ed.stretch, nullptr); one.setProperty("xfade", src.ed.xfade, nullptr);
        one.setProperty("mode", src.ed.mode, nullptr); one.setProperty("fix", src.ed.fix, nullptr);
        one.setProperty("wav", src.b64, nullptr);
        sm.appendChild(one, nullptr);
    }
    return sm;
}

void ReChordProcessor::samplesFromTree(const juce::ValueTree& sm)
{
    samples.clear();
    for (int i = 0; sm.isValid() && i < sm.getNumChildren() && samples.size() < 8; ++i)
    {
        const auto one = sm.getChild(i);
        const auto b64 = one.getProperty("wav").toString();
        if (b64.length() > 32 * 1024 * 1024) continue;                       // more than a 20 s stereo 16-bit file can be
        juce::MemoryBlock mb;
        if (!mb.fromBase64Encoding(b64)) continue;
        juce::WavAudioFormat wav;
        std::unique_ptr<juce::AudioFormatReader> r(wav.createReaderFor(new juce::MemoryInputStream(mb, true), true));
        if (r == nullptr || r->lengthInSamples < 100 || r->numChannels < 1 || r->numChannels > 2 || r->sampleRate < 8000 || r->sampleRate > 384000) continue;
        if (r->lengthInSamples > (juce::int64) (r->sampleRate * 20.5)) continue;
        SampleSrc src; src.name = one.getProperty("name").toString(); src.rate = r->sampleRate;
        src.ed.a = (double) one.getProperty("a", -1.0); src.ed.b = (double) one.getProperty("b", -1.0);
        src.ed.stretch = (float) (double) one.getProperty("stretch", 1.0); src.ed.xfade = (float) (double) one.getProperty("xfade", 0.3);
        src.ed.mode = (int) one.getProperty("mode", 0); src.ed.fix = (int) one.getProperty("fix", 0);
        src.audio.setSize(2, (int) r->lengthInSamples);
        r->read(&src.audio, 0, (int) r->lengthInSamples, 0, true, true);
        samples.push_back(std::move(src));
    }
    sampleSel = 0;
}

juce::File ReChordProcessor::presetDir()
{
    return juce::File::getSpecialLocation(juce::File::userDocumentsDirectory).getChildFile("ReChord").getChildFile("Presets");
}

juce::Array<juce::File> ReChordProcessor::userPresets()
{
    auto files = presetDir().findChildFiles(juce::File::findFiles, false, "*.rcpreset");
    std::sort(files.begin(), files.end(), [](const juce::File& a, const juce::File& b) { return a.getFileName().compareNatural(b.getFileName()) < 0; });
    return files;
}

bool ReChordProcessor::saveUserPreset(const juce::String& rawName)
{
    const auto name = juce::File::createLegalFileName(rawName.trim()).trim();
    if (name.isEmpty()) return false;
    juce::ValueTree t("ReChordPreset");
    t.setProperty("name", name, nullptr);
    t.setProperty("version", RECHORD_VERSION, nullptr);
    for (auto* id : kSoundIds)
        if (auto* p = apvts.getParameter(id)) t.setProperty(id, p->convertFrom0to1(p->getValue()), nullptr);
    if ((int) apvts.getRawParameterValue("osc")->load() == rc::OscSample && !samples.empty()) t.appendChild(samplesTree(), nullptr);
    presetDir().createDirectory();
    auto xml = t.createXml();
    if (xml == nullptr || !xml->writeTo(presetDir().getChildFile(name + ".rcpreset"))) return false;
    userPreset = name;
    ++modelVersion;
    return true;
}

bool ReChordProcessor::loadUserPreset(const juce::File& f)
{
    auto xml = juce::XmlDocument::parse(f);
    if (xml == nullptr || !xml->hasTagName("ReChordPreset")) return false;
    const auto t = juce::ValueTree::fromXml(*xml);
    const auto sm = t.getChildWithName("samples");
    if (sm.isValid()) { audition = -1; samplesFromTree(sm); builtLoop = -1.f; }
    for (auto* id : kSoundIds)
        if (t.hasProperty(id))
            if (auto* p = apvts.getParameter(id))
            {
                p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1((float) (double) t.getProperty(id))); p->endChangeGesture();
            }
    if (sm.isValid()) rebuildSamples();
    userPreset = t.getProperty("name", f.getFileNameWithoutExtension()).toString();
    ++modelVersion;
    return true;
}
