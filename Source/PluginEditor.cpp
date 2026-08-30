#include "PluginEditor.h"

using namespace whm;
using namespace whm::panel;

namespace
{
/* Knob table, in build order — which is the order the signal runs: the pitch
   both engines read, then the subtractive engine, then the FM one, then
   everything the two of them are summed into. */
enum Kn
{
    kTune = 0, kBend, kFall, kVel,

    kLvlSine, kLvlTri, kLvlSaw, kLvlFold,
    kFold, kFoldEnv, kCutoff, kReso, kFiltEnv,
    kSubA, kSubD, kSubS, kSubR, kSubLevel,

    kRatio1, kRatio2, kIndex, kIndexEnv, kXfm,
    kFmA, kFmD, kFmS, kFmR, kFmLevel,

    kModA, kModD, kModS, kModR,
    kDrive, kHiss, kCabMix,
    kRoomSize, kRoomDamp, kRoomMix,
    kLoCut,
    kB1F, kB1G, kB1Q, kB2F, kB2G, kB2Q, kB3F, kB3G, kB3Q,
    kHiCut, kOutLevel,
    kNumKnobs
};

const juce::String* kKnobId[kNumKnobs] = {
    &pid::tune, &pid::bend, &pid::fall, &pid::vel,

    &pid::lvlSine, &pid::lvlTri, &pid::lvlSaw, &pid::lvlFold,
    &pid::fold, &pid::foldEnv, &pid::cutoff, &pid::reso, &pid::filtEnv,
    &pid::subA, &pid::subD, &pid::subS, &pid::subR, &pid::subLevel,

    &pid::opRatio[0], &pid::opRatio[1], &pid::index, &pid::indexEnv, &pid::xfm,
    &pid::fmA, &pid::fmD, &pid::fmS, &pid::fmR, &pid::fmLevel,

    &pid::modA, &pid::modD, &pid::modS, &pid::modR,
    &pid::drive, &pid::hiss, &pid::cabMix,
    &pid::roomSize, &pid::roomDamp, &pid::roomMix,
    &pid::loCut,
    &pid::bellFreq[0], &pid::bellGain[0], &pid::bellQ[0],
    &pid::bellFreq[1], &pid::bellGain[1], &pid::bellQ[1],
    &pid::bellFreq[2], &pid::bellGain[2], &pid::bellQ[2],
    &pid::hiCut, &pid::outLevel
};

/** Where a knob sits and how it is drawn. Trims are not in here — they hang
    off their parent, and the parent draws them. */
struct Cell
{
    int knob; float cx, cy, r;
    const char* label;
    bool bipolar, below;
};

const Cell kPitch[] = {
    { kTune, 150.0f, pitchY, rTune, "TUNE", false, false },
    { kBend, 290.0f, pitchY, 24.0f, "BEND", false, false },
    { kFall, 396.0f, pitchY, 24.0f, "FALL", false, false },
    { kVel,  492.0f, pitchY, 16.0f, "VEL",  false, false },
};

const Cell kSub[] = {
    /* Fold depth sits on the oscillator rail, after the saw tap and before the
       fold tap — which is the whole point: it shapes one column, not the mix. */
    { kFold, subFoldX, subFanY, rMed, "FOLD DEPTH", false, false },

    { kLvlSine, voiceCol (0), subMixRow, 18.0f, "SINE", false, false },
    { kLvlTri,  voiceCol (1), subMixRow, 18.0f, "TRI",  false, false },
    { kLvlSaw,  voiceCol (2), subMixRow, 18.0f, "SAW",  false, false },
    { kLvlFold, voiceCol (3), subMixRow, 18.0f, "FOLD", false, false },

    { kCutoff, subCutoffX, subShapeRow, rBig, "CUTOFF", false, false },
    { kReso,   subResoX,   subShapeRow, rMix, "RESO",   false, false },

    { kSubA, voiceCol (0), envRow, rSml, "ATTACK",  false, false },
    { kSubD, voiceCol (1), envRow, rSml, "DECAY",   false, false },
    { kSubS, voiceCol (2), envRow, rSml, "SUSTAIN", false, false },
    { kSubR, voiceCol (3), envRow, rSml, "RELEASE", false, false },
};

const Cell kFm[] = {
    { kRatio2, fmRatioX, fmRow2, rMix, "RATIO", false, false },
    { kRatio1, fmRatioX, fmRow1, rMix, "RATIO", false, false },
    { kIndex,  fmIndexX, fmRow2, rBig, "INDEX", false, false },
    { kXfm,    fmCrossX, fmCrossY, rMed, "CROSS FM", false, false },

    { kFmA, fmCol (0), envRow, rSml, "ATTACK",  false, false },
    { kFmD, fmCol (1), envRow, rSml, "DECAY",   false, false },
    { kFmS, fmCol (2), envRow, rSml, "SUSTAIN", false, false },
    { kFmR, fmCol (3), envRow, rSml, "RELEASE", false, false },
};

/* The one modulation source, in its own block outside both engines. */
const Cell kMod[] = {
    { kModA, modCol (0), modRow, rSml, "ATTACK",  false, false },
    { kModD, modCol (1), modRow, rSml, "DECAY",   false, false },
    { kModS, modCol (2), modRow, rSml, "SUSTAIN", false, false },
    { kModR, modCol (3), modRow, rSml, "RELEASE", false, false },
};

const Cell kChain[] = {
    { kSubLevel, subLevelX, mergeY, 24.0f, "SUB LEVEL", false, true },
    { kFmLevel,  fmLevelX,  mergeY, 24.0f, "FM LEVEL",  false, true },

    { kDrive,    140.0f, fxRow1, rBig,  "TAPE DRIVE", false, true },
    { kHiss,     240.0f, fxRow1, 16.0f, "HISS",       false, true },
    { kCabMix,   550.0f, fxRow1, rMed,  "CAB MIX",    false, true },
    { kRoomSize, 700.0f, fxRow1, rMed,  "ROOM SIZE",  false, true },
    { kRoomDamp, 790.0f, fxRow1, rMix,  "DAMPING",    false, true },
    { kRoomMix,  876.0f, fxRow1, rMed,  "ROOM MIX",   false, true },

    { kLoCut, 120.0f, fxRow2, rMed, "LOW CUT", false, true },
    { kB1F, bellCol (0, 0), fxRow2, rSml,  "1 FREQ", false, true },
    { kB1G, bellCol (0, 1), fxRow2, rMix,  "1 GAIN", true,  true },
    { kB1Q, bellCol (0, 2), fxRow2, rTiny, "1 Q",    false, true },
    { kB2F, bellCol (1, 0), fxRow2, rSml,  "2 FREQ", false, true },
    { kB2G, bellCol (1, 1), fxRow2, rMix,  "2 GAIN", true,  true },
    { kB2Q, bellCol (1, 2), fxRow2, rTiny, "2 Q",    false, true },
    { kB3F, bellCol (2, 0), fxRow2, rSml,  "3 FREQ", false, true },
    { kB3G, bellCol (2, 1), fxRow2, rMix,  "3 GAIN", true,  true },
    { kB3Q, bellCol (2, 2), fxRow2, rTiny, "3 Q",    false, true },
    { kHiCut, 790.0f, fxRow2, rMed, "HIGH CUT", false, true },

    { kOutLevel, 890.0f, fxRow2, 24.0f, "OUTPUT", false, true },
};

/* The three trims. Each is a depth on the knob it hangs off, and all three
   ride the one modulation envelope — which is what lets a trim be a depth and
   nothing else. All violet, captioned MOD ENV, and answered by the single
   violet block on the panel. */
struct Trim { int knob; float cx, cy, r; bool bipolar; };

const Trim kTrims[] = {
    { kFoldEnv,  subFoldX,   subFanY,     rMed, true  },
    { kFiltEnv,  subCutoffX, subShapeRow, rBig, true  },
    { kIndexEnv, fmIndexX,   fmRow2,      rBig, false },
};

juce::Rectangle<float> box (float cx, float cy, float r)
{
    return juce::Rectangle<float> (r * 2.3f, r * 2.3f).withCentre ({ cx, cy });
}
} // namespace

