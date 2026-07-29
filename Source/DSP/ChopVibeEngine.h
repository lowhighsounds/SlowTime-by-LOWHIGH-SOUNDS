#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

// Bloco B ("Vibe"): the Cymatics Deja Vu / Cableguys HalfTime effect --
// tape-style vari-speed (pitch AND time move together) that always stays
// locked to the host's bar grid.
//
// THE MODEL (confirmed against Deja Vu's documented behaviour):
//
//   * The LOOP window is a fixed reset interval locked to the host grid
//     (e.g. 1 bar). It NEVER changes with the speed setting. The caller
//     (PluginProcessor) decides exactly where each reset boundary lands,
//     using the host's real PPQ position.
//
//   * The speed ratio only decides HOW MUCH real audio is consumed inside
//     that window: captured = window / ratio. That fraction, played back at
//     the ratio's speed, fills the whole window before the next reset -- so
//     it's always perfectly in time, whatever the ratio.
//
// The crucial implementation detail -- and the thing every earlier attempt
// got wrong -- is WHICH audio gets played and in which direction:
//
//   At each reset boundary the read pointer ANCHORS TO THE LIVE INPUT
//   POSITION and moves FORWARD, slower than real time (for slowdown ratios).
//   Because it reads forward from "now" at less than 1x, it only ever needs
//   audio that has already arrived (zero latency, fully causal) and it NEVER
//   reaches back into stale pre-jump history -- so pressing play/seek can't
//   produce the "leftover from the last take" blip that a backward capture
//   of the previous division did. Each window therefore replays the FIRST
//   1/ratio of that window's own incoming audio, stretched to fill the bar
//   (this is exactly the orange-segment-at-the-start-of-the-window picture
//   in Deja Vu's own diagrams), pitched down by the ratio, resetting on the
//   grid. The result is the grid-locked "sampled tape" character.
//
//   Speed-up ratios (<1, our own extension beyond Deja Vu's slowdown-only
//   set) can't read forward faster than real time without needing the
//   future, so they instead anchor so the read CATCHES UP to live exactly at
//   the window's end -- i.e. they replay the most recent 1/ratio of audio
//   sped up. Still causal, still zero latency.
//
// A background ring buffer ("rolling") records the live signal continuously
// so the anchor always has real audio to read, engaged or not.
//
// Click safety: at each boundary the read pointer jumps, so a short
// equal-power crossfade blends the outgoing read into the freshly-anchored
// one. There is no in-window loop seam anymore -- each window is a single
// forward pass, not a wrapped loop.
class ChopVibeEngine
{
public:
    void prepare(double sampleRate, int numChannels, double maxCaptureSeconds);
    void reset();

    // Record the live signal into the rolling history. Call every block Bloc
    // B could produce output, BEFORE process()/resetToLiveBoundary so the
    // history is complete up to "now".
    void pushInput(const juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // ratio = stretch factor: 2.0 = half speed / octave down, 0.5 = double
    // speed / octave up. (Matches vibeModeToParams.)
    void setRatio(float ratio);

    // "Smooth" in [0, 1]: depth of the per-window amplitude envelope -- each
    // window fades IN at its start and OUT at its end, so consecutive chunks
    // meet at zero instead of overlapping. 0 = flat (punchy, hard edges, only
    // the fixed click-safe seam crossfade runs); 1 = the fade-in and fade-out
    // meet in the middle, one continuous swell per window.
    //
    // This deliberately does NOT lengthen the seam crossfade. Scaling the
    // crossfade with Smooth (the previous behaviour) meant the outgoing chunk
    // kept playing further and further into the incoming one, so at high
    // settings the tail of one chord audibly bled into the head of the next
    // and the whole thing turned to mush. Fading each window through zero
    // gives the same soft, wave-like transition with no content overlap; the
    // cost is a volume dip at the seam instead, which is the musical
    // "breathing" this kind of chop is supposed to have.
    void setSmooth(float amount);

    // When true, the NEXT window anchored by resetToLiveBoundary() plays
    // backwards: it anchors at the boundary and reads BACKWARD through the
    // recorded history, so the window replays the audio leading up to that
    // boundary end-first (the classic reverse). Reading backwards is strictly
    // safer than the forward model -- it only ever touches audio that has
    // already been recorded -- but it does require that history to exist, so
    // the caller must not enable it until a full window of fresh post-jump
    // audio has accumulated (see the freshness guard in PluginProcessor).
    void setReversed(bool shouldReverse);

    // Anchor a new window at a grid boundary. samplesBeforeNow is how many
    // input samples back from the current write head that boundary sits (0 =
    // right at "now"); windowLengthSamples is the fixed LOOP length in
    // samples. hardSnap = true skips the boundary crossfade (use on a
    // transport jump / first engage, where there's no meaningful outgoing
    // audio to blend from).
    void resetToLiveBoundary(int samplesBeforeNow, int windowLengthSamples, bool hardSnap);

    // Discard rolling history (e.g. on a transport jump) so no pre-jump audio
    // can ever be read. The forward-reading slowdown model doesn't strictly
    // need this, but it also guards the speed-up (backward-reading) modes.
    void clearHistory();

    // Writes the vari-speed output into buffer[startSample, startSample+numSamples).
    void process(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

private:
    juce::AudioBuffer<float> rolling;
    int rollingSize = 0;
    juce::int64 rollingWriteHead = 0;

    // Absolute read positions into the input stream (same coordinate system
    // as rollingWriteHead). Kept as doubles for fractional vari-speed reads.
    double readHead = 0.0;
    double prevReadHead = 0.0;   // outgoing chunk during a wet-to-wet crossfade
    double prevReadIncrement = 0.0; // outgoing chunk's advance/direction during the seam blend
    double dryReadHead = 0.0;    // real-speed dry read during a dry-bleed crossfade
    int crossfadeRemaining = 0;
    int crossfadeLength = 0;
    bool crossfadeDryBleed = false;
    bool hasActiveWindow = false;

    int windowLength = 0;   // current LOOP length in samples
    int windowPos = 0;      // output samples since the last boundary (envelope phase)

    bool reverseRequested = false;  // set by setReversed(), latched per window
    bool windowReversed = false;    // whether the CURRENT window plays backward

    float currentRatio = 1.0f;
    float smoothAmount = 0.0f;   // [0, 1], see setSmooth
    int channels = 2;
    double sampleRate = 44100.0;

    int minCrossfadeSamples = 1;

    int ringIndex(juce::int64 absolutePosition) const;
    float readRing(int channel, double absolutePosition) const;
    float windowEnvelopeGain(int posInWindow, int length) const;
};
