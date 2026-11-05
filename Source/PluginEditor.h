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

// ---------------- 2.0: architectural minimalism on cold, worn metal ----------------
// a rectangle with its corners cut at 45 degrees (mask: 1 top-left, 2 top-right, 4 bottom-right, 8 bottom-left)
inline juce::Path chamfer(juce::Rectangle<float> r, float c, int mask = 15)
{
    juce::Path p;
    const float x0 = r.getX(), y0 = r.getY(), x1 = r.getRight(), y1 = r.getBottom();
    c = std::min(c, std::min(r.getWidth(), r.getHeight()) * 0.45f);
    p.startNewSubPath((mask & 1) ? x0 + c : x0, y0);
    if (mask & 2) { p.lineTo(x1 - c, y0); p.lineTo(x1, y0 + c); } else p.lineTo(x1, y0);
    if (mask & 4) { p.lineTo(x1, y1 - c); p.lineTo(x1 - c, y1); } else p.lineTo(x1, y1);
    if (mask & 8) { p.lineTo(x0 + c, y1); p.lineTo(x0, y1 - c); } else p.lineTo(x0, y1);
    if (mask & 1) p.lineTo(x0, y0 + c);
    p.closeSubPath();
    return p;
}

// the metal: grain, horizontal brushing, scratches and pits - the same every time (fixed seed), made once
inline const juce::Image& metalTexture()
{
    static juce::Image img;
    if (img.isValid()) return img;
    const int W = 1024, H = 1024;
    img = juce::Image(juce::Image::ARGB, W, H, true);
    juce::Random rnd(20261106);
    std::vector<float> row((size_t) W);
    {
        juce::Image::BitmapData bd(img, juce::Image::BitmapData::writeOnly);
        for (int y = 0; y < H; ++y)
        {
            // brushing: each row a slightly different, slowly varying level
            // a periodic walk (wraps at the tile edge, so the tiling shows no seam)
            float acc = 0.f;
            const float rowLevel = (rnd.nextFloat() - 0.5f) * 0.012f;
            for (int x = 0; x < W + 400; ++x) { acc = acc * 0.985f + (rnd.nextFloat() - 0.5f) * 0.012f; if (x >= 400) row[(size_t) (x - 400)] = acc; }
            for (int x = 0; x < 64; ++x) { const float t = x / 64.f; row[(size_t) (W - 64 + x)] = row[(size_t) (W - 64 + x)] * (1 - t) + row[0] * t; }
            for (int x = 0; x < W; ++x)
            {
                // stored as a signed deviation: light = white with alpha, dark = black with alpha
                const float grain = (rnd.nextFloat() - 0.5f) * 0.09f;
                const float dv = grain + row[(size_t) x] * 0.7f + rowLevel * 2.f;
                const float al = juce::jlimit(0.f, 1.f, std::abs(dv) * 2.2f);
                bd.setPixelColour(x, y, (dv > 0 ? juce::Colours::white : juce::Colours::black).withAlpha(al));
            }
        }
    }
    juce::Graphics g(img);
    // scratches: thin, light, at all angles but mostly along the brushing; some are arcs
    for (int i = 0; i < 260; ++i)
    {
        const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H;
        const float len = 8.f + std::pow(rnd.nextFloat(), 2.f) * 160.f;
        const float ang = (rnd.nextFloat() < 0.7f ? (rnd.nextFloat() - 0.5f) * 0.35f : rnd.nextFloat() * 3.1416f);
        const float a = 0.05f + rnd.nextFloat() * 0.12f;
        juce::Path p;
        p.startNewSubPath(x, y);
        const float bend = (rnd.nextFloat() - 0.5f) * len * 0.15f;
        p.quadraticTo(x + std::cos(ang) * len * 0.5f - std::sin(ang) * bend, y + std::sin(ang) * len * 0.5f + std::cos(ang) * bend,
                      x + std::cos(ang) * len, y + std::sin(ang) * len);
        g.setColour(juce::Colours::white.withAlpha(a));
        g.strokePath(p, juce::PathStrokeType(0.6f + rnd.nextFloat() * 0.5f));
        g.setColour(juce::Colours::black.withAlpha(a * 0.8f));
        g.strokePath(p, juce::PathStrokeType(0.5f), juce::AffineTransform::translation(0.6f, 0.8f));
    }
    // pits and stains
    for (int i = 0; i < 900; ++i)
    {
        const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H, r = 0.4f + rnd.nextFloat() * 1.3f;
        g.setColour((rnd.nextFloat() < 0.5f ? juce::Colours::black : juce::Colours::white).withAlpha(0.05f + rnd.nextFloat() * 0.08f));
        g.fillEllipse(x - r, y - r, r * 2, r * 2);
    }
    for (int i = 0; i < 18; ++i)
    {
        const float x = rnd.nextFloat() * W, y = rnd.nextFloat() * H, r = 30.f + rnd.nextFloat() * 120.f;
        juce::ColourGradient cg(juce::Colours::black.withAlpha(0.06f), x, y, juce::Colours::transparentBlack, x + r, y, true);
        g.setGradientFill(cg); g.fillEllipse(x - r, y - r, r * 2, r * 2);
    }
    return img;
}

// fill a shape with the metal at a given darkness (base colour) - the texture only modulates it
inline void fillMetal(juce::Graphics& g, const juce::Path& p, juce::Colour base, float texture = 0.55f)
{
    g.setColour(base); g.fillPath(p);
    g.saveState();
    g.reduceClipRegion(p);
    g.setTiledImageFill(metalTexture(), 0, 0, texture);
    g.fillPath(p);
    g.restoreState();
}

