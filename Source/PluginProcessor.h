#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Limiter.h"
#include "Parameters.h"
#include "Random.h"
#include "StorEngine.h"

/** Everything the panel animates, published from the audio thread. */
struct PanelState
{
    std::atomic<float> voiceEnv { 0.0f };    // the amp envelopes, whichever is louder
    std::atomic<float> pitchEnv { 0.0f };    // where the fall has got to
    std::atomic<float> outLevel { 0.0f };
    std::atomic<float> limitGr  { 0.0f };    // gain reduction, 0 when idle
    std::atomic<bool>  sounding { false };
    std::atomic<int>   lastNote { str::kReferenceNote };
};

class StorProcessor final : public juce::AudioProcessor
{
public:
    StorProcessor();
    ~StorProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    /* juce::String's `const char*` ctor assumes ASCII/Latin-1, not UTF-8 — an
       accented literal needs CharPointer_UTF8 or it silently mangles. */
    const juce::String getName() const override { return juce::CharPointer_UTF8 ("Stór"); }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    /* Four seconds of decay is reachable on both envelopes, and the room runs
       on past it. Reported so an offline bounce keeps the last hit whole. */
    double getTailLengthSeconds() const override { return 6.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    PanelState panel;
    str::RandomEngine random;

private:
    str::EngineParams gather() const;

    /** A knob's engineering value with its randomiser offset folded in —
        the value the engine actually gets; the parameter itself is
        untouched, so automation and preset recall see only what was set. */
    float randomised (int knobIndex) const;

    /** Follows the host's tempo and transport, rolling the randomiser on the
        chosen grid while pid::rndSync is on. */
    void updateRandomSync (int numSamples);

    /** One stretch of samples between MIDI events. */
    void renderSegment (juce::AudioBuffer<float>&, int start, int count);

    str::StorEngine engine_;
    str::Limiter      limiter_;

    /* The right side of a mono render, so the fold-down does not allocate on
       the audio thread. */
    juce::AudioBuffer<float> monoScratch_;

    float outEnv_ = 0.0f, envCoeff_ = 0.01f;

    /* Every continuous control, fetched once in str::Kn order — gather()
       reads through this rather than one member pointer per knob, which is
       also what lets randomised() apply to any of them by index. */
    std::array<juce::RangedAudioParameter*, str::kNumKnobs> knobParams_ {};

    std::array<juce::AudioParameterChoice*, str::numOps> opWave_ {};
    juce::AudioParameterChoice* cab_ = nullptr;
    juce::AudioParameterBool*  limiterOn_ = nullptr;

    juce::AudioParameterFloat*  rndStrength_ = nullptr;
    juce::AudioParameterBool*   rndSync_ = nullptr;
    juce::AudioParameterChoice* rndRate_ = nullptr;

    /* The randomiser's own tempo clock, in samples — a free-running counter
       rather than a lock to the host's bar grid, reset when the transport
       starts so the first tick lands on the downbeat of playback. */
    double rndTickAccum_ = 0.0;
    bool   rndWasPlaying_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StorProcessor)
};
