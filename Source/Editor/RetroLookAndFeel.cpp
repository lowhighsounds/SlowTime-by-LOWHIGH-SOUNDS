#include "RetroLookAndFeel.h"
#include "BinaryData.h"

// Minify line-art cleanly: a single big bilinear downscale (e.g. 1530 -> 50 px)
// aliases the thin isometric edges into a "pixelated" look. Repeated halving
// approximates area-averaging (mip-style), so the final image is smooth.
static juce::Image minifyHighQuality(juce::Image src, int targetW, int targetH)
{
    if (! src.isValid() || targetW <= 0 || targetH <= 0)
        return src;

    while (src.getWidth() >= targetW * 2 && src.getHeight() >= targetH * 2)
        src = src.rescaled(src.getWidth() / 2, src.getHeight() / 2,
                           juce::Graphics::highResamplingQuality);

    return src.rescaled(targetW, targetH, juce::Graphics::highResamplingQuality);
}

// Crop away fully-transparent margins so a logo with lots of empty padding
// fills its target box instead of shrinking inside it.
static juce::Image trimTransparent(const juce::Image& src)
{
    if (! src.isValid())
        return src;

    juce::Image::BitmapData data(src, juce::Image::BitmapData::readOnly);
    int minX = src.getWidth(), minY = src.getHeight(), maxX = -1, maxY = -1;
    for (int y = 0; y < src.getHeight(); ++y)
        for (int x = 0; x < src.getWidth(); ++x)
            if (data.getPixelColour(x, y).getAlpha() > 8)
            {
                minX = juce::jmin(minX, x); maxX = juce::jmax(maxX, x);
                minY = juce::jmin(minY, y); maxY = juce::jmax(maxY, y);
            }

    if (maxX < minX || maxY < minY)
        return src; // fully transparent -- nothing to trim

    return src.getClippedImage({ minX, minY, maxX - minX + 1, maxY - minY + 1 })
              .createCopy();
}

RetroResources::RetroResources()
{
    fontRegular = juce::Typeface::createSystemTypefaceFor(
        BinaryData::SpaceMonoRegular_ttf, (size_t) BinaryData::SpaceMonoRegular_ttfSize);
    fontBold = juce::Typeface::createSystemTypefaceFor(
        BinaryData::SpaceMonoBold_ttf, (size_t) BinaryData::SpaceMonoBold_ttfSize);

    noise = juce::Image(juce::Image::ARGB, 96, 96, true);
    juce::Random r(0x5eed17);
    for (int y = 0; y < noise.getHeight(); ++y)
        for (int x = 0; x < noise.getWidth(); ++x)
        {
            const auto v = (juce::uint8) r.nextInt(256);
            noise.setPixelAt(x, y, juce::Colour(v, v, v));
        }

    // LowHigh Sounds brand assets (transparent PNGs, embedded via BinaryData).
    // Pre-minified once here to sizes near their on-screen footprint (window
    // scales 0.7x..1.6x), so the per-frame draw is only a gentle resample and
    // the fine line art stays crisp instead of aliasing.
    auto rawMark = juce::ImageCache::getFromMemory(
        BinaryData::lowhighmark_png, BinaryData::lowhighmark_pngSize);
    auto rawWord = juce::ImageCache::getFromMemory(
        BinaryData::lowhighwordmark_png, BinaryData::lowhighwordmark_pngSize);

    brandMark = minifyHighQuality(rawMark, 160, 160);
    if (rawWord.isValid())
    {
        const int w = 460;
        const int h = juce::jmax(1, juce::roundToInt(w * (double) rawWord.getHeight()
                                                       / (double) rawWord.getWidth()));
        brandWordmark = minifyHighQuality(rawWord, w, h);
    }

    auto rawLogo = trimTransparent(juce::ImageCache::getFromMemory(
        BinaryData::slowtime_logo_png, BinaryData::slowtime_logo_pngSize));
    if (rawLogo.isValid())
    {
        const int h = 190;
        const int w = juce::jmax(1, juce::roundToInt(h * (double) rawLogo.getWidth()
                                                       / (double) rawLogo.getHeight()));
        pluginLogo = minifyHighQuality(rawLogo, w, h);
    }
}

juce::Font retro::font(float height, bool bold)
{
    // Space Mono renders a little small for a given em height -- scale up so
    // labels read at a comfortable size. The typefaces live in a shared
    // resource (kept alive by RetroLookAndFeel), so this just references them.
    juce::SharedResourcePointer<RetroResources> res;
    return juce::Font(juce::FontOptions{}
                          .withTypeface(bold ? res->fontBold : res->fontRegular)
                          .withHeight(height * 1.18f));
}

const juce::Image& retro::brandMark()
{
    juce::SharedResourcePointer<RetroResources> res;
    return res->brandMark;
}