/* ── Construction ────────────────────────────────────────────────────────── */

WhoompEditor::WhoompEditor (WhoompProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setOpaque (true);
    buildControls();

    setResizable (true, true);
    setResizeLimits (700, (int) (700.0f * designH / designW),
                     1800, (int) (1800.0f * designH / designW));
    getConstrainer()->setFixedAspectRatio ((double) designW / (double) designH);
    setSize ((int) designW, (int) designH);

    startTimerHz (30);
}

WhoompEditor::~WhoompEditor() = default;

void WhoompEditor::buildControls()
{
    knobs.resize (kNumKnobs);

    auto place = [this] (int k, juce::Rectangle<float> hit, float travel = 100.0f)
    {
        auto& kn = knobs[(std::size_t) k];
        kn.param = proc.apvts.getParameter (*kKnobId[k]);
        kn.hit = hit;
        kn.travel = travel;
        jassert (kn.param != nullptr);
    };

    for (const auto& c : kPitch) place (c.knob, box (c.cx, c.cy, c.r));
    for (const auto& c : kSub)   place (c.knob, box (c.cx, c.cy, c.r));
    for (const auto& c : kFm)    place (c.knob, box (c.cx, c.cy, c.r));
    for (const auto& c : kMod)   place (c.knob, box (c.cx, c.cy, c.r));
    for (const auto& c : kChain) place (c.knob, box (c.cx, c.cy, c.r));

    for (const auto& t : kTrims)
    {
        const auto at = trimAt (t.cx, t.cy, t.r);
        place (t.knob, box (at.x, at.y, rTrim), 90.0f);
    }

    /* Every enum value has to have come through place() above — a knob left
       at its default-constructed null param is a fast, silent crash the
       first time the panel tries to read it, rather than a build error. */
    for (const auto& kn : knobs)
    {
        juce::ignoreUnused (kn);
        jassert (kn.param != nullptr);
    }

    auto addRadio = [this] (const juce::String& id, int index, juce::Rectangle<float> hit)
    {
        Radio r;
        r.param = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (id));
        r.index = index;
        r.hit = hit;
        jassert (r.param != nullptr);
        radios.push_back (r);
    };

    for (int op = 0; op < numOps; ++op)
        for (int w = 0; w < numOpWaves; ++w)
            addRadio (pid::opWave[op], w, opWaveRect (op, w).expanded (2.0f));

    for (int c = 0; c < numCabs; ++c)
        addRadio (pid::cab, c, cabRect (c).expanded (2.0f));

    Latch lim;
    lim.param = proc.apvts.getParameter (pid::limiter);
    lim.hit = limRect().expanded (3.0f);
    jassert (lim.param != nullptr);
    latches.push_back (lim);
}

