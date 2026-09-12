#pragma once

#include <cstdint>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include "Panel.h"
#include "PluginProcessor.h"

/**
 * The panel: every control on one surface. Drawing and hit testing both work in
 * the fixed design space of Panel.h, with a single scale transform applied on
 * the way out, so the layout constants are the only source of truth.
 */
class StorEditor final : public juce::AudioProcessorEditor,
                           private juce::Timer
{
public:
    explicit StorEditor (StorProcessor&);
    ~StorEditor() override;

    void paint (juce::Graphics&) override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    void timerCallback() override
    {
        const auto seq = proc.random.rollSeq();
        if (seq != lastRollSeq) { lastRollSeq = seq; rollFlashTicks = 4; }
        else if (rollFlashTicks > 0) --rollFlashTicks;

        repaint();
    }

    /** A continuous control. */
    struct Knob
    {
        juce::RangedAudioParameter* param = nullptr;
        juce::Rectangle<float> hit;      // design space
        float travel = 100.0f;           // pixels of drag for a full sweep
    };

    /** A two-state control. Only the limiter, so far. */
    struct Latch
    {
        juce::RangedAudioParameter* param = nullptr;
        juce::Rectangle<float> hit;
    };

    /** One position of a multi-way choice — a wave, or a cabinet. Clicking it
        selects it; the group is the parameter, not the button. */
    struct Radio
    {
        juce::AudioParameterChoice* param = nullptr;
        int index = 0;
        juce::Rectangle<float> hit;
    };

    /** The little box that arms a control for the randomiser. `knob` indexes
        the same knobs vector a Knob does — arming is a property of a
        control that already exists, not a control of its own. */
    struct Arm
    {
        int knob = 0;
        juce::Rectangle<float> hit;
    };

    void buildControls();
    void toggle (const Latch&);
    void select (const Radio&);
    void setRandomSync (bool sync);

    float scale() const;
    juce::Point<float> toDesign (juce::Point<float> px) const;
    int knobAt (juce::Point<float> design) const;

    float norm (int i) const { return knobs[(std::size_t) i].param->getValue(); }
    float value (int i) const;
    juce::String readout (int i) const;

    /** The frequency the next hit settles at, given the note the panel last
        saw. Written out because tune plus a key offset is two numbers and one
        of them is the one you want. */
    juce::String noteReadout() const;

    void paintPitch (juce::Graphics&);
    void paintSubtractive (juce::Graphics&);
    void paintFm (juce::Graphics&);
    void paintMod (juce::Graphics&);
    void paintChain (juce::Graphics&);
    void paintRandom (juce::Graphics&);

    StorProcessor& proc;
    std::vector<Knob>  knobs;
    std::vector<Latch> latches;
    std::vector<Radio> radios;
    std::vector<Arm>   arms;

    int   dragIdx = -1;
    float dragStartNorm = 0.0f;
    juce::Point<float> dragStart;

    /* The roll lamp flashes off proc.random's sequence number rather than a
       timer of its own, so it lights up on a synced tick as reliably as on a
       MIDI hit. */
    std::uint32_t lastRollSeq = 0;
    int rollFlashTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StorEditor)
};
