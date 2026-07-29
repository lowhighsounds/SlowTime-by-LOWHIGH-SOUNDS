#include "WaveformDragDisplay.h"
#include "../PluginProcessor.h"
#include "../Parameters/ParameterIDs.h"
#include "RetroLookAndFeel.h"

WaveformDragDisplay::WaveformDragDisplay(SlowTimeAudioProcessor& processorToDisplay)
    : processor(processorToDisplay)
{
    setOpaque(true);

    // Red record-arm LED in the visor, bound to the recordEnabled parameter.
    addAndMakeVisible(recordButton);
    recordAtt = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamIDs::recordEnabled, recordButton);

    startTimerHz(15);
}

juce::Rectangle<int> WaveformDragDisplay::recGroupBounds() const
{
    return { 11, 10, 58, 16 }; // top-left of the visor: LED + "REC" label
}

void WaveformDragDisplay::resized()
{
    auto g = recGroupBounds();
    recordButton.setBounds(g.removeFromLeft(g.getHeight())); // square LED on the left
}

WaveformDragDisplay::~WaveformDragDisplay()
{
    stopTimer();
}

juce::String WaveformDragDisplay::formatTime(double seconds)
{
    const int total = juce::jmax(0, static_cast<int>(seconds));
    return juce::String(total / 60) + ":" + juce::String(total % 60).paddedLeft('0', 2);
}

void WaveformDragDisplay::rebuildPeaksIncrementally()
{
    const int recorded = processor.getRecordedNumSamples();

    // Capture restarted (new play) -- drop the cache and start over.
    if (recorded < peaksBuiltToSample)
    {
        peakMin.clear();
        peakMax.clear();
        peaksBuiltToSample = 0;
    }

    const int chans = processor.getRecordedNumChannels();
    if (chans <= 0)
        return;

    const float* data = processor.getRecordedReadPointer(0);
    if (data == nullptr)
        return;

    // Only bucket whole buckets that are fully recorded -- the tail waits for
    // the next refresh. This is what keeps each pass O(new samples).
    while (peaksBuiltToSample + kSamplesPerBucket <= recorded)
    {
        float mn = 0.0f, mx = 0.0f;
        const int start = peaksBuiltToSample;
        for (int i = start; i < start + kSamplesPerBucket; ++i)
        {
            const float s = data[i];
            mn = juce::jmin(mn, s);
            mx = juce::jmax(mx, s);
        }
        peakMin.push_back(mn);
        peakMax.push_back(mx);
        peaksBuiltToSample += kSamplesPerBucket;
    }
}

void WaveformDragDisplay::timerCallback()
{
    const size_t before = peakMin.size();
    rebuildPeaksIncrementally();
    if (peakMin.size() != before)
        repaint();
}

void WaveformDragDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    g.fillAll(retro::paper);

    // Outer frame (matches the panel border style).
    auto frame = bounds.toFloat().reduced(1.0f);
    g.setColour(retro::ink);
    g.drawRoundedRectangle(frame, 5.0f, 1.8f);

    auto area = bounds.reduced(6);
    auto statusArea = area.removeFromBottom(18);
    const auto areaF = area.toFloat();

    // Recessed CRT screen.
    retro::drawScreenRecess(g, areaF, 3.0f);

    const int recorded = processor.getRecordedNumSamples();
    const int capacity = juce::jmax(1, processor.getRecordCapacity());
    const double sr = juce::jmax(1.0, processor.getRecordSampleRate());
    const bool full = processor.isRecordingFull();
    const float fillFraction = juce::jlimit(0.0f, 1.0f, static_cast<float>(recorded) / static_cast<float>(capacity));

    {
        juce::Graphics::ScopedSaveState save(g);
        juce::Path clip;
        clip.addRoundedRectangle(areaF, 3.0f);
        g.reduceClipRegion(clip);

        // Dashed grid lines (the retro "road" look).
        g.setColour(retro::screenLine);
        const float dashes[] = { 3.0f, 4.0f };
        for (int i = 1; i < 8; ++i)
        {
            const float gx = area.getX() + area.getWidth() * (i / 8.0f);
            g.drawDashedLine({ gx, (float) area.getY(), gx, (float) area.getBottom() }, dashes, 2, 1.0f);
        }
        const float my = (float) area.getCentreY();
        g.drawDashedLine({ (float) area.getX(), my, (float) area.getRight(), my }, dashes, 2, 1.0f);

        // --- Waveform ---
        const int numBuckets = static_cast<int>(peakMin.size());
        if (numBuckets <= 0)
        {
            g.setColour(retro::steel.withAlpha(0.75f));
            g.setFont(retro::font(12.0f, false));
            g.drawText(juce::CharPointer_UTF8("press play to record \xe2\x80\x94 drag the waveform to the playlist"),
                       area, juce::Justification::centred);
        }
        else
        {
            const int w = area.getWidth();
            const float midY = static_cast<float>(area.getCentreY());
            const float halfH = area.getHeight() * 0.5f - 3.0f;
            const juce::Colour wave = full ? retro::salmon : retro::green;

            for (int x = 0; x < w; ++x)
            {
                const int b0 = static_cast<int>(static_cast<juce::int64>(x) * numBuckets / w);
                const int b1 = juce::jmax(b0 + 1,
                               static_cast<int>(static_cast<juce::int64>(x + 1) * numBuckets / w));

                float mn = 0.0f, mx = 0.0f;
                for (int b = b0; b < b1 && b < numBuckets; ++b)
                {
                    mn = juce::jmin(mn, peakMin[static_cast<size_t>(b)]);
                    mx = juce::jmax(mx, peakMax[static_cast<size_t>(b)]);
                }

                // Flat: solid vertical line, no curvature/bloom.
                const float topY = midY - mx * halfH;
                const float botY = midY - mn * halfH;
                const float px = area.getX() + (float) x;
                g.setColour(wave);
                g.drawLine(px, topY, px, botY, 1.0f);
            }
        }

        retro::drawScreenGloss(g, areaF, 3.0f);
    }

    // --- REC label (the red LED itself is a child drawn on top) ---
    {
        auto grp = recGroupBounds();
        // Dark chip so the group reads clearly over the waveform.
        g.setColour(retro::screenBg.withAlpha(0.8f));
        g.fillRoundedRectangle(grp.toFloat().expanded(3.0f, 2.0f), 3.0f);

        auto labelRect = grp;
        labelRect.removeFromLeft(grp.getHeight() + 3);
        g.setColour(retro::steel);
        g.setFont(retro::font(10.5f));
        g.drawText("REC", labelRect, juce::Justification::centredLeft, false);
    }

    // --- Capacity meter + read-out ---
    auto meter = statusArea.removeFromTop(4).reduced(0, 1);
    g.setColour(retro::ink);
    g.drawRect(meter, 1);
    const juce::Colour meterColour = full                  ? retro::salmon
                                   : fillFraction > 0.75f  ? retro::lavender
                                                           : retro::green;
    g.setColour(meterColour);
    g.fillRect(meter.reduced(1).withWidth(static_cast<int>((meter.getWidth() - 2) * fillFraction)));

    g.setColour(full ? retro::ink : retro::ink.withAlpha(0.7f));
    g.setFont(retro::font(10.5f));
    const juce::String readout = formatTime(recorded / sr) + " / " + formatTime(capacity / sr)
                               + (full ? juce::CharPointer_UTF8("   LIMIT REACHED \xe2\x80\x94 press play again to re-record")
                                       : juce::String());
    g.drawText(readout, statusArea, juce::Justification::centredLeft);
}

void WaveformDragDisplay::mouseUp(const juce::MouseEvent&)
{
    dragInProgress = false;
}

void WaveformDragDisplay::mouseDrag(const juce::MouseEvent& e)
{
    if (dragInProgress)
        return;
    if (e.getDistanceFromDragStart() < 6)
        return;

    // One-off full copy: fine here (user gesture), unlike per-frame drawing.
    juce::AudioBuffer<float> snapshot;
    const int n = processor.getRecordingSnapshot(snapshot);
    if (n <= 0)
        return;

    const double sr = processor.getRecordSampleRate();
    if (sr <= 0.0)
        return;

    auto target = juce::File::getSpecialLocation(juce::File::tempDirectory)
                      .getChildFile("SlowTime_" + juce::String(juce::Time::currentTimeMillis()) + ".wav");

    if (! renderBufferToWav(snapshot, sr, target))
        return;

    dragInProgress = true;
    juce::StringArray files;
    files.add(target.getFullPathName());

    // Static: kicks off an OS-level file drag the DAW receives as a file drop.
    juce::DragAndDropContainer::performExternalDragDropOfFiles(
        files, /*canMoveFiles*/ false, this,
        [safeThis = juce::Component::SafePointer<WaveformDragDisplay>(this)]
        {
            if (safeThis != nullptr)
                safeThis->dragInProgress = false;
        });
}

bool WaveformDragDisplay::renderBufferToWav(const juce::AudioBuffer<float>& buffer,
                                            double sampleRate, const juce::File& target)
{
    target.deleteFile();

    juce::WavAudioFormat wav;
    std::unique_ptr<juce::FileOutputStream> stream(target.createOutputStream());
    if (stream == nullptr)
        return false;

    std::unique_ptr<juce::AudioFormatWriter> writer(
        wav.createWriterFor(stream.get(), sampleRate,
                            static_cast<unsigned int>(buffer.getNumChannels()),
                            24, {}, 0));
    if (writer == nullptr)
        return false;

    stream.release(); // writer owns the stream now
    const bool ok = writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
    writer.reset();   // flush + close
    return ok;
}
