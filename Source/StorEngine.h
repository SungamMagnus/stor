#pragma once

#include <algorithm>
#include <array>
#include <vector>

#include "Dsp.h"
#include "Parameters.h"

namespace str
{

/** Everything the engine needs for a block, already in engineering units. */
struct EngineParams
{
    /* Pitch — one frequency, shared. */
    float tuneHz = 52.0f;
    float bendSemis = 30.0f;
    float fallMs = 22.0f;
    float velAmount = 0.6f;

    /* Subtractive. lvl is sine, triangle, saw, fold — panel order. */
    std::array<float, 4> lvl { { 1.0f, 0.0f, 0.0f, 0.0f } };
    float fold = 0.25f, foldEnv = 0.0f;
    float cutoffHz = 18000.0f, resoQ = 0.707f, filtOctaves = 0.0f;
    float subA = 0.5f, subD = 320.0f, subS = 0.0f, subR = 120.0f;
    float subGain = 1.0f;

    /* FM. [0] is the carrier, [1] the modulator. */
    std::array<OpWave, numOps> wave { { OpWave::sine, OpWave::sine } };
    std::array<float, numOps> ratio { { 1.0f, 2.0f } };
    float index = 2.0f, indexEnv = 1.0f, xfm = 0.0f;
    float fmA = 0.5f, fmD = 90.0f, fmS = 0.0f, fmR = 60.0f;
    float fmGain = 0.0f;

    /* The modulation envelope: one source for every depth on the panel. */
    float modA = 0.5f, modD = 120.0f, modS = 0.0f, modR = 80.0f;

    /* Tape, cabinet, room, EQ, output. */
    float drive = 0.0f, hiss = 0.5f;
    Cab   cab = Cab::eighteen;
    float cabMix = 0.0f;
    float roomSize = 0.35f, roomDamp = 0.55f, roomMix = 0.0f;
    float loCutHz = kLoCutOffHz, hiCutHz = kHiCutOffHz;
    std::array<float, numBells> bellF { { 60.0f, 400.0f, 3000.0f } };
    std::array<float, numBells> bellG { { 0.0f, 0.0f, 0.0f } };
    std::array<float, numBells> bellQ { { 1.0f, 1.0f, 1.0f } };
    float outGain = 1.0f;
};

/**
 * The instrument. One monophonic voice — a kick is one drum, and a second
 * simultaneous one is a second instance — through a fixed chain: tape, then
 * the cabinet, then the room that cabinet is standing in, and an EQ behind all
 * of it that gets to shape the room as well.
 *
 * The voice and the tape saturator run at twice the host rate and come back
 * down through a linear-phase halfband. Everything after them is linear, so it
 * stays at the host rate where it belongs.
 */
class StorEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset();

    void setParams (const EngineParams&);

    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);
    void allNotesOff();

    /** Renders the voice and its chain into the two buffers, overwriting. */
    void process (float* left, float* right, int numSamples);

    /** Reported to the host: the halfband's group delay, and nothing else. */
    static constexpr int latencySamples() { return Halfband::latency; }

    /* Panel telemetry, read on the message thread between blocks. */
    float voiceEnv() const { return voiceEnv_; }
    float pitchEnv() const { return fall_.value(); }
    bool  sounding() const { return subEnv_.active() || fmEnv_.active(); }

private:
    /** One oversampled sample of the raw voice, before the tape. */
    float renderVoice();

    /** The tape stage, at the oversampled rate. */
    float tape (float x);

    /** Cabinet: linear, mono, at the host rate, in front of the room. */
    float cabinet (float x);

    /** EQ: behind the room, so it shapes what the room did too. One set of
        state per side. */
    float eq (float x, int channel);

    void designCab();
    void designEq();
    void designRoom();

    static float opSample (OpWave, float phase, Noise&);

    double sr_ = 48000.0, sr2_ = 96000.0;

    EngineParams p_;

    /* Voice */
    Osc  osc_;
    Svf  filter_;
    Adsr subEnv_, fmEnv_, modEnv_;
    Fall fall_;
    std::array<float, numOps> opPhase_ { { 0.0f, 0.0f } };

    /* Op 1's last output, held for op 2 to read. The loop has to break
       somewhere, and one sample is the cheapest place to break it. */
    float prevCar_ = 0.0f;
    /* Distinct seeds: two generators left on the default would run the same
       sequence, and the day they were ever consumed at the same rate the hiss
       and the operator would be the same noise. */
    Noise opNoise_ { 0x1234567u }, hissNoise_ { 0x9e3779b9u };

    int   note_ = kReferenceNote;
    float baseHz_ = 52.0f;
    float velGain_ = 1.0f;
    std::array<int, 8> held_ {};
    int   numHeld_ = 0;

    /* Tape */
    Halfband decim_;
    Biquad bump_;
    float lossState_ = 0.0f, lossCoeff_ = 1.0f;
    float hissHp_ = 0.0f;
    float hissGate_ = 0.0f, hissRelease_ = 0.9999f;
    float dcX_ = 0.0f, dcY_ = 0.0f, dcCoeff_ = 0.999f;

    /* Cabinet — highpass, the bump, the mid scoop, and where it stops. */
    std::array<Biquad, 4> cabChain_;

    /* Room — four combs and two allpasses a side. */
    struct Delay
    {
        std::vector<float> buf;
        int   size = 1, pos = 0;
        float store = 0.0f;      // the comb's damping one-pole

        void resize (int n) { size = n < 1 ? 1 : n; buf.assign ((std::size_t) size, 0.0f); pos = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); pos = 0; store = 0.0f; }
    };

    static constexpr int numCombs = 4, numAllpass = 2;
    std::array<std::array<Delay, numCombs>, 2> combs_;
    std::array<std::array<Delay, numAllpass>, 2> allpass_;
    float combFb_ = 0.7f, combDamp_ = 0.4f, combNorm_ = 0.25f;

    /* The room is fed through a fixed high pass. Sending a 50 Hz drum into
       12 ms combs gives comb filtering on the fundamental, not a room — what
       you actually hear in front of a kick is the top half of it coming back
       off the walls. */
    float roomHpState_ = 0.0f, roomHpCoeff_ = 0.98f;

    /* EQ — low cut, three bells, high cut, per side. */
    std::array<Biquad, 2> loCut_, hiCut_;
    std::array<std::array<Biquad, numBells>, 2> bells_;
    bool loCutOn_ = false, hiCutOn_ = false;

    /* Cached so coefficients are only redesigned when a knob actually moved. */
    EngineParams cached_ {};
    bool haveCache_ = false;

    float voiceEnv_ = 0.0f;
    float envCoeff_ = 0.01f;
};

} // namespace str
