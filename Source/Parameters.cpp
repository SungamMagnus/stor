#include "Parameters.h"

#include <cmath>

namespace whm
{

namespace pid
{
const juce::String tune = "tune";
const juce::String bend = "bend";
const juce::String fall = "fall";
const juce::String vel  = "vel";

const juce::String lvlSine  = "lvlsine";
const juce::String lvlTri   = "lvltri";
const juce::String lvlSaw   = "lvlsaw";
const juce::String lvlFold  = "lvlfold";
const juce::String fold     = "fold";
const juce::String foldEnv  = "foldenv";
const juce::String cutoff   = "cutoff";
const juce::String reso     = "reso";
const juce::String filtEnv  = "filtenv";
const juce::String subA     = "suba";
const juce::String subD     = "subd";
const juce::String subS     = "subs";
const juce::String subR     = "subr";
const juce::String subLevel = "sublevel";

const juce::String opWave[numOps]  = { "op1wave", "op2wave" };
const juce::String opRatio[numOps] = { "op1ratio", "op2ratio" };
const juce::String index    = "fmindex";
const juce::String indexEnv = "fmindexenv";
const juce::String xfm      = "fmxfm";
const juce::String fmA      = "fma";
const juce::String fmD      = "fmd";
const juce::String fmS      = "fms";
const juce::String fmR      = "fmr";
const juce::String fmLevel  = "fmlevel";

const juce::String modA = "moda";
const juce::String modD = "modd";
const juce::String modS = "mods";
const juce::String modR = "modr";

const juce::String drive = "drive";
const juce::String hiss  = "hiss";

const juce::String cab    = "cab";
const juce::String cabMix = "cabmix";

const juce::String roomSize = "roomsize";
const juce::String roomDamp = "roomdamp";
const juce::String roomMix  = "roommix";

const juce::String loCut = "locut";
const juce::String hiCut = "hicut";
const juce::String bellFreq[numBells] = { "bell1f", "bell2f", "bell3f" };
const juce::String bellGain[numBells] = { "bell1g", "bell2g", "bell3g" };
const juce::String bellQ[numBells]    = { "bell1q", "bell2q", "bell3q" };

const juce::String outLevel = "outlevel";
const juce::String limiter  = "limiter";
}

const char* const opWaveNames[numOpWaves] = { "SIN", "TRI", "NSE" };
const char* const cabNames[numCabs]       = { "12\"", "15\"", "18\"" };

/* ── Engineering units ───────────────────────────────────────────────────── */

float noteRatio (int midiNote)
{
    return std::pow (2.0f, (float) (midiNote - kReferenceNote) / 12.0f);
}

float resonanceQ (float n)   { return 0.707f + juce::jlimit (0.0f, 1.0f, n) * 8.3f; }
float filterOctaves (float n) { return juce::jlimit (-1.0f, 1.0f, n) * 6.0f; }

float levelGain (float db)
{
    return db <= kLevelOffDb + 0.1f ? 0.0f : juce::Decibels::decibelsToGain (db);
}

bool loCutActive (float hz) { return hz > kLoCutOffHz + 0.5f; }
bool hiCutActive (float hz) { return hz < kHiCutOffHz - 100.0f; }

/* ── Readouts ────────────────────────────────────────────────────────────── */

juce::String percentReadout (float v01)
{
    return juce::String (juce::roundToInt (juce::jlimit (0.0f, 1.0f, v01) * 100.0f)) + "%";
}

juce::String dbReadout (float db)
{
    if (std::abs (db) < 0.05f) return "0.0 dB";
    return (db > 0.0f ? "+" : "") + juce::String (db, 1) + " dB";
}

juce::String levelReadout (float db)
{
    return db <= kLevelOffDb + 0.1f ? juce::String ("OFF") : dbReadout (db);
}

juce::String hzReadout (float hz)
{
    if (hz >= 1000.0f)
        return juce::String (hz / 1000.0f, hz < 10000.0f ? 2 : 1) + " kHz";

    return (hz < 100.0f ? juce::String (hz, 1)
                        : juce::String (juce::roundToInt (hz))) + " Hz";
}

juce::String msReadout (float ms)
{
    if (ms < 10.0f)  return juce::String (ms, 2) + " ms";
    if (ms < 100.0f) return juce::String (ms, 1) + " ms";
    return juce::String (juce::roundToInt (ms)) + " ms";
}

juce::String semitoneReadout (float semis)
{
    return semis < 0.05f ? juce::String ("OFF") : "+" + juce::String (semis, 1) + " st";
}

juce::String ratioReadout (float ratio)
{
    return juce::String (ratio, ratio < 10.0f ? 2 : 1) + ":1";
}

juce::String indexReadout (float radians)
{
    return radians < 0.005f ? juce::String ("OFF") : juce::String (radians, 2) + " rad";
}

juce::String octaveReadout (float octaves)
{
    if (std::abs (octaves) < 0.02f) return "OFF";
    return (octaves > 0.0f ? "+" : "") + juce::String (octaves, 2) + " oct";
}

juce::String bipolarReadout (float v)
{
    if (std::abs (v) < 0.01f) return "OFF";
    return (v > 0.0f ? "+" : "") + juce::String (v, 2);
}

juce::String loCutReadout (float hz) { return loCutActive (hz) ? hzReadout (hz) : juce::String ("OFF"); }
juce::String hiCutReadout (float hz) { return hiCutActive (hz) ? hzReadout (hz) : juce::String ("OPEN"); }

/* ── Layout ──────────────────────────────────────────────────────────────── */

namespace
{
using Range  = juce::NormalisableRange<float>;
using Attrib = juce::AudioParameterFloatAttributes;

Range logRange (float lo, float hi)
{
    Range r (lo, hi);
    r.setSkewForCentre (std::sqrt (lo * hi));
    return r;
}

std::unique_ptr<juce::AudioParameterFloat> makeFloat (const juce::String& id,
                                                      const juce::String& name,
                                                      Range range, float def,
                                                      std::function<juce::String (float, int)> fmt)
{
    return std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { id, 1 }, name, range, def,
        Attrib().withStringFromValueFunction (std::move (fmt)));
}

std::unique_ptr<juce::AudioParameterBool> makeBool (const juce::String& id,
                                                    const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}

std::unique_ptr<juce::AudioParameterChoice> makeChoice (const juce::String& id,
                                                        const juce::String& name,
                                                        juce::StringArray choices, int def)
{
    return std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { id, 1 }, name,
                                                         std::move (choices), def);
}

