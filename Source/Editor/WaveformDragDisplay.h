#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "RetroComponents.h"
#include <vector>
#include <memory>

class SlowTimeAudioProcessor;

// Central panel that draws the play-to-pause capture of the plugin's output
// and lets the user drag that audio straight out to the host (e.g. FL Studio's
// playlist), the way Rolling Sample / Deja Vu do. The drag exports a temp WAV
// via JUCE's external file drag-drop -- a real file the DAW imports, not a live
// stream.
//
// The waveform is drawn from an INCREMENTAL peak cache: each refresh only
// buckets the samples recorded since the last one. The first version copied the
// whole capture every frame, which at the capacity cap meant ~42 MB reallocated
// and copied 15x a second on the message thread -- enough to starve the audio
// thread and produce the crackling the user heard as the recording grew.
class WaveformDragDisplay : public juce::Component,
                            private juce::Timer
{
public:
    explicit WaveformDragDisplay(SlowTimeAudioProcessor& processorToDisplay);
    ~WaveformDragDisplay() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    juce::Rectangle<int> recGroupBounds() const; // REC LED + label area (top-left)
    void rebuildPeaksIncrementally();
    static bool renderBufferToWav(const juce::AudioBuffer<float>& buffer,
                                  double sampleRate, const juce::File& target);
    static juce::String formatTime(double seconds);

    SlowTimeAudioProcessor& processor;

    // One min/max pair per bucket of kSamplesPerBucket input samples.
    static constexpr int kSamplesPerBucket = 1024;
    std::vector<float> peakMin, peakMax;
    int peaksBuiltToSample = 0;

    bool dragInProgress = false;

    // Red record-arm LED sitting in the visor, bound to the recordEnabled param.
    LedToggle recordButton { retro::accentAlert };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> recordAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformDragDisplay)
};
