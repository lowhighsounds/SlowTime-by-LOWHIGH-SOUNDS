#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "RetroLookAndFeel.h"
#include <vector>
#include <algorithm>
#include <cmath>

// A bordered panel with a classic-Mac title bar: centred title flanked by the
// striped "grip" lines. Children are added by the editor and positioned within
// getContentBounds().
class TitledPanel : public juce::Component
{
public:
    explicit TitledPanel(juce::String titleText) : title(std::move(titleText)) {}

    static constexpr int titleHeight = 24;

    juce::Rectangle<int> getContentBounds() const
    {
        return getLocalBounds().withTrimmedTop(titleHeight).reduced(9);
    }

    void paint(juce::Graphics& g) override
    {
        // FLAT panel: one solid fill + a thick outline (its solid cast shadow
        // is drawn by the ContentPanel behind it). No gradient/bevel.
        auto b = getLocalBounds().toFloat().reduced(1.0f);
        g.setColour(retro::panelBase);
        g.fillRoundedRectangle(b, 5.0f);
        g.setColour(retro::outlineColour);
        g.drawRoundedRectangle(b, 5.0f, 2.0f);

        // Title bar: solid divider + label, with the striped "grip" marks.
        auto bar = getLocalBounds().removeFromTop(titleHeight);
        auto f = retro::labelFont(13.0f);
        g.setFont(f);
        const int tw = juce::GlyphArrangement::getStringWidthInt(f, title) + 18;
        juce::Rectangle<int> textRect(bar.getCentreX() - tw / 2, bar.getY(), tw, bar.getHeight());

        drawStripes(g, bar.withRight(textRect.getX()).reduced(8, 0));
        drawStripes(g, bar.withLeft(textRect.getRight()).reduced(8, 0));

        g.setColour(retro::outlineColour);
        g.drawText(title, textRect, juce::Justification::centred, false);
        g.drawHorizontalLine(titleHeight, b.getX() + 2.0f, b.getRight() - 2.0f);
    }

private:
    static void drawStripes(juce::Graphics& g, juce::Rectangle<int> area)
    {
        if (area.getWidth() < 8) return;
        g.setColour(retro::ink.withAlpha(0.85f));
        const int lines = 4;
        const int cy = area.getCentreY();
        for (int i = 0; i < lines; ++i)
        {
            const int y = cy - (lines - 1) + i * 2;
            g.drawHorizontalLine(y, (float) area.getX(), (float) area.getRight());
        }
    }

    juce::String title;
};

// A purely decorative "MODE" caption with a circuit-trace connector: the label
// sits centred at the top, a stem drops to a horizontal bus, and the bus fans
// out into N equally-spaced drops that line up with the N segment buttons below
// (stopping just short of them, never touching). Non-interactive.
class BranchLabel : public juce::Component
{
public:
    BranchLabel(juce::String labelText, int branches)
        : label(std::move(labelText)), n(juce::jmax(1, branches))
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();

        // Label centred at the top.
        const float labelH = 14.0f;
        auto labelRect = r.removeFromTop(labelH);
        g.setFont(retro::labelFont(10.5f));
        g.setColour(retro::ink.withAlpha(0.85f));
        g.drawText(label, labelRect.toNearestInt(), juce::Justification::centred, false);

        // Circuit trace below the label.
        g.setColour(retro::ink.withAlpha(0.5f));
        const float cx     = r.getCentreX();
        const float top    = r.getY();
        const float busY   = r.getY() + r.getHeight() * 0.5f;
        const float bottom = r.getBottom() - 1.0f;   // stops short of the strip
        const float lw     = 1.2f;

        const float colW   = r.getWidth() / (float) n;
        const float firstX = r.getX() + colW * 0.5f;
        const float lastX  = r.getX() + colW * ((float) n - 0.5f);

        g.drawLine(cx, top, cx, busY, lw);           // stem: label -> bus
        g.drawLine(firstX, busY, lastX, busY, lw);   // horizontal bus
        for (int i = 0; i < n; ++i)                  // drop to each column centre
        {
            const float x = r.getX() + colW * ((float) i + 0.5f);
            g.drawLine(x, busY, x, bottom, lw);
        }
    }

private:
    juce::String label;
    int n;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BranchLabel)
};