const juce::Image& retro::brandWordmark()
{
    juce::SharedResourcePointer<RetroResources> res;
    return res->brandWordmark;
}

const juce::Image& retro::pluginLogo()
{
    juce::SharedResourcePointer<RetroResources> res;
    return res->pluginLogo;
}

void retro::drawNoise(juce::Graphics&, juce::Rectangle<int>, float)
{
    // Intentionally empty: the flat/cel-shaded style uses no image grain.
}

void retro::drawScreenRecess(juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    // FLAT display: one solid dark fill + a solid outline. No gradient/glass.
    g.setColour(retro::screenBg);
    g.fillRoundedRectangle(r, corner);
    g.setColour(retro::outlineColour);
    g.drawRoundedRectangle(r, corner, 2.0f);
}

void retro::drawScreenGloss(juce::Graphics&, juce::Rectangle<float>, float)
{
    // Intentionally empty: no glass reflection in the flat style.
}

void retro::drawGlowText(juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                         juce::Colour colour, const juce::Font& f, juce::Justification just)
{
    // Flat LED text: solid colour, no image blur. At most one faint 1px-offset
    // copy behind for a hint of "lit" (allowed by the flat spec).
    g.setFont(f);
    g.setColour(colour.withAlpha(0.28f));
    g.drawText(text, area.translated(0, 1), just, false);
    g.setColour(colour);
    g.drawText(text, area, just, false);
}

RetroLookAndFeel::RetroLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, retro::paper);
    setColour(juce::Label::textColourId, retro::ink);
    setColour(juce::Slider::rotarySliderFillColourId, retro::ink);
    setColour(juce::ComboBox::backgroundColourId, retro::paper);
    setColour(juce::ComboBox::textColourId, retro::ink);
    setColour(juce::ComboBox::outlineColourId, retro::ink);
    setColour(juce::ComboBox::arrowColourId, retro::ink);
    setColour(juce::TextButton::buttonColourId, retro::paper);
    setColour(juce::TextButton::textColourOffId, retro::ink);
    setColour(juce::TextButton::textColourOnId, retro::paper);
    setColour(juce::PopupMenu::backgroundColourId, retro::paper);
    setColour(juce::PopupMenu::textColourId, retro::ink);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, retro::ink);
    setColour(juce::PopupMenu::highlightedTextColourId, retro::paper);
    setColour(juce::Slider::textBoxTextColourId, retro::ink);
}

void RetroLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float startAngle, float endAngle,
                                        juce::Slider& slider)
{
    // FLAT / cel-shaded knob: solid shapes only, no gradient.
    auto area = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const float cx = area.getCentreX();
    const float cy = area.getCentreY();
    const float outerR = juce::jmax(4.0f, juce::jmin(area.getWidth(), area.getHeight()) * 0.5f - 4.0f);
    const float angle = startAngle + sliderPos * (endAngle - startAngle);

    // A ring of value ticks (studio "level-meter" style) around specific knobs.
    // The ticks eat into the outer radius so the body shrinks to make room.
    const bool tickArc = (bool) slider.getProperties().getWithDefault("tickArc", false);
    if (tickArc)
    {
        const float tickOuter = outerR;                 // rim of the ring
        const float tickInner = outerR * 0.82f;         // inner end of each tick
        const int   numTicks  = 13;
        for (int i = 0; i < numTicks; ++i)
        {
            const float t   = (float) i / (float) (numTicks - 1);
            const float ta  = startAngle + t * (endAngle - startAngle);
            const float s   = std::sin(ta), c = std::cos(ta);
            // Lit up to the current value; unlit ticks stay pale grey. The lit
            // colour dims when the owning block is switched off.
            const bool lit  = t <= sliderPos + 1.0e-4f;
            g.setColour(lit ? retro::ledFor(&slider, retro::ledOn) : retro::highlightFlat);
            g.drawLine(cx + s * tickInner, cy - c * tickInner,
                       cx + s * tickOuter, cy - c * tickOuter, lit ? 2.6f : 1.8f);
        }
    }
    // Knob body radius: shrink when ringed by ticks.
    const float r = tickArc ? outerR * 0.70f : outerR;

    // ===== STATIC BASE LAYER (never rotates) =====
    // The body + its ambient reflection are fixed: real hardware reflects the
    // room off a radially-symmetric surface, so the sheen stays put as you turn.

    // 1. Solid cast shadow (offset down-right, hard edge).
    g.setColour(retro::shadowFlat);
    g.fillEllipse(cx - r + retro::shadowOffsetX, cy - r + retro::shadowOffsetY, r * 2.0f, r * 2.0f);

    // 2. Body -- one solid colour.
    g.setColour(retro::blackComp);
    g.fillEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f);

    // 3. Outline on top of the static body.
    g.setColour(retro::outlineColour);
    g.drawEllipse(cx - r, cy - r, r * 2.0f, r * 2.0f, 2.0f);

    // ===== ROTATING VALUE LAYER =====
    // A thin white arc traces the knob's rim from the start of travel up to the
    // current value, then a short notch finishes it off at the pointer.
    const float sinA = std::sin(angle);
    const float cosA = std::cos(angle);

    // JUCE arc angles are measured clockwise from 12 o'clock -- the same
    // convention the rotary start/end use, so we can feed them straight in.
    juce::Path rim;
    rim.addCentredArc(cx, cy, r, r, 0.0f, startAngle, angle, true);
    g.setColour(retro::highlightFlat);
    g.strokePath(rim, juce::PathStrokeType(1.6f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    // Pointer notch: a short mark at the rim where the arc ends.
    const float pInner = r * 0.72f;
    const float pOuter = r;
    g.drawLine(cx + sinA * pInner, cy - cosA * pInner,
               cx + sinA * pOuter, cy - cosA * pOuter, 2.4f);
}

int RetroLookAndFeel::getSliderThumbRadius(juce::Slider&)
{
    return 8; // small, so the fader travel nearly fills the visible bar
}

void RetroLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float minSliderPos, float maxSliderPos,
                                        juce::Slider::SliderStyle style, juce::Slider& slider)
{
    if (style == juce::Slider::TwoValueHorizontal)
    {
        // FLAT band: solid inset track, solid lit passband between the cuts.
        const float cy = (float) y + height * 0.5f;
        const float left = (float) x + 6.0f;
        const float right = (float) x + width - 6.0f;

        juce::Rectangle<float> track(left, cy - 2.5f, right - left, 5.0f);
        g.setColour(retro::panelInset);
        g.fillRoundedRectangle(track, 2.0f);
        g.setColour(retro::outlineColour);
        g.drawRoundedRectangle(track, 2.0f, 1.2f);

        const float lo = juce::jlimit(track.getX(), track.getRight(), minSliderPos);
        const float hi = juce::jlimit(track.getX(), track.getRight(), maxSliderPos);
        if (hi > lo + 0.5f)
        {
            juce::Rectangle<float> lit(lo, track.getY() + 1.0f, hi - lo, track.getHeight() - 2.0f);
            g.setColour(retro::ledOn);
            g.fillRoundedRectangle(lit, 1.5f);
        }
        return;
    }

    if (style != juce::Slider::LinearVertical)
    {
        LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        return;
    }

    // FLAT vertical fader: solid track + stacked-solid cap (shadow -> wall -> top).
    // The rect JUCE hands us (sliderRect) is ALREADY inset by the thumb radius,
    // and sliderPos maps the value across exactly [housing.top, housing.bottom].
    // So the cap centre is sliderPos itself, and the track + tick scale span that
    // SAME region -- the cap reaching the track end lines up precisely with the
    // parameter reaching its extreme. (Adding the radius again here double-insets
    // and clamps the last steps on top of each other.)
    auto housing = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
    const float travelTop = housing.getY();
    const float travelBot = housing.getBottom();
    const float capY = juce::jlimit(travelTop, travelBot, sliderPos);
    const float value01 = travelBot > travelTop
                        ? juce::jlimit(0.0f, 1.0f, (travelBot - capY) / (travelBot - travelTop))
                        : 0.0f;
    const juce::Colour tickLit = retro::ledFor(&slider, juce::Colour(0xff9be34f)); // lime visor green

    const float trackWidth = 6.0f;
    juce::Rectangle<float> trackRect(0.0f, 0.0f, trackWidth, travelBot - travelTop);
    trackRect.setCentre(housing.getCentreX(), (travelTop + travelBot) * 0.5f);
    g.setColour(retro::panelInset);
    g.fillRoundedRectangle(trackRect, 2.0f);
    g.setColour(retro::outlineColour);
    g.drawRoundedRectangle(trackRect, 2.0f, 1.2f);

    const float capW = juce::jmin(housing.getWidth() - 40.0f, 26.0f);
    const float capH = 16.0f;

    // --- Interactive tick scale flanking the track (lights up to the cap) ---
    {
        const int   numTicks = 15;
        const float tickInner = housing.getCentreX() + capW * 0.5f + 4.0f;
        const float tickOuter = tickInner + 5.0f;
        const float mirrorIn  = housing.getCentreX() - capW * 0.5f - 4.0f;
        const float mirrorOut = mirrorIn - 5.0f;
        for (int i = 0; i < numTicks; ++i)
        {
            const float f  = (float) i / (float) (numTicks - 1); // 0 bottom .. 1 top
            const float ty = travelBot - f * (travelBot - travelTop);
            const bool  lit = f <= value01 + 1.0e-4f;
            g.setColour(lit ? tickLit : retro::highlightFlat);
            const float th = lit ? 2.2f : 1.4f;
            g.drawLine(tickInner, ty, tickOuter, ty, th);
            g.drawLine(mirrorIn, ty, mirrorOut, ty, th);
        }
    }

    auto capRect = juce::Rectangle<float>(capW, capH).withCentre({ housing.getCentreX(), capY });

    g.setColour(retro::shadowFlat);                 // solid cast shadow
    g.fillRoundedRectangle(capRect.translated(2.0f, 2.0f), 2.5f);
    g.setColour(retro::blackComp.darker(0.25f));    // wall (solid, darker)
    g.fillRoundedRectangle(capRect.translated(0.0f, 2.0f), 2.5f);
    g.setColour(retro::blackComp);                  // top (solid)
    g.fillRoundedRectangle(capRect, 2.5f);
    g.setColour(retro::outlineColour);
    g.drawRoundedRectangle(capRect, 2.5f, 1.5f);

    g.setColour(retro::highlightFlat.withAlpha(0.5f)); // flat grip lines
    for (int i = -1; i <= 1; ++i)
    {
        const float gy = capRect.getCentreY() + (float) i * 3.5f;
        g.drawLine(capRect.getX() + 3.0f, gy, capRect.getRight() - 3.0f, gy, 1.0f);
    }
}

void RetroLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& b,
                                            const juce::Colour&, bool highlighted, bool down)
{
    // FLAT keycap: solid shadow -> darker "wall" -> solid top -> outline. The
    // 3D comes from the hard colour step between wall and top, not a gradient.
    auto r = b.getLocalBounds().toFloat().reduced(1.6f);
    const float corner = 3.0f;
    const float press = down ? 1.0f : 0.0f; // pressed keycaps sink toward the wall

    g.setColour(retro::shadowFlat);
    g.fillRoundedRectangle(r.translated(retro::shadowOffsetX, retro::shadowOffsetY - press), corner);
    g.setColour(retro::blackComp.darker(0.30f)); // wall
    g.fillRoundedRectangle(r.translated(0.0f, 2.5f - press), corner);
    g.setColour(retro::blackComp);               // top
    g.fillRoundedRectangle(r.translated(0.0f, -press), corner);
    g.setColour(retro::outlineColour);
    g.drawRoundedRectangle(r.translated(0.0f, -press), corner, 1.5f);
}

void RetroLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& b, bool, bool)
{
    // Text acts as the keycap's LED indicator: mint when active, light grey off.
    // The active mint dims when the owning block is switched off.
    const bool on = b.getToggleState();
    g.setFont(getTextButtonFont(b, b.getHeight()));
    g.setColour(on ? retro::ledFor(&b, retro::ledOn) : retro::ledOff);
    g.drawText(b.getButtonText(), b.getLocalBounds().translated(0, b.getToggleState() ? 0 : 0),
               juce::Justification::centred, false);
}

juce::Font RetroLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return retro::font(juce::jlimit(9.0f, 13.0f, buttonHeight * 0.5f));
}

void RetroLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                    int, int, int, int, juce::ComboBox&)
{
    auto r = juce::Rectangle<float>(0.0f, 0.0f, (float) width, (float) height).reduced(0.9f);
    g.setColour(retro::paper);
    g.fillRoundedRectangle(r, 2.5f);
    g.setColour(retro::ink);
    g.drawRoundedRectangle(r, 2.5f, 1.4f);

    // Stepper-style up/down triangles on the right (classic retro dropdown).
    const float aw = 14.0f;
    auto arrow = r.removeFromRight(aw);
    g.drawLine(arrow.getX(), r.getY() + 2.0f, arrow.getX(), r.getBottom() - 2.0f, 1.2f);
    const float mx = arrow.getCentreX();
    const float my = arrow.getCentreY();
    juce::Path up, down;
    up.addTriangle(mx, my - 5.0f, mx - 3.5f, my - 1.0f, mx + 3.5f, my - 1.0f);
    down.addTriangle(mx, my + 5.0f, mx - 3.5f, my + 1.0f, mx + 3.5f, my + 1.0f);
    g.setColour(retro::ink);
    g.fillPath(up);
    g.fillPath(down);
}

void RetroLookAndFeel::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(6, 1, box.getWidth() - 20, box.getHeight() - 2);
    label.setFont(getComboBoxFont(box));
    label.setJustificationType(juce::Justification::centredLeft);
}

juce::Font RetroLookAndFeel::getComboBoxFont(juce::ComboBox& box)
{
    return retro::font(juce::jlimit(10.0f, 13.0f, box.getHeight() * 0.42f));
}

juce::Font RetroLookAndFeel::getPopupMenuFont()
{
    return retro::font(13.0f);
}

void RetroLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(label.getFont());
    g.drawFittedText(label.getText(), label.getLocalBounds(),
                     label.getJustificationType(), 1, 1.0f);
}
