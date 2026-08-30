// Offscreen render of the real editor. Dev tool: verifies the panel without a
// host. Writes panel.png into the directory given as argv[1].
#include <juce_gui_basics/juce_gui_basics.h>

#include "Panel.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"

namespace
{
void setNorm (WhoompProcessor& p, const juce::String& id, float norm)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (norm);
}

void setValue (WhoompProcessor& p, const juce::String& id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}
} // namespace

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;

    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("."));

    WhoompProcessor proc;
    proc.setRateAndBufferSizeDetails (48000.0, 128);
    proc.prepareToPlay (48000.0, 128);

    /* A working patch rather than the defaults: every kind of control on the
       panel has something to show, and every section is doing something. */
    setValue (proc, whm::pid::tune, 47.0f);
    setValue (proc, whm::pid::bend, 34.0f);
    setValue (proc, whm::pid::fall, 31.0f);
    setNorm  (proc, whm::pid::vel, 0.72f);

    setNorm  (proc, whm::pid::lvlSine, 1.0f);
    setNorm  (proc, whm::pid::lvlTri, 0.34f);
    setNorm  (proc, whm::pid::lvlSaw, 0.12f);
    setNorm  (proc, whm::pid::lvlFold, 0.46f);
    setNorm  (proc, whm::pid::fold, 0.42f);
    setValue (proc, whm::pid::foldEnv, 0.55f);
    setValue (proc, whm::pid::cutoff, 2600.0f);
    setNorm  (proc, whm::pid::reso, 0.38f);
    setValue (proc, whm::pid::filtEnv, 0.44f);
    setValue (proc, whm::pid::subD, 410.0f);
    setValue (proc, whm::pid::subR, 180.0f);
    setValue (proc, whm::pid::subLevel, -1.5f);

    setNorm  (proc, whm::pid::opWave[0], 0.0f);            // carrier: sine
    setNorm  (proc, whm::pid::opWave[1], 0.5f);            // modulator: triangle
    setValue (proc, whm::pid::opRatio[0], 1.0f);
    setValue (proc, whm::pid::opRatio[1], 3.5f);
    setValue (proc, whm::pid::index, 4.2f);
    setNorm  (proc, whm::pid::indexEnv, 0.85f);
    setValue (proc, whm::pid::modA, 1.5f);
    setValue (proc, whm::pid::modD, 260.0f);
    setNorm  (proc, whm::pid::modS, 0.15f);
    setValue (proc, whm::pid::modR, 90.0f);
    setValue (proc, whm::pid::xfm, 2.6f);
    setValue (proc, whm::pid::fmD, 62.0f);
    setValue (proc, whm::pid::fmLevel, -7.0f);

    setNorm  (proc, whm::pid::drive, 0.66f);
    setNorm  (proc, whm::pid::hiss, 0.58f);
    setNorm  (proc, whm::pid::cab, 0.5f);                  // the 15"
    setNorm  (proc, whm::pid::cabMix, 0.72f);
    setNorm  (proc, whm::pid::roomSize, 0.30f);
    setNorm  (proc, whm::pid::roomDamp, 0.62f);
    setNorm  (proc, whm::pid::roomMix, 0.22f);

    setValue (proc, whm::pid::loCut, 34.0f);
    setValue (proc, whm::pid::bellFreq[0], 58.0f);
    setValue (proc, whm::pid::bellGain[0], 4.5f);
    setValue (proc, whm::pid::bellQ[0], 1.4f);
    setValue (proc, whm::pid::bellFreq[1], 380.0f);
    setValue (proc, whm::pid::bellGain[1], -5.0f);
    setValue (proc, whm::pid::bellQ[1], 2.2f);
    setValue (proc, whm::pid::bellFreq[2], 3400.0f);
    setValue (proc, whm::pid::bellGain[2], 3.0f);
    setValue (proc, whm::pid::bellQ[2], 0.8f);
    setValue (proc, whm::pid::hiCut, 9000.0f);

    setValue (proc, whm::pid::outLevel, 1.5f);
    setNorm  (proc, whm::pid::limiter, 1.0f);

    /* Play one, and stop partway down the decay so the meters, the lamps and
       the pitch envelope all read something honest. */
    juce::AudioBuffer<float> buffer (2, 128);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (1, whm::kReferenceNote, 0.95f), 4);

    for (int block = 0; block < 3; ++block)
    {
        proc.processBlock (buffer, midi);
        midi.clear();
    }

    std::unique_ptr<juce::AudioProcessorEditor> editor (proc.createEditor());
    editor->setSize ((int) whm::panel::designW, (int) whm::panel::designH);

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
