#pragma once

#include <initializer_list>

#include <juce_gui_basics/juce_gui_basics.h>

/**
 * Flat drawing primitives and the panel grid, in a fixed 1280 x 1000 design
 * space; the editor applies one scale transform, so the layout constants below
 * double as hit-test geometry.
 *
 * The panel is the circuit. The pitch bus runs across the top because both
 * engines hang off it; the two engines sit side by side in their own frames
 * because that is what parallel means; and everything after they are summed
 * runs in one line, in the order the signal actually goes through it. Size
 * says how much a control matters. Colour says which part of the circuit it
 * belongs to, and nothing is coloured for any other reason.
 */
namespace whm::panel
{

constexpr float designW = 1280.0f, designH = 1000.0f;

/* ── Palette ─────────────────────────────────────────────────────────────
 * Four hues, four roles, plus the limiter's. Pitch has no hue of its own — it
 * draws in ink, because it is not a section, it is the spine both sections are
 * hung on.
 */
namespace hue
{
const juce::Colour paper  { 0xfff0ece2 };
const juce::Colour ink    { 0xff1a1a17 };
const juce::Colour coral  { 0xffed8159 };   // the subtractive engine
const juce::Colour teal   { 0xff52b0a4 };   // the FM engine
const juce::Colour steel  { 0xff4f7ea8 };   // everything after the two are summed
const juce::Colour violet { 0xff6b5bc4 };   // modulation, and only modulation
const juce::Colour amber  { 0xffc08d16 };   // the limiter
}

inline juce::Colour ink (float alpha) { return hue::ink.withAlpha (alpha); }

/* ── Sizes ───────────────────────────────────────────────────────────────── */
constexpr float rTune = 32.0f, rBig = 26.0f, rMed = 20.0f, rMix = 18.0f;
constexpr float rSml = 15.0f, rTiny = 13.0f, rTrim = 11.0f;

/* ── The pitch bus ───────────────────────────────────────────────────────── */
constexpr float pitchY = 136.0f;
constexpr float midiX  = 46.0f, busEndX = 1000.0f;
constexpr float pitchLegSubX = 340.0f, pitchLegFmX = 950.0f;

inline juce::Rectangle<float> fallMeterRect() { return { 700.0f, 158.0f, 180.0f, 9.0f }; }

/* ── The two engines ─────────────────────────────────────────────────────── */
constexpr float frameTop = 196.0f, frameBot = 576.0f;
constexpr float subFrameL = 46.0f, subFrameR = 636.0f;
constexpr float fmFrameL = 660.0f, fmFrameR = 1234.0f;

inline juce::Rectangle<float> subFrame()
{
    return { subFrameL, frameTop, subFrameR - subFrameL, frameBot - frameTop };
}

inline juce::Rectangle<float> fmFrame()
{
    return { fmFrameL, frameTop, fmFrameR - fmFrameL, frameBot - frameTop };
}

/* Both engines put their amp envelope on the same bottom row, and hang it off
   the signal path on a dashed leg — an envelope is a modulator, not a stage,
   and drawing it in line would say it was one. */
constexpr float envRow = 520.0f, envLegY = 470.0f;

/* ── Subtractive: one oscillator, fanned to four shapes and summed ───────── */
constexpr float subFanY = 266.0f, subMixRow = 300.0f, subSumY = 334.0f;
constexpr float subShapeRow = 400.0f;          // fold, cutoff, resonance
constexpr float subFoldX = 150.0f, subCutoffX = 340.0f, subResoX = 500.0f;
constexpr float subOutX = 500.0f;

inline float voiceCol (int i) { return 150.0f + 120.0f * (float) i; }

/* ── FM: op two across the top, looping back into op one below it ────────── */
constexpr float fmRow2 = 300.0f;               // the modulator
constexpr float fmLoopY = 360.0f;              // and its way back down
constexpr float fmRow1 = 420.0f;               // the carrier
constexpr float fmInX = 700.0f, fmOutX = 1000.0f, fmLoopRightX = 1180.0f;
constexpr float fmRatioX = 930.0f, fmIndexX = 1060.0f;

constexpr float opLatchX0 = 723.0f, opLatchW = 44.0f, opLatchStep = 48.0f, opLatchH = 17.0f;
constexpr float opLatchEndX = opLatchX0 + opLatchStep * 2.0f + opLatchW;

inline juce::Rectangle<float> opWaveRect (int op, int wave)
{
    const float cy = op == 0 ? fmRow1 : fmRow2;
    return { opLatchX0 + opLatchStep * (float) wave, cy - opLatchH * 0.5f, opLatchW, opLatchH };
}

inline float fmCol (int i) { return 730.0f + 105.0f * (float) i; }

/* ── Where the two engines meet ──────────────────────────────────────────── */
constexpr float mergeY = 628.0f, mergeNodeX = 750.0f;
constexpr float subLevelX = 600.0f, fmLevelX = 900.0f;
constexpr float returnY = 706.0f, chainLeftX = 80.0f;

/* ── The chain ───────────────────────────────────────────────────────────── */
constexpr float fxRow1 = 760.0f, fxTurnX = 960.0f, fxLinkY = 840.0f, fxRow2 = 874.0f;

constexpr float cabLatchW = 46.0f, cabLatchH = 17.0f;
inline juce::Rectangle<float> cabRect (int i)
{
    return { 327.0f + 50.0f * (float) i, fxRow1 - cabLatchH * 0.5f, cabLatchW, cabLatchH };
}

/** Each bell is three knobs: where, how much, how narrow. */
inline float bellCol (int bell, int knob)
{
    return 210.0f + 190.0f * (float) bell + 58.0f * (float) knob;
}

constexpr float outX = 1200.0f;
constexpr float outMeterTop = 785.0f;

inline juce::Rectangle<float> limRect() { return { 961.0f, fxRow2 - 8.5f, 38.0f, 17.0f }; }

/** Every trim sits off its control's upper right, clear of the value text. */
inline juce::Point<float> trimAt (float cx, float cy, float r)
{
    return { cx + r + 26.0f, cy - r - 8.0f };
}

/* ── Type ────────────────────────────────────────────────────────────────── */
juce::Font mono (float h, bool bold = false);

void text (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
           float size, juce::Colour,
           juce::Justification = juce::Justification::centred, bool bold = true);

/** Letter-spaced run — the wordmark. */
void tracked (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
              float size, juce::Colour, float tracking,
              bool leftAlign = true, bool bold = true);

/* ── Circuit ─────────────────────────────────────────────────────────────── */

/** A trace: a run of right-angle corners through the points given. */
void wire (juce::Graphics&, std::initializer_list<juce::Point<float>>, juce::Colour,
           float alpha, float width = 1.4f, float dashLen = 0.0f);

void node (juce::Graphics&, float x, float y, juce::Colour, float alpha, float r = 3.0f);

/** Lit when something is happening, outlined when it is not. */
void lamp (juce::Graphics&, float cx, float cy, bool on, juce::Colour);
void terminal (juce::Graphics&, float x, float y, const juce::String& label);

/** A section, drawn as the box the components in it sit inside. */
void frame (juce::Graphics&, juce::Rectangle<float>, juce::Colour, const juce::String& title);

/* ── Controls ────────────────────────────────────────────────────────────── */

/** One circle, one arc, one pointer. Bipolar controls grow their arc from
    noon; unipolar ones from the anticlockwise stop. */
void knob (juce::Graphics&, float cx, float cy, float r, float norm,
           juce::Colour, bool bipolar = false);

/** A control on the circuit: its dial, with designation and value above — or
    below, where the trace above it is spoken for. */
void knobCell (juce::Graphics&, float cx, float cy, float r, float norm,
               juce::Colour, const juce::String& label, const juce::String& value,
               bool bipolar = false, bool below = false);

/** A modulation trim, tapped off its control's shoulder. */
void trim (juce::Graphics&, float cx, float cy, float r, float norm, bool bipolar = true);

void latch (juce::Graphics&, juce::Rectangle<float>, bool on, juce::Colour,
            const juce::String& label, float size = 8.0f);

void meter (juce::Graphics&, juce::Rectangle<float>, float value, juce::Colour);

/** A level meter stood on end, filling from the bottom. */
void segMeter (juce::Graphics&, float cx, float top, float level, juce::Colour,
               int segments = 7);

} // namespace whm::panel
