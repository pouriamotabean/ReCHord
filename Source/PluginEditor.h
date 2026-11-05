#pragma once
#include "PluginProcessor.h"
#ifndef HAS_ASSETS
 #define HAS_ASSETS 0
#endif
#if HAS_ASSETS
 #include <BinaryData.h>
#endif

// =====================================================================================================
//  RECHORD  -  write 4 chords, see how every note travels to the next chord, fix it, drag the loop out.
//  Same family as ReKit 2: black body, monochrome, one orange.  Design size 960 x 600 (scaled).
//  by Pouria Motabean
// =====================================================================================================
namespace ui
{
static const juce::Colour white(0xfff2f2f0), dim(0xff8a8a8c), faint(0xff5e5e61), line(0xff2e2e31), dark(0xff3a3a3d),
                          screen(0xff050506), body(0xff0b0b0c), chip(0xff141416), chip2(0xff1c1c1f), chipOn(0xff2a2a2d),
                          card(0xff101012), orange(0xffff5a1f), label(0xffa6a6a8), soft(0xffb0b0b2);

#if HAS_ASSETS
inline juce::Typeface::Ptr face(int i)
{
    static juce::Typeface::Ptr t[6];
    if (t[i] == nullptr)
    {
        const void* d = nullptr; int n = 0;
        switch (i)
        {
            case 0: d = BinaryData::InterLight_ttf;       n = BinaryData::InterLight_ttfSize; break;
            case 1: d = BinaryData::InterRegular_ttf;     n = BinaryData::InterRegular_ttfSize; break;
            case 2: d = BinaryData::InterMedium_ttf;      n = BinaryData::InterMedium_ttfSize; break;
            case 3: d = BinaryData::InterSemiBold_ttf;    n = BinaryData::InterSemiBold_ttfSize; break;
            case 4: d = BinaryData::SpaceMonoRegular_ttf; n = BinaryData::SpaceMonoRegular_ttfSize; break;
            default: d = BinaryData::SpaceMonoBold_ttf;   n = BinaryData::SpaceMonoBold_ttfSize; break;
        }
        t[i] = juce::Typeface::createSystemTypefaceFor(d, (size_t) n);
    }
    return t[i];
}
#endif
inline juce::Font inter(float px, int w = 400, float kern = 0.f)
{
#if HAS_ASSETS
    const int i = w <= 300 ? 0 : w <= 400 ? 1 : w <= 500 ? 2 : 3;
    if (auto f = face(i)) return juce::Font(juce::FontOptions(f).withPointHeight(px).withKerningFactor(kern));
#endif
    return juce::Font(juce::FontOptions(px * 1.2f, w >= 500 ? juce::Font::bold : juce::Font::plain).withKerningFactor(kern));
}
inline juce::Font mono(float px, bool bold = false)
{
#if HAS_ASSETS
    if (auto f = face(bold ? 5 : 4)) return juce::Font(juce::FontOptions(f).withPointHeight(px));
#endif
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), px * 1.2f, bold ? juce::Font::bold : juce::Font::plain));
}
inline juce::String U(const char* s) { return juce::String::fromUTF8(s); }   // text with · and …
inline void shadow(juce::Graphics& g, juce::Rectangle<float> r, float rad, float alpha, float dy, float spread, bool round = false)
{
    for (int i = 4; i >= 1; --i)
    {
        const float e = spread * i / 4.f;
        const auto q = r.translated(0, dy * i / 4.f).expanded(e);
        g.setColour(juce::Colours::black.withAlpha(alpha * 0.22f));
        if (round) g.fillEllipse(q); else g.fillRoundedRectangle(q, rad + e);
    }
}
inline void glow(juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c, float alpha, float spread)
{
    for (int i = 4; i >= 1; --i) { const float e = spread * i / 4.f; g.setColour(c.withAlpha(alpha * 0.18f)); g.fillEllipse(r.expanded(e)); }
}

