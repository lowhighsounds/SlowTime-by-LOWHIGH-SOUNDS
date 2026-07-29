#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include "signalsmith-stretch/signalsmith-stretch.h"

// Wraps Signalsmith Stretch for pitch-only shifting (duration preserved 1:1
// with the host block, i.e. Bloco A). Not used for time stretching:
// Signalsmith's own docs note quality degrades outside ~0.75x-1.5x, which
// covers none of this plugin's fixed Vibe ratios (Bloco B's job).
//
// The STFT analysis window is always the library's own high-quality
// presetDefault(), regardless of the "Mode" (Smooth/Transient/Punchy) the
// user picks -- Waves SoundShifter's own manual describes that ladder as a
// transient-preservation character, expressed here via the tonality limit
// (see setPitchSemitones), not a window-size/quality tradeoff. A shorter
// window only degrades frequency resolution and causes pitch instability.
class StretchEngine
{
public:
    void prepare(double sampleRate, int maxBlockSize, int numChannels);
    void reset();

    // tonalityLimitHz: 0 = no limit (full harmonic accuracy across the
    // spectrum). >0 = Hz threshold above which the library's frequency
    // mapping switches from multiplicative (pitch-correct) to additive
    // (phase-stable but less timbrally accurate) -- see mapFreq() in
    // signalsmith-stretch.h. Lower values trade more high-frequency accuracy
    // for reduced phase-smearing on percussive transients.
    void setPitchSemitones(float semitones, float tonalityLimitHz = 0.0f);

    // In-place from the caller's point of view: reads buffer, writes result back into it.
    void process(juce::AudioBuffer<float>& buffer, int numSamples);

    int getLatencySamples() const;

private:
    signalsmith::stretch::SignalsmithStretch<float> stretch;
    juce::AudioBuffer<float> scratchOutput;
    double sampleRate = 44100.0;
    int channels = 2;
};
