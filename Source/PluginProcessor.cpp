#include "PluginProcessor.h"

#include "PluginEditor.h"

using namespace whm;

namespace
{
template <typename T>
T* fetch (juce::AudioProcessorValueTreeState& s, const juce::String& id)
{
    auto* p = dynamic_cast<T*> (s.getParameter (id));
    jassert (p != nullptr);
    return p;
}
} // namespace

WhoompProcessor::WhoompProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "state", createLayout())
{
    tune_ = fetch<juce::AudioParameterFloat> (apvts, pid::tune);
    bend_ = fetch<juce::AudioParameterFloat> (apvts, pid::bend);
    fall_ = fetch<juce::AudioParameterFloat> (apvts, pid::fall);
    vel_  = fetch<juce::AudioParameterFloat> (apvts, pid::vel);

    const juce::String* lvlIds[4] = { &pid::lvlSine, &pid::lvlTri, &pid::lvlSaw, &pid::lvlFold };
    for (int i = 0; i < 4; ++i)
        lvl_[(std::size_t) i] = fetch<juce::AudioParameterFloat> (apvts, *lvlIds[i]);

    fold_     = fetch<juce::AudioParameterFloat> (apvts, pid::fold);
    foldEnv_  = fetch<juce::AudioParameterFloat> (apvts, pid::foldEnv);
    cutoff_   = fetch<juce::AudioParameterFloat> (apvts, pid::cutoff);
    reso_     = fetch<juce::AudioParameterFloat> (apvts, pid::reso);
    filtEnv_  = fetch<juce::AudioParameterFloat> (apvts, pid::filtEnv);
    subA_     = fetch<juce::AudioParameterFloat> (apvts, pid::subA);
    subD_     = fetch<juce::AudioParameterFloat> (apvts, pid::subD);
    subS_     = fetch<juce::AudioParameterFloat> (apvts, pid::subS);
    subR_     = fetch<juce::AudioParameterFloat> (apvts, pid::subR);
    subLevel_ = fetch<juce::AudioParameterFloat> (apvts, pid::subLevel);

    for (int o = 0; o < numOps; ++o)
    {
        opWave_[(std::size_t) o]  = fetch<juce::AudioParameterChoice> (apvts, pid::opWave[o]);
        opRatio_[(std::size_t) o] = fetch<juce::AudioParameterFloat> (apvts, pid::opRatio[o]);
    }
    index_    = fetch<juce::AudioParameterFloat> (apvts, pid::index);
    indexEnv_ = fetch<juce::AudioParameterFloat> (apvts, pid::indexEnv);
    xfm_      = fetch<juce::AudioParameterFloat> (apvts, pid::xfm);
    fmA_      = fetch<juce::AudioParameterFloat> (apvts, pid::fmA);
    fmD_      = fetch<juce::AudioParameterFloat> (apvts, pid::fmD);
    fmS_      = fetch<juce::AudioParameterFloat> (apvts, pid::fmS);
    fmR_      = fetch<juce::AudioParameterFloat> (apvts, pid::fmR);
    fmLevel_  = fetch<juce::AudioParameterFloat> (apvts, pid::fmLevel);

    modA_ = fetch<juce::AudioParameterFloat> (apvts, pid::modA);
    modD_ = fetch<juce::AudioParameterFloat> (apvts, pid::modD);
    modS_ = fetch<juce::AudioParameterFloat> (apvts, pid::modS);
    modR_ = fetch<juce::AudioParameterFloat> (apvts, pid::modR);

    drive_  = fetch<juce::AudioParameterFloat> (apvts, pid::drive);
    hiss_   = fetch<juce::AudioParameterFloat> (apvts, pid::hiss);
    cab_    = fetch<juce::AudioParameterChoice> (apvts, pid::cab);
    cabMix_ = fetch<juce::AudioParameterFloat> (apvts, pid::cabMix);

    roomSize_ = fetch<juce::AudioParameterFloat> (apvts, pid::roomSize);
    roomDamp_ = fetch<juce::AudioParameterFloat> (apvts, pid::roomDamp);
    roomMix_  = fetch<juce::AudioParameterFloat> (apvts, pid::roomMix);

    loCut_ = fetch<juce::AudioParameterFloat> (apvts, pid::loCut);
    hiCut_ = fetch<juce::AudioParameterFloat> (apvts, pid::hiCut);
    for (int b = 0; b < numBells; ++b)
    {
        bellF_[(std::size_t) b] = fetch<juce::AudioParameterFloat> (apvts, pid::bellFreq[b]);
        bellG_[(std::size_t) b] = fetch<juce::AudioParameterFloat> (apvts, pid::bellGain[b]);
        bellQ_[(std::size_t) b] = fetch<juce::AudioParameterFloat> (apvts, pid::bellQ[b]);
    }

    outLevel_  = fetch<juce::AudioParameterFloat> (apvts, pid::outLevel);
    limiterOn_ = fetch<juce::AudioParameterBool> (apvts, pid::limiter);
}

bool WhoompProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void WhoompProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine_.prepare (sampleRate, samplesPerBlock);
    limiter_.prepare (sampleRate);

    envCoeff_ = (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0, sampleRate) * 0.060)));
    outEnv_ = 0.0f;

    monoScratch_.setSize (1, juce::jmax (1, samplesPerBlock), false, true, true);

    setLatencySamples (whm::WhoompEngine::latencySamples());
}

