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

void set (StorProcessor& p, const juce::String& id, float value)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (param->convertTo0to1 (value));
}

void setNorm (StorProcessor& p, const juce::String& id, float norm)
{
    if (auto* param = p.apvts.getParameter (id))
        param->setValueNotifyingHost (norm);
}

/** One note, three seconds, and what came back. */
Result render (StorProcessor& proc)
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
        if (b == 0) midi.addEvent (juce::MidiMessage::noteOn (1, str::kReferenceNote, 1.0f), 0);
        if (b == 4) midi.addEvent (juce::MidiMessage::noteOff (1, str::kReferenceNote), 0);

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

using Setup = void (*) (StorProcessor&);

void plainSine (StorProcessor&) {}

void allShapes (StorProcessor& p)
{
    setNorm (p, str::pid::lvlTri, 1.0f);
    setNorm (p, str::pid::lvlSaw, 1.0f);
    setNorm (p, str::pid::lvlFold, 1.0f);
    setNorm (p, str::pid::fold, 1.0f);
}

void foldSwept (StorProcessor& p)
{
    setNorm (p, str::pid::lvlSine, 0.0f);
    setNorm (p, str::pid::lvlFold, 1.0f);
    setNorm (p, str::pid::fold, 0.2f);
    set     (p, str::pid::foldEnv, 0.8f);
}

void filterSinging (StorProcessor& p)
{
    setNorm (p, str::pid::lvlSaw, 1.0f);
    set     (p, str::pid::cutoff, 120.0f);
    setNorm (p, str::pid::reso, 1.0f);
    set     (p, str::pid::filtEnv, 1.0f);
}

void fmOnly (StorProcessor& p)
{
    set (p, str::pid::subLevel, str::kLevelOffDb);
    set (p, str::pid::fmLevel, 0.0f);
    set (p, str::pid::index, 12.0f);
    set (p, str::pid::opRatio[1], 16.0f);
}

/* The loop closed as hard as it goes. Phase modulation is bounded whatever is
   put into it, so this should get bright and inharmonic rather than run away —
   and this is the case that says so. */
void fmCrossMax (StorProcessor& p)
{
    fmOnly (p);
    set (p, str::pid::xfm, 8.0f);
}

void fmNoiseModulator (StorProcessor& p)
{
    fmOnly (p);
    setNorm (p, str::pid::opWave[1], 1.0f);   // the last of three: noise
}

void fmNoiseCarrier (StorProcessor& p)
{
    fmOnly (p);
    setNorm (p, str::pid::opWave[0], 1.0f);
}

void tapeCranked (StorProcessor& p)
{
    setNorm (p, str::pid::drive, 1.0f);
    setNorm (p, str::pid::hiss, 1.0f);
}

void tapeCrankedSilent (StorProcessor& p)
{
    tapeCranked (p);
    set (p, str::pid::subLevel, str::kLevelOffDb);
}

void cabAndRoom (StorProcessor& p)
{
    setNorm (p, str::pid::cabMix, 1.0f);
    setNorm (p, str::pid::roomMix, 1.0f);
    setNorm (p, str::pid::roomSize, 1.0f);
    setNorm (p, str::pid::roomDamp, 0.0f);
}

void eqExtremes (StorProcessor& p)
{
    set (p, str::pid::loCut, 600.0f);
    set (p, str::pid::hiCut, 600.0f);
    for (int b = 0; b < str::numBells; ++b)
    {
        set (p, str::pid::bellGain[b], 18.0f);
        set (p, str::pid::bellQ[b], 12.0f);
    }
}

void everythingAtOnce (StorProcessor& p)
{
    allShapes (p);
    filterSinging (p);
    fmOnly (p);
    set (p, str::pid::subLevel, 12.0f);
    set (p, str::pid::fmLevel, 12.0f);
    tapeCranked (p);
    cabAndRoom (p);
    eqExtremes (p);
    set (p, str::pid::outLevel, 12.0f);
}

void limited (StorProcessor& p)
{
    everythingAtOnce (p);
    setNorm (p, str::pid::limiter, 1.0f);
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

    std::printf ("Stor — %d configurations, one note each, %d s at %.0f Hz\n\n",
                 (int) (sizeof (kCases) / sizeof (kCases[0])), kSeconds, kSampleRate);

    bool allFinite = true;

    for (const auto& c : kCases)
    {
        StorProcessor proc;
        proc.setRateAndBufferSizeDetails (kSampleRate, kBlock);
        proc.prepareToPlay (kSampleRate, kBlock);
        c.setup (proc);

        const auto r = render (proc);
        report (c.name, r);
        allFinite = allFinite && r.finite;
    }

    std::printf ("\nreported latency: %d samples\n", str::StorEngine::latencySamples());
    std::printf ("%s\n", allFinite ? "all finite" : "SOMETHING WENT NON-FINITE");

    return allFinite ? 0 : 1;
}
