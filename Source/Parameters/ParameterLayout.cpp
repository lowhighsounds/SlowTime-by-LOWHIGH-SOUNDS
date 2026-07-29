#include "ParameterLayout.h"
#include "ParameterIDs.h"

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { ParamIDs::blocAEnabled, 1 }, "Bloc A Enabled", true));

    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { ParamIDs::blocBEnabled, 1 }, "Bloc B Enabled", true));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::pitchSemitones, 1 }, "Pitch (semitones)",
        NormalisableRange<float>(-12.0f, 12.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::fineCents, 1 }, "Fine Tune (cents)",
        NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::sourceFreqHz, 1 }, "Source Freq (Hz)",
        NormalisableRange<float>(220.0f, 880.0f, 0.1f), 440.0f));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { ParamIDs::algoMode, 1 }, "Mode",
        StringArray { "Smooth", "Transient", "Punchy" }, 1));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { ParamIDs::vibeMode, 1 }, "Vibe Mode",
        StringArray { "1.5x", "2x", "4x" }, 0));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { ParamIDs::loopDivision, 1 }, "Loop Division",
        StringArray { "1/16", "1/8T", "1/8", "1/4", "1/2",
                      "1 bar", "2 bars", "4 bars" }, 3));

    // Smooth: depth of Bloc B's per-window amplitude envelope, 0..100. At 0 the
    // chunks are flat and punchy (only a fixed few-ms click guard at the seam);
    // turning it up fades each window in at its start and out at its end, so the
    // chop "breathes" without one chunk's tail ever bleeding into the next.
    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::smooth, 1 }, "Smooth",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    // Band: band-pass on the WET signal only (low cut = high-pass, high cut =
    // low-pass). Defaults span the full range (effectively off). Interval 0 =
    // continuous, so the cutoff (and its Hz read-out) is precise, not snapped
    // to whole hertz.
    auto freqRange = NormalisableRange<float>(20.0f, 20000.0f, 0.0f);
    freqRange.setSkewForCentre(1000.0f);

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::bandLowCutHz, 1 }, "Band Low Cut", freqRange, 20.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::bandHighCutHz, 1 }, "Band High Cut", freqRange, 20000.0f));

    // Fade In/Out: engage/disengage time in tempo-synced bars (0 = instant).
    auto barRange = NormalisableRange<float>(0.0f, 16.0f, 0.01f);
    barRange.setSkewForCentre(2.0f);

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::fadeInBars, 1 }, "Fade In (bars)", barRange, 0.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::fadeOutBars, 1 }, "Fade Out (bars)", barRange, 0.0f));

    // Record arm for the play-to-pause capture (drag-to-DAW). On by default so
    // the capture behaves as before; the visor's red LED toggles it.
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { ParamIDs::recordEnabled, 1 }, "Record", true));

    // --- Bloc C: grid-synced reverse (own division/grid, independent of B) ---
    // Starts disabled so it doesn't surprise (A and B stay on by default).
    params.push_back(std::make_unique<AudioParameterBool>(
        ParameterID { ParamIDs::blocCEnabled, 1 }, "Bloc C Enabled", false));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { ParamIDs::blocCDivision, 1 }, "Reverse Division",
        StringArray { "1/16", "1/8T", "1/8", "1/4", "1/2",
                      "1 bar", "2 bars", "4 bars" }, 3));

    params.push_back(std::make_unique<AudioParameterChoice>(
        ParameterID { ParamIDs::blocCPattern, 1 }, "Reverse Pattern",
        StringArray { "Always", "Ping-Pong", "Every 2", "Every 4", "Random" }, 1));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::blocCChance, 1 }, "Reverse Chance",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 50.0f));

    params.push_back(std::make_unique<AudioParameterFloat>(
        ParameterID { ParamIDs::blocCSmooth, 1 }, "Reverse Smooth",
        NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    return { params.begin(), params.end() };
}
