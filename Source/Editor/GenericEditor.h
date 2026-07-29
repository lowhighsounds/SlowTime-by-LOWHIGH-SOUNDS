#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

// Minimal generic editor mirroring JUCE's own GenericAudioProcessorEditor
// (one row per apvts parameter: label + control), but explicitly enabling
// mouse scroll-wheel on every Slider/ComboBox, and binding via standard APVTS
// attachments. Kept intentionally plain -- bespoke visual design is deferred
// to the final "custom UI" milestone.
class GenericEditor : public juce::AudioProcessorEditor
{
public:
    GenericEditor(juce::AudioProcessor& processorToEdit,
                  juce::AudioProcessorValueTreeState& stateToEdit);
    ~GenericEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    // Mounts a component in a reserved strip above the parameter list (used by
    // the real editor to place the waveform / drag panel on top of the plain
    // parameter rows). Grows the window by `height`.
    void setTopComponent(juce::Component* component, int height);

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    struct ParamRow
    {
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> combo;
        std::unique_ptr<juce::ToggleButton> toggle;

        std::unique_ptr<APVTS::SliderAttachment> sliderAttachment;
        std::unique_ptr<APVTS::ComboBoxAttachment> comboAttachment;
        std::unique_ptr<APVTS::ButtonAttachment> buttonAttachment;

        juce::Component* control() const;
    };

    APVTS& apvts;
    juce::Viewport viewport;
    juce::Component content;
    std::vector<std::unique_ptr<ParamRow>> rows;

    juce::Component* topComponent = nullptr;
    int topComponentHeight = 0;

    static constexpr int rowHeight = 32;
    static constexpr int rowPadding = 4;
    static constexpr int labelWidth = 160;
    static constexpr int contentWidth = 480;

    void buildRows();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GenericEditor)
};