/* ── Values ──────────────────────────────────────────────────────────────── */

float WhoompEditor::scale() const { return (float) getWidth() / designW; }

juce::Point<float> WhoompEditor::toDesign (juce::Point<float> px) const
{
    const float k = juce::jmax (0.0001f, scale());
    return { px.x / k, px.y / k };
}

int WhoompEditor::knobAt (juce::Point<float> design) const
{
    /* Trims sit on their control's shoulder, so the smaller target wins. */
    int best = -1;
    float bestArea = 1.0e9f;

    for (std::size_t i = 0; i < knobs.size(); ++i)
        if (knobs[i].hit.contains (design))
        {
            const float a = knobs[i].hit.getWidth() * knobs[i].hit.getHeight();
            if (a < bestArea) { bestArea = a; best = (int) i; }
        }

    return best;
}

float WhoompEditor::value (int i) const
{
    auto* p = knobs[(std::size_t) i].param;
    return p->convertFrom0to1 (p->getValue());
}

juce::String WhoompEditor::readout (int i) const
{
    const float v = value (i);

    switch (i)
    {
        case kTune:   return hzReadout (v);
        case kBend:   return semitoneReadout (v);
        case kFall:
        case kSubA: case kSubD: case kSubR:
        case kFmA:  case kFmD:  case kFmR:
        case kModA: case kModD: case kModR:  return msReadout (v);

        case kCutoff:  return hzReadout (v);
        case kFiltEnv: return octaveReadout (filterOctaves (v));
        case kFoldEnv: return bipolarReadout (v);

        case kRatio1:
        case kRatio2: return ratioReadout (v);
        case kIndex:
        case kXfm:    return indexReadout (v);

        case kSubLevel:
        case kFmLevel: return levelReadout (v);

        case kLoCut: return loCutReadout (v);
        case kHiCut: return hiCutReadout (v);

        case kB1F: case kB2F: case kB3F: return hzReadout (v);
        case kB1G: case kB2G: case kB3G: return dbReadout (v);
        case kB1Q: case kB2Q: case kB3Q: return juce::String (v, 2);

        case kOutLevel: return dbReadout (v);

        default: return percentReadout (v);
    }
}

