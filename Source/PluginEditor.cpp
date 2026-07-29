#include "PluginEditor.h"

// ---------------- RetroKnob ----------------

RetroKnob::RetroKnob(juce::AudioProcessorValueTreeState& state,
                     const juce::String& paramID, const juce::String& captionText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setRotaryParameters(juce::MathConstants<float>::pi * 1.25f,
                               juce::MathConstants<float>::pi * 2.75f, true);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slider);

    caption.setText(captionText, juce::dontSendNotification);
    caption.setJustificationType(juce::Justification::centred);
    caption.setFont(retro::labelFont(10.5f));
    caption.setColour(juce::Label::textColourId, retro::ink);
    addAndMakeVisible(caption);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, paramID, slider);
}

void RetroKnob::resized()
{
    auto r = getLocalBounds();
    caption.setBounds(r.removeFromBottom(14));
    slider.setBounds(r);
}

// ---------------- RetroFader ----------------

RetroFader::RetroFader(juce::AudioProcessorValueTreeState& state, const juce::String& paramID,
                       const juce::String& captionText, int decimalPlaces)
    : decimals(decimalPlaces)
{
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(slider);

    caption.setText(captionText, juce::dontSendNotification);
    caption.setJustificationType(juce::Justification::centred);
    caption.setFont(retro::labelFont(10.5f));
    caption.setColour(juce::Label::textColourId, retro::ink);
    addAndMakeVisible(caption);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(state, paramID, slider);
    slider.onValueChange = [this] { updateReadout(); };
    updateReadout();
}

void RetroFader::updateReadout()
{
    const double v = slider.getValue();
    readoutText = decimals <= 0 ? juce::String(juce::roundToInt(v))
                                : juce::String(v, decimals);
    repaint();
}

void RetroFader::resized()
{
    auto r = getLocalBounds();
    caption.setBounds(r.removeFromTop(15));
    r.removeFromBottom(19); // reserved for the readout box (drawn in paint)
    slider.setBounds(r);
}

void RetroFader::paint(juce::Graphics& g)
{
    auto box = getLocalBounds().removeFromBottom(17).reduced(5, 1).toFloat();
    retro::drawScreenRecess(g, box, 2.5f);
    retro::drawGlowText(g, readoutText, box.toNearestInt(),
                        retro::ledFor(this, retro::ledOn), retro::font(11.5f));
    retro::drawScreenGloss(g, box, 2.5f);
}

// ---------------- HeaderBar ----------------

// Draws a transparent PNG as if forged/engraved into the panel: a light "lip"
// leaks out the lower-right edge (the recess wall catching the top-left light),
// then the dark shape sits on top -- solid offsets only, no blur (flat style).
static void drawEngravedImage(juce::Graphics& g, const juce::Image& img,
                              juce::Rectangle<float> dest,
                              juce::Colour dark, juce::Colour light,
                              float depth = 1.3f)
{
    if (! img.isValid())
        return;

    const float sx = dest.getWidth()  / (float) img.getWidth();
    const float sy = dest.getHeight() / (float) img.getHeight();
    const auto base = juce::AffineTransform::scale(sx, sy)
                          .translated(dest.getX(), dest.getY());
    const auto cover = dest.expanded(depth + 2.0f);

    {   // lit lower-right lip (recess wall facing the top-left light)
        juce::Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(img, base.translated(depth * 0.75f, depth));
        g.setColour(light);
        g.fillRect(cover);
    }
    {   // dark top-left inner-shadow lip -- deepens the "cut" edge
        juce::Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(img, base.translated(-depth * 0.4f, -depth * 0.4f));
        g.setColour(dark);
        g.fillRect(cover);
    }
    {   // recessed dark body on top
        juce::Graphics::ScopedSaveState s(g);
        g.reduceClipRegion(img, base);
        g.setColour(dark);
        g.fillRect(cover);
    }
}

