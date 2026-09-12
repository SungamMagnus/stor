#pragma once

#include <array>
#include <atomic>

#include "Dsp.h"
#include "Parameters.h"

namespace str
{

/**
 * One offset generator per control. A single shared generator would move
 * every armed knob by the same amount on the same hit — a chorus effect, not
 * a randomiser — so each of the kNumKnobs controls gets its own Noise stream,
 * seeded so no two ever coincide.
 *
 * roll() is called from the audio thread, on a MIDI hit or a host-tempo tick;
 * arming is set from the message thread, off the panel. Nothing here
 * allocates, so both are real-time safe.
 */
class RandomEngine
{
public:
    RandomEngine()
    {
        for (int i = 0; i < kNumKnobs; ++i)
            noise_[(std::size_t) i].reset (0x9e3779b9u + (std::uint32_t) i * 0x85ebca6bu);
    }

    void setArmed (int i, bool on) { armed_[(std::size_t) i].store (on, std::memory_order_relaxed); }
    bool isArmed (int i) const { return armed_[(std::size_t) i].load (std::memory_order_relaxed); }

    /** A fresh offset for every armed control, in normalised units scaled by
        strength — zero strength means every roll lands back on zero. */
    void roll (float strength)
    {
        for (int i = 0; i < kNumKnobs; ++i)
            if (isArmed (i))
                offset_[(std::size_t) i].store (noise_[(std::size_t) i].next() * strength,
                                                std::memory_order_relaxed);

        seq_.fetch_add (1u, std::memory_order_relaxed);
    }

    /** The offset an armed control is carrying right now, in normalised
        units; zero for a control that has never been armed or never rolled. */
    float offsetFor (int i) const { return offset_[(std::size_t) i].load (std::memory_order_relaxed); }

    /** Bumped on every roll, so the panel can flash a lamp without polling
        forty-nine offsets to notice one of them changed. */
    std::uint32_t rollSeq() const { return seq_.load (std::memory_order_relaxed); }

private:
    std::array<Noise, kNumKnobs> noise_;
    std::array<std::atomic<bool>, kNumKnobs> armed_ {};
    std::array<std::atomic<float>, kNumKnobs> offset_ {};
    std::atomic<std::uint32_t> seq_ { 0 };
};

} // namespace str