juce::String WhoompEditor::noteReadout() const
{
    const int n = proc.panel.lastNote.load (std::memory_order_relaxed);
    const float hz = value (kTune) * noteRatio (n);

    return juce::MidiMessage::getMidiNoteName (n, true, true, 3) + "   " + hzReadout (hz);
}

/* ── Interaction ─────────────────────────────────────────────────────────── */

void WhoompEditor::toggle (const Latch& l)
{
    l.param->beginChangeGesture();
    l.param->setValueNotifyingHost (l.param->getValue() > 0.5f ? 0.0f : 1.0f);
    l.param->endChangeGesture();
    repaint();
}

void WhoompEditor::select (const Radio& r)
{
    r.param->beginChangeGesture();
    r.param->setValueNotifyingHost (r.param->convertTo0to1 ((float) r.index));
    r.param->endChangeGesture();
    repaint();
}

void WhoompEditor::mouseDown (const juce::MouseEvent& e)
{
    const auto d = toDesign (e.position);

    for (const auto& r : radios)
        if (r.hit.contains (d))
            return select (r);

    for (const auto& l : latches)
        if (l.hit.contains (d))
            return toggle (l);

    const int idx = knobAt (d);
    if (idx < 0)
        return;

    dragIdx = idx;
    dragStartNorm = knobs[(std::size_t) idx].param->getValue();
    dragStart = d;
    knobs[(std::size_t) idx].param->beginChangeGesture();
    setMouseCursor (juce::MouseCursor::NoCursor);
}

void WhoompEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragIdx < 0)
        return;

    auto& k = knobs[(std::size_t) dragIdx];
    const auto d = toDesign (e.position);
    const float sensitivity = e.mods.isShiftDown() ? 0.22f : 1.0f;
    const float delta = (dragStart.y - d.y) / k.travel;

    k.param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f,
                                                  dragStartNorm + delta * sensitivity));
    repaint();
}

void WhoompEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragIdx < 0)
        return;

    knobs[(std::size_t) dragIdx].param->endChangeGesture();
    dragIdx = -1;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}

void WhoompEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int idx = knobAt (toDesign (e.position));
    if (idx < 0)
        return;

    auto* param = knobs[(std::size_t) idx].param;
    param->beginChangeGesture();
    param->setValueNotifyingHost (param->getDefaultValue());
    param->endChangeGesture();
    repaint();
}

void WhoompEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    const int idx = knobAt (toDesign (e.position));
    if (idx < 0)
        return;

    auto* param = knobs[(std::size_t) idx].param;
    const float gain = e.mods.isShiftDown() ? 0.04f : 0.16f;
    const float delta = w.deltaY * (w.isReversed ? -1.0f : 1.0f) * gain;

    param->beginChangeGesture();
    param->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, param->getValue() + delta));
    param->endChangeGesture();
    repaint();
}

/* ── Paint ───────────────────────────────────────────────────────────────── */

void WhoompEditor::paint (juce::Graphics& g)
{
    g.fillAll (hue::paper);
    g.addTransform (juce::AffineTransform::scale (scale()));

    g.setColour (ink (0.22f));
    g.drawRect (juce::Rectangle<float> (16.0f, 16.0f, designW - 32.0f, designH - 32.0f), 1.0f);

    paintPitch (g);
    paintSubtractive (g);
    paintFm (g);
    paintMod (g);
    paintChain (g);

    tracked (g, "WHOOMP", { 990.0f, 1058.0f, 220.0f, 22.0f }, 16.0f, ink (0.55f), 5.0f, false);
    text (g, "KICK SYNTHESISER", { 990.0f, 1082.0f, 220.0f, 12.0f }, 7.5f, ink (0.38f));
}

