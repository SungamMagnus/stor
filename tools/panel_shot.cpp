// Offscreen render of the real editor. Dev tool: verifies the panel without a
// host. Writes panel.png into the directory given as argv[1].
#include <juce_gui_basics/juce_gui_basics.h>

#include "Panel.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
void setNorm (StorProcessor& p, const juce::String& id, float norm)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (norm);
}

void setValue (StorProcessor& p, const juce::String& id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("."));

    StorProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 128);
    proc.prepareToPlay (48000.0, 128);

    /* A working patch rather than the defaults: every kind of control on the
       panel has something to show, and every section is doing something. */
    setValue (proc, str::pid::tune, 47.0f);
    setValue (proc, str::pid::bend, 34.0f);
    setValue (proc, str::pid::fall, 31.0f);
    setNorm  (proc, str::pid::vel, 0.72f);

    setNorm  (proc, str::pid::lvlSine, 1.0f);
    setNorm  (proc, str::pid::lvlTri, 0.34f);
    setNorm  (proc, str::pid::lvlSaw, 0.12f);
    setNorm  (proc, str::pid::lvlFold, 0.46f);
    setNorm  (proc, str::pid::fold, 0.42f);
    setValue (proc, str::pid::foldEnv, 0.55f);
    setValue (proc, str::pid::cutoff, 2600.0f);
    setNorm  (proc, str::pid::reso, 0.38f);
    setValue (proc, str::pid::filtEnv, 0.44f);
    setValue (proc, str::pid::subD, 410.0f);
    setValue (proc, str::pid::subR, 180.0f);
    setValue (proc, str::pid::subLevel, -1.5f);

    setNorm  (proc, str::pid::opWave[0], 0.0f);            // carrier: sine
    setNorm  (proc, str::pid::opWave[1], 0.5f);            // modulator: triangle
    setValue (proc, str::pid::opRatio[0], 1.0f);
    setValue (proc, str::pid::opRatio[1], 3.5f);
    setValue (proc, str::pid::index, 4.2f);
    setNorm  (proc, str::pid::indexEnv, 0.85f);
    setValue (proc, str::pid::modA, 1.5f);
    setValue (proc, str::pid::modD, 260.0f);
    setNorm  (proc, str::pid::modS, 0.15f);
    setValue (proc, str::pid::modR, 90.0f);
    setValue (proc, str::pid::xfm, 2.6f);
    setValue (proc, str::pid::fmD, 62.0f);
    setValue (proc, str::pid::fmLevel, -7.0f);

    setNorm  (proc, str::pid::drive, 0.66f);
    setNorm  (proc, str::pid::hiss, 0.58f);
    setNorm  (proc, str::pid::cab, 0.5f);                  // the 15"
    setNorm  (proc, str::pid::cabMix, 0.72f);
    setNorm  (proc, str::pid::roomSize, 0.30f);
    setNorm  (proc, str::pid::roomDamp, 0.62f);
    setNorm  (proc, str::pid::roomMix, 0.22f);

    setValue (proc, str::pid::loCut, 34.0f);
    setValue (proc, str::pid::bellFreq[0], 58.0f);
    setValue (proc, str::pid::bellGain[0], 4.5f);
    setValue (proc, str::pid::bellQ[0], 1.4f);
    setValue (proc, str::pid::bellFreq[1], 380.0f);
    setValue (proc, str::pid::bellGain[1], -5.0f);
    setValue (proc, str::pid::bellQ[1], 2.2f);
    setValue (proc, str::pid::bellFreq[2], 3400.0f);
    setValue (proc, str::pid::bellGain[2], 3.0f);
    setValue (proc, str::pid::bellQ[2], 0.8f);
    setValue (proc, str::pid::hiCut, 9000.0f);

    setValue (proc, str::pid::outLevel, 1.5f);
    setNorm  (proc, str::pid::limiter, 1.0f);

    /* Play one, and stop partway down the decay so the meters, the lamps and
       the pitch envelope all read something honest. */
    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, str::kReferenceNote, 0.95f), 4);

    for (int block = 0; block < 3; ++block)
    {
        proc.processBlock (buffer, midi);
        midi.clear();
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize ((int) str::panel::designW, (int) str::panel::designH);

    juce::Image img (juce::Image::ARGB, editor->getWidth() * 2, editor->getHeight() * 2, true);
    juce::Graphics g (img);
    g.addTransform (juce::AffineTransform::scale (2.0f));
    editor->paintEntireComponent (g, false);

    const auto out = outDir.getChildFile ("panel.png");
    juce::FileOutputStream stream (out);
    stream.setPosition (0);
    stream.truncate();
    juce::PNGImageFormat().writeImageToStream (img, stream);
    std::printf ("%s\n", out.getFullPathName().toRawUTF8());

    return 0;
}
