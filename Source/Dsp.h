#pragma once

#include <array>
#include <cmath>
#include <cstdint>

/**
 * The small pieces everything else is built from: envelopes, band-limited
 * oscillators, a wavefolder, one biquad, one state-variable filter, and the
 * halfband decimator that lets the voice run at twice the host rate.
 *
 * Nothing here knows about JUCE or about parameters — it takes numbers in
 * engineering units and returns samples.
 */
namespace str
{

constexpr double kPi    = 3.14159265358979323846;
constexpr double kTwoPi = 6.28318530717958647692;

/* ── Noise ───────────────────────────────────────────────────────────────
 * A 32-bit xorshift. Cheap, no allocation, and the same sequence every run,
 * which is what makes the offline renders in tools/ comparable. */
class Noise
{
public:
    explicit Noise (std::uint32_t seed = 0x1234567u) : s_ (seed | 1u) {}

    void reset (std::uint32_t seed) { s_ = seed | 1u; }

    float next()
    {
        s_ ^= s_ << 13; s_ ^= s_ >> 17; s_ ^= s_ << 5;
        return (float) ((std::int32_t) s_) * 4.656613e-10f;   // -1 .. 1
    }

private:
    std::uint32_t s_;
};

/* ── Envelopes ───────────────────────────────────────────────────────────
 * Exponential segments, not linear ones. A kick is all decay, and a linear
 * decay to silence does not sound like one — the ear wants the fast first
 * third that a one-pole running at a target it never quite reaches gives.
 *
 * Each stage runs a one-pole towards a target *past* where it stops, so the
 * curve through the useful part is the steep part and the stage still ends
 * in finite time.
 */
class Adsr
{
public:
    enum class Stage { idle, attack, decay, sustain, release };

    void prepare (double sampleRate)
    {
        sr_ = sampleRate > 0.0 ? sampleRate : 48000.0;
        set (attackMs_, decayMs_, sustain_, releaseMs_);
        reset();
    }

    void reset() { stage_ = Stage::idle; value_ = 0.0f; }

    void set (float attackMs, float decayMs, float sustain, float releaseMs)
    {
        attackMs_ = attackMs; decayMs_ = decayMs;
        sustain_  = sustain;  releaseMs_ = releaseMs;

        aCoeff_ = coeff (attackMs);
        dCoeff_ = coeff (decayMs);
        rCoeff_ = coeff (releaseMs);
    }

    void noteOn() { stage_ = Stage::attack; }

    /**
     * With no sustain there is nothing for a release to release: the decay is
     * already on its way to zero, and cutting it short would mean the length
     * of the drum came from how long the key was held rather than from the
     * decay knob. So an envelope with S at zero is a one-shot, and lifting S
     * off zero is what turns the R back on.
     */
    void noteOff()
    {
        if (stage_ != Stage::idle && sustain_ > 1.0e-4f)
            stage_ = Stage::release;
    }

    bool  active() const { return stage_ != Stage::idle; }
    float value() const  { return value_; }

    float tick()
    {
        switch (stage_)
        {
            case Stage::idle:
                return 0.0f;

            case Stage::attack:
                /* Aiming past 1 is what makes the attack curve convex rather
                   than the concave shape a one-pole settling *at* 1 gives. */
                value_ += (kAttackTarget - value_) * aCoeff_;
                if (value_ >= 1.0f) { value_ = 1.0f; stage_ = Stage::decay; }
                break;

            case Stage::decay:
                value_ += (sustain_ - kUnderShoot - value_) * dCoeff_;
                if (value_ <= sustain_ + 1.0e-4f)
                {
                    value_ = sustain_;
                    stage_ = sustain_ > 1.0e-4f ? Stage::sustain : Stage::idle;
                }
                break;

            case Stage::sustain:
                value_ = sustain_;
                break;

            case Stage::release:
                value_ += (-kUnderShoot - value_) * rCoeff_;
                if (value_ <= 1.0e-4f) { value_ = 0.0f; stage_ = Stage::idle; }
                break;
        }

        return value_;
    }

private:
    /* Three time constants to cross the segment: the stated time is where the
       stage is done, not where it is 63% done. */
    float coeff (float ms) const
    {
        const double tau = (double) (ms > 0.05f ? ms : 0.05f) * 0.001 / 3.0;
        return (float) (1.0 - std::exp (-1.0 / (tau * sr_)));
    }

