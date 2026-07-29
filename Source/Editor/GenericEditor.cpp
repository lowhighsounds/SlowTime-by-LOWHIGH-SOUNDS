#include "GenericEditor.h"

juce::Component* GenericEditor::ParamRow::control() const
{
    if (slider) return slider.get();
    if (combo) return combo.get();
    if (toggle) return toggle.get();
    return nullptr;
}

GenericEditor::GenericEditor(juce::AudioProcessor& processorToEdit, APVTS& stateToEdit)
    : juce::AudioProcessorEditor(processorToEdit), apvts(stateToEdit)
{
    setOpaque(true);
    addAndMakeVisible(viewport);
    viewport.setViewedComponent(&content, false);
    viewport.setScrollBarsShown(true, false);

    buildRows();

    const int totalHeight = static_cast<int>(rows.size()) * (rowHeight + rowPadding);
    content.setSize(contentWidth, totalHeight);

    setResizable(true, false);
    setSize(contentWidth + viewport.getScrollBarThickness(),
            juce::jlimit(150, 600, totalHeight));
}

GenericEditor::~GenericEditor() = default;

void GenericEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void GenericEditor::setTopComponent(juce::Component* component, int height)
{
    topComponent = component;
    topComponentHeight = height;
    if (component != nullptr)
        addAndMakeVisible(*component);

    setSize(getWidth(), getHeight() + height); // triggers resized()
}

void GenericEditor::resized()
{
    auto bounds = getLocalBounds();
    if (topComponent != nullptr)
        topComponent->setBounds(bounds.removeFromTop(topComponentHeight));
    viewport.setBounds(bounds);

    auto area = content.getLocalBounds();
    for (auto& row : rows)
    {
        auto rowArea = area.removeFromTop(rowHeight);
        area.removeFromTop(rowPadding);

        row->label.setBounds(rowArea.removeFromLeft(labelWidth));
        if (auto* c = row->control())
            c->setBounds(rowArea.reduced(4, 2));
    }
}

void GenericEditor::buildRows()
{
    for (auto* param : apvts.processor.getParameters())
    {
        auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(param);
        if (ranged == nullptr)
            continue; // not one of ours -- skip defensively

        auto row = std::make_unique<ParamRow>();
        row->label.setText(ranged->getName(128), juce::dontSendNotification);
        row->label.setJustificationType(juce::Justification::centredRight);
        content.addAndMakeVisible(row->label);

        if (auto* boolParam = dynamic_cast<juce::AudioParameterBool*>(ranged))
        {
            juce::ignoreUnused(boolParam);
            row->toggle = std::make_unique<juce::ToggleButton>();
            content.addAndMakeVisible(*row->toggle);
            row->buttonAttachment = std::make_unique<APVTS::ButtonAttachment>(
                apvts, ranged->paramID, *row->toggle);
        }
        else if (auto* choiceParam = dynamic_cast<juce::AudioParameterChoice*>(ranged))
        {
            row->combo = std::make_unique<juce::ComboBox>();
            row->combo->addItemList(choiceParam->choices, 1);
            row->combo->setScrollWheelEnabled(true);
            content.addAndMakeVisible(*row->combo);
            row->comboAttachment = std::make_unique<APVTS::ComboBoxAttachment>(
                apvts, ranged->paramID, *row->combo);
        }
        else
        {
            row->slider = std::make_unique<juce::Slider>(
                juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight);
            row->slider->setScrollWheelEnabled(true);
            content.addAndMakeVisible(*row->slider);
            row->sliderAttachment = std::make_unique<APVTS::SliderAttachment>(
                apvts, ranged->paramID, *row->slider);
        }

        rows.push_back(std::move(row));
    }
}