void WhoompEditor::paintPitch (juce::Graphics& g)
{
    const auto& state = proc.panel;

    /* The bus, and the two legs that hang the engines off it. Pitch draws in
       ink because it is not a section — it is the spine both sections are hung
       on, and giving it a hue would make it look like a third one. */
    wire (g, { { midiX + 9.0f, pitchY }, { busEndX, pitchY } }, hue::ink, 0.55f);
    wire (g, { { pitchLegSubX, pitchY }, { pitchLegSubX, frameTop } }, hue::ink, 0.55f);
    wire (g, { { pitchLegFmX, pitchY }, { pitchLegFmX, frameTop } }, hue::ink, 0.55f);
    node (g, pitchLegSubX, pitchY, hue::ink, 0.55f);
    node (g, pitchLegFmX, pitchY, hue::ink, 0.55f);

    terminal (g, midiX, pitchY, "MIDI");
    lamp (g, 110.0f, pitchY, state.sounding.load (std::memory_order_relaxed), hue::ink);

    for (const auto& c : kPitch)
        knobCell (g, c.cx, c.cy, c.r, norm (c.knob), hue::ink.withAlpha (0.78f),
                  c.label, readout (c.knob), c.bipolar, c.below);

    /* Where the fall has got to, which is the one thing about this instrument
       you cannot read off a knob. */
    text (g, "PITCH ENV", { 600.0f, 148.0f, 90.0f, 12.0f }, 8.0f, ink (0.62f),
          juce::Justification::left, true, true);
    meter (g, fallMeterRect(), state.pitchEnv.load (std::memory_order_relaxed),
           hue::ink.withAlpha (0.7f));

    text (g, "NOTE", { 1000.0f, 148.0f, 50.0f, 12.0f }, 8.0f, ink (0.62f),
          juce::Justification::left, true, true);
    text (g, noteReadout(), { 1050.0f, 147.0f, 180.0f, 13.0f }, 9.5f, ink (0.78f),
          juce::Justification::left, true, true);
}

void WhoompEditor::paintSubtractive (juce::Graphics& g)
{
    frame (g, subFrame(), hue::coral, "SUBTRACTIVE");

    /* One oscillator on a rail, tapped four times. Everything on the rail to
       the left of a tap reaches that tap; FOLD DEPTH sits between the saw tap
       and the fold tap, so it is in the fold column's path and nothing else's.
       That is the whole reason it is drawn here and not after the sum. */
    wire (g, { { pitchLegSubX, frameTop }, { pitchLegSubX, subFanY } }, hue::coral, 0.45f);
    wire (g, { { voiceCol (0), subFanY }, { subFoldX - rMed, subFanY } }, hue::coral, 0.45f);
    wire (g, { { subFoldX + rMed, subFanY }, { voiceCol (3), subFanY } }, hue::coral, 0.45f);

    for (int i = 0; i < 4; ++i)
    {
        wire (g, { { voiceCol (i), subFanY }, { voiceCol (i), subMixRow - 18.0f } },
              hue::coral, 0.45f);
        node (g, voiceCol (i), subFanY, hue::coral, 0.5f, 2.4f);

        wire (g, { { voiceCol (i), subMixRow + 18.0f }, { voiceCol (i), subSumY } },
              hue::coral, 0.45f);
        node (g, voiceCol (i), subSumY, hue::coral, 0.5f, 2.4f);
    }

    /* The four levels sum, and the sum leaves from the left end of the bus so
       the run down into the filter crosses nothing on its way. */
    wire (g, { { subSumLeftX, subSumY }, { voiceCol (3), subSumY } }, hue::coral, 0.45f);
    wire (g, { { subSumLeftX, subSumY }, { subSumLeftX, subShapeRow },
               { subCutoffX - rBig, subShapeRow } }, hue::coral, 0.45f);

    wire (g, { { subCutoffX + rBig, subShapeRow }, { subResoX - rMix, subShapeRow } },
          hue::coral, 0.45f);
    wire (g, { { subResoX + rMix, subShapeRow }, { subOutX, subShapeRow },
               { subOutX, frameBot } }, hue::coral, 0.45f);

    for (const auto& c : kSub)
        knobCell (g, c.cx, c.cy, c.r, norm (c.knob), hue::coral,
                  c.label, readout (c.knob), c.bipolar, c.below);

    for (const auto& t : kTrims)
        if (t.knob == kFoldEnv || t.knob == kFiltEnv)
            trim (g, t.cx, t.cy, t.r, norm (t.knob), hue::violet, t.bipolar, "MOD ENV");

    /* The envelope, on a dashed leg round the outside of the row: it is what
       gates the path, not a stage standing in it. What the violet trims above
       are riding is the other envelope, in its own block below. */
    wire (g, { { voiceCol (3) + rSml + 8.0f, envRow }, { 560.0f, envRow },
               { 560.0f, envLegY }, { subOutX, envLegY } }, hue::coral, 0.40f, 1.1f, 4.0f);
    node (g, subOutX, envLegY, hue::coral, 0.5f, 2.4f);
    text (g, "AMP ENVELOPE", { 116.0f, envCaptionY, 200.0f, 12.0f },
          8.0f, hue::coral.withAlpha (0.85f), juce::Justification::left, true, true);
}