// A horizontal row of segment buttons bound to an AudioParameterChoice: click a
// segment to set the choice; the active segment draws inverted. Uses the
// low-level ParameterAttachment so it works with a plain choice parameter.
class ChoiceButtonStrip : public juce::Component
{
public:
    ChoiceButtonStrip(juce::RangedAudioParameter& parameter, const juce::StringArray& labels)
    {
        for (int i = 0; i < labels.size(); ++i)
        {
            auto* button = buttons.add(new juce::TextButton(labels[i]));
            button->setClickingTogglesState(false);
            button->onClick = [this, i] { selectIndex(i); };
            addAndMakeVisible(button);
        }

        attachment = std::make_unique<juce::ParameterAttachment>(
            parameter, [this](float v) { reflect((int) std::round(v)); });
        attachment->sendInitialUpdate();
    }

    void resized() override
    {
        auto r = getLocalBounds();
        const int n = buttons.size();
        if (n == 0) return;
        const int w = r.getWidth() / n;
        for (int i = 0; i < n; ++i)
            buttons[i]->setBounds(i == n - 1 ? r : r.removeFromLeft(w));
    }

private:
    void selectIndex(int i)
    {
        attachment->setValueAsCompleteGesture((float) i);
        reflect(i);
    }

    void reflect(int i)
    {
        for (int k = 0; k < buttons.size(); ++k)
            buttons[k]->setToggleState(k == i, juce::dontSendNotification);
    }

    juce::OwnedArray<juce::TextButton> buttons;
    std::unique_ptr<juce::ParameterAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChoiceButtonStrip)
};

// A CRT-visor selector for a choice parameter with a "coverflow" feel: the
// selected value sits big, lit and in front at the centre; the previous/next
// values are smaller, dimmer and set a little behind (depth), just off centre.
// Selecting slides the values across so the new one glides to the centre-front.
class CarouselSelector : public juce::Component,
                         private juce::Timer
{
public:
    CarouselSelector(juce::RangedAudioParameter& parameter, juce::StringArray itemLabels)
        : param(parameter), labels(std::move(itemLabels))
    {
        attachment = std::make_unique<juce::ParameterAttachment>(
            param, [this](float v) { moveTo(juce::roundToInt(v)); });
        attachment->sendInitialUpdate();
        animOffset = 0.0f; // no slide on the very first value
    }

    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced(1.0f);
        retro::drawScreenRecess(g, r, 3.0f);

        const juce::Colour led = retro::ledFor(this, juce::Colour(0xff9be34f));
        const int last = labels.size() - 1;
        const float centreX = getWidth() * 0.5f;
        const float centreY = getHeight() * 0.5f;
        const float spacing = getWidth() * 0.30f; // neighbours kept close, not at the edges

        {
            juce::Graphics::ScopedSaveState save(g);
            juce::Path clip; clip.addRoundedRectangle(r, 3.0f);
            g.reduceClipRegion(clip);

            // Collect visible items, draw far (more "behind") first so the
            // centre one ends up in front.
            struct Item { int i; float pos, dist; };
            std::vector<Item> items;
            for (int slot = -2; slot <= 2; ++slot)
            {
                const int i = index + slot;
                if (i < 0 || i > last) continue;
                const float pos = (float) slot + animOffset;
                items.push_back({ i, pos, std::abs(pos) });
            }
            std::sort(items.begin(), items.end(),
                      [](const Item& a, const Item& b) { return a.dist > b.dist; });

            for (auto& it : items)
            {
                const float x = centreX + it.pos * spacing;
                const float scale = juce::jmax(0.5f, 1.0f - 0.42f * it.dist);   // smaller = further back
                const float alpha = juce::jlimit(0.0f, 1.0f, 1.0f - 0.72f * it.dist);
                if (alpha <= 0.02f) continue;

                const juce::Rectangle<int> box(juce::roundToInt(x - spacing * 0.5f),
                                               juce::roundToInt(centreY - 12.0f),
                                               juce::roundToInt(spacing), 24);
                const auto f = retro::font(15.5f * scale);
                if (it.dist < 0.45f)
                    retro::drawGlowText(g, labels[it.i], box, led.withAlpha(alpha), f);
                else
                {
                    g.setColour(led.withAlpha(alpha));
                    g.setFont(f);
                    g.drawText(labels[it.i], box, juce::Justification::centred, false);
                }
            }
        }

        drawArrow(g, getLocalBounds().removeFromLeft(18).toFloat(), true,  index > 0, led);
        drawArrow(g, getLocalBounds().removeFromRight(18).toFloat(), false, index < last, led);

        retro::drawScreenGloss(g, r, 3.0f);
    }

    void mouseDown(const juce::MouseEvent& e) override
    {
        if (e.x < 20) step(-1);
        else if (e.x > getWidth() - 20) step(1);
    }

    void mouseWheelMove(const juce::MouseEvent&, const juce::MouseWheelDetails& w) override
    {
        if (w.deltaY != 0.0f) step(w.deltaY > 0.0f ? 1 : -1);
    }