    static constexpr float kAttackTarget = 1.22f;
    static constexpr float kUnderShoot   = 0.04f;

    double sr_ = 48000.0;
    Stage  stage_ = Stage::idle;
    float  value_ = 0.0f;
    float  attackMs_ = 1.0f, decayMs_ = 200.0f, sustain_ = 0.0f, releaseMs_ = 60.0f;
    float  aCoeff_ = 0.1f, dCoeff_ = 0.01f, rCoeff_ = 0.05f;
};

/**
 * The pitch drop. One exponential from 1 to 0 with no stages in it — FALL is
 * its time constant, so the sweep is most of the way down at three times that
 * and the knob reads as the thing you hear rather than the thing that is true
 * at the very end.
 */
class Fall
{
public:
    void prepare (double sampleRate) { sr_ = sampleRate > 0.0 ? sampleRate : 48000.0; setMs (ms_); }

    void setMs (float ms)
    {
        ms_ = ms;
        const double tau = (double) (ms > 0.1f ? ms : 0.1f) * 0.001;
        coeff_ = (float) std::exp (-1.0 / (tau * sr_));
    }

    void trigger() { value_ = 1.0f; }
    void reset()   { value_ = 0.0f; }

    float tick()        { const float v = value_; value_ *= coeff_; return v; }
    float value() const { return value_; }

private:
    double sr_ = 48000.0;
    float  ms_ = 40.0f, coeff_ = 0.999f, value_ = 0.0f;
};

/* ── Oscillator ──────────────────────────────────────────────────────────
 * One phase accumulator feeding four shapes at once, because the mixer wants
 * them phase-locked: four oscillators free-running would smear the one
 * transient the whole instrument is about.
 */
class Osc
{
public:
    void prepare (double sampleRate) { sr_ = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }

    void reset() { phase_ = 0.0f; }

    struct Shapes { float sine, tri, saw, fold; };

    /** All four shapes for one sample, advancing the phase once. */
    Shapes tick (float hz, float foldDepth)
    {
        const float inc = (float) ((double) hz / sr_);

        Shapes s {};
        s.sine = std::sin ((float) kTwoPi * phase_);
        s.fold = wavefold (s.sine, foldDepth);

        /* The saw is the one shape that breaks in value, so it is the one that
           needs correcting. The pitch envelope starts a kick several octaves
           up, and an uncorrected saw at 800 Hz folds enough rubbish back down
           to sit right on top of the fundamental. */
        s.saw = 2.0f * phase_ - 1.0f - polyBlep (phase_, inc);

        /* Naive triangle. It is continuous in value and only breaks in slope,
           so its partials fall off as 1/n^2 where the saw's fall off as 1/n —
           at twice the rate that is already below where the saw needs help,
           and a triangle built by integrating a corrected square would trade
           the aliasing for a DC drift a kick's first cycle cannot afford. */
        s.tri = 4.0f * std::fabs (phase_ - 0.5f) - 1.0f;

        phase_ += inc;
        while (phase_ >= 1.0f) phase_ -= 1.0f;

        return s;
    }

    /**
     * The folder: a sine driven into sin() again, so the fold count grows
     * smoothly rather than in the corners a reflecting folder puts in. Output
     * stays inside +/-1 at every depth, so folding is a timbre control and
     * never a level one.
     */
    static float wavefold (float x, float depth)
    {
        const float d = 1.0f + depth * 6.0f;
        return std::sin ((float) kPi * 0.5f * d * x);
    }

private:
    /** One step's worth of correction, over the sample either side of a
        discontinuity at phase 0. */
    static float polyBlep (float t, float dt)
    {
        if (dt <= 0.0f) return 0.0f;

        if (t < dt)        { t /= dt;              return t + t - t * t - 1.0f; }
        if (t > 1.0f - dt) { t = (t - 1.0f) / dt;  return t * t + t + t + 1.0f; }
        return 0.0f;
    }