void WhoompEditor::paintFm (juce::Graphics& g)
{
    frame (g, fmFrame(), hue::teal, "FM");

    /* Pitch comes in once and runs down the inside edge to both operators;
       each one multiplies it by its own ratio. */
    wire (g, { { pitchLegFmX, frameTop }, { pitchLegFmX, fmPitchInY },
               { fmInX, fmPitchInY }, { fmInX, fmRow1 } }, hue::teal, 0.45f);
    wire (g, { { fmInX, fmRow2 }, { opLatchX0, fmRow2 } }, hue::teal, 0.45f);
    wire (g, { { fmInX, fmRow1 }, { opLatchX0, fmRow1 } }, hue::teal, 0.45f);
    node (g, fmInX, fmRow2, hue::teal, 0.5f);

    /* Each operator: shape, then ratio. */
    wire (g, { { opLatchEndX, fmRow2 }, { fmRatioX - rMix, fmRow2 } }, hue::teal, 0.45f);
    wire (g, { { opLatchEndX, fmRow1 }, { fmRatioX - rMix, fmRow1 } }, hue::teal, 0.45f);

    /* Op two out through the index and down into op one's phase, arriving on
       the same node its pitch does — which is exactly what phase modulation
       is. */
    wire (g, { { fmRatioX + rMix, fmRow2 }, { fmIndexX - rBig, fmRow2 } }, hue::teal, 0.45f);
    wire (g, { { fmIndexX + rBig, fmRow2 }, { fmLoopRightX, fmRow2 },
               { fmLoopRightX, fmLoopY }, { fmInX, fmLoopY } }, hue::teal, 0.45f);
    node (g, fmInX, fmLoopY, hue::teal, 0.5f);

    /* Op one out of the bottom — and, tapped off the same line, back up the
       inside edge into op two through CROSS. That return is what makes the
       two operators modulate each other instead of one just feeding the
       other; at zero it is an ordinary two-operator stack. */
    wire (g, { { fmRatioX + rMix, fmRow1 }, { fmOutX, fmRow1 },
               { fmOutX, frameBot } }, hue::teal, 0.45f);

    wire (g, { { fmOutX, fmCrossY }, { fmCrossX + rMed, fmCrossY } }, hue::teal, 0.45f);
    wire (g, { { fmCrossX - rMed, fmCrossY }, { fmCrossLegX, fmCrossY },
               { fmCrossLegX, fmRow2 }, { fmInX, fmRow2 } }, hue::teal, 0.45f);
    node (g, fmOutX, fmCrossY, hue::teal, 0.6f, 3.4f);

    text (g, "OP 2", { opLatchX0, fmRow2 - 34.0f, 44.0f, 12.0f }, 8.5f, hue::teal,
          juce::Justification::left, true, true);
    text (g, "OP 1", { opLatchX0, fmRow1 - 34.0f, 44.0f, 12.0f }, 8.5f, hue::teal,
          juce::Justification::left, true, true);

    for (const auto& r : radios)
        if (r.param->paramID != pid::cab)
            latch (g, r.hit.reduced (2.0f), r.param->getIndex() == r.index, hue::teal,
                   opWaveNames[r.index], 7.5f);

    for (const auto& c : kFm)
        knobCell (g, c.cx, c.cy, c.r, norm (c.knob), hue::teal,
                  c.label, readout (c.knob), c.bipolar, c.below);

    for (const auto& t : kTrims)
        if (t.knob == kIndexEnv)
            trim (g, t.cx, t.cy, t.r, norm (t.knob), hue::violet, t.bipolar, "MOD ENV");

    wire (g, { { fmCol (3) + rSml + 8.0f, envRow }, { 1095.0f, envRow },
               { 1095.0f, envLegY }, { fmOutX, envLegY } }, hue::teal, 0.40f, 1.1f, 4.0f);
    node (g, fmOutX, envLegY, hue::teal, 0.5f, 2.4f);
    text (g, "AMP ENVELOPE", { 696.0f, envCaptionY, 200.0f, 12.0f },
          8.0f, hue::teal.withAlpha (0.85f), juce::Justification::left, true, true);
}

