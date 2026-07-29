#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "../Parameters/ParameterIDs.h"

struct HostSyncInfo
{
    double bpm = 120.0;
    int numerator = 4;
    int denominator = 4;
    bool isPlaying = false;
    bool hasPosition = false; // false when the host gave no playhead/position info at all

    // Absolute musical position, in quarter notes, from the timeline origin.
    // This is what lets recapture boundaries land exactly on the DAW's real
    // bar/beat grid, instead of drifting based on whenever the effect
    // happened to be engaged (a free-running sample countdown can't do this).
    bool hasPpq = false;
    double ppqPosition = 0.0;

    // Unused by the boundary-scheduling math (see samplesUntilNextBoundaryWithinBlock)
    // but kept around as diagnostic info about the host's bar grid.
    bool hasBarStart = false;
    double ppqPositionOfLastBarStart = 0.0;
};

// Reads AudioPlayHead::getPosition() with sane fallbacks (120 BPM/4-4) for
// hosts/contexts that don't report tempo (e.g. plugin validators, Standalone).
HostSyncInfo queryHostSync(juce::AudioProcessor& processor);

// Division length in quarter notes (tempo/sample-rate independent).
double loopDivisionToQuarterNotes(LoopDivision division, int numerator, int denominator);

// Division length in samples at the given tempo/time-signature/sample rate.
int loopDivisionToSamples(LoopDivision division, const HostSyncInfo& hostSync, double sampleRate);

// If the next division boundary (an exact multiple of the division length,
// measured in quarter notes from the timeline origin) falls within
// [0, numSamples) of the current block, returns its sample offset within the
// block. Returns -1 if no boundary falls in this block, or if the host
// provides no ppq position.
int samplesUntilNextBoundaryWithinBlock(const HostSyncInfo& hostSync, LoopDivision division,
                                         int numSamples, double sampleRate);
