#include "ChopVibeEngine.h"
#include "../Utility/DspConstants.h"
#include <cmath>

void ChopVibeEngine::prepare(double newSampleRate, int numChannels, double maxCaptureSeconds)
{
    sampleRate = newSampleRate;
    channels = numChannels;

    rollingSize = juce::jmax(64, static_cast<int>(sampleRate * maxCaptureSeconds));
    rolling.setSize(channels, rollingSize, false, false, true);

    minCrossfadeSamples = juce::jmax(1,
        static_cast<int>(DspConstants::kSmoothMinCrossfadeSeconds * sampleRate));

    reset();
}

void ChopVibeEngine::reset()
{
    rolling.clear();
    rollingWriteHead = 0;
    readHead = 0.0;
    prevReadHead = 0.0;
    prevReadIncrement = 0.0;
    dryReadHead = 0.0;
    crossfadeRemaining = 0;
    crossfadeLength = 0;
    crossfadeDryBleed = false;
    hasActiveWindow = false;
    windowLength = 0;
    windowPos = 0;
    reverseRequested = false;
    windowReversed = false;
}

void ChopVibeEngine::setRatio(float ratio)
{
    currentRatio = juce::jlimit(1.0f / 8.0f, 8.0f, ratio);
}

void ChopVibeEngine::setSmooth(float amount)
{
    smoothAmount = juce::jlimit(0.0f, 1.0f, amount);
}

void ChopVibeEngine::setReversed(bool shouldReverse)
{
    reverseRequested = shouldReverse;
}

float ChopVibeEngine::windowEnvelopeGain(int posInWindow, int length) const
{
    if (length <= 0 || smoothAmount <= 0.0f)
        return 1.0f;

    const float phase = juce::jlimit(0.0f, 1.0f,
        static_cast<float>(posInWindow) / static_cast<float>(length));

    // Smooth = 1 puts the fade-in and fade-out edges back-to-back (half the
    // window each), i.e. one continuous swell with no flat middle.
    const float half = juce::jlimit(1.0e-4f, 0.5f, smoothAmount * 0.5f);

    if (phase < half)
        return std::sin((phase / half) * juce::MathConstants<float>::halfPi);
    if (phase > 1.0f - half)
        return std::sin(((1.0f - phase) / half) * juce::MathConstants<float>::halfPi);
    return 1.0f;
}

void ChopVibeEngine::clearHistory()
{
    rolling.clear();
    // Keep rollingWriteHead advancing monotonically; the cleared samples just
    // read back as silence, which is what we want after a jump.
}

void ChopVibeEngine::pushInput(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    for (int ch = 0; ch < channels; ++ch)
    {
        const float* in = buffer.getReadPointer(ch, startSample);
        for (int i = 0; i < numSamples; ++i)
        {
            auto idx = ringIndex(rollingWriteHead + i);
            rolling.setSample(ch, idx, in[i]);
        }
    }
    rollingWriteHead += numSamples;
}

int ChopVibeEngine::ringIndex(juce::int64 absolutePosition) const
{
    juce::int64 m = ((absolutePosition % rollingSize) + rollingSize) % rollingSize;
    return static_cast<int>(m);
}

float ChopVibeEngine::readRing(int channel, double absolutePosition) const
{
    // Never read past what's been written (the ±2 cubic taps included), nor
    // older than the ring can still hold.
    const double maxPos = static_cast<double>(rollingWriteHead) - 2.0;
    const double minPos = static_cast<double>(rollingWriteHead - rollingSize) + 2.0;
    double p = juce::jlimit(minPos, maxPos, absolutePosition);

    const auto base = static_cast<juce::int64>(std::floor(p));
    const auto frac = static_cast<float>(p - static_cast<double>(base));

    const float y0 = rolling.getSample(channel, ringIndex(base - 1));
    const float y1 = rolling.getSample(channel, ringIndex(base));
    const float y2 = rolling.getSample(channel, ringIndex(base + 1));
    const float y3 = rolling.getSample(channel, ringIndex(base + 2));

    const float a0 = -0.5f * y0 + 1.5f * y1 - 1.5f * y2 + 0.5f * y3;
    const float a1 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
    const float a2 = -0.5f * y0 + 0.5f * y2;
    const float a3 = y1;

    return ((a0 * frac + a1) * frac + a2) * frac + a3;
}