void HeaderBar::paint(juce::Graphics& g)
{
    auto h = getLocalBounds().toFloat().reduced(1.0f);
    g.setColour(retro::paper);
    g.fillRoundedRectangle(h, 4.0f);
    g.setColour(retro::ink);
    g.drawRoundedRectangle(h, 4.0f, 1.8f);

    g.setImageResamplingQuality(juce::Graphics::highResamplingQuality);
    auto area = getLocalBounds().toFloat();

    // Left: the SlowTime plugin logo (embedded PNG), vertically centred.
    const auto& logo = retro::pluginLogo();
    if (logo.isValid())
    {
        // Fit inside the panel with a fixed margin so it never crosses the
        // rounded border (top/bottom), regardless of header height.
        const float vMargin = 9.0f;
        const float logoH = juce::jmax(1.0f, area.getHeight() - vMargin * 2.0f);
        const float logoW = logoH * (float) logo.getWidth() / (float) logo.getHeight();
        juce::Rectangle<float> lr(area.getX() + 14.0f, area.getCentreY() - logoH * 0.5f, logoW, logoH);
        g.drawImage(logo, lr, juce::RectanglePlacement::centred);
    }

    // Right: LowHigh Sounds brand lockup -- monogram + wordmark (embedded PNGs),
    // right-aligned as a group and vertically centred.
    const float cy  = area.getCentreY();
    const float pad = 16.0f;
    float cursorR = area.getRight() - pad; // right edge we place against, moving left

    const auto& wordmark = retro::brandWordmark();
    if (wordmark.isValid())
    {
        const float wordH = area.getHeight() * 0.32f;
        const float wordW = wordH * (float) wordmark.getWidth() / (float) wordmark.getHeight();
        juce::Rectangle<float> wr(cursorR - wordW, cy - wordH * 0.5f, wordW, wordH);
        drawEngravedImage(g, wordmark, wr, retro::outlineColour, retro::highlightFlat);
        cursorR = wr.getX() - 6.0f; // gap before the monogram (closer now)
    }

    const auto& mark = retro::brandMark();
    if (mark.isValid())
    {
        const float markH = area.getHeight() * 0.82f;
        const float markW = markH; // square source
        juce::Rectangle<float> mr(cursorR - markW, cy - markH * 0.5f, markW, markH);
        drawEngravedImage(g, mark, mr, retro::outlineColour, retro::highlightFlat);
    }
}

// ---------------- ContentPanel ----------------

void ContentPanel::paint(juce::Graphics& g)
{
    // FLAT: solid panel background, then a solid hard-edged cast shadow behind
    // every raised element (single light direction: down-right).
    g.fillAll(retro::panelBase);

    for (auto* c : getChildren())
    {
        if (dynamic_cast<TitledPanel*>(c) != nullptr
            || dynamic_cast<WaveformDragDisplay*>(c) != nullptr
            || dynamic_cast<HeaderBar*>(c) != nullptr)
        {
            g.setColour(retro::shadowFlat);
            g.fillRoundedRectangle(c->getBounds().toFloat().translated(retro::shadowOffsetX, retro::shadowOffsetY), 5.0f);
        }
    }
}

// ---------------- Editor: control factories ----------------

LedToggle* SlowTimeAudioProcessorEditor::makeEnable(TitledPanel& panel, const juce::String& paramID)
{
    auto* b = enables.add(new LedToggle());
    panel.addAndMakeVisible(b);
    enableAtts.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(apvts, paramID, *b));
    return b;
}

void SlowTimeAudioProcessorEditor::linkBlockLit(TitledPanel& panel, const juce::String& enableParamID)
{
    auto* param = apvts.getParameter(enableParamID);
    if (param == nullptr) return;

    auto* att = blockLitAtts.add(new juce::ParameterAttachment(*param,
        [&panel](float v)
        {
            panel.getProperties().set("blockLit", v >= 0.5f);
            panel.repaint(); // repaints the panel and all its LED children
        }));
    att->sendInitialUpdate();
}

juce::ComboBox* SlowTimeAudioProcessorEditor::makeCombo(TitledPanel& panel, const juce::String& paramID)
{
    auto* c = combos.add(new juce::ComboBox());
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(paramID)))
        c->addItemList(choice->choices, 1);
    c->setScrollWheelEnabled(true);
    panel.addAndMakeVisible(c);
    comboAtts.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(apvts, paramID, *c));
    return c;
}