struct Look : juce::LookAndFeel_V4
{
    Look()
    {
        setColour(juce::TooltipWindow::backgroundColourId, chip2);
        setColour(juce::TooltipWindow::textColourId, white);
        setColour(juce::TooltipWindow::outlineColourId, line);
    }
    void drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider& s) override
    {
        const auto b = juce::Rectangle<float>((float) x, (float) y, (float) w, (float) h);
        const float d = std::min(b.getWidth(), b.getHeight()) - 10.f, r = d * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        // the travel: a thin ring, the set part in orange
        juce::Path track; track.addCentredArc(c.x, c.y, r + 4.f, r + 4.f, 0.f, a0, a1, true);
        g.setColour(line); g.strokePath(track, juce::PathStrokeType(1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const bool bip = (bool) s.getProperties()["bipolar"];
        const float from = bip ? (a0 + a1) * 0.5f : a0;
        if (std::abs(ang - from) > 0.01f)
        {
            juce::Path v; v.addCentredArc(c.x, c.y, r + 4.f, r + 4.f, 0.f, std::min(from, ang), std::max(from, ang), true);
            g.setColour(orange.withAlpha(s.isMouseOverOrDragging() ? 1.f : 0.85f));
            g.strokePath(v, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        juce::Path cap; cap.addEllipse(c.x - r, c.y - r, d, d);
        shadow(g, cap.getBounds(), r, 0.7f, r * 0.2f, r * 0.12f, true);
        juce::ColourGradient gr(juce::Colour(0xff3c3c3f), c.x - r * 0.28f, c.y - r * 0.44f, juce::Colour(0xff19191b), c.x + r * 0.7f, c.y + r * 0.9f, true);
        gr.addColour(0.55, juce::Colour(0xff242427));
        g.setGradientFill(gr); g.fillPath(cap);
        g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawEllipse(c.x - r + 1.f, c.y - r + 1.f, d - 2.f, d - 2.f, 1.f);
        juce::Path m; m.addRoundedRectangle(-1.6f, -r + 5.f, 3.2f, r * 0.42f, 1.6f);
        g.setColour(white); g.fillPath(m, juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const float rad = std::min(9.f, r.getHeight() * 0.5f);
        juce::Colour bg = b.getToggleState() ? chipOn : chip2;
        g.setColour(bg.brighter(over ? (down ? 0.15f : 0.07f) : 0.f)); g.fillRoundedRectangle(r, rad);
        if (b.getToggleState())
        {
            g.setColour(orange); g.fillEllipse(r.getX() + 11.f, r.getCentreY() - 3.f, 6.f, 6.f);
        }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont(mono(10.f, true));
        g.setColour(b.getToggleState() ? white : soft);
        auto r = b.getLocalBounds();
        if (b.getClickingTogglesState()) r.removeFromLeft(14);
        g.drawText(b.getButtonText(), r, juce::Justification::centred);
    }
};

// geometry the display and the chord strip share: the 4 bars (+ bar 1 again, faded, after them)
struct Bars
{
    float x = 0, w = 100;    // left edge and width of one bar
    float centre(int b) const { return x + (b + 0.5f) * w; }
    float lineX(int b) const { return x + b * w; }
};
} // namespace ui

// -----------------------------------------------------------------------------------------------------
// the display: every note of every chord, and the path it takes to the next chord.
//  drag a note onto a note of the next (or previous) chord = connect them
//  click a line = glide <-> end (the note stops, the next one starts fresh)
//  drag a note up / down = one octave
// -----------------------------------------------------------------------------------------------------
class LinkView : public juce::Component
{
public:
    explicit LinkView(ReChordProcessor& p) : proc(p) {}
    ui::Bars bars;

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        shadow(g, r, 14.f, 0.8f, 6.f, 8.f);
        g.setColour(screen); g.fillRoundedRectangle(r, 14.f);
        g.setColour(juce::Colours::white.withAlpha(0.04f)); g.drawRoundedRectangle(r.reduced(0.5f), 14.f, 1.f);
        computeRange();
        const auto& s = proc.score;
        const int N = s.nVoices, tr = proc.transpose();
        const auto params = proc.currentParams();
        const double barBeats = proc.hostBarBeats.load();

        g.saveState();
        g.reduceClipRegion(getLocalBounds().reduced(1));

        // playing bar + playhead
        const double beat = proc.uiBeat.load();
        int playBar = -1;
        float playX = -1;
        if (beat >= 0)
        {
            const double inCycle = std::fmod(beat, rc::kBars * barBeats);
            playBar = (int) (inCycle / barBeats);
            playX = bars.x + (float) (inCycle / barBeats) * bars.w;
            g.setColour(juce::Colours::white.withAlpha(0.025f));
            g.fillRect(juce::Rectangle<float>(bars.lineX(playBar), top, bars.w, bottom - top));
        }

        // octave lines
        for (int m = (lo / 12) * 12; m <= hi; m += 12)
        {
            if (m < lo) continue;
            const float y = yOf((float) m);
            g.setColour(line.withAlpha(0.6f));
            g.drawHorizontalLine((int) y, bars.x - 4.f, r.getRight() - 4.f);
            g.setColour(faint); g.setFont(mono(8.5f));
            g.drawText(rc::noteName(m), juce::Rectangle<float>(6.f, y - 7.f, bars.x - 12.f, 14.f), juce::Justification::centredRight);
        }
        // bar lines + numbers
        for (int b = 0; b <= rc::kBars; ++b)
        {
            g.setColour(line); g.drawVerticalLine((int) bars.lineX(b), top - 6.f, bottom + 4.f);
            g.setColour(b < rc::kBars ? (b == playBar ? orange : dim) : faint);
            g.setFont(mono(9.f, b == playBar));
            g.drawText(b < rc::kBars ? juce::String(b + 1) : "1", juce::Rectangle<float>(bars.lineX(b) + 7.f, 8.f, 30.f, 14.f), juce::Justification::centredLeft);
        }

        // the paths
        hits.clear();
        for (int b = 0; b < rc::kBars; ++b)
            for (int v = 0; v < N; ++v)
            {
                const int nb = (b + 1) % rc::kBars, t = s.perm[b][v];
                const float y0 = yOf((float) (s.pitch[b][v] + tr)), y1 = yOf((float) (s.pitch[nb][t] + tr));
                const float x0 = bars.centre(b), x1 = bars.centre(b + 1), bl = bars.lineX(b + 1);
                const bool sel = (selBar == b && selSlot == v) || (selBar == (b + 1) % rc::kBars && selSlot == t && selBar >= 0);
                const bool hot = hoverLink.first == b && hoverLink.second == v;
                Hit h { b, v, {} };
                if (s.glide[b][v])
                {
                    const float e = (float) (params.early / barBeats) * bars.w, l = (float) (params.late / barBeats) * bars.w;
                    juce::Path p;
                    p.startNewSubPath(x0, y0);
                    p.lineTo(bl - e, y0);
                    for (int k = 1; k <= 24; ++k)
                    {
                        const float u = k / 24.f;
                        p.lineTo(bl - e + u * (e + l), y0 + (y1 - y0) * rc::glideShape(u, params.curve));
                    }
                    p.lineTo(x1, y1);
                    const float a = sel || hot ? 1.f : (selBar >= 0 ? 0.35f : 0.8f);
                    if (sel || hot) { g.setColour(orange.withAlpha(0.18f)); g.strokePath(p, juce::PathStrokeType(7.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
                    g.setColour(orange.withAlpha(a));
                    g.strokePath(p, juce::PathStrokeType(sel || hot ? 2.4f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    h.pts.push_back({ bl - e - 8.f, y0 });
                    for (int k = 0; k <= 12; ++k) h.pts.push_back({ bl - e + k / 12.f * (e + l), y0 + (y1 - y0) * rc::glideShape(k / 12.f, params.curve) });
                    h.pts.push_back({ bl + l + 8.f, y1 });
                }
                else
                {
                    // the note ends at the bar line; the next note starts fresh
                    const juce::Colour c = (sel || hot ? white : dim).withAlpha(selBar >= 0 && !sel && !hot ? 0.4f : 0.9f);
                    g.setColour(c);
                    g.drawLine(x0, y0, bl - 5.f, y0, 1.6f);
                    g.drawLine(bl - 5.f, y0 - 5.f, bl - 5.f, y0 + 5.f, 1.6f);
                    juce::Path dsh, dl; dl.startNewSubPath(bl - 5.f, y0); dl.lineTo(bl + 5.f, y1);
                    const float dash[] = { 2.f, 3.f };
                    juce::PathStrokeType(1.2f).createDashedStroke(dsh, dl, dash, 2);
                    g.setColour(c.withMultipliedAlpha(0.6f)); g.fillPath(dsh);
                    g.setColour(c); g.fillEllipse(bl + 5.f - 3.f, y1 - 3.f, 6.f, 6.f);
                    g.drawLine(bl + 5.f, y1, x1, y1, 1.6f);
                    h.pts = { { bl - 22.f, y0 }, { bl - 5.f, y0 }, { bl + 5.f, y1 }, { bl + 22.f, y1 } };
                }
                hits.push_back(std::move(h));
            }

        if (playX >= 0)
        {
            g.setColour(orange.withAlpha(0.7f));
            g.drawLine(playX, top - 6.f, playX, bottom + 4.f, 1.2f);
        }

        // dragging a new connection
        if (dragging && dragMode == 1)
        {
            g.setColour(orange);
            const float x0 = bars.centre(selBar), y0 = yOf((float) (s.pitch[selBar][selSlot] + tr));
            const float dsh[] = { 4.f, 3.f };
            juce::Path p, d; p.startNewSubPath(x0, y0); p.lineTo(dragPos);
            juce::PathStrokeType(1.6f).createDashedStroke(d, p, dsh, 2);
            g.fillPath(d);
        }

        // the notes (the 5th column is bar 1 again)
        for (int c = 0; c <= rc::kBars; ++c)
        {
            const int b = c % rc::kBars;
            for (int v = 0; v < N; ++v)
            {
                float y = yOf((float) (s.pitch[b][v] + tr));
                const bool sel = selBar == b && selSlot == v && (c < rc::kBars);
                if (sel && dragging && dragMode == 2) y = dragPos.y;
                const bool tgt = dragging && dragMode == 1 && dropBar == c && dropSlot == v;
                const auto cc = juce::Point<float>(bars.centre(c), y);
                const float rad = sel || tgt ? 8.5f : 7.f;
                const float alpha = c == rc::kBars ? 0.35f : 1.f;
                if (sel || tgt) glow(g, juce::Rectangle<float>(rad * 2, rad * 2).withCentre(cc), orange, 0.9f, 10.f);
                g.setColour((sel || tgt ? orange : white).withAlpha(alpha));
                g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(cc));
                if (hoverDot.first == c && hoverDot.second == v && !sel)
                { g.setColour(orange.withAlpha(alpha)); g.drawEllipse(juce::Rectangle<float>(rad * 2 + 5, rad * 2 + 5).withCentre(cc), 1.4f); }
                g.setColour((sel ? white : label).withAlpha(alpha));
                g.setFont(mono(8.5f, sel));
                const int shown = (sel && dragging && dragMode == 2) ? s.pitch[b][v] + 12 * dragOct() : s.pitch[b][v];
                g.drawText(rc::noteName(shown + tr), juce::Rectangle<float>(cc.x + 11.f, cc.y - 15.f, 40.f, 12.f), juce::Justification::centredLeft);
            }
        }
        g.restoreState();

        // how to use it, at the bottom
        g.setColour(faint); g.setFont(mono(8.5f));
        g.drawText(U("drag a note onto the next chord = connect   ·   click a line = glide / end   ·   drag up or down = octave"),
                   juce::Rectangle<float>(bars.x, r.getBottom() - 22.f, r.getWidth() - bars.x - 10.f, 14.f), juce::Justification::centredLeft);
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        auto d = dotAt(e.position);
        auto l = d.first < 0 ? linkAt(e.position) : std::make_pair(-1, -1);
        if (d != hoverDot || l != hoverLink) { hoverDot = d; hoverLink = l; repaint(); }
        setMouseCursor(d.first >= 0 ? juce::MouseCursor::DraggingHandCursor : l.first >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit(const juce::MouseEvent&) override { hoverDot = hoverLink = { -1, -1 }; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragging = false; dragMode = 0; downOnLink = { -1, -1 };
        auto d = dotAt(e.position);
        if (d.first >= 0)
        {
            const int b = d.first % rc::kBars;
            if (selBar >= 0 && d.first == selBar + 1 && !(selBar == b && selSlot == d.second))
            {   // click-click: a selected note, then a note of the next chord
                proc.linkTo(selBar, selSlot, d.second);
                selBar = selSlot = -1; repaint(); return;
            }
            selBar = b; selSlot = d.second; downPos = e.position;
            repaint(); return;
        }
        downOnLink = linkAt(e.position);
        if (downOnLink.first < 0) { selBar = selSlot = -1; repaint(); }
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (selBar < 0 || e.getDistanceFromDragStart() < 4) return;
        dragging = true;
        dragPos = e.position;
        const float dx = std::abs(e.position.x - downPos.x), dy = std::abs(e.position.y - downPos.y);
        if (dragMode == 0) dragMode = dx > dy * 0.8f ? 1 : 2;     // sideways = connect, up/down = octave
        if (dragMode == 1)
        {
            auto d = dotAt(e.position, 22.f);
            const int prevCol = selBar == 0 ? rc::kBars - 1 : selBar - 1;
            const bool next = d.first == selBar + 1, prev = d.first >= 0 && d.first == prevCol;
            dropBar = (next || prev) ? d.first : -1;
            dropSlot = (next || prev) ? d.second : -1;
        }
        repaint();
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (dragging && dragMode == 1 && dropBar >= 0)
        {
            if (dropBar == selBar + 1) proc.linkTo(selBar, selSlot, dropSlot);                 // to the next chord
            else proc.linkTo(dropBar % rc::kBars, dropSlot, selSlot);                          // from the previous chord
            selBar = selSlot = -1;
        }
        else if (dragging && dragMode == 2)
        {
            const int o = dragOct();
            for (int i = 0; i < std::abs(o); ++i) if (!proc.moveOctave(selBar, selSlot, o > 0 ? 1 : -1)) break;
        }
        else if (!dragging && downOnLink.first >= 0)
            proc.toggleMode(downOnLink.first, downOnLink.second);
        dragging = false; dragMode = 0; dropBar = dropSlot = -1;
        repaint();
    }

private:
    struct Hit { int bar, slot; std::vector<juce::Point<float>> pts; };
    ReChordProcessor& proc;
    std::vector<Hit> hits;
    int lo = 48, hi = 84;
    float top = 30, bottom = 100;
    int selBar = -1, selSlot = -1, dropBar = -1, dropSlot = -1, dragMode = 0;
    bool dragging = false;
    juce::Point<float> downPos, dragPos;
    std::pair<int, int> hoverDot { -1, -1 }, hoverLink { -1, -1 }, downOnLink { -1, -1 };

    void computeRange()
    {
        const auto& s = proc.score;
        int mn = 127, mx = 0;
        for (int b = 0; b < rc::kBars; ++b)
            for (int v = 0; v < s.nVoices; ++v) { mn = std::min(mn, s.pitch[b][v]); mx = std::max(mx, s.pitch[b][v]); }
        mn += proc.transpose(); mx += proc.transpose();
        const int span = std::max(20, mx - mn + 8);
        lo = (mn + mx) / 2 - span / 2;
        hi = lo + span;
        top = 36.f; bottom = (float) getHeight() - 34.f;
    }
    float yOf(float pitch) const { return bottom - (pitch - (float) lo) / (float) (hi - lo) * (bottom - top); }
    float pxPerOctave() const { return 12.f / (float) (hi - lo) * (bottom - top); }
    int dragOct() const { return juce::roundToInt((downPos.y - dragPos.y) / pxPerOctave()); }

    std::pair<int, int> dotAt(juce::Point<float> p, float radius = 12.f) const
    {
        const auto& s = proc.score;
        const int tr = proc.transpose();
        std::pair<int, int> best { -1, -1 };
        float bd = radius;
        for (int c = 0; c <= rc::kBars; ++c)
            for (int v = 0; v < s.nVoices; ++v)
            {
                const float d = p.getDistanceFrom({ bars.centre(c), yOf((float) (s.pitch[c % rc::kBars][v] + tr)) });
                if (d < bd) { bd = d; best = { c, v }; }
            }
        return best;
    }
    std::pair<int, int> linkAt(juce::Point<float> p) const
    {
        std::pair<int, int> best { -1, -1 };
        float bd = 7.f;
        for (auto& h : hits)
            for (size_t i = 1; i < h.pts.size(); ++i)
            {
                const juce::Line<float> ln(h.pts[i - 1], h.pts[i]);
                juce::Point<float> np;
                const float d = ln.getDistanceFromPoint(p, np);
                if (d < bd) { bd = d; best = { h.bar, h.slot }; }
            }
        return best;
    }
};

// -----------------------------------------------------------------------------------------------------
// the 4 chords under the display. Click one = the picker opens (root + kind)
// -----------------------------------------------------------------------------------------------------
class ChordStrip : public juce::Component
{
public:
    explicit ChordStrip(ReChordProcessor& p) : proc(p) {}
    ui::Bars bars;
    int open = -1;
    std::function<void(int)> onClick;

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        for (int b = 0; b < rc::kBars; ++b)
        {
            const auto r = cell(b);
            const bool on = open == b, hot = hover == b;
            shadow(g, r, 12.f, 0.6f, 3.f, 4.f);
            g.setColour(on ? chipOn : hot ? chip2.brighter(0.04f) : chip2);
            g.fillRoundedRectangle(r, 12.f);
            if (on) { g.setColour(orange); g.drawRoundedRectangle(r.reduced(0.5f), 12.f, 1.2f); }
            g.setColour(white); g.setFont(inter(21.f, 500));
            g.drawText(rc::chordName(proc.chords[b].root, proc.chords[b].quality, proc.transpose()), r.withTrimmedBottom(10.f), juce::Justification::centred);
            g.setColour(faint); g.setFont(mono(8.f));
            g.drawText("BAR " + juce::String(b + 1), r.withTrimmedTop(r.getHeight() - 18.f), juce::Justification::centred);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = cellAt(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override { const int b = cellAt(e.position); if (b >= 0 && onClick) onClick(b); }

private:
    ReChordProcessor& proc;
    int hover = -1;
    juce::Rectangle<float> cell(int b) const { return { bars.lineX(b) + 5.f, 2.f, bars.w - 10.f, (float) getHeight() - 8.f }; }
    int cellAt(juce::Point<float> p) const { for (int b = 0; b < rc::kBars; ++b) if (cell(b).contains(p)) return b; return -1; }
};

// -----------------------------------------------------------------------------------------------------
// chord picker: 12 roots + 8 kinds, over the bottom of the display. Changes are heard at once.
// -----------------------------------------------------------------------------------------------------
class Picker : public juce::Component
{
public:
    explicit Picker(ReChordProcessor& p) : proc(p) {}
    int bar = 0;
    std::function<void()> onChange, onClose;

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        shadow(g, r, 14.f, 0.9f, 6.f, 10.f);
        g.setColour(card); g.fillRoundedRectangle(r, 14.f);
        g.setColour(line); g.drawRoundedRectangle(r.reduced(0.5f), 14.f, 1.f);
        g.setColour(dim); g.setFont(mono(9.f, true));
        g.drawText("BAR " + juce::String(bar + 1), juce::Rectangle<float>(18.f, 12.f, 100.f, 14.f), juce::Justification::centredLeft);
        g.setColour(faint); g.setFont(mono(8.5f));
        g.drawText("the chord as written (TRANSPOSE moves all of them)", juce::Rectangle<float>(80.f, 12.f, 400.f, 14.f), juce::Justification::centredLeft);
        const auto& c = proc.chords[bar];
        for (int i = 0; i < 12; ++i) chipDraw(g, rootCell(i), rc::rootName(i), c.root == i, hover == i);
        for (int q = 0; q < rc::kNumQualities; ++q) chipDraw(g, qualCell(q), rc::qualityLabel(q), c.quality == q, hover == 100 + q);
        // close
        const auto x = closeCell();
        g.setColour(hover == 200 ? white : dim);
        g.drawLine(x.getX() + 6, x.getY() + 6, x.getRight() - 6, x.getBottom() - 6, 1.4f);
        g.drawLine(x.getRight() - 6, x.getY() + 6, x.getX() + 6, x.getBottom() - 6, 1.4f);
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = at(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = at(e.position);
        auto& c = proc.chords[bar];
        if (h >= 0 && h < 12) proc.setChord(bar, h, c.quality);
        else if (h >= 100 && h < 100 + rc::kNumQualities) proc.setChord(bar, c.root, h - 100);
        else if (h == 200) { if (onClose) onClose(); return; }
        else return;
        repaint();
        if (onChange) onChange();
    }

private:
    ReChordProcessor& proc;
    int hover = -1;
    void chipDraw(juce::Graphics& g, juce::Rectangle<float> r, const juce::String& t, bool on, bool hot) const
    {
        using namespace ui;
        g.setColour(on ? orange : hot ? chipOn : chip2);
        g.fillRoundedRectangle(r, 8.f);
        g.setColour(on ? juce::Colours::white : hot ? white : soft);
        g.setFont(inter(12.f, on ? 600 : 500));
        g.drawText(t, r, juce::Justification::centred);
    }
    juce::Rectangle<float> rootCell(int i) const
    {
        const float w = ((float) getWidth() - 36.f - 11 * 6.f) / 12.f;
        return { 18.f + i * (w + 6.f), 36.f, w, 34.f };
    }
    juce::Rectangle<float> qualCell(int q) const
    {
        const float w = ((float) getWidth() - 36.f - 7 * 6.f) / 8.f;
        return { 18.f + q * (w + 6.f), 78.f, w, 34.f };
    }
    juce::Rectangle<float> closeCell() const { return { (float) getWidth() - 36.f, 8.f, 24.f, 24.f }; }
    int at(juce::Point<float> p) const
    {
        for (int i = 0; i < 12; ++i) if (rootCell(i).contains(p)) return i;
        for (int q = 0; q < rc::kNumQualities; ++q) if (qualCell(q).contains(p)) return 100 + q;
        if (closeCell().contains(p)) return 200;
        return -1;
    }
};

// -----------------------------------------------------------------------------------------------------
// a knob with its name and value under it
// -----------------------------------------------------------------------------------------------------
class Knob : public juce::Component
{
public:
    Knob(ReChordProcessor& p, const char* id, const juce::String& name, bool bipolar = false) : title(name)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        slider.getProperties().set("bipolar", bipolar);
        slider.setMouseDragSensitivity(180);
        param = p.apvts.getParameter(id);
        addAndMakeVisible(slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts, id, slider);
        slider.onValueChange = [this] { repaint(); };
        slider.setDoubleClickReturnValue(true, (double) param->convertFrom0to1(param->getDefaultValue()));
    }
    void resized() override { slider.setBounds(getLocalBounds().withTrimmedBottom(30).withSizeKeepingCentre(52, 52)); }
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        auto r = getLocalBounds().toFloat();
        g.setColour(label); g.setFont(mono(8.5f, true));
        g.drawText(title, r.removeFromBottom(30.f).removeFromTop(13.f), juce::Justification::centred);
        g.setColour(slider.isMouseOverOrDragging() ? orange : dim); g.setFont(mono(8.5f));
        g.drawText(param->getCurrentValueAsText(), getLocalBounds().toFloat().removeFromBottom(16.f), juce::Justification::centred);
    }
    juce::Slider slider;
private:
    juce::String title;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};

// -----------------------------------------------------------------------------------------------------
// drag the 4 bars out as a WAV (at the project tempo). Click = make the file and show it.
// -----------------------------------------------------------------------------------------------------
class DragChip : public juce::Component, public juce::SettableTooltipClient
{
public:
    explicit DragChip(ReChordProcessor& p) : proc(p) { setTooltip("Drag into Cubase: the 4 bars as a loop, at the project tempo"); }
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat().reduced(1.f);
        glow(g, juce::Rectangle<float>(r.getWidth() * 0.6f, 10.f).withCentre(r.getCentre().translated(0, 10)), orange, hot ? 0.5f : 0.3f, 10.f);
        juce::ColourGradient gr(juce::Colour(0xffff7b47), r.getX(), r.getY(), juce::Colour(0xffe2490f), r.getX(), r.getBottom(), false);
        g.setGradientFill(gr); g.fillRoundedRectangle(r, 11.f);
        g.setColour(juce::Colours::white.withAlpha(0.25f)); g.drawRoundedRectangle(r.reduced(0.5f), 11.f, 1.f);
        // arrow
        const float ax = r.getX() + 20.f, ay = r.getCentreY();
        juce::Path a; a.startNewSubPath(ax, ay - 8.f); a.lineTo(ax, ay + 6.f);
        a.startNewSubPath(ax - 5.f, ay + 1.f); a.lineTo(ax, ay + 6.f); a.lineTo(ax + 5.f, ay + 1.f);
        g.setColour(juce::Colours::white); g.strokePath(a, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont(inter(13.f, 600));
        g.drawText(busy ? U("rendering…") : juce::String("DRAG WAV"), juce::Rectangle<float>(r.getX() + 36.f, r.getY() + 5.f, r.getWidth() - 40.f, 18.f), juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.75f)); g.setFont(mono(8.5f));
        g.drawText(ui::U("4 BARS · ") + juce::String(juce::roundToInt(proc.hostBpm.load())) + " BPM",
                   juce::Rectangle<float>(r.getX() + 36.f, r.getY() + 22.f, r.getWidth() - 40.f, 14.f), juce::Justification::centredLeft);
    }
    void mouseEnter(const juce::MouseEvent&) override { hot = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hot = false; repaint(); }
    void mouseDown(const juce::MouseEvent&) override { started = false; }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (started || e.getDistanceFromDragStart() < 5) return;
        started = true;
        auto f = render();
        if (f.existsAsFile()) juce::DragAndDropContainer::performExternalDragDropOfFiles({ f.getFullPathName() }, false, this);
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        if (started) return;
        auto f = render();
        if (f.existsAsFile()) f.revealToUser();
    }
private:
    ReChordProcessor& proc;
    bool hot = false, started = false, busy = false;
    juce::File render()
    {
        busy = true; repaint();
        auto f = proc.renderWav();
        busy = false; repaint();
        return f;
    }
};

// -----------------------------------------------------------------------------------------------------
// the device (design size 960 x 600) and the editor that scales it
// -----------------------------------------------------------------------------------------------------
class Device : public juce::Component
{
public:
    explicit Device(ReChordProcessor& p)
        : proc(p), view(p), strip(p), picker(p), drag(p),
          kAttack(p, "attack", "ATTACK"), kBright(p, "bright", "BRIGHT"), kVib(p, "vibrato", "VIBRATO"), kRel(p, "release", "RELEASE"),
          kEarly(p, "early", "EARLY"), kLate(p, "late", "LATE"), kCurve(p, "curve", "CURVE", true), kSpread(p, "spread", "SPREAD"),
          kTrans(p, "transpose", "TRANSPOSE", true), kOct(p, "octave", "REGISTER", true), kVoices(p, "voices", "VOICES"), kVol(p, "volume", "VOLUME")
    {
        addAndMakeVisible(view);
        addAndMakeVisible(strip);
        addChildComponent(picker);
        addAndMakeVisible(drag);
        for (auto* k : knobs()) addAndMakeVisible(*k);

        play.setClickingTogglesState(true);
        play.onClick = [this] { proc.preview = play.getToggleState(); };
        live.setClickingTogglesState(true);
        liveAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "live", live);
        smartB.onClick = [this] { proc.smart(); };
        play.setTooltip("Play the 4 bars here (while Cubase is stopped)");
        live.setTooltip("Play along when Cubase plays (bar 1 of the project = chord 1)");
        smartB.setTooltip("Fresh smart voicing: undoes your connection / octave edits");
        for (auto* b : { &play, &live, &smartB }) addAndMakeVisible(*b);

        strip.onClick = [this](int b)
        {
            if (picker.isVisible() && picker.bar == b) { closePicker(); return; }
            picker.bar = b; strip.open = b;
            picker.setVisible(true); picker.toFront(false);
            strip.repaint(); picker.repaint();
        };
        picker.onClose = [this] { closePicker(); };
        picker.onChange = [this] { strip.repaint(); view.repaint(); };
        setSize(960, 600);
    }
    void closePicker() { picker.setVisible(false); strip.open = -1; strip.repaint(); }

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        g.fillAll(body);
        juce::ColourGradient sheen(juce::Colour(0xff151517), 0.f, 0.f, body, 0.f, 220.f, false);
        g.setGradientFill(sheen); g.fillRect(getLocalBounds().removeFromTop(220));
        g.setColour(white); g.setFont(inter(22.f, 600, -0.02f));
        g.drawText("ReChord", juce::Rectangle<float>(28.f, 18.f, 140.f, 28.f), juce::Justification::centredLeft);
        g.setColour(orange); g.fillEllipse(124.f, 27.f, 6.f, 6.f);
        g.setColour(faint); g.setFont(mono(8.5f));
        g.drawText(U("BY POURIA MOTABEAN   ·   v" RECHORD_VERSION), juce::Rectangle<float>(140.f, 23.f, 260.f, 14.f), juce::Justification::centredLeft);
        // group names over the knobs
        g.setColour(faint); g.setFont(mono(8.f, true));
        auto group = [&](const char* t, Knob& first, Knob& last)
        {
            const float x0 = (float) first.getX() + 8.f, x1 = (float) last.getRight() - 8.f, y = (float) first.getY() - 10.f;
            g.drawText(t, juce::Rectangle<float>(x0, y - 7.f, x1 - x0, 14.f), juce::Justification::centred);
            const float tw = 70.f;
            g.setColour(line);
            g.drawHorizontalLine((int) y, x0, (x0 + x1) * 0.5f - tw * 0.5f);
            g.drawHorizontalLine((int) y, (x0 + x1) * 0.5f + tw * 0.5f, x1);
            g.setColour(faint);
        };
        group("SOUND", kAttack, kRel);
        group("GLIDE", kEarly, kSpread);
        group("KEY", kTrans, kVol);
    }
    void resized() override
    {
        drag.setBounds(782, 14, 154, 44);
        smartB.setBounds(686, 20, 84, 32);
        live.setBounds(566, 20, 112, 32);
        play.setBounds(482, 20, 76, 32);

        view.setBounds(24, 72, 912, 300);
        ui::Bars b; b.x = 56.f; b.w = (912.f - 56.f - 6.f) / 4.62f;
        view.bars = b;
        strip.bars = b;          // the strip has the same x as the display: each chord sits under its bar
        strip.setBounds(24, 382, 912, 66);
        picker.setBounds(24 + 40, 252, 912 - 80, 124);

        auto k = knobs();
        float x = 24.f;
        for (int i = 0; i < 12; ++i)
        {
            if (i == 4 || i == 8) x += 24.f;
            k[(size_t) i]->setBounds((int) x, 478, 72, 92);
            x += 72.f;
        }
    }
    void refresh() { view.repaint(); strip.repaint(); if (picker.isVisible()) picker.repaint(); drag.repaint(); }
    void tickPlay() { if (play.getToggleState() != proc.preview.load()) play.setToggleState(proc.preview.load(), juce::dontSendNotification); }

    LinkView view;
private:
    ReChordProcessor& proc;
    ChordStrip strip;
    Picker picker;
    DragChip drag;
    Knob kAttack, kBright, kVib, kRel, kEarly, kLate, kCurve, kSpread, kTrans, kOct, kVoices, kVol;
    juce::TextButton play { "PLAY" }, live { "WITH CUBASE" }, smartB { "SMART" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> liveAtt;
    std::vector<Knob*> knobs() { return { &kAttack, &kBright, &kVib, &kRel, &kEarly, &kLate, &kCurve, &kSpread, &kTrans, &kOct, &kVoices, &kVol }; }
};

class ReChordEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit ReChordEditor(ReChordProcessor& p) : AudioProcessorEditor(p), proc(p), device(p)
    {
        setLookAndFeel(&look);
        addAndMakeVisible(device);
        tooltips.setLookAndFeel(&look);
        setResizable(true, true);
        setResizeLimits(672, 420, 1920, 1200);
        getConstrainer()->setFixedAspectRatio(960.0 / 600.0);
        setSize(960, 600);
        startTimerHz(30);
    }
    ~ReChordEditor() override { tooltips.setLookAndFeel(nullptr); setLookAndFeel(nullptr); }
    void resized() override { device.setTransform(juce::AffineTransform::scale((float) getWidth() / 960.f)); }
    void paint(juce::Graphics& g) override { g.fillAll(ui::body); }

private:
    void timerCallback() override
    {
        const int mv = proc.modelVersion.load();
        const double beat = proc.uiBeat.load();
        const int pv = (int) (proc.currentParams().early * 1000 + proc.currentParams().late * 100000 + proc.currentParams().curve * 7777 + proc.transpose() * 31);
        if (mv != lastModel || beat != lastBeat || pv != lastParams || ++slow % 15 == 0)
        {
            lastModel = mv; lastBeat = beat; lastParams = pv;
            device.refresh();
        }
        device.tickPlay();
    }
    ReChordProcessor& proc;
    ui::Look look;
    juce::TooltipWindow tooltips { this, 600 };
    Device device;
    int lastModel = -1, lastParams = 0, slow = 0;
    double lastBeat = -2;
};
