#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterIDs.h"
#include "DSP/StretchEngine.h"
#include "DSP/ChopVibeEngine.h"

class SlowTimeAudioProcessor : public juce::AudioProcessor
{
public:
    SlowTimeAudioProcessor();
    ~SlowTimeAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    // --- Play-to-pause capture of the final output, for drag-to-DAW ---
    // Recording resets on each transport start and grows while playing (up to
    // the pre-allocated cap). Read from the editor thread via the snapshot
    // accessor below; recordedSamples is the lock-free hand-off (audio thread
    // publishes the valid length with a release store after writing).
    int getRecordedNumSamples() const { return recordedSamples.load(std::memory_order_acquire); }
    double getRecordSampleRate() const { return currentSampleRate; }
    // Copies the currently-recorded audio into dest; returns the sample count.
    // O(recorded length) -- for one-off use (rendering the drag file) only,
    // never per UI frame: at the capacity cap that is a ~42 MB copy.
    int getRecordingSnapshot(juce::AudioBuffer<float>& dest) const;

    // Direct read access for the editor's incremental peak cache. Safe from the
    // message thread: recordBuffer is pre-allocated in prepareToPlay and never
    // resized while running, and the audio thread only ever appends BELOW the
    // published recordedSamples length.
    int getRecordedNumChannels() const { return recordBuffer.getNumChannels(); }
    const float* getRecordedReadPointer(int channel) const { return recordBuffer.getReadPointer(channel); }
    int getRecordCapacity() const { return recordCapacity; }
    bool isRecordingFull() const { return recordedSamples.load(std::memory_order_acquire) >= recordCapacity; }

private:
    static float tonalityLimitHzForMode(AlgoMode mode);

    StretchEngine stretchEngine;
    ChopVibeEngine chopVibeEngine;   // Bloc B: chop / vari-speed
    ChopVibeEngine reverseEngine;    // Bloc C: grid-synced reverse (always reverse; gated)

    juce::SmoothedValue<float> smoothedPitchSemitones;
    juce::SmoothedValue<float> smoothedTonalityLimitHz;
    juce::SmoothedValue<float> smoothedSmooth;       // Bloc B "Smooth" (0..1)
    juce::SmoothedValue<float> smoothedBandLowCutHz; // Band high-pass cutoff
    juce::SmoothedValue<float> smoothedBandHighCutHz; // Band low-pass cutoff
    double currentSampleRate = 44100.0;
    int cachedBlocALatencySamples = 0;

    // Band: band-pass applied to the WET signal only (see processBlock).
    juce::dsp::StateVariableTPTFilter<float> bandHighPass; // low cut
    juce::dsp::StateVariableTPTFilter<float> bandLowPass;  // high cut

    // Dry copy of the block's input, and a delay line that holds the dry path
    // back by the wet path's latency (Bloc A's) so the Mix / Fade crossfades
    // line up instead of comb-filtering.
    juce::AudioBuffer<float> dryBuffer;
    juce::AudioBuffer<float> dryDelayRing;
    int dryDelaySize = 0;
    juce::int64 dryDelayWriteHead = 0;

    // Fade In/Out engage gain (0 = fully dry, 1 = full processed output),
    // ramped at the Fade In rate while engaging and the Fade Out rate while
    // disengaging. Snapped to its target on the first block so it doesn't fade
    // in from silence. Currently always targets 1.0 (the Bypass toggle that
    // drove it was removed); Fade In/Out will re-drive it once wired up.
    float engageGain = 1.0f;
    bool engageInitialised = false;

    bool lastBlocAEnabled = false;
    bool lastBlocBEnabled = false;
    LoopDivision lastLoopDivision = LoopDivision::Quarter;
    int lastReportedLatency = -1;

    // True once Bloc B has anchored to a real, on-grid boundary and is
    // actively chopping. False right after enabling, after a loop-division
    // change, or after a transport jump -- while false, Bloc B is "armed"
    // but passes audio through untouched (see processBlock) until the next
    // clean grid boundary arrives, instead of anchoring immediately wherever
    // the transport happens to be sitting. Anchoring immediately was the
    // cause of the reported "first window plays off-tempo, then snaps into
    // place" symptom: since the very first window basically never lands on
    // an exact grid line, it always used to end up starting off-beat.
    bool blocBEngaged = false;

    // Transport-discontinuity detection for Bloc B (see HostSync / ChopVibeEngine):
    // a play/seek/loop jump re-anchors the engine to the live position at once
    // (see processBlock), instead of letting a window from before the jump
    // keep reading until the next scheduled grid boundary.
    bool lastHostIsPlaying = false;
    double expectedNextPpqPosition = -1.0;

    // Free-running boundary counter for hosts that report no PPQ position
    // (e.g. Standalone/validators): with no grid to lock to, Bloc B re-anchors
    // every division worth of output samples so its read pointer can't drift
    // arbitrarily far behind the live signal.
    juce::int64 blocBSamplesSinceAnchor = 0;

    // --- Bloc C (reverse) state ---
    // The reverse engine always runs reversed; a per-sample gate crossfades the
    // buffer between dry (normal windows) and the reversed engine output (the
    // windows the pattern selects). Reverse reads BACKWARD into history, so
    // blocCSamplesSinceJump must reach a full division before any window may
    // reverse (freshness guard).
    juce::AudioBuffer<float> scratchC;
    juce::SmoothedValue<float> blocCGate;      // 0 = dry, 1 = reversed
    juce::int64 blocCSamplesSinceJump = 0;
    int blocCWindowIndex = -1;                 // grid-window counter for the pattern
    bool blocCCurrentReversed = false;         // is the window currently playing reversed
    bool blocCNeedsHardSnap = false;           // hard-cut the engine's first post-jump anchor
    juce::int64 blocCFreeRun = 0;              // no-PPQ free-running division counter
    bool lastBlocCEnabled = false;
    LoopDivision lastBlocCDivision = LoopDivision::Quarter;
    juce::SmoothedValue<float> smoothedBlocCSmooth; // ramped: a jumped envelope depth clicks
    juce::Random blocCRandom;                  // Random pattern (audio-thread safe, no alloc)

    // Play-to-pause capture backing store (see the public accessors above).
    juce::AudioBuffer<float> recordBuffer;
    int recordCapacity = 0;
    int recordWritePos = 0;
    std::atomic<int> recordedSamples { 0 };

    void updateLatencyReport(bool blocAEnabled);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowTimeAudioProcessor)
};
