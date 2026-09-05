#pragma once

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
    void timerCallback() override { repaint(); }

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

    void buildControls();
    void toggle (const Latch&);
    void select (const Radio&);

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

    StorProcessor& proc;
    std::vector<Knob>  knobs;
    std::vector<Latch> latches;
    std::vector<Radio> radios;

    int   dragIdx = -1;
    float dragStartNorm = 0.0f;
    juce::Point<float> dragStart;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StorEditor)
};