const auto pct     = [] (float v, int) { return percentReadout (v); };
const auto ms      = [] (float v, int) { return msReadout (v); };
const auto hz      = [] (float v, int) { return hzReadout (v); };
const auto lvl     = [] (float v, int) { return levelReadout (v); };
const auto ratioFn = [] (float v, int) { return ratioReadout (v); };
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    /* ── Pitch ───────────────────────────────────────────────────────────
     * The pitch envelope is the kick. Both engines read the same frequency,
     * because two of them sweeping separately is two drums, not one. */
    layout.add (makeFloat (pid::tune, "Tune", logRange (20.0f, 200.0f), 52.0f, hz));

    layout.add (makeFloat (pid::bend, "Pitch Bend", Range (0.0f, 48.0f), 30.0f,
                           [] (float v, int) { return semitoneReadout (v); }));

    layout.add (makeFloat (pid::fall, "Pitch Fall", logRange (1.0f, 500.0f), 22.0f, ms));
    layout.add (makeFloat (pid::vel, "Velocity", Range (0.0f, 1.0f), 0.6f, pct));

    /* ── Subtractive ─────────────────────────────────────────────────────
     * Four shapes off one phase accumulator, so they stay locked. Sine up,
     * everything else down — the others are what you add to it. */
    layout.add (makeFloat (pid::lvlSine, "Sine Level", Range (0.0f, 1.0f), 1.0f, pct));
    layout.add (makeFloat (pid::lvlTri,  "Triangle Level", Range (0.0f, 1.0f), 0.0f, pct));
    layout.add (makeFloat (pid::lvlSaw,  "Saw Level", Range (0.0f, 1.0f), 0.0f, pct));
    layout.add (makeFloat (pid::lvlFold, "Fold Level", Range (0.0f, 1.0f), 0.0f, pct));

    layout.add (makeFloat (pid::fold, "Fold Depth", Range (0.0f, 1.0f), 0.25f, pct));

    /* Bipolar: a fold that opens with the hit and closes as it decays is the
       useful direction, but running it backwards gives a click that cleans up
       into a tone. */
    layout.add (makeFloat (pid::foldEnv, "Fold Envelope", Range (-1.0f, 1.0f), 0.0f,
                           [] (float v, int) { return bipolarReadout (v); }));

    layout.add (makeFloat (pid::cutoff, "Cutoff", logRange (30.0f, 18000.0f), 18000.0f, hz));
    layout.add (makeFloat (pid::reso, "Resonance", Range (0.0f, 1.0f), 0.0f, pct));
    layout.add (makeFloat (pid::filtEnv, "Filter Envelope", Range (-1.0f, 1.0f), 0.0f,
                           [] (float v, int) { return octaveReadout (filterOctaves (v)); }));

    layout.add (makeFloat (pid::subA, "Sub Attack", logRange (0.05f, 200.0f), 0.5f, ms));
    layout.add (makeFloat (pid::subD, "Sub Decay", logRange (5.0f, 4000.0f), 320.0f, ms));
    layout.add (makeFloat (pid::subS, "Sub Sustain", Range (0.0f, 1.0f), 0.0f, pct));
    layout.add (makeFloat (pid::subR, "Sub Release", logRange (5.0f, 4000.0f), 120.0f, ms));
    layout.add (makeFloat (pid::subLevel, "Sub Level", Range (kLevelOffDb, 12.0f), 0.0f, lvl));

    /* ── FM ──────────────────────────────────────────────────────────────
     * Two operators, op 2 into op 1. Ratios multiply the same swept pitch the
     * subtractive side runs on, so the FM part bends with it. */
    layout.add (makeChoice (pid::opWave[0], "Op 1 Wave",
                            { opWaveNames[0], opWaveNames[1], opWaveNames[2] }, 0));
    layout.add (makeChoice (pid::opWave[1], "Op 2 Wave",
                            { opWaveNames[0], opWaveNames[1], opWaveNames[2] }, 0));

    layout.add (makeFloat (pid::opRatio[0], "Op 1 Ratio", logRange (0.25f, 16.0f), 1.0f, ratioFn));
    layout.add (makeFloat (pid::opRatio[1], "Op 2 Ratio", logRange (0.25f, 16.0f), 2.0f, ratioFn));

    layout.add (makeFloat (pid::index, "FM Index", Range (0.0f, 12.0f), 2.0f,
                           [] (float v, int) { return indexReadout (v); }));

    /* All of it by default: an index that does not move is a static timbre,
       and the click at the front of an FM kick is the index collapsing. */
    layout.add (makeFloat (pid::indexEnv, "Index Envelope", Range (0.0f, 1.0f), 1.0f, pct));

    /* And the way back: op 1's own output into op 2's phase, which is what
       makes the pair modulate each other rather than one feeding the other.
       Off by default — a plain 2-op stack is the thing you reach for first,
       and this turns it into something with a lot more edge in it. */
    layout.add (makeFloat (pid::xfm, "Cross FM", Range (0.0f, 8.0f), 0.0f,
                           [] (float v, int) { return indexReadout (v); }));

    layout.add (makeFloat (pid::fmA, "FM Attack", logRange (0.05f, 200.0f), 0.5f, ms));
    layout.add (makeFloat (pid::fmD, "FM Decay", logRange (5.0f, 4000.0f), 90.0f, ms));
    layout.add (makeFloat (pid::fmS, "FM Sustain", Range (0.0f, 1.0f), 0.0f, pct));
    layout.add (makeFloat (pid::fmR, "FM Release", logRange (5.0f, 4000.0f), 60.0f, ms));

    /* Off by default: the subtractive side alone is already a kick, and the FM
       layer is what you bring in on top of it. */
    layout.add (makeFloat (pid::fmLevel, "FM Level", Range (kLevelOffDb, 12.0f), kLevelOffDb, lvl));

    /* ── Modulation envelope ─────────────────────────────────────────────
     * Every depth on the panel rides this one, and nothing else does. Keeping
     * it off the two amp envelopes is the whole point: how far a fold opens
     * and how long the drum rings are different questions, and tying them
     * together is what makes a kick synth feel like it only has one shape in
     * it. Short by default, because a modulation that outlasts the transient
     * it is shaping is not shaping the transient. */
    layout.add (makeFloat (pid::modA, "Mod Attack", logRange (0.05f, 200.0f), 0.5f, ms));
    layout.add (makeFloat (pid::modD, "Mod Decay", logRange (5.0f, 4000.0f), 120.0f, ms));
    layout.add (makeFloat (pid::modS, "Mod Sustain", Range (0.0f, 1.0f), 0.0f, pct));
    layout.add (makeFloat (pid::modR, "Mod Release", logRange (5.0f, 4000.0f), 80.0f, ms));

    /* ── Tape ────────────────────────────────────────────────────────────
     * Gain into an asymmetric soft clip, a head bump under it, and the top
     * coming off as it goes — the three things a cassette does that a clean
     * distortion does not. Hiss rides the drive and the voice's own envelope,
     * so a loaded instance sitting idle does not put a noise floor in the mix
     * that never goes away. */
    layout.add (makeFloat (pid::drive, "Tape Drive", Range (0.0f, 1.0f), 0.0f, pct));
    layout.add (makeFloat (pid::hiss, "Tape Hiss", Range (0.0f, 1.0f), 0.5f, pct));

    /* ── Cabinet ─────────────────────────────────────────────────────────── */
    layout.add (makeChoice (pid::cab, "Cabinet", { cabNames[0], cabNames[1], cabNames[2] }, 2));
    layout.add (makeFloat (pid::cabMix, "Cabinet Mix", Range (0.0f, 1.0f), 0.0f, pct));

    /* ── Room ────────────────────────────────────────────────────────────
     * Short and small on purpose. A kick in a hall is a different record; what
     * this is for is the half-metre of air a microphone hears. */
    layout.add (makeFloat (pid::roomSize, "Room Size", Range (0.0f, 1.0f), 0.35f, pct));
    layout.add (makeFloat (pid::roomDamp, "Room Damping", Range (0.0f, 1.0f), 0.55f, pct));
    layout.add (makeFloat (pid::roomMix, "Room Mix", Range (0.0f, 1.0f), 0.0f, pct));

    /* ── EQ ──────────────────────────────────────────────────────────────
     * Both cuts default to out of the way, so the section is three bells until
     * you ask it for more. */
    layout.add (makeFloat (pid::loCut, "Low Cut", logRange (kLoCutOffHz, 600.0f), kLoCutOffHz,
                           [] (float v, int) { return loCutReadout (v); }));
    layout.add (makeFloat (pid::hiCut, "High Cut", logRange (600.0f, kHiCutOffHz), kHiCutOffHz,
                           [] (float v, int) { return hiCutReadout (v); }));

    static const char* bellNames[numBells]    = { "Bell 1", "Bell 2", "Bell 3" };
    static const float bellDefaults[numBells] = { 60.0f, 400.0f, 3000.0f };

    for (int b = 0; b < numBells; ++b)
    {
        layout.add (makeFloat (pid::bellFreq[b], juce::String (bellNames[b]) + " Frequency",
                               logRange (20.0f, 18000.0f), bellDefaults[b], hz));
        layout.add (makeFloat (pid::bellGain[b], juce::String (bellNames[b]) + " Gain",
                               Range (-18.0f, 18.0f), 0.0f,
                               [] (float v, int) { return dbReadout (v); }));
        layout.add (makeFloat (pid::bellQ[b], juce::String (bellNames[b]) + " Q",
                               logRange (0.2f, 12.0f), 1.0f,
                               [] (float v, int) { return juce::String (v, 2); }));
    }

    /* ── Output ──────────────────────────────────────────────────────────── */
    layout.add (makeFloat (pid::outLevel, "Output", Range (-24.0f, 12.0f), 0.0f,
                           [] (float v, int) { return dbReadout (v); }));

    /* Off by default: a kick running hot into the DAW is often what you want,
       and the host has somewhere to put it. */
    layout.add (makeBool (pid::limiter, "Limiter", false));

    return layout;
}

} // namespace whm
