#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "Editor/RetroLookAndFeel.h"
#include "Editor/RetroComponents.h"
#include "Editor/WaveformDragDisplay.h"

// A rotary knob with a caption underneath, bound to an APVTS parameter.
struct RetroKnob : public juce::Component
{
    RetroKnob(juce::AudioProcessorValueTreeState&, const juce::String& paramID,
              const juce::String& captionText);
    void resized() override;

    // Draw the studio-style value tick arc around this knob (lit up to value).
    void setTickArc(bool on) { slider.getProperties().set("tickArc", on); }

    juce::Slider slider { juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox };
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
};

// A console-style vertical fader: caption on top, fader in the middle, and a
// small dark LED-style readout box with red digits at the bottom.
struct RetroFader : public juce::Component
{
    RetroFader(juce::AudioProcessorValueTreeState&, const juce::String& paramID,
               const juce::String& captionText, int decimals);
    void resized() override;
    void paint(juce::Graphics&) override;
    void updateReadout();

    juce::Slider slider { juce::Slider::LinearVertical, juce::Slider::NoTextBox };
    juce::Label caption;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    int decimals = 0;
    juce::String readoutText;
};

// Draws the top title bar: the SlowTime plugin logo on the left, the LowHigh
// Sounds brand lockup on the right (its own component so it scales with the
// rest of the content).
struct HeaderBar : public juce::Component
{
    void paint(juce::Graphics&) override;
};

// Holds & scales all UI; paints the paper background plus soft drop shadows
// behind every raised element for depth.
struct ContentPanel : public juce::Component
{
    void paint(juce::Graphics&) override;
};

// Custom editor: retro monochrome look, three block panels (Pitch / Vibe /
// Reverse) + waveform/drag display + a global Output strip, all on one screen.
// The whole thing is laid out at a fixed design size inside `content` and then
// uniformly scaled to the window (fixed aspect ratio) so it stays crisp and
// on-design at any size within the resize limits.
class SlowTimeAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit SlowTimeAudioProcessorEditor(SlowTimeAudioProcessor&);
    ~SlowTimeAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    static constexpr int designWidth = 940;
    static constexpr int designHeight = 600;

    void layoutContent(); // positions everything in the design-size coordinate space

    LedToggle*        makeEnable(TitledPanel&, const juce::String& paramID);
    juce::ComboBox*   makeCombo(TitledPanel&, const juce::String& paramID);
    juce::Label*      makeCaption(TitledPanel&, const juce::String& text);
    RetroKnob*        makeKnob(TitledPanel&, const juce::String& paramID, const juce::String& caption);
    RetroFader*       makeFader(TitledPanel&, const juce::String& paramID, const juce::String& caption, int decimals);

    SlowTimeAudioProcessor& processorRef;
    juce::AudioProcessorValueTreeState& apvts;
    RetroLookAndFeel lnf;

    ContentPanel content;      // holds & scales everything (draws shadows)
    HeaderBar header;
    TitledPanel panelA { "PITCH" }, panelB { "LOOP" }, panelC { "REVERSE" }, panelOut { "BAND" };
    WaveformDragDisplay waveform;

    std::unique_ptr<ChoiceButtonStrip> modeStrip, vibeStrip, patternStrip;
    std::unique_ptr<BranchLabel> modeBranch, vibeBranch, patternBranch;
    juce::Label* loopDivCap = nullptr;
    juce::Label* revDivCap = nullptr;
    std::unique_ptr<CarouselSelector> divBCarousel, divCCarousel;
    std::unique_ptr<FadeGlyph> fadeInGlyph, fadeOutGlyph;

    juce::OwnedArray<LedToggle> enables;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> enableAtts;
    // Mirror each block's enable onto its panel's "blockLit" property so every
    // LED-like element inside dims together when the block is switched off.
    juce::OwnedArray<juce::ParameterAttachment> blockLitAtts;
    void linkBlockLit(TitledPanel& panel, const juce::String& enableParamID);
    std::unique_ptr<BandFilter> bandFilter;
    juce::OwnedArray<juce::ComboBox> combos;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAtts;
    juce::OwnedArray<juce::Label> captions;
    juce::OwnedArray<RetroKnob> knobs;
    juce::OwnedArray<RetroFader> faders;

    // Named handles for layout.
    LedToggle* enaA = nullptr; LedToggle* enaB = nullptr; LedToggle* enaC = nullptr;
    RetroFader* semitonesF = nullptr; RetroFader* centsF = nullptr; RetroFader* freqF = nullptr;
    RetroKnob* smoothBK = nullptr;
    RetroKnob* chanceK = nullptr; RetroKnob* smoothCK = nullptr;
    RetroKnob* fadeInK = nullptr; RetroKnob* fadeOutK = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlowTimeAudioProcessorEditor)
};
