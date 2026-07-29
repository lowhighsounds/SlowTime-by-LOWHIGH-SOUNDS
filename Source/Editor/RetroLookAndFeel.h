#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

// Monochrome "retro computer / classic Mac System" look: near-black ink on
// off-white paper, 1px-ish borders, pixel-ish monospaced type, knobs ringed
// with tick dots, buttons that invert (fill black, white text) when active.
// A few muted pastels are reserved for accents/meters. This is our own take on
// that general aesthetic -- not a clone of any specific product.
namespace retro
{
    // FLAT / CEL-SHADED palette (from the reference). NO gradients anywhere:
    // every surface is a single solid colour; depth comes from hard-edged solid
    // shadow shapes + thick outlines + flat highlight shapes.
    const juce::Colour panelBase     { 0xff9c9c9c }; // main panel, mid grey
    const juce::Colour panelInset    { 0xff6b6b6b }; // recessed sections / dividers
    const juce::Colour shadowFlat    { 0xff5a5a5a }; // solid cast shadow
    const juce::Colour blackComp     { 0xff141414 }; // knob / button bodies
    const juce::Colour highlightFlat { 0xffd8d8d8 }; // flat reflection strip
    const juce::Colour screenBg      { 0xff101010 }; // display background
    const juce::Colour ledOn         { 0xff3dffa0 }; // mint green -- LED lit
    const juce::Colour ledOff        { 0xffb0afa8 }; // light grey -- LED off (never dark)
    const juce::Colour accentAlert   { 0xffff4433 }; // red -- only rare rec/alert
    const juce::Colour outlineColour { 0xff0a0a0a }; // near-black outline

    // Single, shared light direction: shadows always fall down-right.
    constexpr float shadowOffsetX = 3.0f;
    constexpr float shadowOffsetY = 3.0f;

    // Back-compat aliases so existing component code maps onto the flat palette.
    const juce::Colour ink        = outlineColour;
    const juce::Colour paper      = panelBase;
    const juce::Colour paperHi    = highlightFlat;
    const juce::Colour paperLo    = panelInset;
    const juce::Colour paperShade = panelInset;
    const juce::Colour bezel      = panelInset;
    const juce::Colour screen     = screenBg;
    const juce::Colour screenTop  = screenBg;
    const juce::Colour screenLine = shadowFlat;
    const juce::Colour knobHi     = highlightFlat;
    const juce::Colour knobLo     = blackComp;
    const juce::Colour amber      = accentAlert;
    const juce::Colour green      = ledOn;
    const juce::Colour red        = accentAlert;
    const juce::Colour lavender   = ledOff;
    const juce::Colour salmon     = accentAlert;
    const juce::Colour steel      = ledOff;

    // --- Per-block LED dimming ---------------------------------------------
    // When a block (PITCH/VIBE/REVERSE) is switched off, every LED-like element
    // inside it should read as inactive. The editor tags each block's panel with
    // a "blockLit" property (bool); LED elements walk up the component tree to
    // find it and pick a dimmed colour when the owning block is off.

    // A dimmer, desaturated version of a lit LED colour.
    inline juce::Colour ledDimmed(juce::Colour lit)
    {
        return lit.withMultipliedSaturation(0.32f).withMultipliedBrightness(0.5f);
    }

    // Is the block owning this component lit? Walks up to the nearest ancestor
    // carrying a "blockLit" property; absent (e.g. BAND) => always lit.
    inline bool blockLit(const juce::Component* c)
    {
        for (; c != nullptr; c = c->getParentComponent())
        {
            const auto& props = c->getProperties();
            if (props.contains("blockLit"))
                return (bool) props["blockLit"];
        }
        return true;
    }

    // Lit colour when the owning block is on, dimmed when off.
    inline juce::Colour ledFor(const juce::Component* c, juce::Colour lit)
    {
        return blockLit(c) ? lit : ledDimmed(lit);
    }

    // Space Mono (embedded via BinaryData). Defined in the .cpp so it can reach
    // the binary resources.
    juce::Font font(float height, bool bold = true);

    // LowHigh Sounds brand assets (embedded transparent PNGs). Shared, kept alive
    // for the editor's lifetime like the fonts.
    const juce::Image& brandMark();
    const juce::Image& brandWordmark();
    const juce::Image& pluginLogo();

    // A recessed dark "CRT" panel: gradient body, corner vignette, scanlines and
    // an inset border. Draw content on top, then drawScreenGloss last.
    void drawScreenRecess(juce::Graphics&, juce::Rectangle<float> area, float corner = 3.0f);
    // Glass reflection sheen across the top -- call AFTER drawing screen content.
    void drawScreenGloss(juce::Graphics&, juce::Rectangle<float> area, float corner = 3.0f);
    // Text with a soft LED bloom (halo passes + crisp core).
    void drawGlowText(juce::Graphics&, const juce::String& text, juce::Rectangle<int> area,
                      juce::Colour colour, const juce::Font&,
                      juce::Justification = juce::Justification::centred);

    // Subtle cached film grain over an area (material feel). Cheap: one static
    // tiled image, drawn at a low alpha.
    void drawNoise(juce::Graphics&, juce::Rectangle<int> area, float alpha);

    // Monospace label font with slight positive letter-spacing ("laser-etched"
    // look for section titles / captions).
    inline juce::Font labelFont(float height)
    {
        auto f = font(height);
        f.setExtraKerningFactor(0.10f);
        return f;
    }
}

// Shared UI resources (embedded fonts + cached grain image). Held via
// SharedResourcePointer so they're released on the message thread when the last
// editor closes -- NOT as function-static objects, which get torn down during
// DLL unload (when a host removes the plugin) and crash. Ctor defined in the
// .cpp where BinaryData is available.
struct RetroResources
{
    RetroResources();
    juce::Typeface::Ptr fontRegular, fontBold;
    juce::Image noise;
    juce::Image brandMark;     // LowHigh Sounds isometric monogram
    juce::Image brandWordmark; // "LOWHIGH SOUNDS" wordmark
    juce::Image pluginLogo;    // "SlowTime" plugin name logo (snail)
};

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle,
                          juce::Slider&) override;

    void drawLinearSlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float minSliderPos, float maxSliderPos,
                          juce::Slider::SliderStyle, juce::Slider&) override;
    int getSliderThumbRadius(juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&,
                              const juce::Colour& backgroundColour,
                              bool highlighted, bool down) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&,
                        bool highlighted, bool down) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox&) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;

    void drawLabel(juce::Graphics&, juce::Label&) override;

private:
    // Keeps the shared resources alive for the editor's lifetime (freed on the
    // message thread when the last editor closes -- see RetroResources).
    juce::SharedResourcePointer<RetroResources> resources;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RetroLookAndFeel)
};
