#include "Panel.h"

namespace str::panel
{

namespace
{
/* The pot sweep: 317 degrees, leaving a gap at the bottom where the pointer
   never goes. Wide enough that a knob at 50% points at noon and you can read
   the position from across the room. */
constexpr float kSweep = 158.6f;

float angleFor (float norm)
{
    return juce::degreesToRadians (-kSweep + 2.0f * kSweep * juce::jlimit (0.0f, 1.0f, norm));
}

void arc (juce::Graphics& g, float cx, float cy, float r, float from, float to,
          juce::Colour c, float width)
{
    if (std::abs (to - from) < 1.0e-4f)
        return;

    juce::Path p;
    p.addCentredArc (cx, cy, r, r, 0.0f, from, to, true);
    g.setColour (c);
    g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::butt));
}
} // namespace

/* ── Type ────────────────────────────────────────────────────────────────── */

juce::Font mono (float h, bool bold)
{
    return juce::Font (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), h,
                                          bold ? juce::Font::bold : juce::Font::plain));
}

void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
           float size, juce::Colour c, juce::Justification just, bool bold, bool backed)
{
    const auto f = mono (size, bold);

    /* Paper behind the glyphs, so a trace running under a label stops at it
       rather than being scribbled through. Sized to the string, not to the
       box, or every label would punch a hole the width of its cell. */
    if (backed && s.isNotEmpty())
    {
        const float w = juce::GlyphArrangement::getStringWidth (f, s) + 8.0f;
        const float x = just == juce::Justification::left  ? r.getX() - 4.0f
                      : just == juce::Justification::right ? r.getRight() - w + 4.0f
                                                           : r.getCentreX() - w * 0.5f;
        /* A pixel proud top and bottom, so a label and the value stacked
           under it mask as one block rather than leaving a stub of trace
           showing through the seam between them. */
        g.setColour (hue::paper);
        g.fillRect (x, r.getY() - 1.0f, w, r.getHeight() + 2.0f);
    }

    g.setFont (f);
    g.setColour (c);
    g.drawText (s, r, just, false);
}

void tracked (juce::Graphics& g, const juce::String& s, juce::Rectangle<float> r,
              float size, juce::Colour c, float tracking, bool leftAlign, bool bold)
{
    const auto f = mono (size, bold);
    g.setFont (f);
    g.setColour (c);

    float total = 0.0f;
    for (int i = 0; i < s.length(); ++i)
        total += juce::GlyphArrangement::getStringWidth (f, s.substring (i, i + 1)) + tracking;
    total -= tracking;

    float x = leftAlign ? r.getX() : r.getCentreX() - total * 0.5f;

    for (int i = 0; i < s.length(); ++i)
    {
        const auto ch = s.substring (i, i + 1);
        const float w = juce::GlyphArrangement::getStringWidth (f, ch);
        g.drawText (ch, juce::Rectangle<float> (x, r.getY(), w, r.getHeight()),
                    juce::Justification::left, false);
        x += w + tracking;
    }
}

/* ── Circuit ─────────────────────────────────────────────────────────────── */

void wire (juce::Graphics& g, std::initializer_list<juce::Point<float>> pts, juce::Colour c,
           float alpha, float width, float dashLen)
{
    if (pts.size() < 2)
        return;

    juce::Path p;
    bool first = true;
    for (const auto& pt : pts)
    {
        if (first) { p.startNewSubPath (pt); first = false; }
        else        p.lineTo (pt);
    }

    g.setColour (c.withAlpha (alpha));

    if (dashLen > 0.0f)
    {
        const float dashes[] = { dashLen, dashLen };
        juce::Path dashed;
        juce::PathStrokeType (width).createDashedStroke (dashed, p, dashes, 2);
        g.fillPath (dashed);
    }
    else
    {
        g.strokePath (p, juce::PathStrokeType (width, juce::PathStrokeType::mitered,
                                               juce::PathStrokeType::butt));
    }
}

void node (juce::Graphics& g, float x, float y, juce::Colour c, float alpha, float r)
{
    g.setColour (c.withAlpha (alpha));
    g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
}