whm::EngineParams WhoompProcessor::gather() const
{
    EngineParams p;

    p.tuneHz    = tune_->get();
    p.bendSemis = bend_->get();
    p.fallMs    = fall_->get();
    p.velAmount = vel_->get();

    for (int i = 0; i < 4; ++i)
        p.lvl[(std::size_t) i] = lvl_[(std::size_t) i]->get();

    p.fold        = fold_->get();
    p.foldEnv     = foldEnv_->get();
    p.cutoffHz    = cutoff_->get();
    p.resoQ       = resonanceQ (reso_->get());
    p.filtOctaves = filterOctaves (filtEnv_->get());
    p.subA = subA_->get(); p.subD = subD_->get();
    p.subS = subS_->get(); p.subR = subR_->get();
    p.subGain = levelGain (subLevel_->get());

    for (int o = 0; o < numOps; ++o)
    {
        p.wave[(std::size_t) o]  = (OpWave) opWave_[(std::size_t) o]->getIndex();
        p.ratio[(std::size_t) o] = opRatio_[(std::size_t) o]->get();
    }
    p.index    = index_->get();
    p.indexEnv = indexEnv_->get();
    p.xfm      = xfm_->get();
    p.fmA = fmA_->get(); p.fmD = fmD_->get();
    p.fmS = fmS_->get(); p.fmR = fmR_->get();
    p.fmGain = levelGain (fmLevel_->get());

    p.modA = modA_->get(); p.modD = modD_->get();
    p.modS = modS_->get(); p.modR = modR_->get();

    p.drive  = drive_->get();
    p.hiss   = hiss_->get();
    p.cab    = (Cab) cab_->getIndex();
    p.cabMix = cabMix_->get();

    p.roomSize = roomSize_->get();
    p.roomDamp = roomDamp_->get();
    p.roomMix  = roomMix_->get();

    p.loCutHz = loCut_->get();
    p.hiCutHz = hiCut_->get();
    for (int b = 0; b < numBells; ++b)
    {
        p.bellF[(std::size_t) b] = bellF_[(std::size_t) b]->get();
        p.bellG[(std::size_t) b] = bellG_[(std::size_t) b]->get();
        p.bellQ[(std::size_t) b] = bellQ_[(std::size_t) b]->get();
    }

    p.outGain = juce::Decibels::decibelsToGain (outLevel_->get());

    return p;
}

void WhoompProcessor::renderSegment (juce::AudioBuffer<float>& buffer, int start, int count)
{
    if (count <= 0)
        return;

    float* l = buffer.getWritePointer (0, start);

    if (buffer.getNumChannels() > 1)
    {
        engine_.process (l, buffer.getWritePointer (1, start), count);
    }
    else
    {
        /* Mono out: the room still runs in stereo internally — folding it down
           here keeps both sides' early reflections rather than throwing one of
           them away. */
        if (monoScratch_.getNumSamples() < count)
            monoScratch_.setSize (1, count, false, true, true);

        float* scratch = monoScratch_.getWritePointer (0);
        engine_.process (l, scratch, count);

        for (int i = 0; i < count; ++i)
            l[i] = 0.5f * (l[i] + scratch[i]);
    }
}

void WhoompProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    engine_.setParams (gather());

    /* Sub-blocks between MIDI events, so a note lands on the sample the host
       put it on. At a 512-sample block, whole-block granularity would move
       hits by up to 10 ms, and this is a drum. */
    int pos = 0;
    for (const auto meta : midi)
    {
        const int at = juce::jlimit (0, numSamples, meta.samplePosition);
        renderSegment (buffer, pos, at - pos);
        pos = at;

        const auto m = meta.getMessage();
        if (m.isNoteOn())
        {
            engine_.noteOn (m.getNoteNumber(), m.getFloatVelocity());
            panel.lastNote.store (m.getNoteNumber(), std::memory_order_relaxed);
        }
        else if (m.isNoteOff())
        {
            engine_.noteOff (m.getNoteNumber());
        }
        else if (m.isAllNotesOff() || m.isAllSoundOff())
        {
            engine_.allNotesOff();
        }
    }
    renderSegment (buffer, pos, numSamples - pos);

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : l;

    const bool limiting = limiterOn_->get();
    if (limiting)
        limiter_.process (l, r, numSamples);

    panel.limitGr.store (limiting ? 1.0f - limiter_.readReduction() : 0.0f,
                         std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        const float a = std::fabs (l[i]);
        outEnv_ += (a - outEnv_) * (a > outEnv_ ? 0.4f : envCoeff_);
    }

    panel.outLevel.store (juce::jlimit (0.0f, 1.0f, outEnv_), std::memory_order_relaxed);
    panel.voiceEnv.store (engine_.voiceEnv(), std::memory_order_relaxed);
    panel.pitchEnv.store (engine_.pitchEnv(), std::memory_order_relaxed);
    panel.sounding.store (engine_.sounding(), std::memory_order_relaxed);
}

juce::AudioProcessorEditor* WhoompProcessor::createEditor()
{
    return new WhoompEditor (*this);
}

void WhoompProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void WhoompProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new WhoompProcessor();
}