private:
    void step(int delta)
    {
        const int ni = juce::jlimit(0, labels.size() - 1, index + delta);
        if (ni != index)
        {
            attachment->setValueAsCompleteGesture((float) ni);
            moveTo(ni);
        }
    }

    void moveTo(int newIndex)
    {
        if (newIndex == index) return;
        animOffset += (float) (newIndex - index); // start shifted, slide back to 0
        index = newIndex;
        startTimerHz(60);
        repaint();
    }

    void timerCallback() override
    {
        animOffset *= 0.72f; // ease-out toward the settled position
        if (std::abs(animOffset) < 0.01f)
        {
            animOffset = 0.0f;
            stopTimer();
        }
        repaint();
    }

    static void drawArrow(juce::Graphics& g, juce::Rectangle<float> a, bool pointLeft,
                          bool enabled, juce::Colour led)
    {
        const auto c = a.getCentre();
        const float s = 4.0f;
        juce::Path p;
        if (pointLeft) p.addTriangle(c.x + s, c.y - 5.0f, c.x + s, c.y + 5.0f, c.x - s, c.y);
        else           p.addTriangle(c.x - s, c.y - 5.0f, c.x - s, c.y + 5.0f, c.x + s, c.y);
        g.setColour(led.withAlpha(enabled ? 0.9f : 0.22f));
        g.fillPath(p);
    }

    juce::RangedAudioParameter& param;
    juce::StringArray labels;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    int index = 0;
    float animOffset = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CarouselSelector)
};

// A small fade-in/out ramp etched into the panel surface that fills with light
// (glowing LED, no dark box) in proportion to its parameter -- as if the
// surface were cut out and light shone through.
class FadeGlyph : public juce::Component
{
public:
    FadeGlyph(juce::RangedAudioParameter& parameter, bool isFadeIn)
        : param(parameter), fadeIn(isFadeIn)
    {
        setInterceptsMouseClicks(false, false);
        attachment = std::make_unique<juce::ParameterAttachment>(
            param, [this](float v) { value = param.convertTo0to1(v); repaint(); });
        attachment->sendInitialUpdate();
    }

    void paint(juce::Graphics& g) override
    {
        // Segmented LED bar-graph in a ramp shape, etched into the surface (no
        // dark box) -- lit segments glow green with depth; unlit ones are faint
        // grooves. Fade-in and fade-out are true horizontal mirrors: fade-in's
        // tall end and fill are on the right, fade-out's on the left.
        auto r = getLocalBounds().toFloat().reduced(2.0f);
        const int segments = 9;
        const float gap = 1.4f;
        const float segW = (r.getWidth() - (segments - 1) * gap) / (float) segments;
        const float fullH = r.getHeight();

        for (int i = 0; i < segments; ++i)
        {
            const float frac = (i + 0.5f) / (float) segments;           // 0..1 across width
            const float ramp = fadeIn ? frac : (1.0f - frac);           // bar height ratio
            const float h = juce::jmax(2.0f, fullH * (0.18f + 0.82f * ramp));
            const float x = r.getX() + i * (segW + gap);
            juce::Rectangle<float> seg(x, r.getBottom() - h, segW, h);

            const bool lit = fadeIn ? (frac <= value) : (frac >= 1.0f - value);

            // FLAT LED segment: solid mint if lit, solid light-grey if not. The
            // lit mint dims when the owning block is switched off.
            g.setColour(lit ? retro::ledFor(this, retro::ledOn) : retro::ledOff);
            g.fillRoundedRectangle(seg, 1.0f);
        }
    }

private:
    juce::RangedAudioParameter& param;
    std::unique_ptr<juce::ParameterAttachment> attachment;
    bool fadeIn;
    float value = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FadeGlyph)
};