void WhoompEditor::paintMod (juce::Graphics& g)
{
    frame (g, modFrame(), hue::violet, "MOD ENVELOPE");

    for (const auto& c : kMod)
        knobCell (g, c.cx, c.cy, c.r, norm (c.knob), hue::violet,
                  c.label, readout (c.knob), c.bipolar, c.below);

    /* Named targets, so the block says what it reaches without three wires
       crossing the panel to say the same thing. */
    text (g, juce::String (juce::CharPointer_UTF8 ("DRIVES  FOLD \xc2\xb7 CUTOFF \xc2\xb7 INDEX")),
          { modFrameL + 20.0f, modTargetsY, 340.0f, 12.0f }, 7.5f,
          hue::violet.withAlpha (0.75f), juce::Justification::left, true, true);
}

void WhoompEditor::paintChain (juce::Graphics& g)
{
    const auto& state = proc.panel;

    /* Both engines onto one rail, then one line through everything after. */
    wire (g, { { subOutX, frameBot }, { subOutX, mergeY }, { fmOutX, mergeY },
               { fmOutX, frameBot } }, hue::steel, 0.55f);
    node (g, subOutX, mergeY, hue::steel, 0.55f);
    node (g, fmOutX, mergeY, hue::steel, 0.55f);
    node (g, mergeNodeX, mergeY, hue::steel, 0.6f, 3.6f);

    wire (g, { { mergeNodeX, mergeY }, { mergeNodeX, returnY },
               { chainLeftX, returnY }, { chainLeftX, fxRow1 } }, hue::steel, 0.55f);
    wire (g, { { chainLeftX, fxRow1 }, { fxTurnX, fxRow1 }, { fxTurnX, fxLinkY },
               { chainLeftX, fxLinkY }, { chainLeftX, fxRow2 } }, hue::steel, 0.55f);
    wire (g, { { chainLeftX, fxRow2 }, { outX - 10.0f, fxRow2 } }, hue::steel, 0.55f);

    for (const auto& c : kChain)
        knobCell (g, c.cx, c.cy, c.r, norm (c.knob), hue::steel,
                  c.label, readout (c.knob), c.bipolar, c.below);

    text (g, "CAB", { cabRect (0).getX(), fxRow1 + 24.0f, 46.0f, 12.0f }, 8.0f, ink (0.62f),
          juce::Justification::left, true, true);

    for (const auto& r : radios)
        if (r.param->paramID == pid::cab)
            latch (g, r.hit.reduced (2.0f), r.param->getIndex() == r.index, hue::steel,
                   cabNames[r.index], 8.0f);

    /* The limiter lives on the meter it guards. */
    latch (g, limRect(), latches[0].param->getValue() > 0.5f, hue::amber, "LIM");
    lamp (g, 1025.0f, fxRow2, state.limitGr.load (std::memory_order_relaxed) > 0.02f, hue::amber);

    terminal (g, outX, fxRow2, "OUT");
    segMeter (g, outX, outMeterTop, state.outLevel.load (std::memory_order_relaxed), hue::steel);
}