// a panel: raised (frames, cards) or recessed (screens). Light comes from the top
inline void panel(juce::Graphics& g, juce::Rectangle<float> r, float cut, juce::Colour base, bool recessed, int mask = 15)
{
    auto p = chamfer(r, cut, mask);
    if (!recessed)
    {
        g.setColour(juce::Colours::black.withAlpha(0.45f));
        g.fillPath(chamfer(r.translated(0, 3.f).expanded(1.f), cut, mask));
    }
    fillMetal(g, p, base, recessed ? 0.35f : 0.75f);
    if (recessed)
    {
        // inner shadow at the top, a hairline of light at the bottom edge
        juce::ColourGradient sh(juce::Colours::black.withAlpha(0.55f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getY() + 14.f, false);
        g.saveState(); g.reduceClipRegion(p); g.setGradientFill(sh); g.fillRect(r.withHeight(14.f)); g.restoreState();
        g.setColour(juce::Colours::black.withAlpha(0.8f)); g.strokePath(p, juce::PathStrokeType(1.2f));
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.drawLine(r.getX() + cut, r.getBottom() + 0.8f, r.getRight() - cut, r.getBottom() + 0.8f, 1.f);
    }
    else
    {
        g.setColour(juce::Colours::white.withAlpha(0.075f));
        g.strokePath(chamfer(r.reduced(0.5f), cut, mask), juce::PathStrokeType(1.f));
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        g.drawLine(r.getX() + cut, r.getY() + 1.f, r.getRight() - cut, r.getY() + 1.f, 1.f);
    }
}

// a screw: recessed hole, worn cap, a cross slot turned a little differently each time
inline void screw(juce::Graphics& g, juce::Point<float> c, float rad, float angle)
{
    g.setColour(juce::Colours::black.withAlpha(0.7f)); g.fillEllipse(juce::Rectangle<float>(rad * 2 + 2.f, rad * 2 + 2.f).withCentre(c));
    juce::ColourGradient cg(juce::Colour(0xff4a4a4d), c.x - rad * 0.5f, c.y - rad * 0.6f, juce::Colour(0xff161618), c.x + rad * 0.6f, c.y + rad * 0.8f, true);
    g.setGradientFill(cg); g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(c));
    g.setColour(juce::Colours::white.withAlpha(0.12f)); g.drawEllipse(juce::Rectangle<float>(rad * 2 - 1.f, rad * 2 - 1.f).withCentre(c), 0.7f);
    juce::Path slot;
    slot.addRectangle(-rad * 0.7f, -0.6f, rad * 1.4f, 1.2f);
    slot.addRectangle(-0.6f, -rad * 0.7f, 1.2f, rad * 1.4f);
    g.setColour(juce::Colours::black.withAlpha(0.85f));
    g.fillPath(slot, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
    g.setColour(juce::Colours::white.withAlpha(0.08f));
    g.fillPath(slot, juce::AffineTransform::rotation(angle).translated(c.x + 0.5f, c.y + 0.6f));
}

// a small chamfered chip (buttons, segments): metal, darker when off
inline void chipShape(juce::Graphics& g, juce::Rectangle<float> r, bool on, bool hot, float cut = 5.f)
{
    auto p = chamfer(r, cut, 2 | 8);
    fillMetal(g, p, on ? juce::Colour(0xff2b2b2e) : hot ? juce::Colour(0xff222225) : juce::Colour(0xff17171a), 0.35f);
    g.setColour(juce::Colours::white.withAlpha(on ? 0.12f : 0.06f)); g.strokePath(p, juce::PathStrokeType(1.f));
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
        const float d = std::min(b.getWidth(), b.getHeight()) - 14.f, r = d * 0.5f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        // the scale: ticks around, the set part in orange
        for (int t = 0; t <= 10; ++t)
        {
            const float ta = a0 + (a1 - a0) * t / 10.f;
            const float r0 = r + 4.f, r1 = r + ((t % 5 == 0) ? 7.5f : 6.f);
            g.setColour(line.brighter(0.15f));
            g.drawLine(c.x + std::sin(ta) * r0, c.y - std::cos(ta) * r0, c.x + std::sin(ta) * r1, c.y - std::cos(ta) * r1, 1.f);
        }
        const bool bip = (bool) s.getProperties()["bipolar"];
        const float from = bip ? (a0 + a1) * 0.5f : a0;
        if (std::abs(ang - from) > 0.01f)
        {
            juce::Path v; v.addCentredArc(c.x, c.y, r + 2.5f, r + 2.5f, 0.f, std::min(from, ang), std::max(from, ang), true);
            g.setColour(orange.withAlpha(s.isMouseOverOrDragging() ? 1.f : 0.85f));
            g.strokePath(v, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }
        // the skirt: knurled
        shadow(g, juce::Rectangle<float>(d, d).withCentre(c), r, 0.8f, r * 0.22f, r * 0.14f, true);
        juce::ColourGradient sk(juce::Colour(0xff36363a), c.x - r * 0.4f, c.y - r * 0.7f, juce::Colour(0xff0f0f11), c.x + r * 0.5f, c.y + r, true);
        g.setGradientFill(sk); g.fillEllipse(juce::Rectangle<float>(d, d).withCentre(c));
        juce::Path knurl;
        const int teeth = 40;
        for (int k = 0; k < teeth; ++k)
        {
            const float ka = (float) k / teeth * 6.2831853f;
            knurl.startNewSubPath(c.x + std::sin(ka) * (r - 3.2f), c.y - std::cos(ka) * (r - 3.2f));
            knurl.lineTo(c.x + std::sin(ka) * (r - 0.4f), c.y - std::cos(ka) * (r - 0.4f));
        }
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.strokePath(knurl, juce::PathStrokeType(1.f));
        g.setColour(juce::Colours::white.withAlpha(0.05f)); g.strokePath(knurl, juce::PathStrokeType(0.6f), juce::AffineTransform::rotation(0.035f, c.x, c.y));
        // the cap: a chamfered top (bevel ring), brushed in circles
        const float rc = r * 0.74f;
        juce::ColourGradient bev(juce::Colour(0xff4d4d52), c.x - rc, c.y - rc, juce::Colour(0xff121214), c.x + rc, c.y + rc, false);
        g.setGradientFill(bev); g.fillEllipse(juce::Rectangle<float>(rc * 2 + 3.f, rc * 2 + 3.f).withCentre(c));
        juce::ColourGradient top(juce::Colour(0xff2c2c30), c.x, c.y - rc, juce::Colour(0xff1a1a1d), c.x, c.y + rc, false);
        g.setGradientFill(top); g.fillEllipse(juce::Rectangle<float>(rc * 2 - 1.f, rc * 2 - 1.f).withCentre(c));
        for (float rr = rc * 0.25f; rr < rc - 1.f; rr += 1.6f)
        {
            g.setColour(juce::Colours::white.withAlpha(0.018f));
            g.drawEllipse(juce::Rectangle<float>(rr * 2, rr * 2).withCentre(c), 0.5f);
        }
        // the mark
        const float m0 = rc * 0.25f, m1 = r - 1.f;
        juce::Path mk; mk.startNewSubPath(0, -m0); mk.lineTo(0, -m1);
        g.setColour(orange.withAlpha(0.35f));
        g.strokePath(mk, juce::PathStrokeType(4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::rotation(ang).translated(c.x, c.y));
        g.setColour(orange);
        g.strokePath(mk, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        chipShape(g, r, b.getToggleState() || down, over, std::min(7.f, r.getHeight() * 0.3f));
        if (b.getToggleState())
        {
            g.setColour(orange); g.fillEllipse(r.getX() + 11.f, r.getCentreY() - 3.f, 6.f, 6.f);   // lit
        }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        g.setFont(mono(b.getHeight() < 24 ? 8.5f : 10.f, true));
        g.setColour(b.getToggleState() ? white : soft);
        auto r = b.getLocalBounds();
        if (b.getToggleState()) r.removeFromLeft(14);
        g.drawText(b.getButtonText(), r, juce::Justification::centred);
    }
};

// geometry the display and the chord strip share: beats -> x.  The chord before the pattern sits left of bar 1,
// the chord after it right of bar 4 ("ghost" columns).
struct Bars
{
    float x0 = 0, w = 100;      // x of beat 0, width of one bar
    double barBeats = 4;
    float x(double beat) const { return x0 + (float) (beat / barBeats) * w; }
    float ghostLeft() const { return x0 - 0.42f * w; }
    float ghostRight() const { return x0 + rc::kBars * w + 0.42f * w; }
};
} // namespace ui

// -----------------------------------------------------------------------------------------------------
// the display: every note of every chord and the path it takes to the next chord.
//  each glide has 3 handles, like automation: START and END (drag sideways = when), the middle one = curve
//  drag the END handle onto another note = reconnect.  drag a note sideways onto a note = connect,
//  up / down = one octave.  click a line = glide <-> end.  double-click a line = its default timing
// -----------------------------------------------------------------------------------------------------
class LinkView : public juce::Component
{
public:
    explicit LinkView(ReChordProcessor& p) : proc(p) {}
    ui::Bars bars;
    int page = 0;                                 // 8 bars: 0 = bars 1-4, 1 = bars 5-8
    std::function<void()> onSelection, onPage;
    int navHot = 0;
    juce::String learnText;                       // LEARN / drop status, shown on the bottom line

    // the selected glide (for the panel): t, v or -1
    int selectedT() const { return selLinkT; }
    int selectedV() const { return selLinkV; }
    bool hasSelLink() const { return selLinkV >= 0; }

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        panel(g, r.reduced(1.f), 12.f, screen, true);
        layout();
        const auto& s = proc.score;
        const int N = s.nVoices, n = s.nSlots, tr = proc.shift();

        g.saveState();
        g.reduceClipRegion(chamfer(r.reduced(2.f), 11.f));

        // playing slot + playhead
        const double beat = proc.uiBeat.load();
        float playX = -1;
        int playBar = -1;
        if (beat >= 0)
        {
            const double inCycle = std::fmod(beat, proc.pattern.activeBars() * bars.barBeats);
            playBar = (int) (inCycle / bars.barBeats);
            if (playBar >= ws() && playBar < we())
            {
                playX = bx(inCycle);
                g.setColour(juce::Colours::white.withAlpha(0.025f));
                g.fillRect(juce::Rectangle<float>(bx(playBar * bars.barBeats), top, bars.w, bottom - top));
            }
        }
        // outside this page / the pattern: darker
        g.setColour(juce::Colours::black.withAlpha(0.35f));
        g.fillRect(juce::Rectangle<float>(0, 0, bars.x(0), r.getHeight()));
        g.fillRect(juce::Rectangle<float>(bx(we() * bars.barBeats), 0, r.getWidth(), r.getHeight()));

        // octave lines
        for (int m = (lo / 12) * 12; m <= hi; m += 12)
        {
            if (m < lo) continue;
            const float y = yOf((float) m);
            g.setColour(line.withAlpha(0.6f));
            g.drawHorizontalLine((int) y, 40.f, r.getRight() - 4.f);
            g.setColour(faint); g.setFont(mono(8.5f));
            g.drawText(rc::noteName(m), juce::Rectangle<float>(4.f, y - 7.f, 32.f, 14.f), juce::Justification::centredRight);
        }
        // bar lines + numbers, half-bar lines
        for (int b = ws(); b <= we(); ++b)
        {
            const float x = bx(b * bars.barBeats);
            g.setColour(line); g.drawVerticalLine((int) x, top - 6.f, bottom + 4.f);
            if (b < we())
            {
                if (proc.pattern.split[proc.pattern.barAt(b)]) { g.setColour(line.withAlpha(0.5f)); g.drawVerticalLine((int) bx((b + 0.5) * bars.barBeats), top + 4.f, bottom); }
                g.setColour(b == playBar ? orange : dim);
                g.setFont(mono(9.f, b == playBar));
                g.drawText(juce::String(proc.pattern.barAt(b) + 1), juce::Rectangle<float>(x + 7.f, 8.f, 30.f, 14.f), juce::Justification::centredLeft);
            }
        }
        for (int side : { -1, -2 })
        {
            const bool nav = side == -1 ? page > 0 : we() < proc.pattern.activeBars();      // the other page of a long pattern
            const auto rr = navRect(side);
            if (nav)
            {
                chipShape(g, rr, navHot == side, false, 5.f);
                g.setColour(navHot == side ? white : orange);
            }
            else g.setColour(faint);
            g.setFont(mono(8.f, true));
            g.drawText(nav ? (side == -1 ? U("‹ PAGE 1") : U("PAGE 2 ›")) : endLabel(side), rr, juce::Justification::centred);
        }

        // the paths
        hits.clear(); handles.clear(); handlePts.clear();
        for (auto& tr0 : transitions())
            for (int v = 0; v < N; ++v) drawLink(g, tr0, v);

        // a note being dragged to connect
        if (dragKind == DragNote && noteMode == 1)
        {
            g.setColour(orange);
            const float dsh[] = { 4.f, 3.f };
            juce::Path p, d; p.startNewSubPath(colX(selCol), yOf((float) (s.pitchAt(selCol, selV) + tr))); p.lineTo(dragPos);
            juce::PathStrokeType(1.6f).createDashedStroke(d, p, dsh, 2);
            g.fillPath(d);
        }

        // the notes
        for (int c = -1; c <= n; ++c)
        {
            if (!colShown(c)) continue;
            const bool ghost = c == firstCol() - 1 || c == endCol();
            const bool mirror = ghost && (c < 0 || c >= n) && s.loop();
            const bool neighbour = ghost && c >= 0 && c < n;              // a real chord of the other page
            for (int v = 0; v < N; ++v)
            {
                float y = yOf((float) (colPitch(c, v) + tr));
                const bool sel = dragKind == DragNote && selCol == c && selV == v;
                if (sel && noteMode == 2) y = yOf((float) (colPitch(c, v) + dragSemis() + tr));     // snaps to the note it will be
                const bool tgt = dropCol == c && dropV == v;
                const auto cc = juce::Point<float>(colX(c), y);
                const float rad = sel || tgt ? 8.5f : 7.f;
                const float alpha = mirror ? 0.3f : ghost ? 0.7f : 1.f;
                if (sel || tgt) glow(g, juce::Rectangle<float>(rad * 2, rad * 2).withCentre(cc), orange, 0.9f, 10.f);
                g.setColour((sel || tgt ? orange : white).withAlpha(alpha));
                if (ghost && !mirror && !neighbour) { g.drawEllipse(juce::Rectangle<float>(rad * 2 - 2, rad * 2 - 2).withCentre(cc), 2.f); }
                else g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(cc));
                if (hoverDot.first == c && hoverDot.second == v && !sel)
                { g.setColour(orange.withAlpha(alpha)); g.drawEllipse(juce::Rectangle<float>(rad * 2 + 5, rad * 2 + 5).withCentre(cc), 1.4f); }
                g.setColour((sel ? white : label).withAlpha(alpha));
                g.setFont(mono(8.5f, sel));
                const int shown = (sel && noteMode == 2) ? colPitch(c, v) + dragSemis() : colPitch(c, v);
                g.drawText(rc::noteName(shown + tr), juce::Rectangle<float>(cc.x + 10.f, cc.y - 15.f, 40.f, 12.f), juce::Justification::centredLeft);
            }
        }
        if (playX >= 0) { g.setColour(orange.withAlpha(0.7f)); g.drawLine(playX, top - 6.f, playX, bottom + 4.f, 1.2f); }
        g.restoreState();
        const auto bottomLine = juce::Rectangle<float>(40.f, r.getBottom() - 21.f, r.getWidth() - 200.f, 14.f);
        if (learnText.isNotEmpty())
        {
            g.setColour(orange); g.setFont(mono(8.5f, true));
            g.drawText(learnText, bottomLine.withWidth(r.getWidth() - 50.f), juce::Justification::centredLeft);
        }
        else if (selLinkV >= 0 && selLinkV < N)
        {
            // the selected glide, in numbers (ALL / RESET buttons sit right of it)
            const auto& l = s.linkAt(selLinkT, selLinkV);
            g.setColour(l.glide ? orange : dim); g.setFont(mono(8.5f, true));
            g.drawText(l.glide ? "SELECTED GLIDE   " + timingText(l) + "   (shift = no snap)" : juce::String("THIS NOTE ENDS AT THE CHANGE - click the line to make it glide"),
                       bottomLine, juce::Justification::centredLeft);
        }
        else
        {
            g.setColour(faint); g.setFont(mono(8.f));
            g.drawText(ui::U("glide handles: start · curve · end   —   drag a line's end onto a note = reconnect   —   click a line = glide / end   —   double-click = reset"),
                       bottomLine.withWidth(r.getWidth() - 50.f), juce::Justification::centredLeft);
        }
    }

    void mouseMove(const juce::MouseEvent& e) override
    {
        int nh = 0;
        for (int side : { -1, -2 }) if (navRect(side).contains(e.position)) nh = side;
        if (nh != navHot) { navHot = nh; repaint(); }
        auto h = handleAt(e.position);
        auto d = h.kind < 0 ? dotAt(e.position) : std::make_pair(-99, -1);
        auto l = (h.kind < 0 && d.first == -99) ? linkAt(e.position) : Hit2 {};
        const bool ch = d != hoverDot || l.t != hoverT || l.v != hoverV || h.kind != hoverHandle;
        hoverDot = d; hoverT = l.t; hoverV = l.v; hoverHandle = h.kind;
        if (h.kind >= 0) { hoverT = h.t; hoverV = h.v; }
        if (ch) repaint();
        if (e.position.y < top - 8.f) { setMouseCursor(juce::MouseCursor::PointingHandCursor); return; }
        setMouseCursor(h.kind == 1 ? juce::MouseCursor::UpDownResizeCursor : h.kind >= 0 ? juce::MouseCursor::LeftRightResizeCursor
                       : d.first != -99 ? juce::MouseCursor::DraggingHandCursor : l.v >= 0 ? juce::MouseCursor::PointingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseExit(const juce::MouseEvent&) override { hoverDot = { -99, -1 }; hoverT = hoverV = -1; hoverHandle = -1; repaint(); }

    void mouseDown(const juce::MouseEvent& e) override
    {
        dragKind = None; noteMode = 0; dropCol = -99; dropV = -1; moved = false;
        for (int side : { -1, -2 })
            if (navRect(side).contains(e.position) && (side == -1 ? page > 0 : we() < proc.pattern.activeBars()))
            {
                page = side == -1 ? page - 1 : page + 1;
                setSel(-2, -1);
                if (onPage) onPage();
                repaint();
                return;
            }
        // the bar numbers: play from here
        if (e.position.y < top - 8.f && e.position.x >= bars.x(0) && e.position.x <= bx(we() * bars.barBeats))
        {
            seekFrom(e.position.x);
            dragKind = Seek;
            return;
        }
        downPos = dragPos = e.position;
        if (auto h = handleAt(e.position); h.kind >= 0)
        {
            dragKind = DragHandle; handle = h;
            setSel(h.t, h.v);
            grab = proc.score.linkAt(h.t, h.v);
            return;
        }
        if (auto d = dotAt(e.position); d.first != -99)
        {
            const auto& s = proc.score;
            if (s.loop() && (d.first < 0 || d.first >= s.nSlots)) { d.first = d.first < 0 ? s.nSlots - 1 : 0; }   // the faded copies are the real chords
            dragKind = DragNote; selCol = d.first; selV = d.second;
            repaint(); return;
        }
        if (auto l = linkAt(e.position); l.v >= 0)
        {
            dragKind = ClickLink; setSel(l.t, l.v);
            if (e.getNumberOfClicks() == 2) { proc.resetTiming(l.t, l.v); dragKind = None; }
            return;
        }
        setSel(-2, -1);
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        shiftDrag = e.mods.isShiftDown();
        if (dragKind == Seek) { seekFrom(e.position.x); return; }
        if (e.getDistanceFromDragStart() < 3) return;
        moved = true;
        dragPos = e.position;
        const auto& s = proc.score;
        if (dragKind == DragHandle)
        {
            const float pxBeat = bars.w / (float) bars.barBeats;
            const auto snap = [&](float b) { return e.mods.isShiftDown() ? b : std::round(b * 8.f) / 8.f; };   // 1/32 notes; shift = free
            auto l = grab;
            if (handle.kind == 0) l.early = std::max(0.f, snap((handle.bx - e.position.x) / pxBeat));
            else if (handle.kind == 2)
            {
                l.late = std::max(0.f, snap((e.position.x - handle.bx) / pxBeat));
                auto d = dotAt(e.position, 16.f);
                dropCol = d.first == handle.toCol ? d.first : -99;
                dropV = dropCol != -99 ? d.second : -1;
            }
            else l.curve = curveFor(e.position.y);
            proc.setTiming(handle.t, handle.v, l.early, l.late, l.curve);
            repaint();
            return;
        }
        if (dragKind == DragNote)
        {
            const float dx = std::abs(e.position.x - downPos.x), dy = std::abs(e.position.y - downPos.y);
            if (noteMode == 0) noteMode = dx > dy * 0.8f ? 1 : 2;
            if (noteMode == 1)
            {
                auto d = dotAt(e.position, 22.f);
                const bool ok = d.first != -99 && (d.first == selCol + 1 || d.first == selCol - 1) && colShown(d.first);
                dropCol = ok ? d.first : -99; dropV = ok ? d.second : -1;
                (void) s;
            }
            repaint();
        }
    }
    void mouseUp(const juce::MouseEvent&) override
    {
        const auto& s = proc.score;
        if (dragKind == DragHandle && handle.kind == 2 && dropCol != -99 && dropV >= 0 && dropV != s.linkAt(handle.t, handle.v).to)
        {
            proc.setTiming(handle.t, handle.v, grab.early, grab.late, grab.curve);   // a reconnect keeps the timing it had
            proc.linkTo(handle.t, handle.v, dropV);
        }
        else if (dragKind == DragNote && noteMode == 1 && dropCol != -99)
        {
            if (dropCol == selCol + 1) proc.linkTo(transFrom(selCol), selV, dropV);
            else proc.linkTo(transFrom(dropCol), dropV, selV);
        }
        else if (dragKind == DragNote && noteMode == 2)
        {
            proc.movePitch(selCol, selV, dragSemis());
        }
        else if (dragKind == ClickLink && !moved) proc.toggleGlide(selLinkT, selLinkV);
        dragKind = None; noteMode = 0; dropCol = -99; dropV = -1;
        repaint();
    }

    void setSel(int t, int v) { selLinkT = t; selLinkV = v; repaint(); if (onSelection) onSelection(); }

private:
    enum { None, DragNote, DragHandle, ClickLink, Seek };
    void seekFrom(float x)
    {
        const double beat = juce::jlimit(0.0, proc.pattern.activeBars() * bars.barBeats - 0.01, ws() * bars.barBeats + (double) (x - bars.x(0)) / bars.w * bars.barBeats);
        auto* l = proc.apvts.getParameter("listen");
        if (l->getValue() < 0.5f) { l->beginChangeGesture(); l->setValueNotifyingHost(1.f); l->endChangeGesture(); }
        proc.seekTo(beat);
    }
    struct Trans { int t, from, to; };                     // transition t drawn from column `from` to column `to`
    struct Hit { int t, v; std::vector<juce::Point<float>> pts; };
    struct Hit2 { int t = -1, v = -1; };
    struct Handle { int kind = -1, t = -1, v = -1, toCol = 0; float bx = 0; };   // kind 0 start, 1 curve, 2 end

    ReChordProcessor& proc;
    std::vector<Hit> hits;
    std::vector<Handle> handles;
    std::vector<juce::Point<float>> handlePts;
    int lo = 48, hi = 84;
    float top = 30, bottom = 100;
    int dragKind = None, noteMode = 0, selCol = -1, selV = -1, dropCol = -99, dropV = -1;
    int selLinkT = -2, selLinkV = -1, hoverT = -1, hoverV = -1, hoverHandle = -1;
    bool moved = false;
    Handle handle;
    rc::Link grab;
    juce::Point<float> downPos, dragPos;
    std::pair<int, int> hoverDot { -99, -1 };

    // ---- what is shown ----
    // the page: bars ws()..we()-1; the columns firstCol()..endCol()-1 are its chords, firstCol()-1 and endCol() the ghosts
    int ws() const { return proc.pattern.activeBars() > rc::kBars ? std::min(page, 1) * rc::kBars : 0; }
    int we() const { return std::min(proc.pattern.activeBars(), ws() + rc::kBars); }
    int firstCol() const { const auto& s = proc.score; int i = 0; while (i < s.nSlots && s.start[i] < (float) ws() - 0.01f) ++i; return i; }
    int endCol() const { const auto& s = proc.score; int i = 0; while (i < s.nSlots && s.start[i] < (float) we() - 0.01f) ++i; return i; }
    float bx(double beat) const { return bars.x(beat - ws() * bars.barBeats); }
    juce::Rectangle<float> navRect(int side) const { return { (side == -1 ? bars.ghostLeft() : bars.ghostRight()) - 42.f, 6.f, 84.f, 17.f }; }
    bool colShown(int c) const
    {
        const auto& s = proc.score;
        const int a = firstCol(), b = endCol();
        if (c < a - 1 || c > b) return false;
        if (c < 0) return s.loop() || s.entryMode == rc::ToChord;
        if (c >= s.nSlots) return s.exitMode != rc::None;
        return true;
    }
    int colPitch(int c, int v) const
    {
        const auto& s = proc.score;
        if (c < 0) return s.loop() ? s.pitch[s.nSlots - 1][v] : s.entryPitch[v];
        return s.pitchAt(c, v);
    }
    float colX(int c) const
    {
        const auto& s = proc.score;
        if (c < firstCol()) return bars.ghostLeft();
        if (c >= endCol()) return bars.ghostRight();
        return bx((s.start[c] + s.start[c + 1]) * 0.5 * bars.barBeats);
    }
    int transFrom(int col) const { const auto& s = proc.score; return col < 0 ? (s.loop() ? s.nSlots - 1 : -1) : col; }
    std::vector<Trans> transitions() const
    {
        const auto& s = proc.score;
        std::vector<Trans> t;
        (void) s;
        for (int c = firstCol() - 1; c < endCol(); ++c)
            if (colShown(c) && colShown(c + 1)) t.push_back({ transFrom(c), c, c + 1 });
        return t;
    }
    juce::String endLabel(int which) const
    {
        const auto& s = proc.score;
        const auto& p = proc.pattern;
        if (s.loop()) return which == -1 ? "FROM " + juce::String(p.barAt(p.activeBars() - 1) + 1) : "TO " + juce::String(p.barAt(0) + 1);
        const int m = which == -1 ? s.entryMode : s.exitMode;
        if (m == rc::None) return which == -1 ? "START" : "END";
        const auto& c = which == -1 ? p.entry : p.exit;
        return (which == -1 ? "FROM " : "TO ") + juce::String(rc::chordName(c.root, c.quality, proc.transpose()));
    }

    void layout()
    {
        const auto& s = proc.score;
        int mn = 127, mx = 0;
        for (int c = -1; c <= s.nSlots; ++c)
            if (colShown(c))
                for (int v = 0; v < s.nVoices; ++v) { mn = std::min(mn, colPitch(c, v)); mx = std::max(mx, colPitch(c, v)); }
        mn += proc.shift(); mx += proc.shift();
        const int span = std::max(20, mx - mn + 8);
        lo = (mn + mx) / 2 - span / 2;
        hi = lo + span;
        top = 36.f; bottom = (float) getHeight() - 34.f;
    }
    float yOf(float pitch) const { return bottom - (pitch - (float) lo) / (float) (hi - lo) * (bottom - top); }
    float pxPerOctave() const { return 12.f / (float) (hi - lo) * (bottom - top); }
    // up / down: semitones; with shift: octaves
    int dragSemis() const
    {
        const float d = (downPos.y - dragPos.y) / pxPerOctave() * 12.f;
        return shiftDrag ? 12 * juce::roundToInt(d / 12.f) : juce::roundToInt(d);
    }
    bool shiftDrag = false;

    // one link: a glide (orange, with handles when hovered / selected) or an end (grey)
    void drawLink(juce::Graphics& g, const Trans& tr0, int v)
    {
        using namespace ui;
        const auto& s = proc.score;
        const int tp = proc.shift();
        const auto& l = s.linkAt(tr0.t, v);
        const float x0 = colX(tr0.from), x1 = colX(tr0.to);
        const float y0 = yOf((float) (colPitch(tr0.from, v) + tp)), y1 = yOf((float) (colPitch(tr0.to, l.to) + tp));
        const double bBeat = tr0.to >= s.nSlots ? s.nBars * bars.barBeats : tr0.to == 0 ? 0.0 : s.start[tr0.to] * bars.barBeats;
        const float bx = this->bx(bBeat);
        const float pxBeat = bars.w / (float) bars.barBeats;
        const bool sel = selLinkT == tr0.t && selLinkV == v;
        const bool hot = hoverT == tr0.t && hoverV == v;
        const bool fromNote = dragKind == DragNote && ((selCol == tr0.from && selV == v) || (selCol == tr0.to && selV == l.to));
        const bool dim2 = (selLinkV >= 0 || dragKind == DragNote) && !sel && !hot && !fromNote;
        const bool mirror = s.loop() && tr0.from < 0;     // the copy of the last change, left of bar 1

        Hit h { tr0.t, v, {} };
        if (l.glide)
        {
            // the glide as it plays: limited to half of each chord
            const double fromLen = tr0.from < 0 ? bars.barBeats : rc::slotBeats(s, tr0.from, bars.barBeats);
            const double toLen = tr0.to >= s.nSlots ? bars.barBeats : rc::slotBeats(s, tr0.to, bars.barBeats);
            const float e = (float) std::min((double) l.early, fromLen * 0.5) * pxBeat, la = (float) std::min((double) l.late, toLen * 0.5) * pxBeat;
            juce::Path p;
            p.startNewSubPath(x0, y0);
            p.lineTo(bx - e, y0);
            const int mode = (int) proc.apvts.getRawParameterValue("mode")->load();
            const float a = (float) (colPitch(tr0.from, v) + tp), b = (float) (colPitch(tr0.to, l.to) + tp);
            for (int k = 1; k <= 48; ++k)
            {
                const float u = k / 48.f;
                p.lineTo(bx - e + u * (e + la), yOf(rc::glideValue(a, b, u, l.curve, mode, s.scaleMask)));
            }
            p.lineTo(x1, y1);
            const float alpha = sel || hot || fromNote ? 1.f : dim2 ? 0.3f : mirror ? 0.4f : 0.8f;
            if (sel || hot) { g.setColour(orange.withAlpha(0.16f)); g.strokePath(p, juce::PathStrokeType(8.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded)); }
            g.setColour(orange.withAlpha(alpha));
            g.strokePath(p, juce::PathStrokeType(sel || hot ? 2.4f : 1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            h.pts.push_back({ bx - e - 10.f, y0 });
            for (int k = 0; k <= 16; ++k) h.pts.push_back({ bx - e + k / 16.f * (e + la), y0 + (y1 - y0) * rc::glideShape(k / 16.f, l.curve) });
            h.pts.push_back({ bx + la + 10.f, y1 });

            // handles
            const juce::Point<float> hs(bx - e, y0), he(bx + la, y1),
                                     hm(bx - e + 0.5f * (e + la), y0 + (y1 - y0) * rc::glideShape(0.5f, l.curve));
            handles.push_back({ 0, tr0.t, v, tr0.to, bx }); handlePts.push_back(hs);
            handles.push_back({ 1, tr0.t, v, tr0.to, bx }); handlePts.push_back(hm);
            handles.push_back({ 2, tr0.t, v, tr0.to, bx }); handlePts.push_back(he);
            if (sel || hot)
            {
                for (int k = 0; k < 3; ++k)
                {
                    const auto pt = k == 0 ? hs : k == 1 ? hm : he;
                    const bool hh = hoverHandle == k && hot;
                    const auto rr = juce::Rectangle<float>(hh ? 11.f : 9.f, hh ? 11.f : 9.f).withCentre(pt);
                    g.setColour(screen); g.fillRoundedRectangle(rr.expanded(1.5f), 2.5f);
                    g.setColour(k == 1 ? white : orange);
                    if (k == 1) g.fillEllipse(rr); else g.fillRoundedRectangle(rr, 2.f);
                }
                if (dragKind == DragHandle && sel)
                {
                    g.setColour(white); g.setFont(mono(8.5f, true));
                    const auto txt = handle.kind == 1 ? curveText(l.curve) : beatText(handle.kind == 0 ? l.early : l.late);
                    const auto pt = handle.kind == 0 ? hs : handle.kind == 1 ? hm : he;
                    g.drawText(txt, juce::Rectangle<float>(pt.x - 40.f, pt.y - 24.f, 80.f, 12.f), juce::Justification::centred);
                }
            }
        }
        else
        {
            const juce::Colour c = (sel || hot || fromNote ? white : dim).withAlpha(dim2 ? 0.35f : 0.9f);
            g.setColour(c);
            g.drawLine(x0, y0, bx - 5.f, y0, 1.6f);
            g.drawLine(bx - 5.f, y0 - 5.f, bx - 5.f, y0 + 5.f, 1.6f);
            juce::Path dsh, dl; dl.startNewSubPath(bx - 5.f, y0); dl.lineTo(bx + 5.f, y1);
            const float dash[] = { 2.f, 3.f };
            juce::PathStrokeType(1.2f).createDashedStroke(dsh, dl, dash, 2);
            g.setColour(c.withMultipliedAlpha(0.6f)); g.fillPath(dsh);
            g.setColour(c); g.fillEllipse(bx + 5.f - 3.f, y1 - 3.f, 6.f, 6.f);
            g.drawLine(bx + 5.f, y1, x1, y1, 1.6f);
            h.pts = { { bx - 22.f, y0 }, { bx - 5.f, y0 }, { bx + 5.f, y1 }, { bx + 22.f, y1 } };
        }
        hits.push_back(std::move(h));
    }

    static juce::String beatText(float b)
    {
        struct N { float b; const char* t; } names[] = { { 0.f, "0" }, { 0.125f, "1/32" }, { 0.25f, "1/16" }, { 0.375f, "3/32" }, { 0.5f, "1/8" },
                                                        { 0.75f, "3/16" }, { 1.f, "1/4" }, { 1.5f, "3/8" }, { 2.f, "1/2" }, { 3.f, "3/4" }, { 4.f, "1 bar" } };
        for (auto& n : names) if (std::abs(b - n.b) < 0.01f) return n.t;
        return juce::String(b, 2) + " beat";
    }
public:
    static juce::String timingText(const rc::Link& l)
    {
        return "START " + beatText(l.early) + "   END " + beatText(l.late) + "   CURVE " + curveText(l.curve);
    }
    static juce::String curveText(float c)
    {
        return std::abs(c) < 0.03f ? juce::String("smooth") : (c < 0 ? "early " : "late ") + juce::String(juce::roundToInt(std::abs(c) * 100));
    }
private:
    // curve from where the middle handle is dragged: the glide passes through it
    float curveFor(float y) const
    {
        const auto& s = proc.score;
        const auto tr = transitions();
        const auto& l = s.linkAt(handle.t, handle.v);
        float y0 = 0, y1 = 0;
        for (auto& t : tr)
            if (t.t == handle.t && t.to == handle.toCol)
            { y0 = yOf((float) (colPitch(t.from, handle.v) + proc.shift())); y1 = yOf((float) (colPitch(t.to, l.to) + proc.shift())); }
        if (std::abs(y1 - y0) < 4.f) return juce::jlimit(-1.f, 1.f, grab.curve + (downPos.y - y) / 80.f);
        const float target = juce::jlimit(0.03f, 0.97f, (y - y0) / (y1 - y0));
        float a = -1.f, b = 1.f;                       // glideShape(0.5) falls as the curve rises
        for (int i = 0; i < 30; ++i) { const float m = 0.5f * (a + b); if (rc::glideShape(0.5f, m) > target) a = m; else b = m; }
        return juce::jlimit(-1.f, 1.f, 0.5f * (a + b));
    }

    std::pair<int, int> dotAt(juce::Point<float> p, float radius = 12.f) const
    {
        const auto& s = proc.score;
        const int tr = proc.shift();
        std::pair<int, int> best { -99, -1 };
        float bd = radius;
        for (int c = -1; c <= s.nSlots; ++c)
        {
            if (!colShown(c)) continue;
            for (int v = 0; v < s.nVoices; ++v)
            {
                const float d = p.getDistanceFrom({ colX(c), yOf((float) (colPitch(c, v) + tr)) });
                if (d < bd) { bd = d; best = { c, v }; }
            }
        }
        return best;
    }
    Hit2 linkAt(juce::Point<float> p) const
    {
        Hit2 best;
        float bd = 7.f;
        for (auto& h : hits)
            for (size_t i = 1; i < h.pts.size(); ++i)
            {
                const juce::Line<float> ln(h.pts[i - 1], h.pts[i]);
                juce::Point<float> np;
                const float d = ln.getDistanceFromPoint(p, np);
                if (d < bd) { bd = d; best = { h.t, h.v }; }
            }
        return best;
    }
    // handles answer only on the hovered / selected glide, so they never get in the way of the notes
    Handle handleAt(juce::Point<float> p) const
    {
        Handle best;
        float bd = 9.f;
        for (size_t i = 0; i < handles.size(); ++i)
        {
            const auto& h = handles[i];
            const bool active = (h.t == selLinkT && h.v == selLinkV) || (h.t == hoverT && h.v == hoverV);
            if (!active) continue;
            const float d = p.getDistanceFrom(handlePts[i]);
            if (d < bd) { bd = d; best = h; }
        }
        return best;
    }
public:
    void clearHandles() { handles.clear(); handlePts.clear(); }
};

// -----------------------------------------------------------------------------------------------------
// the 8 bars, two rows. Click a chord = picker (and the display shows its page). The dot top-left = bar on / off
// (off = not in the pattern: 4 bars on = a 4-bar loop). The ½ corner splits a bar into two half-bar chords.
// The small cards left / right: the chord before and after the pattern (LOOP / START / END / a chord).
// -----------------------------------------------------------------------------------------------------
class ChordStrip : public juce::Component
{
public:
    explicit ChordStrip(ReChordProcessor& p) : proc(p) {}
    ui::Bars bars;
    int page = 0;
    int open = -99;
    std::function<void(int)> onClick;      // key: 0..15, -1 before, -2 after, -99 close
    std::function<void(int)> onPage;       // the display should show this page

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto& P = proc.pattern;
        for (int b = 0; b < rc::kMaxBars; ++b)
        {
            const bool on = P.on[b], sp = P.split[b];
            const bool here = on && pageOf(b) == page;
            const auto bar = barRect(b);
            shadow(g, bar, 10.f, here ? 0.6f : 0.3f, 2.f, 3.f);
            for (int h = 0; h < (sp ? 2 : 1); ++h)
            {
                const int key = b * 2 + h;
                const auto r = cell(key);
                const bool isOpen = open == key, hot = hover == key;
                const int m = (sp ? (h == 0 ? 1 | 8 : 2 | 4) : 15);
                const auto cp = chamfer(r, 8.f, m);
                fillMetal(g, cp, !on ? juce::Colour(0xff0e0e10) : isOpen ? juce::Colour(0xff2a2a2e) : hot ? juce::Colour(0xff232326) : here ? juce::Colour(0xff1d1d20) : juce::Colour(0xff151517), on ? 0.7f : 0.4f);
                g.setColour(juce::Colours::white.withAlpha(on ? 0.07f : 0.03f)); g.strokePath(cp, juce::PathStrokeType(1.f));
                if (isOpen) { g.setColour(orange); g.strokePath(chamfer(r.reduced(0.5f), 8.f, m), juce::PathStrokeType(1.2f)); }
                else if (on && here) { g.setColour(orange.withAlpha(0.55f)); g.fillRect(r.getX() + 10.f, r.getBottom() - 3.f, std::min(18.f, r.getWidth() * 0.3f), 1.5f); }
                g.setColour(!on ? faint.withAlpha(0.6f) : here ? white : soft);
                g.setFont(inter(sp ? 14.f : 16.5f, 500));
                g.drawText(label(key), r.withTrimmedLeft(sp && h == 1 ? 0.f : 14.f), juce::Justification::centred);
            }
            // on / off
            const auto d = dotCell(b);
            g.setColour(hover == 200 + b ? white : on ? orange : faint);
            if (on) g.fillEllipse(d.reduced(2.f)); else g.drawEllipse(d.reduced(2.5f), 1.3f);
            g.setColour(on ? (here ? dim : faint) : faint.withAlpha(0.7f)); g.setFont(mono(7.5f, true));
            g.drawText(juce::String(b + 1).paddedLeft('0', 2), d.translated(1.f, 12.f).withWidth(16.f), juce::Justification::centredLeft);
            // split corner
            if (on)
            {
                const auto s2 = splitCell(b);
                g.setColour(hover == 100 + b ? white : sp ? orange : faint);
                g.setFont(mono(9.f, true));
                g.drawText(ui::U("½"), s2, juce::Justification::centred);
            }
        }
        for (int which : { -1, -2 })
        {
            const auto r = cell(which);
            const bool isOpen = open == which, hot = hover == which;
            panel(g, r, 8.f, isOpen ? juce::Colour(0xff222225) : hot ? juce::Colour(0xff151518) : juce::Colour(0xff0a0a0b), true);
            if (isOpen) { g.setColour(orange); g.strokePath(chamfer(r.reduced(0.5f), 8.f), juce::PathStrokeType(1.2f)); }
            const auto& sc = proc.score;
            const int m = which == -1 ? sc.entryMode : sc.exitMode;
            const auto& c = which == -1 ? proc.pattern.entry : proc.pattern.exit;
            g.setColour(faint); g.setFont(mono(7.5f, true));
            g.drawText(which == -1 ? "BEFORE" : "AFTER", r.withHeight(18.f).translated(0, 8.f), juce::Justification::centred);
            g.setColour(m == rc::ToChord ? white : soft); g.setFont(m == rc::ToChord ? inter(13.f, 500) : mono(9.f, true));
            g.drawText(m == rc::Loop ? juce::String("LOOP") : m == rc::None ? juce::String(which == -1 ? "START" : "END") : juce::String(rc::chordName(c.root, c.quality, proc.transpose())),
                       r.withTrimmedTop(22.f), juce::Justification::centred);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = at(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -99; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int k = at(e.position);
        if (k >= 200) { proc.setBarOn(k - 200, !proc.pattern.on[k - 200]); if (onClick) onClick(-99); repaint(); return; }
        if (k >= 100) { proc.setSplit(k - 100, !proc.pattern.split[k - 100]); if (onClick) onClick(-99); repaint(); return; }
        if (k == -99) return;
        if (k >= 0 && proc.pattern.on[k / 2] && pageOf(k / 2) != page && onPage) onPage(pageOf(k / 2));
        if (onClick) onClick(k);
    }

private:
    ReChordProcessor& proc;
    int hover = -99;
    int pageOf(int bar) const { return proc.pattern.indexOf(bar) / rc::kBars; }
    juce::String label(int key) const
    {
        const auto& sc = proc.score;
        if (proc.pattern.on[key / 2])
            for (int i = 0; i < sc.nSlots; ++i) if (sc.key[i] == key) return proc.slotLabel(i);
        const auto& c = proc.pattern.chord[key];
        return rc::chordName(c.root, c.quality, proc.transpose());
    }
    float rowH() const { return ((float) getHeight() - 6.f) / 2.f; }
    juce::Rectangle<float> barRect(int b) const
    {
        const float x0 = bars.x(0) - bars.w * 0.12f, w = (bars.x(rc::kBars * bars.barBeats) + bars.w * 0.12f - x0) / 4.f;
        return { x0 + (b % 4) * w + 3.f, 1.f + (b / 4) * (rowH() + 4.f), w - 6.f, rowH() };
    }
    juce::Rectangle<float> cell(int key) const
    {
        if (key == -1) return { 6.f, 1.f, 52.f, (float) getHeight() - 4.f };
        if (key == -2) return { (float) getWidth() - 58.f, 1.f, 52.f, (float) getHeight() - 4.f };
        const auto r = barRect(key / 2);
        if (!proc.pattern.split[key / 2]) return r;
        const float half = r.getWidth() * 0.5f;
        return key % 2 ? r.withTrimmedLeft(half + 2.f) : r.withWidth(half - 2.f);
    }
    juce::Rectangle<float> dotCell(int b) const { const auto r = barRect(b); return { r.getX() + 5.f, r.getY() + 4.f, 12.f, 12.f }; }
    juce::Rectangle<float> splitCell(int b) const { const auto r = barRect(b); return { r.getRight() - 22.f, r.getY() + 2.f, 18.f, 16.f }; }
    int at(juce::Point<float> p) const
    {
        for (int b = 0; b < rc::kMaxBars; ++b) if (dotCell(b).expanded(4.f).contains(p)) return 200 + b;
        for (int b = 0; b < rc::kMaxBars; ++b) if (proc.pattern.on[b] && splitCell(b).expanded(2.f).contains(p)) return 100 + b;
        for (int k : { -1, -2 }) if (cell(k).contains(p)) return k;
        for (int b = 0; b < rc::kMaxBars; ++b)
            for (int h = 0; h < (proc.pattern.split[b] ? 2 : 1); ++h) if (cell(b * 2 + h).contains(p)) return b * 2 + h;
        return -99;
    }
};

// -----------------------------------------------------------------------------------------------------
// chord picker: 12 roots + 8 kinds (+ LOOP / START-END for the chords before and after the pattern)
// -----------------------------------------------------------------------------------------------------
class Picker : public juce::Component
{
public:
    explicit Picker(ReChordProcessor& p) : proc(p) {}
    int key = 0;
    std::function<void()> onChange, onClose;

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        panel(g, r.reduced(1.f), 12.f, juce::Colour(0xff161618), false);
        for (auto c : { juce::Point<float>(12.f, r.getBottom() - 12.f), juce::Point<float>(r.getRight() - 12.f, r.getBottom() - 12.f) }) screw(g, c, 3.5f, c.x * 0.13f);
        g.setColour(dim); g.setFont(mono(9.f, true));
        const bool end = key < 0;
        const juce::String title = key == -1 ? juce::String("BEFORE THE PATTERN") : key == -2 ? juce::String("AFTER THE PATTERN")
                                 : (proc.pattern.split[key / 2] ? "BAR " + juce::String(key / 2 + 1) + ui::U(key % 2 ? " · 2ND HALF" : " · 1ST HALF") : "BAR " + juce::String(key / 2 + 1));
        g.drawText(title, juce::Rectangle<float>(18.f, 12.f, 300.f, 14.f), juce::Justification::centredLeft);
        g.setColour(faint); g.setFont(mono(8.5f));
        g.drawText(end ? "LOOP = the pattern repeats.  A chord = the notes glide into / out of it (join two patterns)"
                       : "the chord as written (TRANSPOSE moves all of them)",
                   juce::Rectangle<float>(end ? 200.f : 130.f, 12.f, 560.f, 14.f), juce::Justification::centredLeft);
        const auto& c = proc.chordFor(key);
        const int mode = key == -1 ? proc.score.entryMode : key == -2 ? proc.score.exitMode : rc::ToChord;
        const bool chordOn = mode == rc::ToChord;
        for (int i = 0; i < 12; ++i) chipDraw(g, rootCell(i), rc::rootName(i), chordOn && c.root == i, hover == i);
        for (int q = 0; q < rc::kNumQualities; ++q) chipDraw(g, qualCell(q), rc::qualityLabel(q), chordOn && c.quality == q, hover == 100 + q);
        if (end)
        {
            chipDraw(g, modeCell(0), "LOOP", mode == rc::Loop, hover == 300);
            chipDraw(g, modeCell(1), key == -1 ? "START" : "END", mode == rc::None, hover == 301);
        }
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
        const auto c = proc.chordFor(key);
        auto setC = [&](int root, int q)
        {
            if (key < 0) { proc.chordFor(key) = { root, q }; proc.setEnd(key, rc::ToChord); }
            else proc.setChord(key, root, q);
        };
        if (h >= 0 && h < 12) setC(h, c.quality);
        else if (h >= 100 && h < 100 + rc::kNumQualities) setC(c.root, h - 100);
        else if (h == 300) proc.setEnd(key, rc::Loop);
        else if (h == 301) proc.setEnd(key, rc::None);
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
        if (on) { g.setColour(orange); g.fillPath(chamfer(r, 6.f, 2 | 8)); }
        else chipShape(g, r, hot, false, 6.f);
        g.setColour(on ? juce::Colours::white : hot ? white : soft);
        g.setFont(inter(12.f, on ? 600 : 500));
        g.drawText(t, r, juce::Justification::centred);
    }
    float left() const { return key < 0 ? 112.f : 18.f; }
    juce::Rectangle<float> modeCell(int i) const { return { 18.f, 36.f + i * 42.f, 82.f, 34.f }; }
    juce::Rectangle<float> rootCell(int i) const
    {
        const float w = ((float) getWidth() - left() - 18.f - 11 * 6.f) / 12.f;
        return { left() + i * (w + 6.f), 36.f, w, 34.f };
    }
    juce::Rectangle<float> qualCell(int q) const
    {
        const float w = ((float) getWidth() - left() - 18.f - 7 * 6.f) / 8.f;
        return { left() + q * (w + 6.f), 78.f, w, 34.f };
    }
    juce::Rectangle<float> closeCell() const { return { (float) getWidth() - 36.f, 8.f, 24.f, 24.f }; }
    int at(juce::Point<float> p) const
    {
        if (key < 0) for (int i = 0; i < 2; ++i) if (modeCell(i).contains(p)) return 300 + i;
        for (int i = 0; i < 12; ++i) if (rootCell(i).contains(p)) return i;
        for (int q = 0; q < rc::kNumQualities; ++q) if (qualCell(q).contains(p)) return 100 + q;
        if (closeCell().contains(p)) return 200;
        return -1;
    }
};

// -----------------------------------------------------------------------------------------------------
// glide mode: SMOOTH / STEPS / ROBOT (one parameter, three segments)
// -----------------------------------------------------------------------------------------------------
class ModeSwitch : public juce::Component
{
public:
    explicit ModeSwitch(ReChordProcessor& p) : proc(p) {}
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const int m = (int) proc.apvts.getRawParameterValue("mode")->load();
        const char* names[] = { "SMOOTH", "STEPS", "ROBOT" };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = seg(i);
            chipShape(g, r, m == i, hover == i, 4.f);
            if (m == i) { g.setColour(orange); g.fillRect(r.getX() + 10.f, r.getCentreY() - 2.5f, 5.f, 5.f); }
            g.setColour(m == i ? white : soft); g.setFont(mono(9.5f, true));
            g.drawText(names[i], r.withTrimmedLeft(m == i ? 14.f : 0.f), juce::Justification::centred);
        }
        g.setColour(label); g.setFont(mono(8.5f, true));
        g.drawText("MODE", juce::Rectangle<float>(0, (float) getHeight() - 30.f, (float) getWidth(), 13.f), juce::Justification::centred);
    }
    void mouseMove(const juce::MouseEvent& e) override { int h = -1; for (int i = 0; i < 3; ++i) if (seg(i).contains(e.position)) h = i; if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i)
            if (seg(i).contains(e.position))
            {
                auto* p = proc.apvts.getParameter("mode");
                p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1((float) i)); p->endChangeGesture();
                repaint();
            }
    }
private:
    ReChordProcessor& proc;
    int hover = -1;
    juce::Rectangle<float> seg(int i) const { return { 4.f, 2.f + i * 19.f, (float) getWidth() - 8.f, 16.f }; }
};

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
    void resized() override { const int d = std::min(56, getWidth() - 14); slider.setBounds(getLocalBounds().withTrimmedBottom(30).withSizeKeepingCentre(d, d)); }
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        auto r = getLocalBounds().toFloat();
        g.setColour(label); g.setFont(mono(8.5f, true));
        g.drawText(titleText ? titleText() : title, r.removeFromBottom(30.f).removeFromTop(13.f), juce::Justification::centred);
        g.setColour(slider.isMouseOverOrDragging() ? orange : dim); g.setFont(mono(8.5f));
        g.drawText(valueText ? valueText() : param->getCurrentValueAsText(), getLocalBounds().toFloat().removeFromBottom(16.f), juce::Justification::centred);
    }
    juce::Slider slider;
    std::function<juce::String()> titleText, valueText;     // when the meaning depends on another control (CHARACTER)