// A round LED toggle: lit (green, glowing, raised) when on, dark and recessed
// when off. Bind with an APVTS ButtonAttachment like any toggle button.
class LedToggle : public juce::Button
{
public:
    explicit LedToggle(juce::Colour onColour = retro::ledOn)
        : juce::Button("led"), onCol(onColour) { setClickingTogglesState(true); }

    void paintButton(juce::Graphics& g, bool, bool) override
    {
        // FLAT LED: solid cast shadow + solid lamp (lit colour on / grey off) + outline.
        auto b = getLocalBounds().toFloat();
        const float d = juce::jmin(b.getWidth(), b.getHeight()) - 2.0f;
        auto c = juce::Rectangle<float>(d, d).withCentre(b.getCentre());
        const bool on = getToggleState();

        g.setColour(retro::shadowFlat);
        g.fillEllipse(c.translated(retro::shadowOffsetX * 0.6f, retro::shadowOffsetY * 0.6f));
        g.setColour(on ? onCol : retro::ledOff);
        g.fillEllipse(c);
        g.setColour(retro::outlineColour);
        g.drawEllipse(c, 1.6f);
    }

private:
    juce::Colour onCol;

public:

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LedToggle)
};

// Dual-thumb band filter: one bar with a draggable low-cut and high-cut, the
// lit passband between them, LOW CUT / HIGH CUT labels and a centre read-out of
// the frequency last moved.
class BandFilter : public juce::Component
{
public:
    BandFilter(juce::RangedAudioParameter& lowParam, juce::RangedAudioParameter& highParam)
    {
        slider.setSliderStyle(juce::Slider::TwoValueHorizontal);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        slider.setRange(20.0, 20000.0);
        slider.setSkewFactorFromMidPoint(1000.0);
        addAndMakeVisible(slider);

        lowAtt = std::make_unique<juce::ParameterAttachment>(lowParam, [this](float v)
        {
            slider.setMinValue(v, juce::dontSendNotification); lastFreq = v; repaint();
        });
        highAtt = std::make_unique<juce::ParameterAttachment>(highParam, [this](float v)
        {
            slider.setMaxValue(v, juce::dontSendNotification); repaint();
        });
        lowAtt->sendInitialUpdate();
        highAtt->sendInitialUpdate();
        lastLow = slider.getMinValue();
        lastHigh = slider.getMaxValue();

        slider.onValueChange = [this]
        {
            if (slider.getMinValue() != lastLow)  { lowAtt->setValueAsCompleteGesture((float) slider.getMinValue());  lastFreq = (float) slider.getMinValue(); }
            if (slider.getMaxValue() != lastHigh) { highAtt->setValueAsCompleteGesture((float) slider.getMaxValue()); lastFreq = (float) slider.getMaxValue(); }
            lastLow = slider.getMinValue();
            lastHigh = slider.getMaxValue();
            repaint();
        };
    }

    void resized() override
    {
        auto r = getLocalBounds();
        labelRow = r.removeFromBottom(15);
        slider.setBounds(r.reduced(4, 2));
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(retro::ink.withAlpha(0.8f));
        g.setFont(retro::font(9.5f));
        g.drawText("LOW CUT", labelRow.reduced(4, 0), juce::Justification::centredLeft, false);
        g.drawText("HIGH CUT", labelRow.reduced(4, 0), juce::Justification::centredRight, false);

        g.setColour(retro::ink);
        g.setFont(retro::font(12.0f));
        g.drawText(formatFreq(lastFreq), labelRow, juce::Justification::centred, false);
    }

private:
    static juce::String formatFreq(float hz)
    {
        // More decimals where they matter (low end), fewer up high.
        const int decimals = hz < 100.0f ? 2 : (hz < 1000.0f ? 1 : 0);
        return juce::String(hz, decimals) + " Hz";
    }

    juce::Slider slider { juce::Slider::TwoValueHorizontal, juce::Slider::NoTextBox };
    std::unique_ptr<juce::ParameterAttachment> lowAtt, highAtt;
    juce::Rectangle<int> labelRow;
    double lastLow = 20.0, lastHigh = 20000.0;
    float lastFreq = 20.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BandFilter)
};