void ChopVibeEngine::resetToLiveBoundary(int samplesBeforeNow, int windowLengthSamples, bool hardSnap)
{
    windowLengthSamples = juce::jmax(1, windowLengthSamples);

    // The outgoing (about-to-be-old) window's advance, captured before we latch
    // the new direction, so the short seam blend continues the old chunk in the
    // direction it was actually playing (matters when reverse flips each window,
    // e.g. ping-pong).
    const double oldIncrement = (windowReversed ? -1.0 : 1.0) / static_cast<double>(currentRatio);

    // Latch the requested direction for THIS window.
    windowReversed = reverseRequested;

    // Absolute input position of this boundary.
    const double boundaryAbs = static_cast<double>(rollingWriteHead - samplesBeforeNow);

    double newRead;
    if (windowReversed)
    {
        // Reverse: anchor AT the boundary and read BACKWARD (negative increment
        // in process()) through the previous window's already-recorded audio,
        // replaying it end-first. Only touches the past, so it's always causal
        // -- provided the caller waited for a full window of fresh history.
        newRead = boundaryAbs;
    }
    else if (currentRatio >= 1.0f)
    {
        // Slowdown / unity: anchor AT the boundary and read forward, slower
        // than real time. Only ever touches audio at/after the boundary, so
        // no stale pre-jump history is ever read.
        newRead = boundaryAbs;
    }
    else
    {
        // Speed-up: anchor in the recent past so the (faster) read catches up
        // to the live position exactly at the window's end -- still causal.
        const double extra = static_cast<double>(windowLengthSamples)
                             * (1.0 / static_cast<double>(currentRatio) - 1.0);
        newRead = boundaryAbs - extra;
    }

    // Clamp to what the ring can still supply.
    const double minAvail = static_cast<double>(rollingWriteHead - rollingSize) + 4.0;
    newRead = juce::jmax(newRead, minAvail);

    if (! hardSnap && hasActiveWindow)
    {
        // Smooth: crossfade length scales with |smoothAmount|, always at least
        // the click-safe minimum and never more than half the window.
        //
        // Tried and reverted: giving hardSnap a short crossfade too (blending
        // the tail of the outgoing read into the new one, like a normal
        // boundary) instead of a clean cut. In theory an instant jump is a
        // click; in practice, at a real loop-wrap/transport-jump the two
        // sides are two DIFFERENT pitch/time-shifted streams (e.g. both 2x
        // slowed, but from unrelated source positions), so blending them even
        // briefly produced an audible warble that read as much worse than the
        // plain cut -- confirmed by ear against the known-good build. Keep
        // hardSnap as a true hard cut.
        // Fixed, short: purely a click guard for the read-pointer jump. It is
        // deliberately NOT scaled by Smooth -- see setSmooth() for why letting
        // the outgoing chunk run long turned consecutive chords to mush.
        crossfadeLength = juce::jmax(1, juce::jmin(minCrossfadeSamples, windowLengthSamples / 4));
        crossfadeRemaining = crossfadeLength;

        crossfadeDryBleed = false;
        prevReadHead = readHead;          // outgoing chunk, for the short seam blend
        prevReadIncrement = oldIncrement; // ...continuing in its own direction
        dryReadHead = boundaryAbs;
    }
    else
    {
        crossfadeRemaining = 0;
    }

    readHead = newRead;
    windowLength = windowLengthSamples;
    windowPos = 0;
    hasActiveWindow = true;
}

void ChopVibeEngine::process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (! hasActiveWindow || windowLength <= 0)
    {
        for (int ch = 0; ch < channels; ++ch)
            buffer.clear(ch, startSample, numSamples);
        return;
    }

    const double increment = (windowReversed ? -1.0 : 1.0) / static_cast<double>(currentRatio);

    for (int i = 0; i < numSamples; ++i)
    {
        // Per-window Smooth envelope: fades each window in at its start and out
        // at its end, so neighbouring chunks meet at zero rather than overlapping.
        const float envGain = windowEnvelopeGain(windowPos, windowLength);

        const bool fading = crossfadeRemaining > 0;
        float gOld = 0.0f, gNew = 1.0f;
        if (fading)
        {
            // Short equal-power blend across the read-pointer jump only -- a
            // click guard, not the Smooth character (which is the envelope above).
            const float t = 1.0f - static_cast<float>(crossfadeRemaining)
                                   / static_cast<float>(juce::jmax(1, crossfadeLength));
            gOld = std::cos(t * juce::MathConstants<float>::halfPi);
            gNew = std::sin(t * juce::MathConstants<float>::halfPi);
        }

        for (int ch = 0; ch < channels; ++ch)
        {
            float sample = readRing(ch, readHead);

            if (fading)
                sample = readRing(ch, prevReadHead) * gOld + sample * gNew;

            buffer.setSample(ch, startSample + i, sample * envGain);
        }

        readHead += increment;
        ++windowPos;
        if (fading)
        {
            prevReadHead += prevReadIncrement;
            --crossfadeRemaining;
        }
    }
}
