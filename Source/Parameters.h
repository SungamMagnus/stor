#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

namespace whm
{

static constexpr int numBells = 3;      // the parametric section
static constexpr int numOps   = 2;      // the FM pair

/** Operator shapes. Sine, triangle, noise — in panel order. */
enum class OpWave { sine = 0, tri, noise };
static constexpr int numOpWaves = 3;

/** Cabinet voicings, smallest to largest. */
enum class Cab { twelve = 0, fifteen, eighteen };
static constexpr int numCabs = 3;

namespace pid
{
/* ── Pitch — shared by both engines, because a kick has one pitch ─────── */
extern const juce::String tune;     // base frequency at the reference key
extern const juce::String bend;     // how far above it the hit starts, semitones
extern const juce::String fall;     // and how long it takes to get down, ms
extern const juce::String vel;      // velocity sensitivity

/* ── Subtractive ─────────────────────────────────────────────────────── */
extern const juce::String lvlSine;
extern const juce::String lvlTri;
extern const juce::String lvlSaw;
extern const juce::String lvlFold;
extern const juce::String fold;     // fold depth
extern const juce::String foldEnv;  // and how far the amp envelope moves it
extern const juce::String cutoff;
extern const juce::String reso;
extern const juce::String filtEnv;  // octaves of cutoff from the amp envelope
extern const juce::String subA, subD, subS, subR;
extern const juce::String subLevel;

/* ── FM ──────────────────────────────────────────────────────────────── */
extern const juce::String opWave[numOps];   // [0] carrier, [1] modulator
extern const juce::String opRatio[numOps];
extern const juce::String index;            // modulation index, in radians
extern const juce::String indexEnv;         // how much of it the envelope owns
extern const juce::String xfm;              // op 1 back into op 2, closing the loop
extern const juce::String fmA, fmD, fmS, fmR;
extern const juce::String fmLevel;

/* ── Modulation envelope ─────────────────────────────────────────────
 * One envelope, shared, driving every depth on the panel. It is not either
 * engine's amp envelope: those say how loud, this says how far. */
extern const juce::String modA, modD, modS, modR;

/* ── Tape ────────────────────────────────────────────────────────────── */
extern const juce::String drive;
extern const juce::String hiss;

/* ── Cabinet ─────────────────────────────────────────────────────────── */
extern const juce::String cab;
extern const juce::String cabMix;

/* ── Room ────────────────────────────────────────────────────────────── */
extern const juce::String roomSize;
extern const juce::String roomDamp;
extern const juce::String roomMix;

/* ── EQ ──────────────────────────────────────────────────────────────── */
extern const juce::String loCut;
extern const juce::String hiCut;
extern const juce::String bellFreq[numBells];
extern const juce::String bellGain[numBells];
extern const juce::String bellQ[numBells];

/* ── Output ──────────────────────────────────────────────────────────── */
extern const juce::String outLevel;
extern const juce::String limiter;
}

juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/* ── Engineering units ───────────────────────────────────────────────────
 * Knobs carry Hz, dB and milliseconds; the engine wants gains, ratios and
 * radians. One place converts. */

/** The note that plays TUNE as written. Everything else is an offset from it,
    so a kick stays a kick and the octave above it is a tom. */
static constexpr int kReferenceNote = 36;   // C1

/** MIDI note to the multiplier it puts on TUNE. */
float noteRatio (int midiNote);

/** Resonance knob to filter Q, over the range a lowpass goes from neutral to
    singing. Kept short of self-oscillation: a kick wants a resonant thump, not
    a sine bolted onto the one it already has. */
float resonanceQ (float norm);

/** Filter envelope depth in octaves, bipolar. */
float filterOctaves (float norm);

/** Off at the bottom of the range rather than very quiet. */
float levelGain (float db);
static constexpr float kLevelOffDb = -60.0f;

/** Low cut is off at the bottom of its range, high cut at the top. */
bool loCutActive (float hz);
bool hiCutActive (float hz);
static constexpr float kLoCutOffHz = 20.0f;
static constexpr float kHiCutOffHz = 20000.0f;

/* ── Panel readouts ──────────────────────────────────────────────────────
 * Compact strings for the faceplate. The host keeps the verbose forms that
 * createLayout() installs. */

juce::String percentReadout (float v01);
juce::String dbReadout (float db);
juce::String levelReadout (float db);
juce::String hzReadout (float hz);
juce::String msReadout (float ms);
juce::String semitoneReadout (float semis);
juce::String ratioReadout (float ratio);
juce::String indexReadout (float radians);
juce::String octaveReadout (float octaves);
juce::String bipolarReadout (float v);
juce::String loCutReadout (float hz);
juce::String hiCutReadout (float hz);

extern const char* const opWaveNames[numOpWaves];
extern const char* const cabNames[numCabs];

} // namespace whm
