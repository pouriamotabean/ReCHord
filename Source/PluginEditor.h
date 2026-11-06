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
static const juce::Colour white(0xffeeebe6), dim(0xff8c8a86), faint(0xff5f5d5a), line(0xff2b2a28), dark(0xff3a3937),
                          screen(0xff0c0c0c), body(0xff141414), chip(0xff151515), chip2(0xff1b1b1b), chipOn(0xff262626),
                          card(0xff121212), orange(0xffee6f2d), label(0xffc9c6c0), soft(0xffaeaba6),
                          copper(0xffbf6c3b), frame(0xff1c1c1c), well(0xff0f0f0f);

// 2.5.4: fonts and the texture belong to the open editors (a SharedResourcePointer the editor holds), and are
// let go when the last editor closes - static ones outlived JUCE / DirectWrite / Direct2D when Cubase unloaded
// the plugin, which can hang the host on the way out.
struct Res
{
    juce::Typeface::Ptr t[10];
    juce::Image tex;
};
#if HAS_ASSETS
inline juce::Typeface::Ptr face(int i)
{
    juce::SharedResourcePointer<Res> res;
    auto& t = res->t;
    if (t[i] == nullptr)
    {
        const void* d = nullptr; int n = 0;
        switch (i)
        {
            case 0: d = BinaryData::InterLight_ttf;           n = BinaryData::InterLight_ttfSize; break;
            case 1: d = BinaryData::InterRegular_ttf;         n = BinaryData::InterRegular_ttfSize; break;
            case 2: d = BinaryData::InterMedium_ttf;          n = BinaryData::InterMedium_ttfSize; break;
            case 3: d = BinaryData::InterSemiBold_ttf;        n = BinaryData::InterSemiBold_ttfSize; break;
            case 4: d = BinaryData::IBMPlexMonoRegular_ttf;   n = BinaryData::IBMPlexMonoRegular_ttfSize; break;
            case 5: d = BinaryData::IBMPlexMonoMedium_ttf;    n = BinaryData::IBMPlexMonoMedium_ttfSize; break;
            case 6: d = BinaryData::IBMPlexMonoSemiBold_ttf;  n = BinaryData::IBMPlexMonoSemiBold_ttfSize; break;
            case 7: d = BinaryData::IBMPlexSerifRegular_ttf;  n = BinaryData::IBMPlexSerifRegular_ttfSize; break;
            case 8: d = BinaryData::IBMPlexSansLight_ttf;     n = BinaryData::IBMPlexSansLight_ttfSize; break;
            default: d = BinaryData::IBMPlexSansRegular_ttf;  n = BinaryData::IBMPlexSansRegular_ttfSize; break;
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
// labels: IBM Plex Mono (bold = medium)
inline juce::Font mono(float px, bool bold = false, float kern = 0.f)
{
#if HAS_ASSETS
    if (auto f = face(bold ? 5 : 4)) return juce::Font(juce::FontOptions(f).withPointHeight(px).withKerningFactor(kern));
#endif
    return juce::Font(juce::FontOptions(juce::Font::getDefaultMonospacedFontName(), px * 1.2f, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor(kern));
}
inline juce::Font serif(float px)
{
#if HAS_ASSETS
    if (auto f = face(7)) return juce::Font(juce::FontOptions(f).withPointHeight(px));
#endif
    return juce::Font(juce::FontOptions(juce::Font::getDefaultSerifFontName(), px * 1.2f, juce::Font::plain));
}
inline juce::Font sans(float px, bool light = false)
{
#if HAS_ASSETS
    if (auto f = face(light ? 8 : 9)) return juce::Font(juce::FontOptions(f).withPointHeight(px));
#endif
    return juce::Font(juce::FontOptions(px * 1.2f));
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
inline juce::Image metalTexture()
{
    juce::SharedResourcePointer<Res> res;
    auto& img = res->tex;
    if (img.isValid()) return img;
    const int W = 1024, H = 1024;
    img = juce::Image(juce::Image::ARGB, W, H, true, juce::SoftwareImageType());
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
    g.saveState(); g.reduceClipRegion(p); g.setTiledImageFill(metalTexture(), 0, 0, texture * 0.18f); g.fillPath(p); g.restoreState();   // a trace only
}

// a panel: raised (frames, cards) or recessed (screens). Light comes from the top
inline void panel(juce::Graphics& g, juce::Rectangle<float> r, float cut, juce::Colour base, bool recessed, int = 15)
{
    const float rad = std::max(6.f, cut * 0.85f);
    juce::Path p; p.addRoundedRectangle(r, rad);
    if (!recessed) { g.setColour(juce::Colours::black.withAlpha(0.45f)); juce::Path sh; sh.addRoundedRectangle(r.translated(0, 2.f), rad); g.fillPath(sh); }
    juce::ColourGradient gr(recessed ? base : base.brighter(0.05f), 0, r.getY(), recessed ? base.brighter(0.02f) : base.darker(0.1f), 0, r.getBottom(), false);
    g.setGradientFill(gr); g.fillPath(p);
    g.setColour(juce::Colours::black.withAlpha(recessed ? 0.7f : 0.6f)); g.strokePath(p, juce::PathStrokeType(1.f));
    g.setColour(juce::Colours::white.withAlpha(recessed ? 0.035f : 0.06f));
    juce::Path in; in.addRoundedRectangle(r.reduced(1.f), rad - 1.f); g.strokePath(in, juce::PathStrokeType(1.f));
}

// a screw: recessed hole, worn cap, a cross slot turned a little differently each time
inline void screw(juce::Graphics& g, juce::Point<float> c, float rad, float angle)
{
    g.setColour(juce::Colours::black.withAlpha(0.8f)); g.fillEllipse(juce::Rectangle<float>(rad * 2 + 3.f, rad * 2 + 3.f).withCentre(c));
    g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawEllipse(juce::Rectangle<float>(rad * 2 + 3.f, rad * 2 + 3.f).withCentre(c.translated(0, 0.6f)), 0.7f);
    juce::ColourGradient cg(juce::Colour(0xff5a5956), c.x - rad * 0.6f, c.y - rad * 0.8f, juce::Colour(0xff121212), c.x + rad * 0.5f, c.y + rad, true);
    g.setGradientFill(cg); g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(c));
    juce::Path slot;
    slot.addRectangle(-rad * 0.62f, -0.55f, rad * 1.24f, 1.1f);
    slot.addRectangle(-0.55f, -rad * 0.62f, 1.1f, rad * 1.24f);
    g.setColour(juce::Colours::black.withAlpha(0.9f));
    g.fillPath(slot, juce::AffineTransform::rotation(angle).translated(c.x, c.y));
    g.setColour(juce::Colours::white.withAlpha(0.10f));
    g.fillPath(slot, juce::AffineTransform::rotation(angle).translated(c.x + 0.4f, c.y + 0.5f));
}

// a small chamfered chip (buttons, segments): metal, darker when off
inline void chipShape(juce::Graphics& g, juce::Rectangle<float> r, bool on, bool hot, float = 5.f)
{
    juce::Path p; p.addRoundedRectangle(r, 6.f);
    g.setColour(on ? juce::Colour(0xff252525) : hot ? juce::Colour(0xff1f1f1f) : juce::Colour(0xff191919)); g.fillPath(p);
    g.setColour(juce::Colours::black.withAlpha(0.6f)); g.strokePath(p, juce::PathStrokeType(1.f));
    g.setColour(juce::Colours::white.withAlpha(on ? 0.07f : 0.04f));
    juce::Path in; in.addRoundedRectangle(r.reduced(1.f), 5.f); g.strokePath(in, juce::PathStrokeType(1.f));
}

// a rounded rectangle panel with a bevel: light on the top edge, dark under it (the mock's frames and buttons)
inline void bevelRect(juce::Graphics& g, juce::Rectangle<float> r, float rad, juce::Colour base, bool sunk, float tex = 0.35f)
{
    juce::Path p; p.addRoundedRectangle(r, rad);
    if (!sunk)
    {
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        juce::Path sh; sh.addRoundedRectangle(r.translated(0, 2.f).expanded(0.5f), rad); g.fillPath(sh);
    }
    juce::ColourGradient gr(base.brighter(sunk ? 0.f : 0.08f), 0, r.getY(), base.darker(sunk ? 0.f : 0.15f), 0, r.getBottom(), false);
    g.setGradientFill(gr); g.fillPath(p);
    if (tex > 0) { g.saveState(); g.reduceClipRegion(p); g.setTiledImageFill(metalTexture(), 0, 0, tex * 0.3f); g.fillPath(p); g.restoreState(); }
    if (sunk)
    {
        juce::ColourGradient sh(juce::Colours::black.withAlpha(0.6f), 0, r.getY(), juce::Colours::transparentBlack, 0, r.getY() + 8.f, false);
        g.saveState(); g.reduceClipRegion(p); g.setGradientFill(sh); g.fillRect(r.withHeight(8.f)); g.restoreState();
        g.setColour(juce::Colours::black.withAlpha(0.75f)); g.strokePath(p, juce::PathStrokeType(1.f));
        g.setColour(juce::Colours::white.withAlpha(0.06f));
        juce::Path lo; lo.addRoundedRectangle(r.expanded(1.f).translated(0, 0.5f), rad + 1.f);
        g.saveState(); g.excludeClipRegion(r.toNearestInt().withTrimmedBottom(2)); g.strokePath(lo, juce::PathStrokeType(1.f)); g.restoreState();
    }
    else
    {
        g.setColour(juce::Colours::black.withAlpha(0.7f)); g.strokePath(p, juce::PathStrokeType(1.f));
        juce::Path in; in.addRoundedRectangle(r.reduced(1.f), std::max(0.f, rad - 1.f));
        juce::ColourGradient hl(juce::Colours::white.withAlpha(0.10f), 0, r.getY(), juce::Colours::white.withAlpha(0.02f), 0, r.getBottom(), false);
        g.setGradientFill(hl); g.strokePath(in, juce::PathStrokeType(1.f));
    }
}
// the same on any shape (chamfered cards)
inline void bevelPath(juce::Graphics& g, const juce::Path& p, juce::Rectangle<float> r, juce::Colour base, float tex = 0.35f)
{
    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillPath(p, juce::AffineTransform::translation(0, 2.f));
    juce::ColourGradient gr(base.brighter(0.06f), 0, r.getY(), base.darker(0.12f), 0, r.getBottom(), false);
    g.setGradientFill(gr); g.fillPath(p);
    if (tex > 0) { g.saveState(); g.reduceClipRegion(p); g.setTiledImageFill(metalTexture(), 0, 0, tex * 0.3f); g.fillPath(p); g.restoreState(); }
    g.setColour(juce::Colours::black.withAlpha(0.75f)); g.strokePath(p, juce::PathStrokeType(1.f));
    juce::ColourGradient hl(juce::Colours::white.withAlpha(0.11f), 0, r.getY(), juce::Colours::white.withAlpha(0.025f), 0, r.getBottom(), false);
    g.setGradientFill(hl);
    g.strokePath(p, juce::PathStrokeType(1.f), juce::AffineTransform::scale((r.getWidth() - 2.f) / r.getWidth(), (r.getHeight() - 2.f) / r.getHeight(), r.getCentreX(), r.getCentreY()));
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
        const float R = std::min(b.getWidth(), b.getHeight()) * 0.5f - 6.f;
        const auto c = b.getCentre();
        const float ang = a0 + pos * (a1 - a0);
        const bool hot = s.isMouseOverOrDragging();
        auto circle = [&](float r) { return juce::Rectangle<float>(r * 2, r * 2).withCentre(c); };
        auto tick = [&](float a, float r0, float r1, juce::Colour col, float wd)
        { g.setColour(col); g.drawLine(c.x + std::sin(a) * r0, c.y - std::cos(a) * r0, c.x + std::sin(a) * r1, c.y - std::cos(a) * r1, wd); };
        // ticks outside: the middle one light, the ends and the sides grey
        tick((a0 + a1) * 0.5f, R + 2.f, R + 6.f, juce::Colour(0xffbdb9b2), 1.3f);
        for (float a : { a0, a1, -1.5708f, 1.5708f }) tick(a, R + 2.5f, R + 5.5f, juce::Colour(0xff4b4a48), 1.f);
        // shadow, the ring, the cap
        for (int i = 3; i >= 1; --i) { g.setColour(juce::Colours::black.withAlpha(0.16f)); g.fillEllipse(circle(R + i * 1.2f).translated(0, 2.f + i)); }
        juce::ColourGradient ring(juce::Colour(0xff2e2e2e), c.x, c.y - R, juce::Colour(0xff0d0d0d), c.x, c.y + R, false);
        g.setGradientFill(ring); g.fillEllipse(circle(R));
        g.setColour(juce::Colours::black.withAlpha(0.85f)); g.drawEllipse(circle(R), 1.f);
        const float rc = R - 4.f;
        juce::ColourGradient face(juce::Colour(0xff272727), c.x - rc * 0.3f, c.y - rc * 0.8f, juce::Colour(0xff111111), c.x + rc * 0.4f, c.y + rc, true);
        g.setGradientFill(face); g.fillEllipse(circle(rc));
        juce::ColourGradient rim(juce::Colours::white.withAlpha(hot ? 0.22f : 0.15f), c.x, c.y - rc, juce::Colours::transparentWhite, c.x, c.y + rc * 0.2f, false);
        g.setGradientFill(rim); g.drawEllipse(circle(rc - 0.5f), 1.f);
        g.setColour(juce::Colours::black.withAlpha(0.7f)); g.drawEllipse(circle(rc + 0.5f), 0.8f);
        // the value: a mark on the ring and the pointer
        tick(ang, R - 3.5f, R + 0.5f, orange, 2.4f);
        juce::Path mk; mk.startNewSubPath(0, -rc * 0.12f); mk.lineTo(0, -rc * 0.78f);
        g.setColour(orange);
        g.strokePath(mk, juce::PathStrokeType(2.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded), juce::AffineTransform::rotation(ang).translated(c.x, c.y));
    }
    void drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced(0.5f);
        const int seg = (int) b.getProperties()["seg"];        // 3: text only (the header)
        const bool on = b.getToggleState();
        if (seg == 3) { if (on) { g.setColour(orange); g.fillEllipse(r.getX() + 6.f, r.getCentreY() - 3.f, 6.f, 6.f); } return; }
        if (seg == 0) bevelRect(g, r, 6.f, on || down ? juce::Colour(0xff222222) : over ? juce::Colour(0xff1f1f1f) : juce::Colour(0xff191919), false, 0.25f);
        else
        {
            if (on || over || down) { g.setColour(juce::Colours::white.withAlpha(on ? 0.05f : 0.03f)); g.fillRoundedRectangle(r.reduced(1.5f), 4.f); }
            if (seg == 1) { g.setColour(juce::Colours::black.withAlpha(0.8f)); g.drawVerticalLine((int) r.getRight(), r.getY() + 2.f, r.getBottom() - 2.f);
                            g.setColour(juce::Colours::white.withAlpha(0.06f)); g.drawVerticalLine((int) r.getRight() + 1, r.getY() + 2.f, r.getBottom() - 2.f); }
        }
        if (on) { g.setColour(orange); g.fillEllipse(r.getX() + 10.f, r.getCentreY() - 3.f, 6.f, 6.f); }
    }
    void drawButtonText(juce::Graphics& g, juce::TextButton& b, bool over, bool) override
    {
        g.setFont(mono(b.getHeight() < 24 ? 8.5f : 9.5f, true));
        g.setColour(!b.isEnabled() ? faint.withAlpha(0.6f) : b.getToggleState() || over ? white : label);
        auto r = b.getLocalBounds();
        if (b.getToggleState()) r.removeFromLeft((int) b.getProperties()["seg"] == 3 ? 10 : 14);
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
        panel(g, r.reduced(1.f), 12.f, screen, true, 1 | 8);
        layout();
        const auto& s = proc.score;
        const int N = s.nVoices, n = s.nSlots, tr = proc.shift();

        g.saveState();
        g.reduceClipRegion(chamfer(r.reduced(2.f), 11.f, 1 | 8));

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

        // a faint grid: beats, and a line every 4 semitones
        for (int b = ws(); b < we(); ++b)
            for (int q = 1; q < (int) bars.barBeats; ++q)
            { g.setColour(juce::Colours::white.withAlpha(0.025f)); g.drawVerticalLine((int) bx(b * bars.barBeats + q), top, bottom); }
        for (int m = (lo / 4) * 4; m <= hi; m += 4)
            if (m >= lo && m % 12 != 0) { g.setColour(juce::Colours::white.withAlpha(0.02f)); g.drawHorizontalLine((int) yOf((float) m), 40.f, r.getRight() - 4.f); }
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
                g.setColour(b == playBar ? orange : label);
                g.setFont(mono(9.5f, true));
                g.drawText(juce::String(proc.pattern.barAt(b) + 1), juce::Rectangle<float>(x + 7.f, 8.f, 30.f, 14.f), juce::Justification::centredLeft);
            }
        }
        for (int side : { -1, -2 })
        {
            const bool nav = side == -1 ? page > 0 : we() < proc.pattern.activeBars();      // the other page of a long pattern
            const auto rr = navRect(side);
            if (nav)
            {
                g.setColour(juce::Colour(navHot == side ? 0xff222222 : 0xff1a1a1a)); g.fillRoundedRectangle(rr, 5.f);
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
                if (ghost && !neighbour && !sel && !tgt)
                {
                    if (c == endCol()) { g.setColour(orange.withAlpha(mirror ? 1.f : 0.6f)); g.fillEllipse(juce::Rectangle<float>(9.f, 9.f).withCentre(cc)); }
                    else { g.setColour(juce::Colour(0xffefe9df).withAlpha(mirror ? 0.9f : 0.5f)); g.fillEllipse(juce::Rectangle<float>(rad * 2 - 1.f, rad * 2 - 1.f).withCentre(cc)); }
                }
                else
                {
                    g.setColour(juce::Colours::black.withAlpha(0.5f)); g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(cc).translated(0, 1.5f));
                    g.setColour((sel || tgt ? orange : juce::Colour(0xffefe9df)).withAlpha(neighbour ? 0.7f : 1.f));
                    g.fillEllipse(juce::Rectangle<float>(rad * 2, rad * 2).withCentre(cc));
                }
                if (hoverDot.first == c && hoverDot.second == v && !sel)
                { g.setColour(orange.withAlpha(alpha)); g.drawEllipse(juce::Rectangle<float>(rad * 2 + 5, rad * 2 + 5).withCentre(cc), 1.4f); }
                g.setColour((sel ? white : label).withAlpha(ghost ? (mirror ? 0.55f : 0.45f) : 1.f));
                g.setFont(mono(9.f, true));
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
            g.drawText(learnText, bottomLine.withWidth(r.getWidth() - 200.f), juce::Justification::centredLeft);
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
            g.setColour(dim.darker(0.2f)); g.setFont(mono(8.5f));
            g.drawFittedText(ui::U("glide: drag a line  |  curve: the middle  |  reconnect: end → note  |  on / off: click  |  reset: double-click"),
                             juce::Rectangle<float>(8.f, r.getBottom() - 21.f, r.getWidth() - 400.f, 14.f).toNearestInt(), juce::Justification::centred, 1, 0.75f);
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
            g.strokePath(p, juce::PathStrokeType(sel || hot ? 2.3f : 1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
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
            const int idx = P.indexOf(b);
            for (int h = 0; h < (sp ? 2 : 1); ++h)
            {
                const int key = b * 2 + h;
                const auto r = cell(key);
                const bool isOpen = open == key, hot = hover == key;
                const int m = sp ? (h == 0 ? 1 | 8 : 2 | 4) : 15;
                juce::Path cp; cp.addRoundedRectangle(r.getX(), r.getY(), r.getWidth(), r.getHeight(), 7.f, 7.f, (m & 1) != 0, (m & 2) != 0, (m & 8) != 0, (m & 4) != 0);
                if (on) panel(g, r, 8.f, isOpen ? juce::Colour(0xff252525) : hot ? juce::Colour(0xff212121) : here ? juce::Colour(0xff1d1d1d) : juce::Colour(0xff171717), false);
                else { g.setColour(juce::Colour(0xff121212)); g.fillPath(cp); g.setColour(juce::Colours::white.withAlpha(0.035f)); g.strokePath(cp, juce::PathStrokeType(1.f)); }
                if (isOpen) { g.setColour(orange); g.fillRect(r.getX() + 1.f, r.getY() + 7.f, 2.5f, r.getHeight() - 14.f); }
                // the chord and the bar it is in
                const auto mid = r.withTrimmedLeft(sp && h == 1 ? 0.f : 10.f);
                g.setColour(!on ? faint.withAlpha(0.7f) : here ? white : soft);
                g.setFont(sans(sp ? 15.f : 18.f, true));
                g.drawText(label(key), mid.withHeight(28.f).translated(0, 3.f), juce::Justification::centred);
                g.setColour(!on ? faint.withAlpha(0.5f) : dim); g.setFont(mono(7.5f, true));
                const juce::String sub = !on ? juce::String("OFF") : sp ? juce::String(h == 0 ? "BAR " : "") + juce::String(idx + 1) + (h == 0 ? ".1" : ".3") : "BAR " + juce::String(idx + 1);
                g.drawText(sub, juce::Rectangle<float>(mid.getX(), r.getBottom() - 15.f, mid.getWidth(), 10.f), juce::Justification::centred);
            }
            // on / off: a small lamp top left;  the slot number top right;  ½ next to it
            const auto d = dotCell(b);
            g.setColour(hover == 200 + b ? white : on ? orange : faint);
            if (on) g.fillEllipse(d.reduced(3.f)); else g.drawEllipse(d.reduced(3.5f), 1.2f);
            const auto br = barRect(b);
            g.setColour(on ? (here ? dim : faint) : faint.withAlpha(0.6f)); g.setFont(mono(8.f));
            g.drawText(juce::String(b + 1).paddedLeft('0', 2), juce::Rectangle<float>(br.getRight() - 30.f, br.getY() + 4.f, 22.f, 12.f), juce::Justification::centredRight);
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
            panel(g, r, 8.f, isOpen ? juce::Colour(0xff222222) : hot ? juce::Colour(0xff1e1e1e) : juce::Colour(0xff191919), false);
            if (isOpen) { g.setColour(orange); g.fillRect(r.getX() + 1.f, r.getY() + 9.f, 2.5f, r.getHeight() - 18.f); }
            const auto& sc = proc.score;
            const int m = which == -1 ? sc.entryMode : sc.exitMode;
            const auto& c = which == -1 ? proc.pattern.entry : proc.pattern.exit;
            g.setColour(dim); g.setFont(mono(8.f));
            g.drawText(which == -1 ? "BEFORE" : "AFTER", r.withHeight(16.f).translated(0, r.getHeight() * 0.5f - 20.f), juce::Justification::centred);
            g.setColour(white); g.setFont(m == rc::ToChord ? sans(15.f) : mono(10.f, true));
            g.drawText(m == rc::Loop ? juce::String("LOOP") : m == rc::None ? juce::String(which == -1 ? "START" : "END") : juce::String(rc::chordName(c.root, c.quality, proc.transpose())),
                       r.withTrimmedTop(r.getHeight() * 0.5f - 2.f).withHeight(20.f), juce::Justification::centred);
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
    float rowH() const { return ((float) getHeight() - 14.f) / 2.f; }
    juce::Rectangle<float> barRect(int b) const
    {
        const float x0 = bars.x(0) - bars.w * 0.08f, w = (bars.x(rc::kBars * bars.barBeats) + bars.w * 0.08f - x0) / 4.f;
        return { x0 + (b % 4) * w + 2.f, 5.f + (b / 4) * (rowH() + 4.f), w - 4.f, rowH() };
    }
    juce::Rectangle<float> cell(int key) const
    {
        const float h = barRect(4).getBottom() - barRect(0).getY(), y = barRect(0).getY();
        if (key == -1) { const float x1 = barRect(0).getX() - 16.f; return { x1 - 104.f, y, 104.f, h }; }
        if (key == -2) { const float x0 = barRect(3).getRight() + 16.f; return { x0, y, std::min(104.f, (float) getWidth() - 2.f - x0), h }; }
        const auto r = barRect(key / 2);
        if (!proc.pattern.split[key / 2]) return r;
        const float half = r.getWidth() * 0.5f;
        return key % 2 ? r.withTrimmedLeft(half + 2.f) : r.withWidth(half - 2.f);
    }
    juce::Rectangle<float> dotCell(int b) const { const auto r = barRect(b); return { r.getX() + 8.f, r.getY() + 4.f, 12.f, 12.f }; }
    juce::Rectangle<float> splitCell(int b) const { const auto r = barRect(b); return { r.getRight() - 50.f, r.getY() + 2.f, 18.f, 15.f }; }
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
// 2.3: movement presets - how the voices move on every glide
// 2.5: SAMPLE EDIT - the part of the file that plays, its stretch, the seam, the root
class SampleEditor : public juce::Component
{
public:
    explicit SampleEditor(ReChordProcessor& p) : proc(p) {}
    std::function<void()> onChange;
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        panel(g, r.reduced(1.f), 12.f, screen, true);
        if (proc.samples.empty())
        {
            g.setColour(dim); g.setFont(mono(10.f));
            g.drawText("drop a WAV of a held note here (or click SAMPLE again)", r, juce::Justification::centred);
            return;
        }
        auto& src = cur();
        const auto& in = src.info;
        // the files
        for (int i = 0; i < (int) proc.samples.size(); ++i)
        {
            const auto t = tab(i);
            const bool on = i == proc.sampleSel;
            g.setColour(juce::Colour(on ? 0xff232323 : hover == 10 + i ? 0xff1d1d1d : 0xff171717)); g.fillRoundedRectangle(t, 5.f);
            if (on) { g.setColour(orange); g.fillRect(t.getX() + 1.f, t.getY() + 5.f, 2.f, t.getHeight() - 10.f); }
            g.setColour(on ? white : label); g.setFont(mono(8.5f, on));
            g.drawFittedText(proc.samples[(size_t) i].name + "  " + juce::String(rc::noteName((int) std::lround(proc.samples[(size_t) i].info.root))),
                             t.reduced(8.f, 0.f).toNearestInt(), juce::Justification::centred, 1, 0.7f);
        }
        // buttons on the right
        const char* bn[] = { "LOOP", "CYCLE", "AUTO", "PLAY", "REMOVE" };
        for (int i = 0; i < 5; ++i)
        {
            const auto b = btn(i);
            const bool on = (i == 0 && src.ed.mode == 0) || (i == 1 && src.ed.mode == 1) || (i == 3 && proc.audition.load() >= 0);
            g.setColour(juce::Colour(on ? 0xff252525 : hover == 20 + i ? 0xff202020 : 0xff181818)); g.fillRoundedRectangle(b, 5.f);
            g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(b, 5.f, 1.f);
            if (on) { g.setColour(orange); g.fillEllipse(b.getX() + 7.f, b.getCentreY() - 2.5f, 5.f, 5.f); }
            g.setColour(on ? white : label); g.setFont(mono(8.5f, true));
            g.drawText(bn[i], b.withTrimmedLeft(on ? 8.f : 0.f), juce::Justification::centred);
        }
        // the waveform
        const auto w = wave();
        g.setColour(juce::Colour(0xff0a0a0a)); g.fillRoundedRectangle(w.expanded(4.f, 4.f), 6.f);
        const int n = src.audio.getNumSamples();
        const double len = n / src.rate;
        const float xa = xOf(in.a), xb = xOf(in.b);
        g.setColour(orange.withAlpha(0.08f)); g.fillRect(juce::Rectangle<float>(xa, w.getY(), xb - xa, w.getHeight()));
        const int cols = (int) (xOf(len) - w.getX());
        float pk = 1e-4f;                                   // the waveform fills the height, quiet files too
        for (int i = 0; i < n; i += 4) pk = std::max(pk, std::abs(0.5f * (src.audio.getSample(0, i) + src.audio.getSample(1, i))));
        const float sc = 0.48f / pk;
        for (int c = 0; c < cols; ++c)
        {
            const int i0 = (int) ((double) c / cols * n), i1 = std::max(i0 + 1, (int) ((double) (c + 1) / cols * n));
            float mn = 0, mx = 0;
            for (int i = i0; i < i1 && i < n; ++i) { const float v = 0.5f * (src.audio.getSample(0, i) + src.audio.getSample(1, i)); mn = std::min(mn, v); mx = std::max(mx, v); }
            const float x = w.getX() + c;
            const bool inPart = x >= xa && x <= xb;
            g.setColour(inPart ? juce::Colour(0xffe9e3d8) : juce::Colour(0xff4a4946));
            g.drawVerticalLine((int) x, w.getCentreY() - mx * w.getHeight() * sc, w.getCentreY() - mn * w.getHeight() * sc + 1.f);
        }
        g.setColour(juce::Colours::white.withAlpha(0.06f)); g.drawHorizontalLine((int) w.getCentreY(), w.getX(), w.getRight());
        g.setColour(faint); g.setFont(mono(7.f)); g.drawText("FILE", w.withHeight(10.f).translated(3.f, 2.f), juce::Justification::topLeft);
        // the result: what really plays - the part made level and stretched, the loop in it, the seam
        {
            const auto rr = res();
            g.setColour(juce::Colour(0xff0a0a0a)); g.fillRoundedRectangle(rr.expanded(4.f, 3.f), 6.f);
            const float x0 = xOf(in.a), x1 = xOf(in.a + in.resLen);
            const float lx0 = xOf(in.a + in.resLoopA), lx1 = xOf(in.a + in.resLoopB), sx = xOf(in.a + in.resSeam);
            g.setColour(orange.withAlpha(0.10f)); g.fillRect(juce::Rectangle<float>(lx0, rr.getY(), lx1 - lx0, rr.getHeight()));
            juce::ColourGradient xf(orange.withAlpha(0.f), sx, 0, orange.withAlpha(0.28f), lx1, 0, false);
            g.setGradientFill(xf); g.fillRect(juce::Rectangle<float>(sx, rr.getY(), std::max(1.f, lx1 - sx), rr.getHeight()));
            const int bins = (int) in.resMin.size();
            float rp = 1e-4f; for (int k = 0; k < bins; ++k) rp = std::max(rp, std::max(-in.resMin[(size_t) k], in.resMax[(size_t) k]));
            const float rs = 0.46f / rp;
            for (int px = (int) x0; px < (int) x1; ++px)
            {
                const int k0 = (int) ((px - x0) / (x1 - x0) * bins), k1 = std::max(k0 + 1, (int) ((px + 1 - x0) / (x1 - x0) * bins));
                float mn = 0, mx = 0; for (int k = k0; k < std::min(k1, bins); ++k) { mn = std::min(mn, in.resMin[(size_t) k]); mx = std::max(mx, in.resMax[(size_t) k]); }
                g.setColour(px >= lx0 && px <= lx1 ? juce::Colour(0xffe9e3d8) : juce::Colour(0xff6a6863));
                g.drawVerticalLine(px, rr.getCentreY() - mx * rr.getHeight() * rs, rr.getCentreY() - mn * rr.getHeight() * rs + 1.f);
            }
            g.setColour(orange); g.fillRect(lx0 - 0.5f, rr.getY(), 1.f, rr.getHeight()); g.fillRect(lx1 - 0.5f, rr.getY(), 1.f, rr.getHeight());
            g.setColour(faint); g.setFont(mono(7.f)); g.drawText("RESULT", rr.withHeight(10.f).translated(3.f, 2.f), juce::Justification::topLeft);
        }
        // the part's edges
        for (int k = 0; k < 2; ++k)
        {
            const float x = k == 0 ? xa : xb;
            g.setColour(orange.withAlpha(drag == k + 1 || hover == k + 1 ? 1.f : 0.8f)); g.fillRect(x - 0.75f, w.getY(), 1.5f, w.getHeight());
            juce::Path t; t.addTriangle(x - 5.f, w.getY() - 1.f, x + 5.f, w.getY() - 1.f, x, w.getY() + 6.f); g.fillPath(t);
        }
        g.setColour(faint); g.setFont(mono(7.5f));
        g.drawText("0 s", juce::Rectangle<float>(w.getX(), w.getBottom() + 1.f, 40.f, 9.f), juce::Justification::centredLeft);
        g.drawText(juce::String(len, 2) + " s", juce::Rectangle<float>(xOf(len) - 50.f, w.getBottom() + 1.f, 50.f, 9.f), juce::Justification::centredRight);
        g.setColour(dim);
        g.drawText(ui::U("drag the edges · drag inside = move · double-click = automatic (the steadiest part)"),
                   juce::Rectangle<float>(w.getX(), res().getBottom() + 6.f, w.getWidth(), 10.f), juce::Justification::centred);
        // values
        const float cents = (in.root - std::round(in.root)) * 100.f;
        value(g, 0, "ROOT", juce::String(rc::noteName((int) std::lround(in.root))) + "  " + (cents >= 0 ? "+" : "") + juce::String(juce::roundToInt(cents)) + "c",
              src.ed.fix != 0 ? "fixed " + juce::String(src.ed.fix > 0 ? "+" : "") + juce::String(src.ed.fix) : juce::String(in.namePc >= 0 ? "name + detected" : "detected"));
        value(g, 1, "STRETCH", juce::String(src.ed.stretch, 2) + ui::U("×"), "drag up / down");
        value(g, 2, "SEAM", src.ed.mode == 1 ? juce::String("cycle") : juce::String(juce::roundToInt(src.ed.xfade * 100)) + "%", "crossfade");
        value(g, 3, "PART", juce::String(in.a, 2) + ui::U(" – ") + juce::String(in.b, 2) + " s", "loop " + juce::String(in.loopB - in.loopA, 2) + " s (LOOP knob)");
        const char* rb[] = { "-12", "-1", "+1", "+12" };
        for (int i = 0; i < 4; ++i)
        {
            const auto b = rootBtn(i);
            g.setColour(juce::Colour(hover == 30 + i ? 0xff222222 : 0xff181818)); g.fillRoundedRectangle(b, 4.f);
            g.setColour(juce::Colours::black.withAlpha(0.6f)); g.drawRoundedRectangle(b, 4.f, 1.f);
            g.setColour(label); g.setFont(mono(8.f, true)); g.drawText(rb[i], b, juce::Justification::centred);
        }
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = hit(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        if (proc.samples.empty()) return;
        auto& src = cur();
        const int h = hit(e.position);
        drag = 0; downY = e.position.y; downX = e.position.x;
        if (h >= 10 && h < 20) { proc.sampleSel = h - 10; stopPlay(); }
        else if (h == 20 || h == 21) { src.ed.mode = h - 20; changed(); }
        else if (h == 22) { src.ed = rc::SampleEdit(); changed(); }
        else if (h == 23) { if (proc.audition.load() >= 0) stopPlay(); else proc.audition = (int) std::lround(src.info.root) - 12 * (int) proc.apvts.getRawParameterValue("octave")->load(); }
        else if (h == 24) { stopPlay(); proc.samples.erase(proc.samples.begin() + proc.sampleSel); proc.sampleSel = 0; changed(); }
        else if (h >= 30 && h < 34) { const int d[] = { -12, -1, 1, 12 }; src.ed.fix = juce::jlimit(-36, 36, src.ed.fix + d[h - 30]); changed(); }
        else if (h == 1 || h == 2) drag = h;
        else if (h == 3) { drag = 3; downA = src.info.a; downB = src.info.b; }
        else if (h == 40) { drag = 40; downV = src.ed.stretch; }
        else if (h == 41 && src.ed.mode == 0) { drag = 41; downV = src.ed.xfade; }
        repaint();
    }
    void mouseDrag(const juce::MouseEvent& e) override
    {
        if (proc.samples.empty() || drag == 0) return;
        auto& src = cur();
        const double len = src.audio.getNumSamples() / src.rate;
        const double t = timeOf(e.position.x);
        if (drag == 1) { src.ed.a = juce::jlimit(0.0, src.info.b - 0.05, t); src.ed.b = src.info.b; }
        else if (drag == 2) { src.ed.b = juce::jlimit(src.info.a + 0.05, len, t); src.ed.a = src.info.a; }
        else if (drag == 3)
        {
            const double d = timeOf(e.position.x) - timeOf(downX), w = downB - downA;
            const double a = juce::jlimit(0.0, len - w, downA + d);
            src.ed.a = a; src.ed.b = a + w;
        }
        else if (drag == 40) src.ed.stretch = juce::jlimit(1.f, 4.f, downV + (downY - e.position.y) * 0.02f);
        else if (drag == 41) src.ed.xfade = juce::jlimit(0.05f, 0.5f, downV + (downY - e.position.y) * 0.004f);
        if (drag == 1 || drag == 2 || drag == 3) { src.info.a = src.ed.a; src.info.b = src.ed.b; }
        changed();
    }
    void mouseUp(const juce::MouseEvent&) override { drag = 0; repaint(); }
    void mouseDoubleClick(const juce::MouseEvent& e) override
    {
        if (proc.samples.empty()) return;
        auto& src = cur();
        const int h = hit(e.position);
        if (h == 1 || h == 2 || h == 3 || wave().contains(e.position)) { src.ed.a = src.ed.b = -1; changed(); }
        else if (h == 40) { src.ed.stretch = 1.f; changed(); }
        else if (h == 41) { src.ed.xfade = 0.3f; changed(); }
        else if (h == 42) { src.ed.fix = 0; changed(); }
    }
    void stopPlay() { proc.audition = -1; }
private:
    ReChordProcessor::SampleSrc& cur() { proc.sampleSel = juce::jlimit(0, (int) proc.samples.size() - 1, proc.sampleSel); return proc.samples[(size_t) proc.sampleSel]; }
    void changed()
    {
        proc.sampleEdited();
        if (proc.audition.load() >= 0 && !proc.samples.empty()) proc.audition = -1;     // replays when the new loop is in
        if (onChange) onChange();
        repaint();
    }
    juce::Rectangle<float> tab(int i) const { return { 14.f + i * 132.f, 10.f, 126.f, 22.f }; }
    juce::Rectangle<float> btn(int i) const
    {
        const float w[] = { 50.f, 54.f, 48.f, 50.f, 64.f };
        float x = (float) getWidth() - 14.f; for (int k = 4; k >= i; --k) x -= w[k] + 6.f;
        return { x + 6.f, 10.f, w[i], 22.f };
    }
    juce::Rectangle<float> wave() const { return { 18.f, 42.f, (float) getWidth() - 36.f, 70.f }; }
    juce::Rectangle<float> res() const { return { 18.f, 122.f, (float) getWidth() - 36.f, 48.f }; }
    double viewLen() const { const auto& s = proc.samples[(size_t) proc.sampleSel]; return std::max(s.audio.getNumSamples() / s.rate, s.info.a + s.info.resLen) * 1.01; }
    juce::Rectangle<float> valueBox(int i) const { const float x[] = { 18.f, 250.f, 370.f, 490.f }, w[] = { 220.f, 110.f, 110.f, 250.f }; return { x[i], 200.f, w[i], 50.f }; }
    juce::Rectangle<float> rootBtn(int i) const { return { 118.f + i * 30.f, 214.f, 27.f, 20.f }; }
    void value(juce::Graphics& g, int i, const juce::String& title, const juce::String& v, const juce::String& sub) const
    {
        using namespace ui;
        const auto b = valueBox(i);
        g.setColour(label); g.setFont(mono(8.5f, true)); g.drawText(title, b.withHeight(12.f), juce::Justification::centredLeft);
        g.setColour((i == 1 || i == 2) && (drag == 39 + i || hover == 39 + i) ? orange : white); g.setFont(mono(11.f, true));
        g.drawText(v, juce::Rectangle<float>(b.getX(), b.getY() + 14.f, i == 0 ? 96.f : b.getWidth(), 18.f), juce::Justification::centredLeft);
        g.setColour(faint); g.setFont(mono(7.5f)); g.drawText(sub, juce::Rectangle<float>(b.getX(), b.getY() + 34.f, b.getWidth(), 11.f), juce::Justification::centredLeft);
    }
    float xOf(double t) const { const auto w = wave(); return w.getX() + (float) (t / viewLen()) * w.getWidth(); }
    double timeOf(float x) const { const auto& s = proc.samples[(size_t) proc.sampleSel]; const auto w = wave(); return juce::jlimit(0.0, s.audio.getNumSamples() / s.rate, (double) ((x - w.getX()) / w.getWidth()) * viewLen()); }
    int hit(juce::Point<float> p) const
    {
        if (proc.samples.empty()) return -1;
        for (int i = 0; i < (int) proc.samples.size(); ++i) if (tab(i).contains(p)) return 10 + i;
        for (int i = 0; i < 5; ++i) if (btn(i).contains(p)) return 20 + i;
        for (int i = 0; i < 4; ++i) if (rootBtn(i).contains(p)) return 30 + i;
        const auto w = wave().expanded(0.f, 8.f);
        if (w.contains(p))
        {
            const auto& in = proc.samples[(size_t) proc.sampleSel].info;
            if (std::abs(p.x - xOf(in.a)) < 7.f) return 1;
            if (std::abs(p.x - xOf(in.b)) < 7.f) return 2;
            if (p.x > xOf(in.a) && p.x < xOf(in.b)) return 3;
            return 4;
        }
        if (valueBox(1).contains(p)) return 40;
        if (valueBox(2).contains(p)) return 41;
        if (valueBox(0).withWidth(96.f).contains(p)) return 42;
        return -1;
    }
    ReChordProcessor& proc;
    int hover = -1, drag = 0;
    float downY = 0, downX = 0, downV = 0;
    double downA = 0, downB = 0;
};

// 2.5.4: keyboard - a note added to a held chord: GLIDE IN (from the nearest note) or STRAIGHT (on its key)
class AddSwitch : public juce::Component
{
public:
    explicit AddSwitch(ReChordProcessor& p) : proc(p) {}
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const bool on = proc.apvts.getRawParameterValue("addGlide")->load() > 0.5f;
        g.setColour(label); g.setFont(mono(9.5f, true));
        g.drawText("ADDED NOTE", juce::Rectangle<float>(0, 0, (float) getWidth(), 16.f), juce::Justification::centred);
        const char* n[] = { "GLIDE IN", "STRAIGHT" };
        for (int i = 0; i < 2; ++i)
        {
            const auto r = cell(i);
            const bool sel = (i == 0) == on;
            bevelRect(g, r, 6.f, sel ? juce::Colour(0xff232323) : hover == i ? juce::Colour(0xff1e1e1e) : juce::Colour(0xff181818), false, 0.2f);
            if (sel) { g.setColour(orange); g.fillEllipse(r.getX() + 8.f, r.getCentreY() - 3.f, 6.f, 6.f); }
            g.setColour(sel || hover == i ? white : label); g.setFont(mono(8.5f, true));
            g.drawText(n[i], r.withTrimmedLeft(sel ? 10.f : 0.f), juce::Justification::centred);
        }
        g.setColour(faint); g.setFont(mono(7.5f));
        g.drawText("keyboard: 1-2 keys over a chord", juce::Rectangle<float>(0, (float) getHeight() - 14.f, (float) getWidth(), 12.f), juce::Justification::centred);
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = cell(0).contains(e.position) ? 0 : cell(1).contains(e.position) ? 1 : -1; if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < 2; ++i)
            if (cell(i).contains(e.position))
            {
                auto* p = proc.apvts.getParameter("addGlide");
                p->beginChangeGesture(); p->setValueNotifyingHost(i == 0 ? 1.f : 0.f); p->endChangeGesture();
                repaint();
            }
    }
private:
    juce::Rectangle<float> cell(int i) const { return { 6.f, 22.f + i * 28.f, (float) getWidth() - 12.f, 24.f }; }
    ReChordProcessor& proc;
    int hover = -1;
};

class MovePad : public juce::Component
{
public:
    explicit MovePad(ReChordProcessor& p) : proc(p) {}
    int last = -1;
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        for (int i = 0; i < ReChordProcessor::kNumMoves; ++i)
        {
            const auto r = cell(i);
            bevelRect(g, r, 6.f, last == i ? juce::Colour(0xff232323) : hover == i ? juce::Colour(0xff1e1e1e) : juce::Colour(0xff181818), false, 0.2f);
            if (last == i) { g.setColour(orange); g.fillEllipse(r.getX() + 8.f, r.getCentreY() - 3.f, 6.f, 6.f); }
            g.setColour(last == i || hover == i ? white : label); g.setFont(mono(8.5f, true));
            g.drawText(ReChordProcessor::moveName(i), r.withTrimmedLeft(last == i ? 10.f : 0.f), juce::Justification::centred);
        }
        g.setColour(label); g.setFont(mono(9.5f, true));
        g.drawText("MOVE", juce::Rectangle<float>(0, (float) getHeight() - 40.f, (float) getWidth(), 18.f), juce::Justification::centred);
        g.setColour(soft); g.setFont(mono(8.5f));
        g.drawText("all glides, one click", juce::Rectangle<float>(0, (float) getHeight() - 20.f, (float) getWidth(), 14.f), juce::Justification::centred);
    }
    void mouseMove(const juce::MouseEvent& e) override { const int h = at(e.position); if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override { const int h = at(e.position); if (h >= 0) { proc.applyMove(h); last = h; repaint(); } }
private:
    juce::Rectangle<float> cell(int i) const
    {
        const float w = ((float) getWidth() - 24.f) / 2.f, h = 22.f;
        return { 8.f + (i % 2) * (w + 8.f), 6.f + (i / 2) * (h + 4.f), w, h };
    }
    int at(juce::Point<float> p) const { for (int i = 0; i < ReChordProcessor::kNumMoves; ++i) if (cell(i).contains(p)) return i; return -1; }
    ReChordProcessor& proc;
    int hover = -1;
};

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
            bevelRect(g, r, 6.f, m == i ? juce::Colour(0xff232323) : hover == i ? juce::Colour(0xff1e1e1e) : juce::Colour(0xff181818), false, 0.2f);
            if (m == i) { g.setColour(orange); g.fillEllipse(r.getX() + 10.f, r.getCentreY() - 3.f, 6.f, 6.f); }
            g.setColour(m == i ? white : soft); g.setFont(mono(9.5f, true));
            g.drawText(names[i], r.withTrimmedLeft(m == i ? 14.f : 0.f), juce::Justification::centred);
        }
        g.setColour(label); g.setFont(mono(9.5f, true));
        g.drawText("MODE", juce::Rectangle<float>(0, (float) getHeight() - 40.f, (float) getWidth(), 18.f), juce::Justification::centred);
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
    juce::Rectangle<float> seg(int i) const { return { 8.f, 6.f + i * 24.f, (float) getWidth() - 16.f, 20.f }; }
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
    void resized() override { const int d = std::min(66, getWidth() - 12); slider.setBounds(getLocalBounds().withTrimmedBottom(44).withSizeKeepingCentre(d, d)); }
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        auto r = getLocalBounds().toFloat().removeFromBottom(44.f);
        g.setColour(label); g.setFont(mono(9.5f, true));
        g.drawText(titleText ? titleText() : title, r.removeFromTop(22.f).withTrimmedTop(4.f), juce::Justification::centred);
        g.setColour(slider.isMouseOverOrDragging() ? orange : soft); g.setFont(mono(9.5f));
        g.drawText(valueText ? valueText() : param->getCurrentValueAsText(), r.withTrimmedBottom(4.f), juce::Justification::centred);
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
        const char* names[] = { "FM", "ANALOG", "STRINGS", "VOICE", "EP", "SINE", "KEYS", "BOWED", "808", "SAMPLE" };
        for (int i = 0; i < rc::kNumOsc; ++i)
        {
            const auto r = seg(i);
            g.setFont(mono(8.5f, m == i));
            const float tw = juce::GlyphArrangement::getStringWidth(g.getCurrentFont(), names[i]);
            if (m == i) { g.setColour(orange); g.fillRect(juce::Rectangle<float>(tw + 2.f, 1.6f).withCentre({ r.getCentreX(), r.getCentreY() + 10.f })); }
            g.setColour(m == i ? orange : hover == i ? label : soft.withAlpha(0.8f));
            g.drawText(names[i], r, juce::Justification::centred);
        }
    }

    std::function<void()> onSampleRequest;      // SAMPLE clicked with nothing loaded, or clicked again: pick files
    void mouseMove(const juce::MouseEvent& e) override { int h = -1; for (int i = 0; i < rc::kNumOsc; ++i) if (seg(i).contains(e.position)) h = i; if (h != hover) { hover = h; repaint(); } }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        for (int i = 0; i < rc::kNumOsc; ++i)
            if (seg(i).contains(e.position))
            {
                const int m = (int) proc.apvts.getRawParameterValue("osc")->load();
                if (i == rc::OscSample && (!proc.hasSamples() || m == rc::OscSample)) { if (onSampleRequest) onSampleRequest(); if (!proc.hasSamples()) return; }
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
        const float w = (float) getWidth() / 5.f, h = ((float) getHeight() - 4.f) / 2.f;
        return { (i % 5) * w, (i / 5) * (h + 4.f), w, h };
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
        {
            const auto pr = r.reduced(0.5f);
            g.setColour(juce::Colour(0xff0d0d0d)); g.fillRoundedRectangle(pr, 16.f);
            g.setColour(juce::Colours::black); g.drawRoundedRectangle(pr, 16.f, 1.f);
            g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawRoundedRectangle(pr.translated(0, 1.f), 16.f, 0.8f);
        }
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
        const bool smp = (int) proc.apvts.getRawParameterValue("osc")->load() == rc::OscSample;
        g.setColour(smp ? orange.withAlpha(0.8f) : faint); g.setFont(mono(7.5f));
        const bool usr = proc.userPreset.isNotEmpty();
        g.drawText(smp ? proc.sampleDetail() : usr ? juce::String("USER") : juce::String(ps.group), mid.withHeight(14.f).translated(0, 5.f), juce::Justification::centred);
        g.setColour(hover == 1 ? white : label.brighter(0.2f)); g.setFont(sans(13.f));
        g.drawFittedText(usr ? proc.userPreset : smp ? proc.sampleLabel() : juce::String(ps.name) + (proc.presetModified() ? "  *" : ""), mid.withTrimmedTop(15.f).withTrimmedBottom(3.f).toNearestInt(), juce::Justification::centred, 1, 0.8f);
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
    void visibilityChanged() override { if (isVisible()) { files = ReChordProcessor::userPresets(); tab = proc.userPreset.isNotEmpty() ? kUser : groupOf(proc.presetIndex); } }
    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto r = getLocalBounds().toFloat();
        panel(g, r.reduced(1.f), 12.f, juce::Colour(0xff161618), false);
        g.setColour(dim); g.setFont(mono(9.f, true));
        g.drawText("SOUNDS", juce::Rectangle<float>(18.f, 12.f, 100.f, 14.f), juce::Justification::centredLeft);
        g.setColour(faint); g.setFont(mono(8.5f));
        g.drawText("a preset changes only the sound - chords, connections and glides stay", juce::Rectangle<float>(100.f, 12.f, 500.f, 14.f), juce::Justification::centredLeft);
        for (int t = 0; t < kGroups + 1; ++t)
        {
            const auto rr = tabCell(t);
            const bool on = tab == t, hot = hover == 300 + t, cur = proc.userPreset.isNotEmpty() ? t == kUser : groupOf(proc.presetIndex) == t;
            chipShape(g, rr, on, hot, 5.f);
            if (cur) { g.setColour(orange); g.fillRect(rr.getX() + 8.f, rr.getCentreY() - 2.5f, 5.f, 5.f); }
            g.setColour(on ? white : soft); g.setFont(mono(9.f, true));
            g.drawText(t == kUser ? "USER" : groupName(t), rr.withTrimmedLeft(cur ? 8.f : 0.f), juce::Justification::centred);
        }
        if (tab == kUser)
        {
            // + SAVE, then the saved ones (x = delete)
            for (int i = 0; i <= files.size(); ++i)
            {
                const auto cell = slot(i);
                const bool on = i > 0 && proc.userPreset == files[i - 1].getFileNameWithoutExtension(), hot = hover == 1000 + i;
                if (on) { g.setColour(orange); g.fillPath(chamfer(cell, 7.f, 2 | 8)); }
                else chipShape(g, cell, hot, false, 7.f);
                g.setColour(i == 0 ? (hot ? white : orange) : on || hot ? white : soft);
                g.setFont(i == 0 ? mono(10.f, true) : inter(12.5f, on ? 600 : 500));
                g.drawText(i == 0 ? juce::String("+  SAVE CURRENT SOUND") : files[i - 1].getFileNameWithoutExtension(), cell.reduced(14.f, 0.f), juce::Justification::centredLeft);
                if (i > 0 && hot) { const auto x = delCell(i); g.setColour(hover2 ? white : dim); g.drawLine(x.getX() + 4, x.getY() + 4, x.getRight() - 4, x.getBottom() - 4, 1.2f); g.drawLine(x.getRight() - 4, x.getY() + 4, x.getX() + 4, x.getBottom() - 4, 1.2f); }
            }
            g.setColour(faint); g.setFont(mono(8.f));
            g.drawText("saved in Documents / ReChord / Presets   -   a SAMPLE sound keeps its samples and their edits", juce::Rectangle<float>(18.f, (float) getHeight() - 22.f, (float) getWidth() - 36.f, 12.f), juce::Justification::centredLeft);
        }
        for (int i = 0; i < rc::kNumPresets; ++i)
        {
            if (groupOf(i) != tab) continue;
            const auto cell = item(i);
            const bool on = proc.userPreset.isEmpty() && proc.presetIndex == i, hot = hover == i;
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
    void mouseMove(const juce::MouseEvent& e) override
    {
        const int h = at(e.position);
        const bool h2 = h > 1000 && delCell(h - 1000).contains(e.position);
        if (h != hover || h2 != hover2) { hover = h; hover2 = h2; repaint(); }
    }
    void mouseExit(const juce::MouseEvent&) override { hover = -1; repaint(); }
    void mouseDown(const juce::MouseEvent& e) override
    {
        const int h = at(e.position);
        if (h == 200) { if (onClose) onClose(); return; }
        if (h == 1000) { askName(); return; }
        if (h > 1000)
        {
            const auto f = files[h - 1001];
            if (delCell(h - 1000).contains(e.position)) { f.moveToTrash(); if (proc.userPreset == f.getFileNameWithoutExtension()) proc.userPreset.clear(); }
            else proc.loadUserPreset(f);
            files = ReChordProcessor::userPresets(); repaint(); return;
        }
        if (h >= 300) { tab = h - 300; if (tab == kUser) files = ReChordProcessor::userPresets(); repaint(); return; }
        if (h >= 0) { proc.loadPreset(h); repaint(); }
    }
private:
    ReChordProcessor& proc;
    int hover = -1, tab = 0;
    bool hover2 = false;
    juce::Array<juce::File> files;
    std::unique_ptr<juce::AlertWindow> ask;
    static constexpr int kGroups = 9, kUser = 9;
    void askName()
    {
        juce::String def = proc.userPreset.isNotEmpty() ? proc.userPreset
                         : (int) proc.apvts.getRawParameterValue("osc")->load() == rc::OscSample && proc.hasSamples() ? proc.sampleLabel()
                         : juce::String(rc::kPresets[proc.presetIndex].name) + " 2";
        ask = std::make_unique<juce::AlertWindow>("Save sound", "Name of the preset:", juce::MessageBoxIconType::NoIcon, this);
        ask->addTextEditor("name", def);
        ask->addButton("SAVE", 1, juce::KeyPress(juce::KeyPress::returnKey));
        ask->addButton("CANCEL", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        juce::Component::SafePointer<PresetPanel> safe(this);
        ask->enterModalState(true, juce::ModalCallbackFunction::create([safe](int r)
        {
            if (safe == nullptr) return;                     // the window was closed with the dialog open
            if (r == 1 && safe->ask != nullptr) safe->proc.saveUserPreset(safe->ask->getTextEditorContents("name"));
            safe->files = ReChordProcessor::userPresets();
            safe->repaint();
            juce::MessageManager::callAsync([safe] { if (safe != nullptr) safe->ask.reset(); });
        }), false);
    }
    juce::Rectangle<float> slot(int i) const
    {
        const float w = ((float) getWidth() - 36.f - 2 * 10.f) / 3.f;
        return { 18.f + (i % 3) * (w + 10.f), 86.f + (i / 3) * 50.f, w, 42.f };
    }
    juce::Rectangle<float> delCell(int i) const { const auto c = slot(i); return { c.getRight() - 26.f, c.getCentreY() - 8.f, 16.f, 16.f }; }
    static const char* groupName(int c) { static const char* g[] = { "PIANO", "BOWED", "BASS", "EP", "DREAM", "LEADS", "STRINGS", "VOICES", "PADS" }; return g[c]; }
    static int groupOf(int i) { for (int k = 0; k < kGroups; ++k) if (juce::String(rc::kPresets[i].group) == groupName(k)) return k; return 0; }
    juce::Rectangle<float> tabCell(int t) const { const float w = ((float) getWidth() - 36.f - kGroups * 6.f) / (float) (kGroups + 1); return { 18.f + t * (w + 6.f), 40.f, w, 28.f }; }
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
        for (int t = 0; t < kGroups + 1; ++t) if (tabCell(t).contains(p)) return 300 + t;
        if (tab == kUser) { for (int i = 0; i <= files.size(); ++i) if (slot(i).contains(p)) return 1000 + i; return -1; }
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
        const auto r = getLocalBounds().toFloat().reduced(1.f, 2.f);
        g.setColour(juce::Colours::black.withAlpha(0.55f)); g.fillRoundedRectangle(r.translated(0, 2.5f), 5.f);
        juce::ColourGradient gr(juce::Colour(hot ? 0xffe2d2b4 : 0xffd8c6a6), r.getX(), r.getY(), juce::Colour(0xffa88f6a), r.getRight(), r.getBottom(), false);
        gr.addColour(0.45, juce::Colour(hot ? 0xffcdb995 : 0xffc2ad88));
        g.setGradientFill(gr); g.fillRoundedRectangle(r, 5.f);
        { juce::Path rp; rp.addRoundedRectangle(r, 5.f); g.saveState(); g.reduceClipRegion(rp); g.setTiledImageFill(metalTexture(), 0, 0, 0.22f); g.fillRect(r); g.restoreState(); }
        g.setColour(juce::Colours::white.withAlpha(0.35f)); g.drawLine(r.getX() + 4.f, r.getY() + 0.8f, r.getRight() - 4.f, r.getY() + 0.8f, 1.f);
        g.setColour(juce::Colours::black.withAlpha(0.5f)); g.drawRoundedRectangle(r, 5.f, 1.f);
        const auto ink = juce::Colour(0xff2a241c);
        const float cy = r.getY() + 21.f, ax = r.getX() + 26.f;
        juce::Path a; a.startNewSubPath(ax, cy - 9.f); a.lineTo(ax, cy + 8.f);
        a.startNewSubPath(ax - 6.f, cy + 2.f); a.lineTo(ax, cy + 8.f); a.lineTo(ax + 6.f, cy + 2.f);
        juce::Path w;
        for (int i = 0; i <= 40; ++i) { const float t = i / 40.f, x = ax + 18.f + t * 40.f, y = cy + 1.f - std::sin(t * 6.2832f) * 4.5f * (0.6f + 0.4f * std::sin(t * 3.1416f));
                                        if (i == 0) w.startNewSubPath(x, y); else w.lineTo(x, y); }
        g.setColour(ink);
        g.strokePath(a, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.strokePath(w, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour(ink.withAlpha(0.7f)); g.setFont(mono(7.f, true));
        g.drawText(busy ? U("RENDERING…") : juce::String(proc.pattern.activeBars()) + ui::U(" BARS · ") + juce::String(juce::roundToInt(proc.hostBpm.load())) + " BPM",
                   juce::Rectangle<float>(r.getX(), r.getBottom() - 16.f, r.getWidth(), 11.f), juce::Justification::centred);
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
            if (current == i)
            {
                g.setColour(juce::Colour(0xff121212)); g.fillRoundedRectangle(r, 7.f);
                g.setColour(juce::Colours::black.withAlpha(0.7f)); g.drawRoundedRectangle(r, 7.f, 1.f);
                g.setColour(juce::Colours::white.withAlpha(0.05f)); g.drawLine(r.getX() + 6.f, r.getBottom() + 0.5f, r.getRight() - 6.f, r.getBottom() + 0.5f, 1.f);
                g.setColour(orange); g.fillRect(r.getX() + 1.f, r.getY() + 7.f, 2.5f, r.getHeight() - 14.f);
            }
            if (i > 0 && current != i && current != i - 1) { g.setColour(juce::Colours::white.withAlpha(0.06f)); g.drawHorizontalLine((int) (r.getY() - 3.f), r.getX() + 10.f, r.getRight() - 30.f); }
            g.setColour(current == i ? white : hover == i ? label : soft.withAlpha(0.85f)); g.setFont(mono(10.f, current == i));
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
    juce::Rectangle<float> cell(int i) const { const float h = ((float) getHeight() - 8.f) / 3.f; return { 4.f, 4.f + i * h, (float) getWidth() - 8.f, h - 6.f }; }
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
        for (auto& f : files) if (isMidi(f) || isAudio(f)) return true;
        return false;
    }
    static bool isMidi(const juce::String& f) { return f.endsWithIgnoreCase(".mid") || f.endsWithIgnoreCase(".midi"); }
    static bool isAudio(const juce::String& f) { for (auto* e : { ".wav", ".aif", ".aiff", ".flac" }) if (f.endsWithIgnoreCase(e)) return true; return false; }
    void fileDragEnter(const juce::StringArray& files, int, int) override
    {
        dropping = true; droppingAudio = false;
        for (auto& f : files) droppingAudio = droppingAudio || isAudio(f);
        refresh();
    }
    void fileDragExit(const juce::StringArray&) override { dropping = false; refresh(); }
    void filesDropped(const juce::StringArray& files, int, int) override
    {
        dropping = false;
        juce::StringArray audio;
        for (auto& f : files) if (isAudio(f)) audio.add(f);
        if (!audio.isEmpty() && proc.loadSamples(audio) > 0) openSample(true);
        else for (auto& f : files) if (isMidi(f)) { proc.loadMidiFile(juce::File(f)); break; }
        closePicker();
        view.setSel(-2, -1);
        refresh();
    }
    explicit Device(ReChordProcessor& p)
        : proc(p), view(p), strip(p), picker(p), drag(p), mode(p), move(p), addSw(p), sampleEd(p), type(p), presetBox(p), presets(p),
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
            const char* t[] = { "BITE", "DETUNE", "ENSEMBLE", "VOWEL", "BELL", "HARMONICS", "BODY", "BODY", "DRIVE", "LOOP" };
            return juce::String(t[juce::jlimit(0, rc::kNumOsc - 1, (int) proc.apvts.getRawParameterValue("osc")->load())]);
        };
        kChar.valueText = [this]
        {
            const float c = proc.apvts.getRawParameterValue("character")->load();
            const int o = (int) proc.apvts.getRawParameterValue("osc")->load();
            if (o == rc::OscVoice)
                return juce::String(c < 0.25f ? "ah" : c < 0.42f ? "ah-oh" : c < 0.58f ? "oh" : c < 0.75f ? "oh-oo" : "oo");
            if (o == rc::OscSample)
                return juce::String(c < 0.12f ? "short" : c > 0.88f ? "long" : juce::String(juce::roundToInt(c * 100)) + "%");
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
        listen.getProperties().set("seg", 3); learnB.getProperties().set("seg", 3); smartB.getProperties().set("seg", 3);
        listenAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(proc.apvts, "listen", listen);
        listen.setTooltip("On: plays with Cubase when Cubase plays, and on its own when Cubase is stopped. Off: silent");
        type.onSampleRequest = [this] { pickSamples(); };
        smartB.onClick = [this] { proc.smart(); view.setSel(-2, -1); };
        smartB.setTooltip("Lit = the smart voicing, untouched. Click = back to it (drops your connection / octave / glide edits)");
        learnB.onClick = [this] { proc.startLearn(); };
        learnB.setTooltip("Learn the chords: put the MIDI part on this track, click LEARN, play Cubase (from a bar line). Or drop a .mid file here");
        for (auto* b : { &listen, &smartB, &learnB }) addAndMakeVisible(*b);

        allB.onClick = [this] { if (view.hasSelLink()) proc.timingToAll(view.selectedT(), view.selectedV()); };
        resetB.onClick = [this] { if (view.hasSelLink()) proc.resetTiming(view.selectedT(), view.selectedV()); };
        allB.setTooltip("Give every glide the start / end / curve of the selected one");
        resetB.setTooltip("The selected glide back to its default timing");
        addChildComponent(allB);
        addChildComponent(resetB);
        undoB.onClick = [this] { if (proc.undo()) { view.setSel(-2, -1); move.last = -1; } };
        redoB.onClick = [this] { if (proc.redo()) { view.setSel(-2, -1); move.last = -1; } };
        undoB.setTooltip("Undo the last edit of chords, notes, connections or glides (Ctrl+Z when the plugin has focus)");
        redoB.setTooltip("Redo (Ctrl+Shift+Z / Ctrl+Y)");
        addAndMakeVisible(undoB); addAndMakeVisible(redoB);
        addChildComponent(move);
        addChildComponent(addSw);
        addChildComponent(sampleEd);
        addChildComponent(sampleB);
        sampleB.onClick = [this] { proc.audition = -1; openSample(!showSample); };
        sampleEd.onChange = [this] { presetBox.repaint(); };
        setWantsKeyboardFocus(true);
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
        setSize(960, 654);
    }
    void closePicker() { picker.setVisible(false); strip.open = -99; strip.repaint(); }

    void paint(juce::Graphics& g) override
    {
        using namespace ui;
        const auto all = getLocalBounds().toFloat();
        // three bands: the header, the display + chords, the knobs
        auto band = [&](float y0, float y1, juce::Colour top, juce::Colour bot)
        {
            const auto r = juce::Rectangle<float>(0.f, y0, all.getWidth(), y1 - y0);
            juce::ColourGradient gr(top, 0, y0, bot, 0, y1, false); g.setGradientFill(gr); g.fillRect(r);
            g.setTiledImageFill(metalTexture(), 0, 0, 0.08f); g.fillRect(r);
            g.setColour(juce::Colours::black.withAlpha(0.9f)); g.drawHorizontalLine((int) y1, 0.f, all.getWidth());
            g.setColour(juce::Colours::white.withAlpha(0.045f)); g.drawHorizontalLine((int) y1 + 1, 0.f, all.getWidth());
        };
        band(0.f, 86.f, juce::Colour(0xff1f1f1f), juce::Colour(0xff191919));
        band(87.f, 366.f, juce::Colour(0xff181818), juce::Colour(0xff151515));
        band(367.f, 484.f, juce::Colour(0xff171717), juce::Colour(0xff151515));
        band(485.f, 646.f, juce::Colour(0xff1a1a1a), juce::Colour(0xff161616));
        g.setColour(juce::Colour(0xff101010)); g.fillRect(0.f, 648.f, all.getWidth(), all.getHeight() - 648.f);
        // the title
        g.setColour(white.withAlpha(0.93f)); g.setFont(inter(28.f, 300, 0.01f));
        g.drawText("ReChord", juce::Rectangle<float>(34.f, 16.f, 150.f, 36.f), juce::Justification::centredLeft);
        {
            const float tw = juce::GlyphArrangement::getStringWidth(inter(28.f, 300, 0.01f), "ReChord");
            g.setColour(orange); g.fillEllipse(34.f + tw + 10.f, 31.f, 6.5f, 6.5f);
        }
        g.setColour(dim); g.setFont(mono(7.5f, false, 0.25f));
        g.drawText("BY POURIA MOTABEAN", juce::Rectangle<float>(35.f, 54.f, 170.f, 12.f), juce::Justification::centredLeft);
        // dividers in the header
        auto vline = [&](float x, float y0, float y1)
        { g.setColour(juce::Colours::black.withAlpha(0.7f)); g.drawVerticalLine((int) x, y0, y1); g.setColour(juce::Colours::white.withAlpha(0.07f)); g.drawVerticalLine((int) x + 1, y0, y1); };
        vline(206.f, 18.f, 68.f);
        vline((float) listen.getRight(), 34.f, 52.f);
        vline((float) learnB.getRight(), 34.f, 52.f);
        vline((float) smartB.getRight() + 8.f, 18.f, 68.f);
        // tabs | knobs
        vline(140.f, 500.f, 632.f);
        g.setColour(faint.withAlpha(0.6f)); g.setFont(mono(7.f));
        g.drawText("v" RECHORD_VERSION, juce::Rectangle<float>(840.f, 630.f, 96.f, 10.f), juce::Justification::centredRight);

    }
    void resized() override
    {
        drag.setBounds(836, 15, 108, 56);
        smartB.setBounds(752, 28, 66, 30);
        learnB.setBounds(690, 28, 62, 30);
        listen.setBounds(626, 28, 64, 30);
        type.setBounds(222, 20, 202, 48);
        presetBox.setBounds(440, 19, 172, 48);
        presets.setBounds(24 + 14, 96 + 8, 912 - 28, 250);

        view.setBounds(24, 96, 912, 262);
        ui::Bars b;
        b.w = (912.f - 44.f - 8.f) / 5.24f;
        b.x0 = 44.f + 0.62f * b.w;
        view.bars = b;
        strip.bars = b;
        strip.setBounds(24, 374, 912, 106);
        picker.setBounds(24 + 30, 222, 912 - 60, 128);
        allB.setBounds(24 + 912 - 380, 96 + 262 - 26, 56, 20);
        resetB.setBounds(24 + 912 - 318, 96 + 262 - 26, 68, 20);
        undoB.setBounds(24 + 912 - 236, 96 + 262 - 26, 60, 20);
        redoB.setBounds(24 + 912 - 172, 96 + 262 - 26, 60, 20);
        sampleB.setBounds(24 + 912 - 104, 96 + 262 - 26, 90, 20);
        sampleEd.setBounds(24, 96, 912, 262);

        tabs.setBounds(24, 506, 108, 120);
        showTab();
    }
    bool keyPressed(const juce::KeyPress& k) override
    {
        const bool cmd = k.getModifiers().isCommandDown();
        if (cmd && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z')) { if (k.getModifiers().isShiftDown() ? proc.redo() : proc.undo()) view.setSel(-2, -1); return true; }
        if (cmd && (k.getKeyCode() == 'Y' || k.getKeyCode() == 'y')) { if (proc.redo()) view.setSel(-2, -1); return true; }
        return false;
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
        undoB.setEnabled(proc.canUndo()); redoB.setEnabled(proc.canRedo());
        if (addSw.isVisible()) addSw.repaint();
        {
            const bool smp = (int) proc.apvts.getRawParameterValue("osc")->load() == rc::OscSample;
            if (!smp && showSample) { showSample = false; proc.audition = -1; }
            sampleB.setVisible(smp);
            sampleB.setButtonText(showSample ? "CHORDS" : "SAMPLE EDIT");
            sampleEd.setVisible(showSample);
            if (showSample) { sampleEd.repaint(); sampleB.toFront(false); }
            undoB.setVisible(!showSample); redoB.setVisible(!showSample);
            if (showSample) { allB.setVisible(false); resetB.setVisible(false); }
        }
        learnB.setToggleState(proc.learnState.load() != 0, juce::dontSendNotification);
        const int nb = proc.pattern.activeBars();
        view.learnText = proc.learnState.load() == 1 ? juce::String("LEARN: waiting for Cubase to play - the bar of the first note becomes bar 1 (up to 8 bars)")
                       : proc.learnState.load() == 2 ? (proc.learnPos.load() < 0 ? juce::String("LEARN: playing - waiting for the first note")
                                                        : "LEARN: listening, bar " + juce::String(juce::jlimit(1, 8, (int) proc.learnPos.load() + 1)) + " (stop Cubase when the part is over)")
                       : dropping ? (droppingAudio ? juce::String("DROP: sustained notes as the SAMPLE sound (several files = one per register; a note name in the file name sets its pitch)")
                                                   : juce::String("DROP: the chords of this MIDI file")) : juce::String();
        if (view.hasSelLink() && (view.selectedV() >= proc.score.nVoices || view.selectedT() >= proc.score.nSlots)) view.setSel(-2, -1);
    }

    LinkView view;
private:
    ReChordProcessor& proc;
    bool dropping = false, droppingAudio = false;
    std::unique_ptr<juce::FileChooser> chooser;
    void pickSamples()
    {
        chooser = std::make_unique<juce::FileChooser>("Sustained notes for SAMPLE (one or several)", juce::File(), "*.wav;*.aif;*.aiff;*.flac");
        chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles | juce::FileBrowserComponent::canSelectMultipleItems,
                             [safe = juce::Component::SafePointer<Device>(this)](const juce::FileChooser& fc)
                             {
                                 if (safe == nullptr) return;
                                 juce::StringArray paths;
                                 for (auto& f : fc.getResults()) paths.add(f.getFullPathName());
                                 if (!paths.isEmpty() && safe->proc.loadSamples(paths) > 0) safe->showSample = true;
                                 safe->refresh();
                             });
    }
    ChordStrip strip;
    Picker picker;
    DragChip drag;
    ModeSwitch mode;
    MovePad move;
    AddSwitch addSw;
    juce::TextButton undoB { "UNDO" }, redoB { "REDO" };
    SampleEditor sampleEd;
    juce::TextButton sampleB { "SAMPLE EDIT" };
    bool showSample = false;
public:
    void openSample(bool on) { showSample = on; refresh(); }
private:
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
        move.setVisible(tabs.current == 1);
        move.setBounds(540, 504, 230, 124);
        addSw.setVisible(tabs.current == 1);
        addSw.setBounds(782, 508, 150, 118);
        for (auto* c : key) c->setVisible(tabs.current == 2);
        auto& list = tabs.current == 0 ? sound : tabs.current == 1 ? glide : key;
        const float step = list.size() <= 4 ? 118.f : 98.f;
        float x = 152.f;
        for (auto* c : list)
        {
            const float w = c == &mode ? 140.f : step;
            c->setBounds((int) x, 504, (int) w, 124);
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
        setResizeLimits(672, 458, 1920, 1308);
        getConstrainer()->setFixedAspectRatio(960.0 / 654.0);
        setSize(960, 654);
        startTimerHz(30);
    }
    ~ReChordEditor() override
    {
        stopTimer();
        proc.audition = -1;
        tooltips.setLookAndFeel(nullptr);
        setLookAndFeel(nullptr);
    }
    void resized() override { device.setTransform(juce::AffineTransform::scale((float) getWidth() / 960.f)); }
    void paint(juce::Graphics& g) override { g.fillAll(juce::Colours::black); }

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
    juce::SharedResourcePointer<ui::Res> resources;       // first: the last thing to go
    ui::Look look;
    juce::TooltipWindow tooltips { this, 600 };
    Device device;
    int lastModel = -1, slow = 0;
    unsigned lastParams = 0;
    double lastBeat = -2;
};
