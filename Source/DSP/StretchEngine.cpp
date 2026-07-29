#include "StretchEngine.h"

void StretchEngine::prepare(double newSampleRate, int maxBlockSize, int numChannels)
{
    sampleRate = newSampleRate;
    channels = numChannels;
    scratchOutput.setSize(channels, maxBlockSize, false, false, true);

    // Always the library's own high-quality window -- see the class comment
    // in StretchEngine.h for why this never varies by Mode.
    stretch.presetDefault(channels, static_cast<float>(sampleRate));
}

void StretchEngine::reset()
{
    stretch.reset();
}

void StretchEngine::setPitchSemitones(float semitones, float tonalityLimitHz)
{
    const float tonalityLimit = tonalityLimitHz > 0.0f
        ? tonalityLimitHz / static_cast<float>(sampleRate)
        : 0.0f;
    stretch.setTransposeSemitones(semitones, tonalityLimit);
}

void StretchEngine::process(juce::AudioBuffer<float>& buffer, int numSamples)
{
    stretch.process(buffer.getArrayOfReadPointers(), numSamples,
                     scratchOutput.getArrayOfWritePointers(), numSamples);

    for (int ch = 0; ch < channels; ++ch)
        buffer.copyFrom(ch, 0, scratchOutput, ch, 0, numSamples);
}

int StretchEngine::getLatencySamples() const
{
    return stretch.inputLatency() + stretch.outputLatency();
}
