#include "PluginProcessor.h"

#include "PluginEditor.h"

using namespace str;

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

StorProcessor::StorProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "state", createLayout())
{
    for (int i = 0; i < kNumKnobs; ++i)
    {
        knobParams_[(std::size_t) i] = apvts.getParameter (*knobIds[i]);
        jassert (knobParams_[(std::size_t) i] != nullptr);
    }

    for (int o = 0; o < numOps; ++o)
        opWave_[(std::size_t) o] = fetch<juce::AudioParameterChoice> (apvts, pid::opWave[o]);

    cab_       = fetch<juce::AudioParameterChoice> (apvts, pid::cab);
    limiterOn_ = fetch<juce::AudioParameterBool> (apvts, pid::limiter);

    rndStrength_ = fetch<juce::AudioParameterFloat> (apvts, pid::rndStrength);
    rndSync_     = fetch<juce::AudioParameterBool> (apvts, pid::rndSync);
    rndRate_     = fetch<juce::AudioParameterChoice> (apvts, pid::rndRate);
}

bool StorProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (! layouts.getMainInputChannelSet().isDisabled())
        return false;

    const auto out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo();
}

void StorProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine_.prepare (sampleRate, samplesPerBlock);
    limiter_.prepare (sampleRate);

    envCoeff_ = (float) (1.0 - std::exp (-1.0 / (juce::jmax (1.0, sampleRate) * 0.060)));
    outEnv_ = 0.0f;

    monoScratch_.setSize (1, juce::jmax (1, samplesPerBlock), false, true, true);

    setLatencySamples (str::StorEngine::latencySamples());
}

float StorProcessor::randomised (int knobIndex) const
{
    auto* pr = knobParams_[(std::size_t) knobIndex];
    const float n = juce::jlimit (0.0f, 1.0f, pr->getValue() + random.offsetFor (knobIndex));
    return pr->convertFrom0to1 (n);
}

str::EngineParams StorProcessor::gather() const
{
    EngineParams p;

    p.tuneHz    = randomised (kTune);
    p.bendSemis = randomised (kBend);
    p.fallMs    = randomised (kFall);
    p.velAmount = randomised (kVel);

    p.lvl[0] = randomised (kLvlSine);
    p.lvl[1] = randomised (kLvlTri);
    p.lvl[2] = randomised (kLvlSaw);
    p.lvl[3] = randomised (kLvlFold);

    p.fold        = randomised (kFold);
    p.foldEnv     = randomised (kFoldEnv);
    p.cutoffHz    = randomised (kCutoff);
    p.resoQ       = resonanceQ (randomised (kReso));
    p.filtOctaves = filterOctaves (randomised (kFiltEnv));
    p.subA = randomised (kSubA); p.subD = randomised (kSubD);
    p.subS = randomised (kSubS); p.subR = randomised (kSubR);
    p.subGain = levelGain (randomised (kSubLevel));

    for (int o = 0; o < numOps; ++o)
        p.wave[(std::size_t) o] = (OpWave) opWave_[(std::size_t) o]->getIndex();
    p.ratio[0] = randomised (kRatio1);
    p.ratio[1] = randomised (kRatio2);
    p.index    = randomised (kIndex);
    p.indexEnv = randomised (kIndexEnv);
    p.xfm      = randomised (kXfm);
    p.fmA = randomised (kFmA); p.fmD = randomised (kFmD);
    p.fmS = randomised (kFmS); p.fmR = randomised (kFmR);
    p.fmGain = levelGain (randomised (kFmLevel));

    p.modA = randomised (kModA); p.modD = randomised (kModD);
    p.modS = randomised (kModS); p.modR = randomised (kModR);

    p.drive  = randomised (kDrive);
    p.hiss   = randomised (kHiss);
    p.cab    = (Cab) cab_->getIndex();
    p.cabMix = randomised (kCabMix);

    p.roomSize = randomised (kRoomSize);
    p.roomDamp = randomised (kRoomDamp);
    p.roomMix  = randomised (kRoomMix);

    p.loCutHz = randomised (kLoCut);
    p.hiCutHz = randomised (kHiCut);
    for (int b = 0; b < numBells; ++b)
    {
        p.bellF[(std::size_t) b] = randomised (kB1F + 3 * b);
        p.bellG[(std::size_t) b] = randomised (kB1G + 3 * b);
        p.bellQ[(std::size_t) b] = randomised (kB1Q + 3 * b);
    }

    p.outGain = juce::Decibels::decibelsToGain (randomised (kOutLevel));

    return p;
}

void StorProcessor::renderSegment (juce::AudioBuffer<float>& buffer, int start, int count)
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

void StorProcessor::updateRandomSync (int numSamples)
{
    bool playing = false;
    double bpm = 120.0;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            playing = pos->getIsPlaying();
            bpm = pos->getBpm().orFallback (120.0);
        }

    if (! playing)
    {
        rndWasPlaying_ = false;
        return;
    }

    const int rateIdx = juce::jlimit (0, numRndRates - 1, rndRate_->getIndex());
    const double samplesPerTick = juce::jmax (1.0, (double) rndRateBeats[rateIdx]
                                                   * (60.0 / juce::jmax (1.0, bpm)) * getSampleRate());

    if (! rndWasPlaying_)
    {
        rndWasPlaying_ = true;
        rndTickAccum_ = samplesPerTick;   // fire once, right as playback starts
    }

    rndTickAccum_ += (double) numSamples;
    while (rndTickAccum_ >= samplesPerTick)
    {
        rndTickAccum_ -= samplesPerTick;
        random.roll (rndStrength_->get());
    }
}

void StorProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    const int numSamples = buffer.getNumSamples();
    buffer.clear();

    /* Rolled ahead of gather(), so a hit that lands in this block already
       renders with the offsets it triggered rather than the previous
       block's — the same reason params are only ever refreshed once per
       block regardless of how many notes fall in it. */
    if (rndSync_->get())
    {
        updateRandomSync (numSamples);
    }
    else
    {
        for (const auto meta : midi)
            if (meta.getMessage().isNoteOn())
            {
                random.roll (rndStrength_->get());
                break;
            }
    }

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

juce::AudioProcessorEditor* StorProcessor::createEditor()
{
    return new StorEditor (*this);
}

void StorProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr)
        return;

    /* Which controls the randomiser reaches is not a value a host needs to
       automate, so it is not one of apvts's parameters — it rides along in
       the same state block as a plain bit string instead. */
    juce::String bits;
    for (int i = 0; i < kNumKnobs; ++i)
        bits << (random.isArmed (i) ? '1' : '0');
    xml->createNewChildElement ("RANDOM_ARM")->setAttribute ("bits", bits);

    copyXmlToBinary (*xml, dest);
}

void StorProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    const auto* arm = xml->getChildByName ("RANDOM_ARM");
    const auto bits = arm != nullptr ? arm->getStringAttribute ("bits") : juce::String();

    for (int i = 0; i < kNumKnobs; ++i)
        random.setArmed (i, i < bits.length() && bits[i] == '1');
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StorProcessor();
}