juce::Label* SlowTimeAudioProcessorEditor::makeCaption(TitledPanel& panel, const juce::String& text)
{
    auto* l = captions.add(new juce::Label({}, text));
    l->setFont(retro::labelFont(10.0f));
    l->setJustificationType(juce::Justification::centredLeft);
    l->setColour(juce::Label::textColourId, retro::ink.withAlpha(0.8f));
    panel.addAndMakeVisible(l);
    return l;
}

RetroKnob* SlowTimeAudioProcessorEditor::makeKnob(TitledPanel& panel, const juce::String& paramID,
                                                     const juce::String& caption)
{
    auto* k = knobs.add(new RetroKnob(apvts, paramID, caption));
    panel.addAndMakeVisible(k);
    return k;
}

RetroFader* SlowTimeAudioProcessorEditor::makeFader(TitledPanel& panel, const juce::String& paramID,
                                                       const juce::String& caption, int decimals)
{
    auto* f = faders.add(new RetroFader(apvts, paramID, caption, decimals));
    panel.addAndMakeVisible(f);
    return f;
}

// ---------------- Editor ----------------

SlowTimeAudioProcessorEditor::SlowTimeAudioProcessorEditor(SlowTimeAudioProcessor& p)
    : juce::AudioProcessorEditor(p), processorRef(p), apvts(p.apvts), waveform(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(content);
    content.addAndMakeVisible(header);
    content.addAndMakeVisible(panelA);
    content.addAndMakeVisible(panelB);
    content.addAndMakeVisible(panelC);
    content.addAndMakeVisible(panelOut);
    content.addAndMakeVisible(waveform);

    // Bloc A -- Pitch (vertical faders + LED readouts)
    enaA = makeEnable(panelA, ParamIDs::blocAEnabled);
    linkBlockLit(panelA, ParamIDs::blocAEnabled);
    modeStrip = std::make_unique<ChoiceButtonStrip>(*apvts.getParameter(ParamIDs::algoMode),
                    juce::StringArray { "SMOOTH", "TRANS", "PUNCH" });
    panelA.addAndMakeVisible(*modeStrip);
    modeBranch = std::make_unique<BranchLabel>("MODE", 3);
    panelA.addAndMakeVisible(*modeBranch);
    semitonesF = makeFader(panelA, ParamIDs::pitchSemitones, "SEMITONES", 0);
    centsF     = makeFader(panelA, ParamIDs::fineCents, "CENTS", 0);
    freqF      = makeFader(panelA, ParamIDs::sourceFreqHz, "FREQ (Hz)", 1);

    // Bloc B -- Vibe
    enaB = makeEnable(panelB, ParamIDs::blocBEnabled);
    linkBlockLit(panelB, ParamIDs::blocBEnabled);
    vibeStrip = std::make_unique<ChoiceButtonStrip>(*apvts.getParameter(ParamIDs::vibeMode),
                    juce::StringArray { "1.5x", "2x", "4x" });
    panelB.addAndMakeVisible(*vibeStrip);
    vibeBranch = std::make_unique<BranchLabel>("MODE", 3);
    panelB.addAndMakeVisible(*vibeBranch);

    juce::StringArray divLabels;
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParamIDs::loopDivision)))
        divLabels = cp->choices;
    divBCarousel = std::make_unique<CarouselSelector>(*apvts.getParameter(ParamIDs::loopDivision), divLabels);
    panelB.addAndMakeVisible(*divBCarousel);
    loopDivCap = makeCaption(panelB, "LOOP DIVISION");
    loopDivCap->setJustificationType(juce::Justification::centred);

    smoothBK = makeKnob(panelB, ParamIDs::smooth, "SMOOTH");
    smoothBK->setTickArc(true);

    // Fade In / Out live here (moved out of Output): a small etched glyph that
    // lights up with the value, above a compact knob, in the bottom corners.
    fadeInGlyph  = std::make_unique<FadeGlyph>(*apvts.getParameter(ParamIDs::fadeInBars), true);
    fadeOutGlyph = std::make_unique<FadeGlyph>(*apvts.getParameter(ParamIDs::fadeOutBars), false);
    panelB.addAndMakeVisible(*fadeInGlyph);
    panelB.addAndMakeVisible(*fadeOutGlyph);
    fadeInK  = makeKnob(panelB, ParamIDs::fadeInBars, "FADE IN");
    fadeOutK = makeKnob(panelB, ParamIDs::fadeOutBars, "FADE OUT");

    // Bloc C -- Reverse (mirrors the Vibe panel: segment strip + carousel)
    enaC = makeEnable(panelC, ParamIDs::blocCEnabled);
    linkBlockLit(panelC, ParamIDs::blocCEnabled);

    patternStrip = std::make_unique<ChoiceButtonStrip>(*apvts.getParameter(ParamIDs::blocCPattern),
                       juce::StringArray { "ALW", "P-P", "EV2", "EV4", "RND" });
    panelC.addAndMakeVisible(*patternStrip);
    patternBranch = std::make_unique<BranchLabel>("REVERSE PATTERN", 5);
    panelC.addAndMakeVisible(*patternBranch);

    juce::StringArray divCLabels;
    if (auto* cp = dynamic_cast<juce::AudioParameterChoice*>(apvts.getParameter(ParamIDs::blocCDivision)))
        divCLabels = cp->choices;
    divCCarousel = std::make_unique<CarouselSelector>(*apvts.getParameter(ParamIDs::blocCDivision), divCLabels);
    panelC.addAndMakeVisible(*divCCarousel);
    revDivCap = makeCaption(panelC, "REVERSE DIVISION");
    revDivCap->setJustificationType(juce::Justification::centred);

    chanceK  = makeKnob(panelC, ParamIDs::blocCChance, "CHANCE");
    smoothCK = makeKnob(panelC, ParamIDs::blocCSmooth, "SMOOTH");
    chanceK->setTickArc(true);
    smoothCK->setTickArc(true);

    // Band -- dual-thumb low/high cut range slider.
    bandFilter = std::make_unique<BandFilter>(*apvts.getParameter(ParamIDs::bandLowCutHz),
                                              *apvts.getParameter(ParamIDs::bandHighCutHz));
    panelOut.addAndMakeVisible(*bandFilter);

    // Resizable, but locked to the design aspect ratio so it never distorts.
    setResizable(true, true);
    setResizeLimits(designWidth * 7 / 10, designHeight * 7 / 10,
                    designWidth * 8 / 5,  designHeight * 8 / 5);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio((double) designWidth / (double) designHeight);

    setSize(designWidth, designHeight);
}