void lamp (juce::Graphics& g, float cx, float cy, bool on, juce::Colour c)
{
    const juce::Rectangle<float> r (cx - 4.5f, cy - 4.5f, 9.0f, 9.0f);

    if (on)
    {
        g.setColour (c);
        g.fillRect (r);
    }
    else
    {
        g.setColour (hue::paper);
        g.fillRect (r);
        g.setColour (c.withAlpha (0.45f));
        g.drawRect (r, 1.0f);
    }
}

void terminal (juce::Graphics& g, float x, float y, const juce::String& label)
{
    constexpr float r = 6.5f;

    g.setColour (hue::paper);
    g.fillEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
    g.setColour (ink (0.7f));
    g.drawEllipse (x - r, y - r, r * 2.0f, r * 2.0f, 1.3f);

    text (g, label, { x - 45.0f, y - 26.0f, 90.0f, 12.0f }, 8.0f, ink (0.62f));
}

void frame (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour c,
            const juce::String& title)
{
    g.setColour (c.withAlpha (0.34f));
    g.drawRect (r, 1.0f);

    /* The title sits in the rule rather than under it, so the frame reads as
       one labelled thing rather than a box with a caption above it. */
    const float w = 12.0f + 7.4f * (float) title.length();
    g.setColour (hue::paper);
    g.fillRect (r.getX() + 14.0f, r.getY() - 6.0f, w, 12.0f);
    text (g, title, { r.getX() + 20.0f, r.getY() - 7.0f, w, 13.0f }, 8.5f, c,
          juce::Justification::left);
}

/* ── Controls ────────────────────────────────────────────────────────────── */

void knob (juce::Graphics& g, float cx, float cy, float r, float norm,
           juce::Colour colour, bool bipolar)
{
    /* Paper behind it, so a knob standing on a trace breaks the trace the way
       a component on a board does. */
    g.setColour (hue::paper);
    g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
    g.setColour (ink (0.38f));
    g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.2f);

    const float aStart = angleFor (0.0f);
    const float aEnd   = angleFor (1.0f);
    const float aNow   = angleFor (norm);
    const float track  = r + 4.5f;
    const float w      = r >= 24.0f ? 3.4f : (r >= 16.0f ? 2.8f : 2.2f);

    arc (g, cx, cy, track, aStart, aEnd, ink (0.13f), w);
    arc (g, cx, cy, track, bipolar ? 0.0f : aStart, aNow, colour, w);

    /* The pointer, and a tick at the anticlockwise stop so the sweep has a
       readable origin. */
    const float px = std::sin (aNow), py = -std::cos (aNow);
    g.setColour (ink (0.85f));
    g.drawLine (cx + px * r * 0.16f, cy + py * r * 0.16f,
                cx + px * r * 0.78f, cy + py * r * 0.78f,
                r >= 24.0f ? 2.0f : 1.5f);

    const float tx = std::sin (aStart), ty = -std::cos (aStart);
    g.setColour (ink (0.28f));
    g.drawLine (cx + tx * (r + 1.5f), cy + ty * (r + 1.5f),
                cx + tx * (r + 6.5f), cy + ty * (r + 6.5f), 1.0f);
}

void knobCell (juce::Graphics& g, float cx, float cy, float r, float norm,
               juce::Colour colour, const juce::String& label, const juce::String& value,
               bool bipolar, bool below)
{
    knob (g, cx, cy, r, norm, colour, bipolar);

    const float fs = r >= 30.0f ? 9.5f : (r >= 18.0f ? 8.5f : 7.5f);
    const float ly = below ? cy + r + 6.0f  : cy - r - 34.0f;
    const float vy = below ? cy + r + 19.0f : cy - r - 21.0f;

    /* Both backed: a knob's designation and its reading are the two things
       that must stay readable wherever the trace happens to run. */
    if (label.isNotEmpty())
        text (g, label, { cx - 70.0f, ly, 140.0f, 12.0f }, fs, ink (0.62f),
              juce::Justification::centred, true, true);
    if (value.isNotEmpty())
        text (g, value, { cx - 70.0f, vy, 140.0f, 13.0f }, fs + 2.0f, colour,
              juce::Justification::centred, true, true);
}

