#pragma once

namespace DspConstants
{
    inline constexpr double kMinSupportedBpm = 40.0;
    inline constexpr int kMaxSupportedNumerator = 7;
    inline constexpr int kMaxLoopBars = 4;

    // Longest play-to-pause capture kept for the drag-to-DAW feature. Bounds
    // the pre-allocated record buffer (stereo, 32-bit): 180 s ~ 63 MB.
    inline constexpr double kMaxRecordSeconds = 180.0;

    // How far the host's reported PPQ may drift from the position continuous
    // playback predicts before Bloc B treats it as a real transport jump.
    // This has to sit well ABOVE normal host PPQ jitter and well BELOW any
    // genuine seek/loop. The previous value (0.001 qn ~ 19 samples, under 4%
    // of one 512-sample block) was far too tight: routine jitter tripped it
    // every block, so Bloc B re-anchored at the BUFFER rate instead of the
    // musical division -- which made every LOOP/MODE combination collapse to
    // the same buffer-rate stutter (only pitch differing), click on every
    // hard snap, and skip Smooth entirely. A real seek/loop moves at least a
    // beat (1.0 qn), so a 1/16-note floor still catches all of them.
    inline constexpr double kTransportJumpToleranceQn = 0.25;
    // Bloc B "Smooth" reset-boundary crossfade. Even at Smooth = 0 a tiny
    // crossfade runs so the read-pointer jump can't click; toward 1.0 it
    // grows up to kSmoothMaxWindowFraction of the LOOP window so successive
    // chunks visibly overlap.
    // Fixed, short seam crossfade at every Bloc B reset. This is purely a click
    // guard for the read-pointer jump and is NOT scaled by Smooth: Smooth is a
    // per-window amplitude envelope instead (see ChopVibeEngine::setSmooth), so
    // consecutive chunks meet at zero rather than overlapping and smearing into
    // each other.
    inline constexpr float kSmoothMinCrossfadeSeconds = 0.004f;
    inline constexpr float kModeSwitchCrossfadeSeconds = 0.02f;
    inline constexpr float kParamSmoothingSeconds = 0.02f;

    // Standard concert pitch reference, used as the fixed "destination" for
    // the Source Freq retune calculator (see PluginProcessor::processBlock).
    inline constexpr float kStandardConcertPitchHz = 440.0f;

    // Bloc A "Mode" (Smooth/Transient/Punchy): per Waves SoundShifter's own
    // manual, this is a transient-preservation character ladder, not a
    // window-size/quality tradeoff -- the STFT window (presetDefault,
    // ~120ms/30ms) is identical across all three modes. Character comes
    // solely from the tonality limit passed to setTransposeSemitones() (a
    // dry/wet blend at detected transients was tried and reverted: it
    // reintroduces the original UN-shifted pitch, which audibly clashes with
    // the shifted signal whenever pitchSemitones != 0 -- not viable for a
    // pitch shifter, only for a pure time-stretcher with no pitch change).

    // Hz threshold above which Signalsmith's frequency mapping switches from
    // multiplicative (pitch-correct) to additive (phase-stable); 0 disables
    // the limit entirely (Smooth).
    inline constexpr float kTonalityLimitHzSmooth = 0.0f;
    inline constexpr float kTonalityLimitHzTransient = 6000.0f;
    inline constexpr float kTonalityLimitHzPunchy = 3000.0f;
}