SlowTimeAudioProcessorEditor::~SlowTimeAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void SlowTimeAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(retro::paper); // letterbox margins around the scaled content
}

void SlowTimeAudioProcessorEditor::resized()
{
    const float scale = juce::jmin((float) getWidth() / (float) designWidth,
                                   (float) getHeight() / (float) designHeight);
    const float sw = designWidth * scale;
    const float sh = designHeight * scale;

    content.setBounds(0, 0, designWidth, designHeight);
    content.setTransform(juce::AffineTransform::scale(scale)
                             .translated((getWidth() - sw) * 0.5f, (getHeight() - sh) * 0.5f));

    layoutContent();
}

void SlowTimeAudioProcessorEditor::layoutContent()
{
    auto area = juce::Rectangle<int>(0, 0, designWidth, designHeight);

    // Taller header (reserved for future controls), shorter Band strip.
    header.setBounds(area.removeFromTop(62).reduced(12, 8));
    area.removeFromTop(4);

    auto row = area.removeFromTop(300).reduced(12, 0);
    const int gap = 10;
    const int pw = (row.getWidth() - 2 * gap) / 3;
    panelA.setBounds(row.removeFromLeft(pw)); row.removeFromLeft(gap);
    panelB.setBounds(row.removeFromLeft(pw)); row.removeFromLeft(gap);
    panelC.setBounds(row);

    area.removeFromTop(6);
    waveform.setBounds(area.removeFromTop(120).reduced(12, 0));

    area.removeFromTop(6);
    panelOut.setBounds(area.removeFromTop(80).reduced(12, 0));

    // A small round LED enable, top-right of a panel's content.
    auto placeEnable = [](juce::Rectangle<int> content, LedToggle& led)
    {
        auto top = content.removeFromTop(22);
        led.setBounds(top.removeFromRight(20).withSizeKeepingCentre(15, 15));
    };

    // --- Panel A (faders) ---
    {
        auto c = panelA.getContentBounds();
        placeEnable(c, *enaA);
        c.removeFromTop(22);
        c.removeFromTop(2);
        modeBranch->setBounds(c.removeFromTop(30)); // "MODE" + branch trace
        c.removeFromTop(2);
        modeStrip->setBounds(c.removeFromTop(24));
        c.removeFromTop(12);
        auto fr = c;
        const int fw = fr.getWidth() / 3;
        semitonesF->setBounds(fr.removeFromLeft(fw));
        centsF->setBounds(fr.removeFromLeft(fw));
        freqF->setBounds(fr);
    }

    // --- Panel B (Vibe): SMOOTH centred; FADE IN / FADE OUT on either side ---
    {
        auto c = panelB.getContentBounds();
        placeEnable(c, *enaB);
        c.removeFromTop(22);
        c.removeFromTop(2);
        vibeBranch->setBounds(c.removeFromTop(30)); // "MODE" + branch trace
        c.removeFromTop(2);
        vibeStrip->setBounds(c.removeFromTop(24));
        c.removeFromTop(8);
        loopDivCap->setBounds(c.removeFromTop(13)); // "LOOP DIVISION" caption
        c.removeFromTop(2);
        divBCarousel->setBounds(c.removeFromTop(30));

        // Everything below the carousel is one band; three columns centred in it.
        auto band = c.reduced(0, 4);
        const int col = band.getWidth() / 3;
        auto leftCol = band.removeFromLeft(col);
        auto rightCol = band.removeFromRight(col);
        auto centreCol = band;

        smoothBK->setBounds(centreCol.withSizeKeepingCentre(juce::jmin(96, centreCol.getWidth()),
                                                            juce::jmin(96, centreCol.getHeight())));

        auto placeFade = [](juce::Rectangle<int> c2, FadeGlyph& glyph, RetroKnob& knob)
        {
            auto grp = c2.withSizeKeepingCentre(juce::jmin(74, c2.getWidth()), 70);
            glyph.setBounds(grp.removeFromTop(15).reduced(10, 1));
            knob.setBounds(grp);
        };
        placeFade(leftCol, *fadeInGlyph, *fadeInK);
        placeFade(rightCol, *fadeOutGlyph, *fadeOutK);
    }

    // --- Panel C (Reverse): CHANCE + SMOOTH centred below the carousel ---
    {
        auto c = panelC.getContentBounds();
        placeEnable(c, *enaC);
        c.removeFromTop(22);
        c.removeFromTop(2);
        patternBranch->setBounds(c.removeFromTop(30)); // "REVERSE PATTERN" + branch
        c.removeFromTop(2);
        patternStrip->setBounds(c.removeFromTop(24));
        c.removeFromTop(8);
        revDivCap->setBounds(c.removeFromTop(13)); // "REVERSE DIVISION" caption
        c.removeFromTop(2);
        divCCarousel->setBounds(c.removeFromTop(30));

        auto band = c.reduced(0, 4);
        auto grp = band.withSizeKeepingCentre(juce::jmin(200, band.getWidth()),
                                              juce::jmin(104, band.getHeight()));
        const int kw = grp.getWidth() / 2;
        chanceK->setBounds(grp.removeFromLeft(kw));
        smoothCK->setBounds(grp);
    }

    // --- Band (dual-thumb low/high cut) ---
    bandFilter->setBounds(panelOut.getContentBounds());
}