    double sr_ = 48000.0;
    float  phase_ = 0.0f;
};

/* ── Filters ─────────────────────────────────────────────────────────────── */

/**
 * Topology-preserving state variable filter. Cutoff can be swept per sample
 * without the coefficient-update artefacts a direct-form biquad gives, which
 * is exactly what a filter envelope on a 30 ms decay asks for.
 */
class Svf
{
public:
    void prepare (double sampleRate) { sr_ = sampleRate > 0.0 ? sampleRate : 48000.0; reset(); }
    void reset() { s1_ = s2_ = 0.0f; }

    void set (float cutoffHz, float q)
    {
        const float nyq = (float) (sr_ * 0.5);
        const float fc = cutoffHz < 10.0f ? 10.0f
                                          : (cutoffHz > nyq * 0.98f ? nyq * 0.98f : cutoffHz);
        g_  = (float) std::tan (kPi * (double) fc / sr_);
        k_  = 1.0f / (q > 0.4f ? q : 0.4f);
        a1_ = 1.0f / (1.0f + g_ * (g_ + k_));
    }

    float lowpass (float x)
    {
        const float hp = (x - (k_ + g_) * s1_ - s2_) * a1_;
        const float v1 = g_ * hp + s1_;
        const float lp = g_ * v1 + s2_;
        s1_ = g_ * hp + v1;
        s2_ = g_ * v1 + lp;
        return lp;
    }

private:
    double sr_ = 48000.0;
    float  g_ = 0.1f, k_ = 1.0f, a1_ = 1.0f;
    float  s1_ = 0.0f, s2_ = 0.0f;
};

/** A direct-form-II transposed biquad, with the RBJ designs the EQ needs. */
class Biquad
{
public:
    void reset() { z1_ = z2_ = 0.0f; }

    void bypass() { b0_ = 1.0f; b1_ = b2_ = a1_ = a2_ = 0.0f; }

    float process (float x)
    {
        const float y = b0_ * x + z1_;
        z1_ = b1_ * x - a1_ * y + z2_;
        z2_ = b2_ * x - a2_ * y;
        return y;
    }

    void peak (double sr, float freq, float gainDb, float q)
    {
        const double A = std::pow (10.0, (double) gainDb / 40.0);
        const double w = kTwoPi * clampFreq (sr, freq) / sr;
        const double alpha = std::sin (w) / (2.0 * (double) (q > 0.05f ? q : 0.05f));
        const double cw = std::cos (w);

        assignRaw (1.0 + alpha * A, -2.0 * cw, 1.0 - alpha * A,
                   1.0 + alpha / A, -2.0 * cw, 1.0 - alpha / A);
    }

    void lowShelf (double sr, float freq, float gainDb, float q = 0.707f)
    {
        const double A = std::pow (10.0, (double) gainDb / 40.0);
        const double w = kTwoPi * clampFreq (sr, freq) / sr;
        const double cw = std::cos (w), sw = std::sin (w);
        const double alpha = sw / (2.0 * (double) q);
        const double tsa = 2.0 * std::sqrt (A) * alpha;

        assignRaw (A * ((A + 1.0) - (A - 1.0) * cw + tsa),
                   2.0 * A * ((A - 1.0) - (A + 1.0) * cw),
                   A * ((A + 1.0) - (A - 1.0) * cw - tsa),
                   (A + 1.0) + (A - 1.0) * cw + tsa,
                   -2.0 * ((A - 1.0) + (A + 1.0) * cw),
                   (A + 1.0) + (A - 1.0) * cw - tsa);
    }