private:
    juce::String title;
    juce::RangedAudioParameter* param = nullptr;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
};


// -----------------------------------------------------------------------------------------------------
// the sound: FM / ANALOG / STRINGS / VOICE (one parameter, four segments)
// -----------------------------------------------------------------------------------------------------
class TypeSwitch : public juce::Component
{
public:
    explicit TypeSwitch(ReChordProcessor& p) : proc(p) {}
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const int m = (int) proc.apvts.getRawParameterValue("osc")->load();
        const char* names[] = { "FM", "ANALOG", "STRINGS", "VOICE", "TINE", "SINE", "KEYS", "BOWED", "808" };
        panel(g, getLocalBounds().toFloat().reduced(0.5f), 7.f, juce::Colour(0xff0a0a0b), true);
        for (int i = 0; i < 9; ++i)
        {
            const auto r = seg(i).reduced(1.f);
            if (m == i || hover == i) chipShape(g, r, m == i, hover == i, 3.f);
            g.setColour(m == i ? white : soft); g.setFont(mono(7.5f, true));
            g.drawText(names[i], r.withTrimmedLeft(m == i ? 6.f : 0.f), juce::Justification::centred);
            if (m == i) { g.setColour(orange); g.fillRect(r.getX() + 4.f, r.getCentreY() - 2.f, 4.f, 4.f); }
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { int h = -1; for (int i = 0; i < 9; ++i) if (seg(i).contains(e.position)) h = i; if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 9; ++i)
            if (seg(i).contains(e.position))
            {
                auto* p = proc.apvts.getParameter("osc");
                p->beginChangeGesture(); p->setValueNotifyingHost(p->convertTo0to1((float) i)); p->endChangeGesture();
                repaint();
            }
    }
private:
    ReChordProcessor& proc;
    int hover = -1;
    juce::Rectangle<float> seg(int i) const
    {
        const float w = ((float) getWidth() - 6.f) / 5.f, h = ((float) getHeight() - 6.f) / 2.f;
        return { 3.f + (i % 5) * w, 3.f + (i / 5) * h, w, h };
    }
};

