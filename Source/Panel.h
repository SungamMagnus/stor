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
namespace str::panel
{

constexpr float designW = 1280.0f, designH = 1240.0f;

/* ── Palette ─────────────────────────────────────────────────────────────
 * Four hues, four roles, plus the limiter's. Pitch has no hue of its own — it
 * draws in ink, because it is not a section, it is the spine both sections
 * are hung on.
 *
 * Violet is the modulation envelope and everything it drives: the block
 * itself, and every trim riding it. One source, one colour, so a violet knob
 * anywhere on the panel is answered by the one violet block on it.
 */
namespace hue
{
const juce::Colour paper  { 0xfff0ece2 };
const juce::Colour ink    { 0xff1a1a17 };
const juce::Colour coral  { 0xffed8159 };   // the subtractive engine
const juce::Colour teal   { 0xff52b0a4 };   // the FM engine
const juce::Colour steel  { 0xff4f7ea8 };   // everything after the two are summed
const juce::Colour violet { 0xff6b5bc4 };   // the modulation envelope, and all it drives
const juce::Colour amber  { 0xffc08d16 };   // the limiter
}

inline juce::Colour ink (float alpha) { return hue::ink.withAlpha (alpha); }

/* ── Sizes ───────────────────────────────────────────────────────────────── */
constexpr float rTune = 32.0f, rBig = 26.0f, rMed = 20.0f, rMix = 18.0f;
constexpr float rSml = 15.0f, rTiny = 13.0f, rTrim = 11.0f;

/* ── The pitch bus ───────────────────────────────────────────────────────── */
constexpr float pitchY = 130.0f;
constexpr float midiX  = 46.0f, busEndX = 985.0f;
constexpr float pitchLegSubX = 340.0f, pitchLegFmX = 950.0f;

inline juce::Rectangle<float> fallMeterRect() { return { 700.0f, 152.0f, 180.0f, 9.0f }; }

/* ── The two engines ─────────────────────────────────────────────────────── */
constexpr float frameTop = 210.0f, frameBot = 660.0f;
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
   and drawing it in line would say it was one. These say how loud; the violet
   block below says how far, and is what the trims ride. */
constexpr float envRow = 620.0f, envLegY = 566.0f, envCaptionY = 552.0f;

/* ── Subtractive ─────────────────────────────────────────────────────────
 * One oscillator on a rail, tapped four times. FOLD DEPTH sits on that rail
 * between the saw tap and the fold tap, because that is exactly what it does:
 * everything upstream of it is untouched, and only the fold column is folded.
 */
constexpr float subFanY = 286.0f;          // the oscillator rail
constexpr float subMixRow = 345.0f;        // the four levels
constexpr float subSumY = 381.0f;          // and where they sum
constexpr float subShapeRow = 505.0f;      // the filter
constexpr float subFoldX = 450.0f;         // fold depth, on the rail
constexpr float subCutoffX = 200.0f, subResoX = 330.0f;
constexpr float subSumLeftX = 110.0f;      // where the sum leaves the rail
constexpr float subOutX = 500.0f;

inline float voiceCol (int i) { return 150.0f + 120.0f * (float) i; }

/* ── FM ──────────────────────────────────────────────────────────────────
 * Op 2 across the top into op 1 below it, and op 1 back up into op 2 through
 * CROSS — so the pair modulate each other rather than one feeding the other.
 */
constexpr float fmPitchInY = 250.0f;
constexpr float fmRow2 = 330.0f;           // the modulator
constexpr float fmLoopY = 392.0f;          // op 2 down into op 1
constexpr float fmRow1 = 452.0f;           // the carrier
constexpr float fmCrossY = 520.0f;         // and op 1 back up into op 2
constexpr float fmInX = 700.0f, fmOutX = 1000.0f, fmLoopRightX = 1180.0f;
constexpr float fmCrossLegX = 678.0f;      // the return runs up the inside edge
constexpr float fmRatioX = 930.0f, fmIndexX = 1060.0f, fmCrossX = 800.0f;

constexpr float opLatchX0 = 723.0f, opLatchW = 44.0f, opLatchStep = 48.0f, opLatchH = 17.0f;
constexpr float opLatchEndX = opLatchX0 + opLatchStep * 2.0f + opLatchW;

inline juce::Rectangle<float> opWaveRect (int op, int wave)
{
    const float cy = op == 0 ? fmRow1 : fmRow2;
    return { opLatchX0 + opLatchStep * (float) wave, cy - opLatchH * 0.5f, opLatchW, opLatchH };
}

inline float fmCol (int i) { return 730.0f + 105.0f * (float) i; }

/* ── The modulation envelope ─────────────────────────────────────────────
 * Its own block, outside both frames, because it belongs to neither: one
 * source for every depth on the panel. It sits in the space the two engines'
 * outputs leave on their way to the sum. */
constexpr float modFrameL = 46.0f, modFrameR = 430.0f;
constexpr float modFrameTop = 690.0f, modFrameBot = 800.0f;
constexpr float modRow = 748.0f, modTargetsY = 772.0f;

inline float modCol (int i) { return 100.0f + 80.0f * (float) i; }

inline juce::Rectangle<float> modFrame()
{
    return { modFrameL, modFrameTop, modFrameR - modFrameL, modFrameBot - modFrameTop };
}

/* ── Where the two engines meet ──────────────────────────────────────────── */
constexpr float mergeY = 745.0f, mergeNodeX = 750.0f;
constexpr float subLevelX = 600.0f, fmLevelX = 900.0f;
constexpr float returnY = 828.0f, chainLeftX = 80.0f;

/* ── The chain ───────────────────────────────────────────────────────────── */
constexpr float fxRow1 = 880.0f, fxTurnX = 960.0f, fxLinkY = 960.0f, fxRow2 = 994.0f;

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
constexpr float outMeterTop = 905.0f;

inline juce::Rectangle<float> limRect() { return { 961.0f, fxRow2 - 8.5f, 38.0f, 17.0f }; }

/** Every trim sits off its control's upper right, clear of the value text. */
inline juce::Point<float> trimAt (float cx, float cy, float r)
{
    return { cx + r + 26.0f, cy - r - 8.0f };
}

/* ── Random ──────────────────────────────────────────────────────────────
 * Its own block below the chain: a layer over every control on the panel,
 * not a stage in any one signal path, so it does not live inside either
 * engine's frame or on the chain's rail. */
constexpr float rndFrameL = 46.0f, rndFrameR = 590.0f;
constexpr float rndFrameTop = 1070.0f, rndFrameBot = 1200.0f;
constexpr float rndRow = 1148.0f, rndCaptionY = 1184.0f;
constexpr float rndStrengthX = 110.0f, rndRateX = 430.0f, rndLampX = 520.0f;
constexpr float rndTrigX0 = 236.0f, rndTrigStep = 76.0f, rndTrigW = 70.0f, rndTrigH = 18.0f;

inline juce::Rectangle<float> rndFrame()
{
    return { rndFrameL, rndFrameTop, rndFrameR - rndFrameL, rndFrameBot - rndFrameTop };
}

inline juce::Rectangle<float> rndTrigRect (int i)
{
    return { rndTrigX0 + rndTrigStep * (float) i, rndRow - rndTrigH * 0.5f, rndTrigW, rndTrigH };
}

/** Every control's arm box: a small square on its upper-left shoulder,
    opposite where a trim hangs off the upper right — set far enough out to
    clear the value text above the dial. */
constexpr float armSize = 9.0f;

inline juce::Point<float> armAt (float cx, float cy, float r)
{
    const float d = r + 14.0f;
    return { cx - 0.866f * d, cy - 0.5f * d };
}

/* ── Type ────────────────────────────────────────────────────────────────── */
juce::Font mono (float h, bool bold = false);

/** `backed` paints paper behind the glyphs before drawing them, so a trace
    running under a label is interrupted by it rather than scribbled through
    it — the same trick the frame titles use to sit in their own rule. */
void text (juce::Graphics&, const juce::String&, juce::Rectangle<float>,
           float size, juce::Colour,
           juce::Justification = juce::Justification::centred, bool bold = true,
           bool backed = false);

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

/** A modulation trim, tapped off its control's shoulder. Drawn in the colour
    of whatever is driving it and captioned with the same, so which envelope
    is on the other end is legible two ways. */
void trim (juce::Graphics&, float cx, float cy, float r, float norm,
           juce::Colour source, bool bipolar = true,
           const juce::String& sourceName = "AMP ENV");

void latch (juce::Graphics&, juce::Rectangle<float>, bool on, juce::Colour,
            const juce::String& label, float size = 8.0f);

/** The box that arms a control for the randomiser: outlined when off,
    filled when the control is armed. Always violet — arming is a
    modulation decision, and violet is what modulation always draws in. */
void armBox (juce::Graphics&, float cx, float cy, float r, bool armed);

/** What the randomiser is doing to an armed control right now: the span
    between the value it was set to and where this roll landed, with a tick
    at the far end. The knob's own pointer still shows the set value. */
void ghost (juce::Graphics&, float cx, float cy, float r, float norm, float rnd);

void meter (juce::Graphics&, juce::Rectangle<float>, float value, juce::Colour);

/** A level meter stood on end, filling from the bottom. */
void segMeter (juce::Graphics&, float cx, float top, float level, juce::Colour,
               int segments = 7);

} // namespace str::panel
