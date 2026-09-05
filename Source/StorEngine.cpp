#include "StorEngine.h"

namespace str
{

namespace
{
/* Room geometry, in milliseconds at size 0.5. Mutually prime-ish lengths, so
   the four combs do not line up into a pitch. The right side runs 3.7% longer
   — enough to decorrelate, short enough that it is still the same room. */
constexpr float kCombMs[4]    = { 12.31f, 15.73f, 19.11f, 22.87f };
constexpr float kAllpassMs[2] = { 5.13f, 1.71f };
constexpr float kRightSkew    = 1.037f;
constexpr float kAllpassG     = 0.5f;

/** Cabinet voicings: highpass, the bump, the mid scoop, and where it stops. */
struct CabSpec
{
    float hpHz, hpQ;
    float bumpHz, bumpDb, bumpQ;
    float scoopHz, scoopDb, scoopQ;
    float lpHz, lpQ;
    float makeupDb;
};

constexpr CabSpec kCabs[numCabs] = {
    /* 12" */ { 70.0f, 0.80f,  95.0f, 3.5f, 1.2f,  420.0f, -6.0f, 1.0f, 4200.0f, 0.90f, 2.0f },
    /* 15" */ { 45.0f, 0.80f,  68.0f, 4.5f, 1.3f,  330.0f, -5.0f, 1.0f, 3200.0f, 0.90f, 3.0f },
    /* 18" */ { 32.0f, 0.80f,  48.0f, 5.5f, 1.4f,  260.0f, -4.0f, 1.0f, 2400.0f, 0.90f, 4.0f },
};

bool nearlyEqual (float a, float b) { return std::fabs (a - b) < 1.0e-6f; }
} // namespace

/* ── Setup ───────────────────────────────────────────────────────────────── */

void StorEngine::prepare (double sampleRate, int)
{
    sr_  = sampleRate > 0.0 ? sampleRate : 48000.0;
    sr2_ = sr_ * 2.0;

    osc_.prepare (sr2_);
    filter_.prepare (sr2_);
    subEnv_.prepare (sr2_);
    fmEnv_.prepare (sr2_);
    modEnv_.prepare (sr2_);
    fall_.prepare (sr2_);

    /* Hiss follows the voice down over about a third of a second: long enough
       that the tape is still audibly running after the hit, short enough that
       an idle instance is silent. */
    hissRelease_ = (float) std::exp (-1.0 / (0.3 * sr2_));

    /* 15 Hz, at the oversampled rate — the asymmetric clip's offset is taken
       out arithmetically, and this catches what the head bump puts back. */
    dcCoeff_ = (float) std::exp (-kTwoPi * 15.0 / sr2_);

    envCoeff_    = (float) (1.0 - std::exp (-1.0 / (0.03 * sr_)));
    roomHpCoeff_ = (float) (1.0 - std::exp (-kTwoPi * 180.0 / sr_));

    const int maxRoom = (int) (sr_ * 0.06) + 4;   // longest comb, largest size
    for (int c = 0; c < 2; ++c)
    {
        for (auto& d : combs_[(std::size_t) c])   d.resize (maxRoom);
        for (auto& d : allpass_[(std::size_t) c]) d.resize (maxRoom);
    }

    haveCache_ = false;
    reset();
}

void StorEngine::reset()
{
    osc_.reset();
    filter_.reset();
    subEnv_.reset();
    fmEnv_.reset();
    modEnv_.reset();
    fall_.reset();
    opPhase_ = { { 0.0f, 0.0f } };
    prevCar_ = 0.0f;
    numHeld_ = 0;

    decim_.reset();
    bump_.reset();
    lossState_ = 0.0f;
    hissHp_ = 0.0f;
    hissGate_ = 0.0f;
    dcX_ = dcY_ = 0.0f;
    roomHpState_ = 0.0f;

    for (auto& b : cabChain_) b.reset();

    for (int c = 0; c < 2; ++c)
    {
        loCut_[(std::size_t) c].reset();
        hiCut_[(std::size_t) c].reset();
        for (auto& b : bells_[(std::size_t) c]) b.reset();

        for (auto& d : combs_[(std::size_t) c])   d.clear();
        for (auto& d : allpass_[(std::size_t) c]) d.clear();
    }

    voiceEnv_ = 0.0f;
}

void StorEngine::setParams (const EngineParams& p)
{
    const bool first = ! haveCache_;
    p_ = p;

    subEnv_.set (p_.subA, p_.subD, p_.subS, p_.subR);
    fmEnv_.set  (p_.fmA,  p_.fmD,  p_.fmS,  p_.fmR);
    modEnv_.set (p_.modA, p_.modD, p_.modS, p_.modR);
    fall_.setMs (p_.fallMs);

    /* The tape's top-end loss: open when clean, down to a cassette's 4.5 kHz
       when it is not. One pole, because two would sound like a filter. */
    const float lossHz = 18000.0f - 13500.0f * p_.drive;
    lossCoeff_ = (float) std::exp (-kTwoPi * (double) lossHz / sr2_);

    if (first || p_.cab != cached_.cab)
        designCab();

    if (first || ! nearlyEqual (p_.loCutHz, cached_.loCutHz)
              || ! nearlyEqual (p_.hiCutHz, cached_.hiCutHz)
              || p_.bellF != cached_.bellF || p_.bellG != cached_.bellG
              || p_.bellQ != cached_.bellQ)
        designEq();

    if (first || ! nearlyEqual (p_.roomSize, cached_.roomSize)
              || ! nearlyEqual (p_.roomDamp, cached_.roomDamp))
        designRoom();

    /* The head bump moves with drive, so it is redesigned every block it
       changes on — one biquad, and drive is the knob people ride. */
    if (first || ! nearlyEqual (p_.drive, cached_.drive))
        bump_.lowShelf (sr2_, 70.0f, 5.0f * p_.drive, 0.7f);

    cached_ = p_;
    haveCache_ = true;
}

void StorEngine::designCab()
{
    const auto& c = kCabs[(std::size_t) juce::jlimit (0, numCabs - 1, (int) p_.cab)];

    cabChain_[0].highpass (sr_, c.hpHz, c.hpQ);
    cabChain_[1].peak     (sr_, c.bumpHz, c.bumpDb, c.bumpQ);
    cabChain_[2].peak     (sr_, c.scoopHz, c.scoopDb, c.scoopQ);
    cabChain_[3].lowpass  (sr_, c.lpHz, c.lpQ);
}

void StorEngine::designEq()
{
    loCutOn_ = loCutActive (p_.loCutHz);
    hiCutOn_ = hiCutActive (p_.hiCutHz);

    for (int c = 0; c < 2; ++c)
    {
        if (loCutOn_) loCut_[(std::size_t) c].highpass (sr_, p_.loCutHz, 0.707f);
        else          loCut_[(std::size_t) c].bypass();

        if (hiCutOn_) hiCut_[(std::size_t) c].lowpass (sr_, p_.hiCutHz, 0.707f);
        else          hiCut_[(std::size_t) c].bypass();

        for (int b = 0; b < numBells; ++b)
        {
            auto& bell = bells_[(std::size_t) c][(std::size_t) b];

            /* A bell at unity is bypassed rather than computed. */
            if (std::fabs (p_.bellG[(std::size_t) b]) < 0.05f)
                bell.bypass();
            else
                bell.peak (sr_, p_.bellF[(std::size_t) b],
                           p_.bellG[(std::size_t) b], p_.bellQ[(std::size_t) b]);
        }
    }
}

void StorEngine::designRoom()
{
    /* Half length to one and a half, so the room goes from a booth to a
       live-ish drum room and no further. */
    const float scale = 0.5f + p_.roomSize;

    for (int c = 0; c < 2; ++c)
    {
        const float skew = c == 0 ? 1.0f : kRightSkew;

        for (int i = 0; i < numCombs; ++i)
        {
            auto& d = combs_[(std::size_t) c][(std::size_t) i];
            const int want = juce::jlimit (1, (int) d.buf.size(),
                                           (int) (kCombMs[i] * scale * skew * 0.001f * (float) sr_));
            if (want != d.size) { d.size = want; d.pos = 0; }
        }

        for (int i = 0; i < numAllpass; ++i)
        {
            auto& d = allpass_[(std::size_t) c][(std::size_t) i];
            const int want = juce::jlimit (1, (int) d.buf.size(),
                                           (int) (kAllpassMs[i] * skew * 0.001f * (float) sr_));
            if (want != d.size) { d.size = want; d.pos = 0; }
        }
    }

    /* Short: even wide open this decays in well under a second, which is what
       "room" is here rather than a name for a hall. */
    combFb_   = 0.52f + 0.34f * p_.roomSize;
    combDamp_ = 0.15f + 0.75f * p_.roomDamp;

    /* Each comb's DC gain is 1/(1 - fb), so left alone a big room is not just
       longer, it is seven times louder. Normalising by the square root of that
       splits the difference: size mostly buys tail, and a little level. */
    combNorm_ = 0.25f * std::sqrt (1.0f - combFb_);
}

/* ── Notes ───────────────────────────────────────────────────────────────── */

void StorEngine::noteOn (int midiNote, float velocity)
{
    if (numHeld_ < (int) held_.size())
        held_[(std::size_t) numHeld_++] = midiNote;

    note_ = midiNote;
    baseHz_ = p_.tuneHz * noteRatio (midiNote);
    velGain_ = 1.0f - p_.velAmount + p_.velAmount * juce::jlimit (0.0f, 1.0f, velocity);

    /* Every hit starts at the same place in the cycle. A kick whose first
       quarter-cycle is a different shape every time is a kick with an
       inconsistent transient, and the transient is the whole drum. */
    osc_.reset();
    opPhase_ = { { 0.0f, 0.0f } };
    prevCar_ = 0.0f;
    fall_.trigger();

    subEnv_.noteOn();
    fmEnv_.noteOn();
    modEnv_.noteOn();
}

void StorEngine::noteOff (int midiNote)
{
    for (int i = 0; i < numHeld_; ++i)
        if (held_[(std::size_t) i] == midiNote)
        {
            for (int j = i; j + 1 < numHeld_; ++j)
                held_[(std::size_t) j] = held_[(std::size_t) (j + 1)];
            --numHeld_;
            break;
        }

    /* Only the last finger off it stops it. Releasing one of two held keys must
       not cut a decay the other key is still holding. */
    if (numHeld_ == 0)
    {
        subEnv_.noteOff();
        fmEnv_.noteOff();
        modEnv_.noteOff();
    }
}

void StorEngine::allNotesOff()
{
    numHeld_ = 0;
    subEnv_.noteOff();
    fmEnv_.noteOff();
    modEnv_.noteOff();
}

/* ── Voice ───────────────────────────────────────────────────────────────── */

float StorEngine::opSample (OpWave w, float phase, Noise& n)
{
    switch (w)
    {
        case OpWave::tri:   return 4.0f * std::fabs (phase - 0.5f) - 1.0f;
        case OpWave::noise: return n.next();
        case OpWave::sine:
        default:            return std::sin ((float) kTwoPi * phase);
    }
}

float StorEngine::renderVoice()
{
    const float pitch = fall_.tick();
    const float subE  = subEnv_.tick();
    const float fmE   = fmEnv_.tick();

    /* Ticked before the early out, so the modulation envelope stays in step
       with the two amp envelopes whether or not anything is sounding. */
    const float modE = modEnv_.tick();

    if (subE <= 0.0f && fmE <= 0.0f)
        return 0.0f;

    const float hz = juce::jlimit (10.0f, (float) (sr2_ * 0.45),
                                   baseHz_ * std::pow (2.0f, p_.bendSemis * pitch / 12.0f));

    /* ── Subtractive ─────────────────────────────────────────────────── */
    const float foldDepth = clamp01 (p_.fold + p_.foldEnv * modE);
    const auto s = osc_.tick (hz, foldDepth);

    /* The mixer is a mixer: these sum, and are not normalised. Four shapes at
       full is four times the level, and the output stage is where that is
       dealt with. */
    float sub = s.sine * p_.lvl[0] + s.tri * p_.lvl[1]
              + s.saw  * p_.lvl[2] + s.fold * p_.lvl[3];

    if (p_.cutoffHz < 17900.0f || p_.resoQ > 0.71f || std::fabs (p_.filtOctaves) > 0.02f)
    {
        filter_.set (p_.cutoffHz * std::pow (2.0f, p_.filtOctaves * modE), p_.resoQ);
        sub = filter_.lowpass (sub);
    }

    sub *= subE * p_.subGain;

    /* ── FM: op 2 into op 1 ──────────────────────────────────────────── */
    float fm = 0.0f;
    if (p_.fmGain > 0.0f)
    {
        const float idx = p_.index * (1.0f - p_.indexEnv + p_.indexEnv * modE);

        /* The way back: op 1's last output bent into op 2's phase, so the two
           modulate each other instead of one just feeding the other. Phase
           modulation is bounded whatever you put into it, so the loop cannot
           run away — it only gets brighter and more inharmonic. */
        float ph2 = opPhase_[1] + prevCar_ * p_.xfm * (float) (1.0 / kTwoPi);
        ph2 -= std::floor (ph2);

        const float mod = opSample (p_.wave[1], ph2, opNoise_) * idx;

        float car;
        if (p_.wave[0] == OpWave::noise)
        {
            /* Phase modulating noise does nothing — noise has no phase to
               move. The modulator rings it instead, which keeps op 2 audible
               in the one place phase modulation cannot reach. */
            car = opNoise_.next() * (0.5f + 0.5f * std::cos (mod));
        }
        else
        {
            float ph = opPhase_[0] + mod * (float) (1.0 / kTwoPi);
            ph -= std::floor (ph);
            car = opSample (p_.wave[0], ph, opNoise_);
        }

        prevCar_ = car;
        fm = car * fmE * p_.fmGain;

        for (int o = 0; o < numOps; ++o)
        {
            opPhase_[(std::size_t) o] += (float) ((double) (hz * p_.ratio[(std::size_t) o]) / sr2_);
            opPhase_[(std::size_t) o] -= std::floor (opPhase_[(std::size_t) o]);
        }
    }

    return (sub + fm) * velGain_;
}

/* ── Tape ────────────────────────────────────────────────────────────────── */

float StorEngine::tape (float x)
{
    const float drive = p_.drive;

    if (drive > 0.0f)
    {
        /* Head bump first: what the tape emphasises is what it then saturates,
           which is why a cassette gets fat before it gets dirty. */
        x = bump_.process (x);

        const float g = 1.0f + drive * 15.0f;
        const float bias = drive * 0.12f;

        /* Asymmetry is where the even harmonics come from. The offset is taken
           straight back out so it does not arrive as DC. */
        x = softClip (x * g + bias) - softClip (bias);

        /* Level compensation matched by RMS, not by peak. Saturation is
           supposed to collapse the crest factor — taking the peak back to
           where it was would mean the drive made the drum quieter, which is
           not what anyone reaches for it to do. */
        x *= 1.0f / (1.0f + drive * 2.0f);

        /* Top end coming off. */
        lossState_ += (x - lossState_) * (1.0f - lossCoeff_);
        x = lossState_;
    }

    /* Hiss, gated by the voice so a loaded instance sitting still is silent.
       It is the tape you can hear running, not a noise floor in the mix. */
    const float env = std::fmax (subEnv_.value(), fmEnv_.value());
    hissGate_ = env > hissGate_ ? env : hissGate_ * hissRelease_;

    const float hissAmp = drive * drive * p_.hiss * 0.035f * hissGate_;
    if (hissAmp > 0.0f)
    {
        /* Weighted up top, the way tape noise actually sits. */
        const float n = hissNoise_.next();
        hissHp_ += (n - hissHp_) * 0.12f;
        x += (n - hissHp_) * hissAmp;
    }

    /* DC blocker: the bump and the asymmetry both leave a little behind. */
    const float y = x - dcX_ + dcCoeff_ * dcY_;
    dcX_ = x;
    dcY_ = y;
    return y;
}

/* ── Cabinet and EQ ──────────────────────────────────────────────────────
 * The cabinet is in front of the room, because a speaker in a room is what
 * this is a picture of. The EQ is behind it, because by then it is a finished
 * drum and what you want to do to it you want to do to all of it.
 */

float StorEngine::cabinet (float x)
{
    if (p_.cabMix <= 0.0f)
        return x;

    float c = x;
    for (auto& b : cabChain_)
        c = b.process (c);

    c *= dbToGain (kCabs[(std::size_t) juce::jlimit (0, numCabs - 1, (int) p_.cab)].makeupDb);
    return x + (c - x) * p_.cabMix;
}

float StorEngine::eq (float x, int channel)
{
    const auto c = (std::size_t) channel;

    if (loCutOn_) x = loCut_[c].process (x);
    for (auto& b : bells_[c]) x = b.process (x);
    if (hiCutOn_) x = hiCut_[c].process (x);

    return x;
}

/* ── Block ───────────────────────────────────────────────────────────────── */

void StorEngine::process (float* left, float* right, int numSamples)
{
    const bool room = p_.roomMix > 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        /* Two at the doubled rate, one out. The saturator lives inside it,
           being the only stage in the chain that generates what would fold. */
        const float a = tape (renderVoice());
        const float b = tape (renderVoice());
        const float dry = cabinet (decim_.process (a, b));

        voiceEnv_ += (std::fmax (subEnv_.value(), fmEnv_.value()) - voiceEnv_) * envCoeff_;

        float out[2] = { dry, dry };

        if (room)
        {
            roomHpState_ += (dry - roomHpState_) * roomHpCoeff_;
            const float send = dry - roomHpState_;

            for (int c = 0; c < 2; ++c)
            {
                float wet = 0.0f;

                for (auto& d : combs_[(std::size_t) c])
                {
                    const float y = d.buf[(std::size_t) d.pos];
                    d.store += (y - d.store) * (1.0f - combDamp_);
                    d.buf[(std::size_t) d.pos] = send + d.store * combFb_;
                    d.pos = d.pos + 1 >= d.size ? 0 : d.pos + 1;
                    wet += y;
                }

                wet *= combNorm_;

                for (auto& d : allpass_[(std::size_t) c])
                {
                    const float y = d.buf[(std::size_t) d.pos];
                    d.buf[(std::size_t) d.pos] = wet + y * kAllpassG;
                    d.pos = d.pos + 1 >= d.size ? 0 : d.pos + 1;
                    wet = y - wet * kAllpassG;
                }

                /* Added to the dry rather than crossfaded with it: a room does
                   not take the drum away, it puts something behind it. */
                out[c] = dry + wet * p_.roomMix;
            }
        }

        left[i]  = eq (out[0], 0) * p_.outGain;
        right[i] = eq (out[1], 1) * p_.outGain;
    }
}

} // namespace str
