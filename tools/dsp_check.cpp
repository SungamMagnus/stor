// Runs the real processor through a set of configurations and prints what comes
// out: peak, RMS, how long the tail lasts, and whether anything went
// non-finite. Dev tool — this is what catches a filter that blows up, a stage
// that leaves DC behind, or a section that turns out to do nothing, without
// opening a host.
#include <cmath>
#include <cstdio>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlock = 128;
constexpr int    kSeconds = 3;

struct Result
{
    float peak = 0.0f, rms = 0.0f, tailMs = 0.0f, dc = 0.0f;
    bool  finite = true;
};

void set (WhoompProcessor& p, const juce::String& id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void setNorm (WhoompProcessor& p, const juce::String& id, float norm)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (norm);
}

/** One note, three seconds, and what came back. */
Result render (WhoompProcessor& proc)
{
    juce::AudioBuffer<float> buffer (2, kBlock);
    const int blocks = (int) (kSampleRate * kSeconds) / kBlock;

    Result r;
    double sumSq = 0.0, sum = 0.0;
    long   n = 0;
    int    lastLoud = 0;

    for (int b = 0; b < blocks; ++b)
    {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, whm::kReferenceNote, 1.0f), 0);
        if (b == 4) midi.addEvent (juce::MidiMessage::noteOff (1, whm::kReferenceNote), 0);

        proc.processBlock (buffer, midi);

        for (int i = 0; i < kBlock; ++i)
        {
            const float s = buffer.getSample (0, i);
            if (! std::isfinite (s)) r.finite = false;

            const float a = std::fabs (s);
            if (a > r.peak) r.peak = a;
            if (a > 0.001f) lastLoud = b * kBlock + i;

            sumSq += (double) s * s;
            sum   += (double) s;
            ++n;
        }
    }

    r.rms    = (float) std::sqrt (sumSq / (double) juce::jmax (1L, n));
    r.dc     = (float) (sum / (double) juce::jmax (1L, n));
    r.tailMs = (float) (lastLoud * 1000.0 / kSampleRate);
    return r;
}

void report (const char* name, const Result& r)
{
    std::printf ("%-26s peak %6.3f   rms %6.4f   tail %7.1f ms   dc %+8.5f   %s\n",
                 name, r.peak, r.rms, r.tailMs, r.dc, r.finite ? "ok" : "NOT FINITE");
}

using Setup = void (*) (WhoompProcessor&);

void plainSine (WhoompProcessor&) {}

void allShapes (WhoompProcessor& p)
{
    setNorm (p, whm::pid::lvlTri, 1.0f);
    setNorm (p, whm::pid::lvlSaw, 1.0f);
    setNorm (p, whm::pid::lvlFold, 1.0f);
    setNorm (p, whm::pid::fold, 1.0f);
}

void foldSwept (WhoompProcessor& p)
{
    setNorm (p, whm::pid::lvlSine, 0.0f);
    setNorm (p, whm::pid::lvlFold, 1.0f);
    setNorm (p, whm::pid::fold, 0.2f);
    set     (p, whm::pid::foldEnv, 0.8f);
}

void filterSinging (WhoompProcessor& p)
{
    setNorm (p, whm::pid::lvlSaw, 1.0f);
    set     (p, whm::pid::cutoff, 120.0f);
    setNorm (p, whm::pid::reso, 1.0f);
    set     (p, whm::pid::filtEnv, 1.0f);
}

void fmOnly (WhoompProcessor& p)
{
    set (p, whm::pid::subLevel, whm::kLevelOffDb);
    set (p, whm::pid::fmLevel, 0.0f);
    set (p, whm::pid::index, 12.0f);
    set (p, whm::pid::opRatio[1], 16.0f);
}

/* The loop closed as hard as it goes. Phase modulation is bounded whatever is
   put into it, so this should get bright and inharmonic rather than run away —
   and this is the case that says so. */
void fmCrossMax (WhoompProcessor& p)
{
    fmOnly (p);
    set (p, whm::pid::xfm, 8.0f);
}

void fmNoiseModulator (WhoompProcessor& p)
{
    fmOnly (p);
    setNorm (p, whm::pid::opWave[1], 1.0f);   // the last of three: noise
}

void fmNoiseCarrier (WhoompProcessor& p)
{
    fmOnly (p);
    setNorm (p, whm::pid::opWave[0], 1.0f);
}

void tapeCranked (WhoompProcessor& p)
{
    setNorm (p, whm::pid::drive, 1.0f);
    setNorm (p, whm::pid::hiss, 1.0f);
}

void tapeCrankedSilent (WhoompProcessor& p)
{
    tapeCranked (p);
    set (p, whm::pid::subLevel, whm::kLevelOffDb);
}

void cabAndRoom (WhoompProcessor& p)
{
    setNorm (p, whm::pid::cabMix, 1.0f);
    setNorm (p, whm::pid::roomMix, 1.0f);
    setNorm (p, whm::pid::roomSize, 1.0f);
    setNorm (p, whm::pid::roomDamp, 0.0f);
}

void eqExtremes (WhoompProcessor& p)
{
    set (p, whm::pid::loCut, 600.0f);
    set (p, whm::pid::hiCut, 600.0f);
    for (int b = 0; b < whm::numBells; ++b)
    {
        set (p, whm::pid::bellGain[b], 18.0f);
        set (p, whm::pid::bellQ[b], 12.0f);
    }
}

void everythingAtOnce (WhoompProcessor& p)
{
    allShapes (p);
    filterSinging (p);
    fmOnly (p);
    set (p, whm::pid::subLevel, 12.0f);
    set (p, whm::pid::fmLevel, 12.0f);
    tapeCranked (p);
    cabAndRoom (p);
    eqExtremes (p);
    set (p, whm::pid::outLevel, 12.0f);
}

void limited (WhoompProcessor& p)
{
    everythingAtOnce (p);
    setNorm (p, whm::pid::limiter, 1.0f);
}

const struct { const char* name; Setup setup; } kCases[] = {
    { "default (sine only)",     plainSine },
    { "all four shapes",         allShapes },
    { "fold swept by env",       foldSwept },
    { "filter at full reso",     filterSinging },
    { "FM only, index 12",       fmOnly },
    { "FM cross-fed, max",       fmCrossMax },
    { "FM noise modulator",      fmNoiseModulator },
    { "FM noise carrier",        fmNoiseCarrier },
    { "tape cranked",            tapeCranked },
    { "tape cranked, no voice",  tapeCrankedSilent },
    { "cab + biggest room",      cabAndRoom },
    { "EQ at the extremes",      eqExtremes },
    { "everything at once",      everythingAtOnce },
    { "  and limited",           limited },
};
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI init;

    std::printf ("Whoomp — %d configurations, one note each, %d s at %.0f Hz\n\n",
                 (int) (sizeof (kCases) / sizeof (kCases[0])), kSeconds, kSampleRate);

    bool allFinite = true;

    for (const auto& c : kCases)
    {
        WhoompProcessor proc;
        proc.setRateAndBufferSizeDetails (kSampleRate, kBlock);
        proc.prepareToPlay (kSampleRate, kBlock);
        c.setup (proc);

        const auto r = render (proc);
        report (c.name, r);
        allFinite = allFinite && r.finite;
    }

    std::printf ("\nreported latency: %d samples\n", whm::WhoompEngine::latencySamples());
    std::printf ("%s\n", allFinite ? "all finite" : "SOMETHING WENT NON-FINITE");

    return allFinite ? 0 : 1;
}
