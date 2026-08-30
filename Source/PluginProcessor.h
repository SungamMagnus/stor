#pragma once

#include <array>
#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Limiter.h"
#include "Parameters.h"
#include "WhoompEngine.h"

/** Everything the panel animates, published from the audio thread. */
struct PanelState
{
    std::atomic<float> voiceEnv { 0.0f };    // the amp envelopes, whichever is louder
    std::atomic<float> pitchEnv { 0.0f };    // where the fall has got to
    std::atomic<float> outLevel { 0.0f };
    std::atomic<float> limitGr  { 0.0f };    // gain reduction, 0 when idle
    std::atomic<bool>  sounding { false };
    std::atomic<int>   lastNote { whm::kReferenceNote };
};

class WhoompProcessor final : public juce::AudioProcessor
{
public:
    WhoompProcessor();
    ~WhoompProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Whoomp"; }
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

private:
    whm::EngineParams gather() const;

    /** One stretch of samples between MIDI events. */
    void renderSegment (juce::AudioBuffer<float>&, int start, int count);

    whm::WhoompEngine engine_;
    whm::Limiter      limiter_;

    /* The right side of a mono render, so the fold-down does not allocate on
       the audio thread. */
    juce::AudioBuffer<float> monoScratch_;

    float outEnv_ = 0.0f, envCoeff_ = 0.01f;

    juce::AudioParameterFloat* tune_ = nullptr;
    juce::AudioParameterFloat* bend_ = nullptr;
    juce::AudioParameterFloat* fall_ = nullptr;
    juce::AudioParameterFloat* vel_  = nullptr;

    std::array<juce::AudioParameterFloat*, 4> lvl_ {};
    juce::AudioParameterFloat* fold_ = nullptr;
    juce::AudioParameterFloat* foldEnv_ = nullptr;
    juce::AudioParameterFloat* cutoff_ = nullptr;
    juce::AudioParameterFloat* reso_ = nullptr;
    juce::AudioParameterFloat* filtEnv_ = nullptr;
    juce::AudioParameterFloat* subA_ = nullptr;
    juce::AudioParameterFloat* subD_ = nullptr;
    juce::AudioParameterFloat* subS_ = nullptr;
    juce::AudioParameterFloat* subR_ = nullptr;
    juce::AudioParameterFloat* subLevel_ = nullptr;

    std::array<juce::AudioParameterChoice*, whm::numOps> opWave_ {};
    std::array<juce::AudioParameterFloat*, whm::numOps> opRatio_ {};
    juce::AudioParameterFloat* index_ = nullptr;
    juce::AudioParameterFloat* indexEnv_ = nullptr;
    juce::AudioParameterFloat* fmA_ = nullptr;
    juce::AudioParameterFloat* fmD_ = nullptr;
    juce::AudioParameterFloat* fmS_ = nullptr;
    juce::AudioParameterFloat* fmR_ = nullptr;
    juce::AudioParameterFloat* fmLevel_ = nullptr;

    juce::AudioParameterFloat*  drive_ = nullptr;
    juce::AudioParameterFloat*  hiss_ = nullptr;
    juce::AudioParameterChoice* cab_ = nullptr;
    juce::AudioParameterFloat*  cabMix_ = nullptr;

    juce::AudioParameterFloat* roomSize_ = nullptr;
    juce::AudioParameterFloat* roomDamp_ = nullptr;
    juce::AudioParameterFloat* roomMix_ = nullptr;

    juce::AudioParameterFloat* loCut_ = nullptr;
    juce::AudioParameterFloat* hiCut_ = nullptr;
    std::array<juce::AudioParameterFloat*, whm::numBells> bellF_ {};
    std::array<juce::AudioParameterFloat*, whm::numBells> bellG_ {};
    std::array<juce::AudioParameterFloat*, whm::numBells> bellQ_ {};

    juce::AudioParameterFloat* outLevel_ = nullptr;
    juce::AudioParameterBool*  limiterOn_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WhoompProcessor)
};
