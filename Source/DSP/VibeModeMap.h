#pragma once

#include "../Parameters/ParameterIDs.h"

struct VibeModeParams
{
    float timeRatio;      // output duration / input duration (1.0 = Off/passthrough)
    float pitchSemitones; // derived, not hardcoded: -12 * log2(timeRatio)
};

// Pure lookup: VibeMode -> the (ratio, pitch) pair ChopVibeEngine needs.
// Ratios follow the 2:1/3:2 proportion families from the spec (never 2.5x
// etc.) so they stay rhythmically aligned with musical delay-time logic.
VibeModeParams vibeModeToParams(VibeMode mode);