void trim (juce::Graphics& g, float cx, float cy, float r, float norm,
           juce::Colour source, bool bipolar, const juce::String& sourceName)
{
    const auto p = trimAt (cx, cy, r);

    /* A dashed leg back to its parent: this is a depth on that control, not a
       control of its own. */
    wire (g, { { cx + r * 0.72f, cy - r * 0.72f }, { p.x - rTrim, p.y + rTrim } },
          source, 0.5f, 1.0f, 3.0f);

    knob (g, p.x, p.y, rTrim, norm, source, bipolar);

    /* And a caption saying what is on the other end of it. */
    if (sourceName.isNotEmpty())
        text (g, sourceName, { p.x - 50.0f, p.y + rTrim + 5.0f, 100.0f, 11.0f }, 7.0f,
              source.withAlpha (0.85f), juce::Justification::centred, true, true);
}

void armBox (juce::Graphics& g, float cx, float cy, float r, bool armed)
{
    const auto p = armAt (cx, cy, r);
    const juce::Rectangle<float> box (p.x - armSize * 0.5f, p.y - armSize * 0.5f, armSize, armSize);

    if (armed)
    {
        g.setColour (hue::violet);
        g.fillRect (box);
    }
    else
    {
        g.setColour (hue::paper);
        g.fillRect (box);
        g.setColour (hue::violet.withAlpha (0.38f));
        g.drawRect (box, 1.0f);
    }
}

void ghost (juce::Graphics& g, float cx, float cy, float r, float norm, float rnd)
{
    const float track = r + 4.5f;
    const float w = r >= 24.0f ? 3.4f : (r >= 16.0f ? 2.8f : 2.2f);
    arc (g, cx, cy, track, angleFor (norm), angleFor (rnd), hue::violet.withAlpha (0.75f), w);

    const float a = angleFor (rnd);
    const float px = std::sin (a), py = -std::cos (a);
    g.setColour (hue::violet);
    g.drawLine (cx + px * (r + 1.0f), cy + py * (r + 1.0f),
                cx + px * (r + 9.0f), cy + py * (r + 9.0f), 1.6f);
}

void latch (juce::Graphics& g, juce::Rectangle<float> r, bool on, juce::Colour colour,
            const juce::String& label, float size)
{
    g.setColour (on ? colour : hue::paper);
    g.fillRect (r);
    g.setColour (colour.withAlpha (on ? 1.0f : 0.55f));
    g.drawRect (r, 1.2f);

    text (g, label, r, size, on ? hue::paper : ink (0.62f));
}

void meter (juce::Graphics& g, juce::Rectangle<float> r, float value, juce::Colour c)
{
    g.setColour (hue::paper);
    g.fillRect (r);
    g.setColour (ink (0.3f));
    g.drawRect (r, 1.0f);

    const float w = r.getWidth() * juce::jlimit (0.0f, 1.0f, value);
    if (w > 0.5f)
    {
        g.setColour (c.withAlpha (0.75f));
        g.fillRect (r.getX() + 1.0f, r.getY() + 1.0f,
                    juce::jmax (0.0f, w - 2.0f), r.getHeight() - 2.0f);
    }
}

void segMeter (juce::Graphics& g, float cx, float top, float level, juce::Colour c,
               int segments)
{
    constexpr float w = 16.0f, h = 6.0f, gap = 3.0f;

    const float v = juce::jlimit (0.0f, 1.0f, level);
    const int lit = juce::roundToInt (v * (float) segments);

    for (int i = 0; i < segments; ++i)
    {
        /* Drawn top-down, filled bottom-up: the last segment is the one that
           means you are nearly out of room. */
        const juce::Rectangle<float> r (cx - w * 0.5f, top + (float) i * (h + gap), w, h);
        const bool on = (segments - i) <= lit;

        g.setColour (hue::paper);
        g.fillRect (r);
        g.setColour (on ? (i == 0 ? hue::amber : c) : ink (0.22f));
        if (on) g.fillRect (r); else g.drawRect (r, 1.0f);
    }
}

} // namespace str::panel