    void lowpass (double sr, float freq, float q = 0.707f)
    {
        const double w = kTwoPi * clampFreq (sr, freq) / sr;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * (double) q);
        const double b1 = 1.0 - cw;
        assignRaw (b1 * 0.5, b1, b1 * 0.5, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

    void highpass (double sr, float freq, float q = 0.707f)
    {
        const double w = kTwoPi * clampFreq (sr, freq) / sr;
        const double cw = std::cos (w), alpha = std::sin (w) / (2.0 * (double) q);
        const double b0 = (1.0 + cw) * 0.5;
        assignRaw (b0, -(1.0 + cw), b0, 1.0 + alpha, -2.0 * cw, 1.0 - alpha);
    }

private:
    static double clampFreq (double sr, float f)
    {
        const double nyq = sr * 0.5;
        return (double) f < 10.0 ? 10.0 : ((double) f > nyq * 0.95 ? nyq * 0.95 : (double) f);
    }

    void assignRaw (double b0, double b1, double b2, double a0, double a1, double a2)
    {
        b0_ = (float) (b0 / a0); b1_ = (float) (b1 / a0); b2_ = (float) (b2 / a0);
        a1_ = (float) (a1 / a0); a2_ = (float) (a2 / a0);
    }

    float b0_ = 1.0f, b1_ = 0.0f, b2_ = 0.0f, a1_ = 0.0f, a2_ = 0.0f;
    float z1_ = 0.0f, z2_ = 0.0f;
};

/* ── Halfband decimator ──────────────────────────────────────────────────
 * The voice and the tape saturator run at twice the host rate; this brings
 * them back down. A windowed-sinc halfband, designed at startup rather than
 * carried as a table of constants — the design is three lines and the
 * constants would be three lines of numbers nobody could check.
 *
 * Linear phase, so the whole instrument is delayed by exactly (taps-1)/4 host
 * samples and the plug-in can report that as its latency.
 */
class Halfband
{
public:
    static constexpr int taps = 31;
    static constexpr int latency = (taps - 1) / 4;   // in host samples

    Halfband() { design(); reset(); }

    void reset() { z_.fill (0.0f); pos_ = 0; }

    /** Two samples in at twice the rate, one out at the host rate. */
    float process (float even, float odd)
    {
        push (even);
        push (odd);

        float acc = 0.0f;
        int i = pos_;
        for (int k = 0; k < taps; ++k)
        {
            i = i == 0 ? taps - 1 : i - 1;
            acc += h_[(std::size_t) k] * z_[(std::size_t) i];
        }
        return acc;
    }

private:
    void push (float x)
    {
        z_[(std::size_t) pos_] = x;
        pos_ = pos_ + 1 == taps ? 0 : pos_ + 1;
    }

    void design()
    {
        const int m = (taps - 1) / 2;
        double sum = 0.0;

        for (int k = 0; k < taps; ++k)
        {
            const double t = 0.5 * (double) (k - m);
            const double sinc = k == m ? 1.0 : std::sin (kPi * t) / (kPi * t);

            /* Blackman: the stopband it buys is worth the wider transition at
               a cutoff that only has to hold above a quarter of the doubled
               rate. */
            const double n = (double) k / (double) (taps - 1);
            const double win = 0.42 - 0.5 * std::cos (kTwoPi * n)
                                    + 0.08 * std::cos (2.0 * kTwoPi * n);

            h_[(std::size_t) k] = (float) (0.5 * sinc * win);
            sum += 0.5 * sinc * win;
        }

        /* Unity at DC, so decimation is not also a gain change. */
        const float g = (float) (1.0 / (sum != 0.0 ? sum : 1.0));
        for (auto& c : h_) c *= g;
    }

    std::array<float, taps> h_ {};
    std::array<float, taps> z_ {};
    int pos_ = 0;
};

/* ── Small helpers ───────────────────────────────────────────────────────── */

inline float softClip (float x)
{
    /* Cheaper than tanh and close enough through the range that matters;
       exactly +/-1 past the knee rather than asymptotic to it. */
    if (x <= -1.5f) return -1.0f;
    if (x >=  1.5f) return  1.0f;
    return x - (4.0f / 27.0f) * x * x * x;
}

inline float dbToGain (float db) { return std::pow (10.0f, db * 0.05f); }

inline float clamp01 (float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }

} // namespace str