// -----------------------------------------------------------------------------------------------------
// preset box:  <  NAME  >   (click the name = all presets).  A dot after the name = changed since loading.
// -----------------------------------------------------------------------------------------------------
class PresetBox : public juce::Component
{
public:
    explicit PresetBox(ReChordProcessor& p) : proc(p) {}
    std::function<void()> onOpen;
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        panel(g, r.reduced(0.5f), 7.f, juce::Colour(0xff0a0a0b), true);
        for (int k : { 0, 2 })
        {
            const auto a = arrow(k);
            g.setColour(hover == k ? white : dim);
            juce::Path p;
            const float cx = a.getCentreX(), cy = a.getCentreY(), d = k == 0 ? 1.f : -1.f;
            p.startNewSubPath(cx + 2.5f * d, cy - 5.f); p.lineTo(cx - 2.5f * d, cy); p.lineTo(cx + 2.5f * d, cy + 5.f);
            g.strokePath(p, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }
        const auto& ps = rc::kPresets[proc.presetIndex];
        const auto mid = r.reduced(26.f, 0.f);
        g.setColour(faint); g.setFont(mono(7.5f, true));
        g.drawText(ps.group, mid.withHeight(14.f).translated(0, 3.f), juce::Justification::centred);
        g.setColour(hover == 1 ? white : label.brighter(0.3f)); g.setFont(inter(12.5f, 500));
        g.drawText(juce::String(ps.name) + (proc.presetModified() ? "  *" : ""), mid.withTrimmedTop(13.f), juce::Justification::centred);
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = at(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = at(e.position);
        if (h == 0) proc.loadPreset(proc.presetIndex - 1);
        else if (h == 2) proc.loadPreset(proc.presetIndex + 1);
        else if (onOpen) onOpen();
        repaint();
    }
private:
    ReChordProcessor& proc;
    int hover = -1;
    juce::Rectangle<float> arrow(int k) const { return k == 0 ? juce::Rectangle<float>(2.f, 0.f, 24.f, (float) getHeight()) : juce::Rectangle<float>((float) getWidth() - 26.f, 0.f, 24.f, (float) getHeight()); }
    int at(juce::Point<float> p) const { return arrow(0).contains(p) ? 0 : arrow(2).contains(p) ? 2 : 1; }
};

// every preset: a tab per group, the group's presets under it
class PresetPanel : public juce::Component
{
public:
    explicit PresetPanel(ReChordProcessor& p) : proc(p) {}
    std::function<void()> onClose;
    void visibilityChanged() override { if (isVisible()) tab = groupOf(proc.presetIndex); }
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        panel(g, r.reduced(1.f), 12.f, juce::Colour(0xff161618), false);
        for (auto c : { juce::Point<float>(12.f, r.getBottom() - 12.f), juce::Point<float>(r.getRight() - 12.f, r.getBottom() - 12.f) }) screw(g, c, 3.5f, c.x * 0.13f);
        g.setColour(dim); g.setFont(mono(9.f, true));
        g.drawText("SOUNDS", juce::Rectangle<float>(18.f, 12.f, 100.f, 14.f), juce::Justification::centredLeft);
        g.setColour(faint); g.setFont(mono(8.5f));
        g.drawText("a preset changes only the sound - chords, connections and glides stay", juce::Rectangle<float>(100.f, 12.f, 500.f, 14.f), juce::Justification::centredLeft);
        for (int t = 0; t < kGroups; ++t)
        {
            const auto rr = tabCell(t);
            const bool on = tab == t, hot = hover == 300 + t, cur = groupOf(proc.presetIndex) == t;
            chipShape(g, rr, on, hot, 5.f);
            if (cur) { g.setColour(orange); g.fillRect(rr.getX() + 8.f, rr.getCentreY() - 2.5f, 5.f, 5.f); }
            g.setColour(on ? white : soft); g.setFont(mono(9.f, true));
            g.drawText(groupName(t), rr.withTrimmedLeft(cur ? 8.f : 0.f), juce::Justification::centred);
        }
        for (int i = 0; i < rc::kNumPresets; ++i)
        {
            if (groupOf(i) != tab) continue;
            const auto cell = item(i);
            const bool on = proc.presetIndex == i, hot = hover == i;
            if (on) { g.setColour(orange); g.fillPath(chamfer(cell, 7.f, 2 | 8)); }
            else chipShape(g, cell, hot, false, 7.f);
            g.setColour(on ? juce::Colours::white : hot ? white : soft);
            g.setFont(inter(12.5f, on ? 600 : 500));
            g.drawText(rc::kPresets[i].name, cell.reduced(14.f, 0.f), juce::Justification::centredLeft);
        }
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
        if (h == 200) { if (onClose) onClose(); return; }
        if (h >= 300) { tab = h - 300; repaint(); return; }
        if (h >= 0) { proc.loadPreset(h); repaint(); }
    }
private:
    ReChordProcessor& proc;
    int hover = -1, tab = 0;
    static constexpr int kGroups = 9;
    static const char* groupName(int c) { static const char* g[] = { "PIANO", "BOWED", "BASS", "KEYS", "DREAM", "LEADS", "STRINGS", "VOICES", "PADS" }; return g[c]; }
    static int groupOf(int i) { for (int k = 0; k < kGroups; ++k) if (juce::String(rc::kPresets[i].group) == groupName(k)) return k; return 0; }
    juce::Rectangle<float> tabCell(int t) const { const float w = ((float) getWidth() - 36.f - (kGroups - 1) * 6.f) / (float) kGroups; return { 18.f + t * (w + 6.f), 40.f, w, 28.f }; }
    juce::Rectangle<float> item(int i) const
    {
        int row = 0;
        for (int k = 0; k < i; ++k) if (groupOf(k) == groupOf(i)) ++row;
        const float w = ((float) getWidth() - 36.f - 2 * 10.f) / 3.f;
        return { 18.f + (row % 3) * (w + 10.f), 86.f + (row / 3) * 50.f, w, 42.f };
    }
    juce::Rectangle<float> closeCell() const { return { (float) getWidth() - 36.f, 8.f, 24.f, 24.f }; }
    int at(juce::Point<float> p) const
    {
        if (closeCell().contains(p)) return 200;
        for (int t = 0; t < kGroups; ++t) if (tabCell(t).contains(p)) return 300 + t;
        for (int i = 0; i < rc::kNumPresets; ++i) if (groupOf(i) == tab && item(i).contains(p)) return i;
        return -1;
    }
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
        juce::Path slab;
        slab.startNewSubPath(r.getX() + 12.f, r.getY()); slab.lineTo(r.getRight(), r.getY());
        slab.lineTo(r.getRight(), r.getBottom() - 8.f); slab.lineTo(r.getRight() - 8.f, r.getBottom());
        slab.lineTo(r.getX(), r.getBottom()); slab.lineTo(r.getX(), r.getY() + 12.f); slab.closeSubPath();
        juce::ColourGradient gr(juce::Colour(hot ? 0xffff8550 : 0xffff7340), r.getX(), r.getY(), juce::Colour(0xffd9440c), r.getX(), r.getBottom(), false);
        g.setGradientFill(gr); g.fillPath(slab);
        g.saveState(); g.reduceClipRegion(slab); g.setTiledImageFill(metalTexture(), 0, 0, 0.35f); g.fillPath(slab); g.restoreState();
        g.setColour(juce::Colours::white.withAlpha(0.3f)); g.drawLine(r.getX() + 12.f, r.getY() + 0.8f, r.getRight() - 1.f, r.getY() + 0.8f, 1.f);
        g.setColour(juce::Colours::black.withAlpha(0.35f)); g.strokePath(slab, juce::PathStrokeType(1.f));
        g.setColour(juce::Colours::black.withAlpha(0.22f));
        for (int k = 0; k < 4; ++k) { const float gx = r.getRight() - 30.f + k * 5.f; g.drawLine(gx, r.getBottom() - 9.f, gx + 7.f, r.getY() + 9.f, 1.2f); }
        // arrow
        const float ax = r.getX() + 20.f, ay = r.getCentreY();
        juce::Path a; a.startNewSubPath(ax, ay - 8.f); a.lineTo(ax, ay + 6.f);
        a.startNewSubPath(ax - 5.f, ay + 1.f); a.lineTo(ax, ay + 6.f); a.lineTo(ax + 5.f, ay + 1.f);
        g.setColour(juce::Colours::white); g.strokePath(a, juce::PathStrokeType(1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setFont(inter(13.f, 600));
        g.drawText(busy ? U("rendering…") : juce::String("DRAG WAV"), juce::Rectangle<float>(r.getX() + 36.f, r.getY() + 5.f, r.getWidth() - 40.f, 18.f), juce::Justification::centredLeft);
        g.setColour(juce::Colours::white.withAlpha(0.75f)); g.setFont(mono(8.5f));
        g.drawText(juce::String(proc.pattern.activeBars()) + ui::U(" BARS · ") + juce::String(juce::roundToInt(proc.hostBpm.load())) + " BPM",
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
// the knob panel: SOUND / GLIDE / KEY, one page at a time
// -----------------------------------------------------------------------------------------------------
class PanelTabs : public juce::Component
{
public:
    int current = 0;
    std::function<void(int)> onChange;
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const char* names[] = { "SOUND", "GLIDE", "KEY" };
        for (int i = 0; i < 3; ++i)
        {
            const auto r = cell(i);
            if (current == i || hover == i) chipShape(g, r, current == i, hover == i, 6.f);
            if (current == i) { g.setColour(orange); g.fillRect(r.getX() + 4.f, r.getY() + 9.f, 2.5f, r.getHeight() - 18.f); }
            g.setColour(current == i ? white : soft); g.setFont(mono(10.f, true));
            g.drawText(names[i], r.withTrimmedLeft(16.f), juce::Justification::centredLeft);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { int h = -1; for (int i = 0; i < 3; ++i) if (cell(i).contains(e.position)) h = i; if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 3; ++i) if (cell(i).contains(e.position)) { current = i; repaint(); if (onChange) onChange(i); }
    }
private:
    int hover = -1;
    juce::Rectangle<float> cell(int i) const { const float h = ((float) getHeight() - 8.f) / 3.f; return { 4.f, 4.f + i * h, (float) getWidth() - 8.f, h - 4.f }; }
};

// -----------------------------------------------------------------------------------------------------
// the device (design size 960 x 600) and the editor that scales it
// -----------------------------------------------------------------------------------------------------


// -----------------------------------------------------------------------------------------------------
// the device (design size 960 x 600) and the editor that scales it
// -----------------------------------------------------------------------------------------------------
class Device : public juce::Component, public juce::FileDragAndDropTarget
{
public:
    bool isInterestedInFileDrag(const juce::StringArray& files) override
    {
        for (auto& f : files) if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi")) return true;
        return false;
    }
    void fileDragEnter(const juce::StringArray&, int, int) override { dropping = true; refresh(); }
    void fileDragExit(const juce::StringArray&) override { dropping = false; refresh(); }
    void filesDropped(const juce::StringArray& files, int, int) override
    {
        dropping = false;
        for (auto& f : files)
            if (f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi")) { proc.loadMidiFile(juce::File(f)); break; }
        closePicker();
        view.setSel(-2, -1);
        refresh();
    }
    explicit Device(ReChordProcessor& p)
        : proc(p), view(p), strip(p), picker(p), drag(p), mode(p), type(p), presetBox(p), presets(p),
          kChar(p, "character", "CHARACTER"), kAttack(p, "attack", "ATTACK"), kBright(p, "bright", "BRIGHT"), kVib(p, "vibrato", "VIBRATO"),
          kRel(p, "release", "RELEASE"), kWarm(p, "warmth", "WARMTH"), kSpace(p, "space", "SPACE"),
          kSpread(p, "spread", "SPREAD"), kDouble(p, "double", "DOUBLE"),
          kLive(p, "liveGlide", "KEYBOARD"), kTrans(p, "transpose", "TRANSPOSE", true), kOct(p, "octave", "OCTAVE", true), kVoices(p, "voices", "VOICES"), kVol(p, "volume", "VOLUME")
    {
        addAndMakeVisible(view);
        addAndMakeVisible(strip);
        addChildComponent(picker);
        addAndMakeVisible(drag);
        addAndMakeVisible(mode);
        addAndMakeVisible(tabs);
        tabs.onChange = [this](int) { showTab(); };
        view.onPage = [this] { strip.page = view.page; closePicker(); strip.repaint(); };
        strip.onPage = [this](int pg) { view.page = strip.page = pg; view.setSel(-2, -1); view.repaint(); strip.repaint(); };
        addAndMakeVisible(type);
        addAndMakeVisible(presetBox);
        addChildComponent(presets);
        for (auto* k : knobs()) addAndMakeVisible(*k);
        presetBox.onOpen = [this] { closePicker(); presets.setVisible(!presets.isVisible()); presets.toFront(false); };
        presets.onClose = [this] { presets.setVisible(false); };
        // on TINE the vibrato knob is the tremolo of a Rhodes suitcase
        // on TINE the vibrato knob is the tremolo of a Rhodes suitcase, on 808 the punch (the pitch drop of a new note)
        kVib.titleText = [this]
        {
            const int o = (int) proc.apvts.getRawParameterValue("osc")->load();
            return juce::String(o == rc::OscTine ? "TREMOLO" : o == rc::Osc808 ? "PUNCH" : "VIBRATO");
        };
        kWarm.titleText = [this] { return juce::String((int) proc.apvts.getRawParameterValue("osc")->load() == rc::Osc808 ? "GRIT" : "WARMTH"); };
        kVib.valueText = [this]
        {
            const float v = proc.apvts.getRawParameterValue("vibrato")->load();
            const int o = (int) proc.apvts.getRawParameterValue("osc")->load();
            if (o == rc::Osc808) return juce::String(v / 50.f * 12.f, 1) + " st";
            if (o == rc::OscTine) return juce::String(juce::roundToInt(v * 2.f)) + "%";
            return juce::String(juce::roundToInt(v)) + " ct";
        };
        // CHARACTER means something else for each sound
        kChar.titleText = [this]
        {
            const char* t[] = { "BITE", "DETUNE", "ENSEMBLE", "VOWEL", "BELL", "HARMONICS", "BODY", "BODY", "DRIVE" };
            return juce::String(t[juce::jlimit(0, 8, (int) proc.apvts.getRawParameterValue("osc")->load())]);
        };
        kChar.valueText = [this]
        {
            const float c = proc.apvts.getRawParameterValue("character")->load();
            const int o = (int) proc.apvts.getRawParameterValue("osc")->load();
            if (o == rc::OscVoice)
                return juce::String(c < 0.25f ? "ah" : c < 0.42f ? "ah-oh" : c < 0.58f ? "oh" : c < 0.75f ? "oh-oo" : "oo");
            if (o == rc::OscKeys || o == rc::OscBowed)
            {
                // the measured body it is nearest to (between two = both names)
                const int n = o == rc::OscKeys ? rc::kNumKeyBodies : rc::kNumStringBodies;
                const auto* B = o == rc::OscKeys ? rc::kKeyBodies : rc::kStringBodies;
                const float x = c * (float) (n - 1);
                const int i = juce::jlimit(0, n - 1, juce::roundToInt(x));
                if (std::abs(x - (float) i) < 0.2f) return juce::String(B[i].name).toLowerCase();
                const int a = (int) x;
                auto shortName = [](const char* n) { return juce::String(n).toLowerCase().upToFirstOccurrenceOf(" ", false, false).substring(0, 4); };
                return shortName(B[a].name) + ">" + shortName(B[a + 1].name);
            }
            return juce::String(juce::roundToInt(c * 100)) + "%";
        };

        listen.setClickingTogglesState(true);
        listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listen);
        listen.setTooltip("On: plays with Cubase when Cubase plays, and on its own when Cubase is stopped. Off: silent");
        smartB.onClick = [this] { proc.smart(); view.setSel(-2, -1); };
        smartB.setTooltip("Lit = the smart voicing, untouched. Click = back to it (drops your connection / octave / glide edits)");
        learnB.onClick = [this] { proc.startLearn(); };
        learnB.setTooltip("Learn 4 bars of chords: put the MIDI part on this track, click LEARN, play Cubase. Or drop a .mid file here");
        for (auto* b : { &listen, &smartB, &learnB }) addAndMakeVisible(*b);

        allB.onClick = [this] { if (view.hasSelLink()) proc.timingToAll(view.selectedT(), view.selectedV()); };
        resetB.onClick = [this] { if (view.hasSelLink()) proc.resetTiming(view.selectedT(), view.selectedV()); };
        allB.setTooltip("Give every glide the start / end / curve of the selected one");
        resetB.setTooltip("The selected glide back to its default timing");
        addChildComponent(allB);
        addChildComponent(resetB);
        view.onSelection = [this] { const bool s = view.hasSelLink(); allB.setVisible(s); resetB.setVisible(s); };

        strip.onClick = [this](int key)
        {
            if (key == -99 || (picker.isVisible() && picker.key == key)) { closePicker(); return; }
            presets.setVisible(false);
            picker.key = key; strip.open = key;
            picker.setVisible(true); picker.toFront(false);
            strip.repaint(); picker.repaint();
        };
        picker.onClose = [this] { closePicker(); };
        picker.onChange = [this] { strip.repaint(); view.repaint(); };
        setSize(960, 600);
    }
    void closePicker() { picker.setVisible(false); strip.open = -99; strip.repaint(); }

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto all = getLocalBounds().toFloat();
        // the body: dark worn metal, light from the top, darker at the edges
        { juce::Path bp; bp.addRectangle(all); fillMetal(g, bp, juce::Colour(0xff121214), 0.7f); }
        juce::ColourGradient sheen(juce::Colours::white.withAlpha(0.045f), 480.f, -80.f, juce::Colours::transparentWhite, 480.f, 420.f, true);
        g.setGradientFill(sheen); g.fillRect(all);
        juce::ColourGradient vig(juce::Colours::transparentBlack, 480.f, 300.f, juce::Colours::black.withAlpha(0.45f), 0.f, 0.f, true);
        g.setGradientFill(vig); g.fillRect(all);
        // a machined seam under the header
        g.setColour(juce::Colours::black.withAlpha(0.7f)); g.drawHorizontalLine(64, 0.f, 960.f);
        g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawHorizontalLine(65, 0.f, 960.f);
        screw(g, { 10.f, 10.f }, 4.f, 0.4f);  screw(g, { 950.f, 10.f }, 4.f, 1.1f);
        screw(g, { 10.f, 590.f }, 4.f, 2.2f); screw(g, { 950.f, 590.f }, 4.f, 0.8f);
        // the mark: three lines gliding
        {
            juce::Path m;
            const float ys[3][2] = { { 20.f, 26.f }, { 26.f, 32.f }, { 32.f, 22.f } };
            for (auto& y : ys) { m.startNewSubPath(28.f, y[0]); m.lineTo(32.f, y[0]); m.cubicTo(37.f, y[0], 37.f, y[1], 42.f, y[1]); m.lineTo(46.f, y[1]); }
            g.setColour(white.withAlpha(0.85f)); g.strokePath(m, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.setColour(orange); g.fillRect(45.f, 20.5f, 3.f, 3.f);
        }
        g.setColour(white); g.setFont(inter(21.f, 600, -0.02f));
        g.drawText("ReChord", juce::Rectangle<float>(54.f, 12.f, 140.f, 28.f), juce::Justification::centredLeft);
        g.setColour(faint); g.setFont(mono(7.5f));
        g.drawText(U("BY POURIA MOTABEAN · v" RECHORD_VERSION), juce::Rectangle<float>(55.f, 40.f, 140.f, 12.f), juce::Justification::centredLeft);
        // the knob plate: raised, chamfered, its own screws
        const auto plate = juce::Rectangle<float>(24.f, 462.f, 912.f, 124.f);
        panel(g, plate, 12.f, juce::Colour(0xff19191c), false);
        screw(g, { plate.getRight() - 12.f, plate.getY() + 12.f }, 3.2f, 0.3f);
        screw(g, { plate.getRight() - 12.f, plate.getBottom() - 12.f }, 3.2f, 1.7f);
        g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawVerticalLine(140, plate.getY() + 12.f, plate.getBottom() - 12.f);
        g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawVerticalLine(141, plate.getY() + 12.f, plate.getBottom() - 12.f);
        if (tabs.current == 1)
        {
            g.setColour(faint); g.setFont(mono(8.5f));
            g.drawText("each glide's start / end / curve: the handles on the display", juce::Rectangle<float>(540.f, 490.f, 380.f, 14.f), juce::Justification::centredLeft);
            g.drawText("KEYBOARD = how long a played chord glides to the next", juce::Rectangle<float>(540.f, 510.f, 380.f, 14.f), juce::Justification::centredLeft);
        }
    }
    void resized() override
    {
        drag.setBounds(782, 14, 154, 44);
        smartB.setBounds(718, 20, 58, 32);
        learnB.setBounds(658, 20, 56, 32);
        listen.setBounds(590, 20, 64, 32);
        type.setBounds(196, 16, 236, 40);
        presetBox.setBounds(438, 16, 146, 40);
        presets.setBounds(24 + 14, 72 + 12, 912 - 28, 250);

        view.setBounds(24, 72, 912, 272);
        ui::Bars b;
        b.w = (912.f - 44.f - 8.f) / 5.24f;
        b.x0 = 44.f + 0.62f * b.w;
        view.bars = b;
        strip.bars = b;
        strip.setBounds(24, 350, 912, 104);
        picker.setBounds(24 + 30, 212, 912 - 60, 128);
        allB.setBounds(24 + 912 - 150, 72 + 272 - 26, 56, 20);
        resetB.setBounds(24 + 912 - 88, 72 + 272 - 26, 72, 20);

        tabs.setBounds(30, 470, 104, 108);
        showTab();
    }
    void refresh()
    {
        const double bb = proc.hostBarBeats.load();
        if (view.bars.barBeats != bb) { view.bars.barBeats = bb; strip.bars.barBeats = bb; }
        if (presets.isVisible()) {}
        view.repaint(); strip.repaint(); mode.repaint(); type.repaint(); presetBox.repaint(); kChar.repaint(); kVib.repaint(); kWarm.repaint();
        if (picker.isVisible()) picker.repaint();
        if (presets.isVisible()) presets.repaint();
        drag.repaint();
        smartB.setToggleState(!proc.score.edited, juce::dontSendNotification);
        learnB.setToggleState(proc.learnState.load() != 0, juce::dontSendNotification);
        const int nb = proc.pattern.activeBars();
        view.learnText = proc.learnState.load() == 1 ? "LEARN: waiting for Cubase to play - the next bar line starts " + juce::String(nb) + " bars"
                       : proc.learnState.load() == 2 ? "LEARN: listening, bar " + juce::String(juce::jlimit(1, nb, (int) proc.learnPos.load() + 1)) + " of " + juce::String(nb)
                       : dropping ? "DROP: the chords of the first " + juce::String(nb) + " bars of this MIDI file" : juce::String();
        if (view.hasSelLink() && (view.selectedV() >= proc.score.nVoices || view.selectedT() >= proc.score.nSlots)) view.setSel(-2, -1);
    }

    LinkView view;
private:
    ReChordProcessor& proc;
    bool dropping = false;
    ChordStrip strip;
    Picker picker;
    DragChip drag;
    ModeSwitch mode;
    TypeSwitch type;
    PresetBox presetBox;
    PresetPanel presets;
    Knob kChar, kAttack, kBright, kVib, kRel, kWarm, kSpace, kSpread, kDouble, kLive, kTrans, kOct, kVoices, kVol;
    PanelTabs tabs;
    // the knobs of the open tab, side by side
    void showTab()
    {
        std::vector<juce::Component*> sound { &kChar, &kAttack, &kBright, &kVib, &kRel, &kWarm, &kSpace, &kDouble },
                                      glide { &mode, &kSpread, &kLive },
                                      key { &kTrans, &kOct, &kVoices, &kVol };
        for (auto* c : sound) c->setVisible(tabs.current == 0);
        for (auto* c : glide) c->setVisible(tabs.current == 1);
        for (auto* c : key) c->setVisible(tabs.current == 2);
        auto& list = tabs.current == 0 ? sound : tabs.current == 1 ? glide : key;
        float x = 150.f;
        for (auto* c : list)
        {
            const float w = c == &mode ? 130.f : 97.f;
            c->setBounds((int) x, 478, (int) w, 100);
            x += w;
        }
        repaint();
    }
    juce::TextButton listen { "LISTEN" }, smartB { "SMART" }, learnB { "LEARN" }, allB { "ALL" }, resetB { "RESET" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> listenAtt;
    std::vector<Knob*> knobs() { return { &kChar, &kAttack, &kBright, &kVib, &kRel, &kWarm, &kSpace, &kSpread, &kDouble, &kLive, &kTrans, &kOct, &kVoices, &kVol }; }
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
        unsigned pv = (unsigned) (proc.apvts.getRawParameterValue("mode")->load() * 1000 + (float) (proc.transpose() + 12) * 31 + (float) proc.hostBarBeats.load() * 7);
        for (auto* id : { "osc", "character", "attack", "bright", "vibrato", "release", "warmth", "space", "double", "volume", "octave", "liveGlide" })
            pv = pv * 31u + (unsigned) (int) (proc.apvts.getRawParameterValue(id)->load() * 977.f);
        if (mv != lastModel || beat != lastBeat || pv != lastParams || ++slow % 15 == 0)
        {
            lastModel = mv; lastBeat = beat; lastParams = pv;
            device.refresh();
        }
    }
    ReChordProcessor& proc;
    ui::Look look;
    juce::TooltipWindow tooltips { this, 600 };
    Device device;
    int lastModel = -1, slow = 0;
    unsigned lastParams = 0;
    double lastBeat = -2;
};
