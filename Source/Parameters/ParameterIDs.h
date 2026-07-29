#pragma once

namespace ParamIDs
{
    inline constexpr auto blocAEnabled    = "blocAEnabled";
    inline constexpr auto blocBEnabled    = "blocBEnabled";
    inline constexpr auto pitchSemitones  = "pitchSemitones";
    inline constexpr auto fineCents       = "fineCents";
    inline constexpr auto sourceFreqHz    = "sourceFreqHz";
    inline constexpr auto algoMode        = "algoMode";
    inline constexpr auto vibeMode        = "vibeMode";
    inline constexpr auto loopDivision    = "loopDivision";
    inline constexpr auto smooth          = "smooth";       // Bloc B reset-transition smoothing (0..100)
    inline constexpr auto bandLowCutHz    = "bandLowCutHz";  // Band: high-pass (low cut) on wet only
    inline constexpr auto bandHighCutHz   = "bandHighCutHz"; // Band: low-pass (high cut) on wet only
    inline constexpr auto fadeInBars      = "fadeInBars";    // engage time (tempo-synced bars)
    inline constexpr auto fadeOutBars     = "fadeOutBars";   // disengage time (tempo-synced bars)
    inline constexpr auto recordEnabled   = "recordEnabled"; // arm the play-to-pause capture (drag-to-DAW)

    // Bloc C: grid-synced reverse, independent of Bloc B.
    inline constexpr auto blocCEnabled    = "blocCEnabled";
    inline constexpr auto blocCDivision   = "blocCDivision"; // its own LOOP grid
    inline constexpr auto blocCPattern    = "blocCPattern";  // which windows get reversed
    inline constexpr auto blocCChance     = "blocCChance";   // 0..100, only used by Random
    inline constexpr auto blocCSmooth     = "blocCSmooth";   // per-window envelope (0..100)
}

enum class AlgoMode { Smooth, Transient, Punchy };
// Off was removed: the block's enable LED is now the on/off switch, so every
// VibeMode is an active slowdown ratio.
enum class VibeMode { Ratio1_5x, Ratio2x, Ratio4x };
enum class ReversePattern { Always, PingPong, EveryTwo, EveryFour, Random };
// Deja Vu's own division set. Every entry divides the bar evenly, which is
// what keeps the reset grid regular -- dotted divisions (1/8., 1/4.) were
// tried and removed: at 0.75 / 1.5 quarter notes they fit 5.333 / 2.667 times
// into a 4/4 bar, so the last reset of every bar lands truncated and the
// chop audibly stumbles. Triplets (1/8T) divide evenly (12 per bar) and are
// what Deja Vu actually ships.
enum class LoopDivision
{
    Sixteenth, EighthTriplet, Eighth, Quarter, Half,
    OneBar, TwoBars, FourBars
};
