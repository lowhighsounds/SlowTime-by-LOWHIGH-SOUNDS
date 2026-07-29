#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "Utility/DspConstants.h"
#include "DSP/VibeModeMap.h"
#include "DSP/HostSync.h"
#include <cmath>

SlowTimeAudioProcessor::SlowTimeAudioProcessor()
    : AudioProcessor(BusesProperties()
                          .withInput("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMS", createParameterLayout())
{
}

float SlowTimeAudioProcessor::tonalityLimitHzForMode(AlgoMode mode)
{
    switch (mode)
    {
        case AlgoMode::Transient: return DspConstants::kTonalityLimitHzTransient;
        case AlgoMode::Punchy:    return DspConstants::kTonalityLimitHzPunchy;
        default:                  return DspConstants::kTonalityLimitHzSmooth;
    }
}

void SlowTimeAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    const int numChannels = getTotalNumInputChannels();

    stretchEngine.prepare(sampleRate, samplesPerBlock, numChannels);

    const double maxDivisionSeconds = (60.0 / DspConstants::kMinSupportedBpm)
        * DspConstants::kMaxSupportedNumerator * DspConstants::kMaxLoopBars;
    // Slowdown reads forward from "now", but the speed-up (backward-reading)
    // modes anchor up to roughly one extra division into the past, so the ring
    // needs to hold more than a single division of history.
    const double maxCaptureSeconds = maxDivisionSeconds * 2.0;
    chopVibeEngine.prepare(sampleRate, numChannels, maxCaptureSeconds);
    reverseEngine.prepare(sampleRate, numChannels, maxCaptureSeconds);
    scratchC.setSize(numChannels, samplesPerBlock, false, false, true);
    blocCGate.reset(sampleRate, 0.008); // ~8 ms dry<->reverse crossfade
    blocCGate.setCurrentAndTargetValue(0.0f);

    cachedBlocALatencySamples = stretchEngine.getLatencySamples();

    smoothedPitchSemitones.reset(sampleRate, DspConstants::kParamSmoothingSeconds);
    smoothedPitchSemitones.setCurrentAndTargetValue(0.0f);

    // Mode switches only change the tonality limit now (window is always the
    // same), but that's still an abrupt change to the frequency-mapping
    // internals, so it's smoothed too, to avoid a click at the switch instant.
    smoothedTonalityLimitHz.reset(sampleRate, DspConstants::kModeSwitchCrossfadeSeconds);
    smoothedTonalityLimitHz.setCurrentAndTargetValue(DspConstants::kTonalityLimitHzTransient);

    smoothedSmooth.reset(sampleRate, DspConstants::kParamSmoothingSeconds);
    smoothedSmooth.setCurrentAndTargetValue(0.0f);

    // Band filter cutoffs are smoothed at block rate so automating them doesn't
    // zipper. The TPT SVF itself is zero-latency and stable across changes.
    smoothedBandLowCutHz.reset(sampleRate, DspConstants::kParamSmoothingSeconds);
    smoothedBandLowCutHz.setCurrentAndTargetValue(20.0f);
    smoothedBandHighCutHz.reset(sampleRate, DspConstants::kParamSmoothingSeconds);
    smoothedBandHighCutHz.setCurrentAndTargetValue(20000.0f);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(juce::jmax(1, samplesPerBlock));
    spec.numChannels = static_cast<juce::uint32>(juce::jmax(1, numChannels));

    bandHighPass.prepare(spec);
    bandHighPass.setType(juce::dsp::StateVariableTPTFilterType::highpass);
    bandHighPass.setCutoffFrequency(20.0f);
    bandLowPass.prepare(spec);
    bandLowPass.setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    bandLowPass.setCutoffFrequency(20000.0f);

    dryBuffer.setSize(numChannels, samplesPerBlock, false, false, true);

    // Dry delay line: long enough to hold the dry path back by Bloc A's
    // latency (plus one block of write-ahead headroom).
    dryDelaySize = juce::jmax(samplesPerBlock * 2, cachedBlocALatencySamples + samplesPerBlock + 8);
    dryDelayRing.setSize(numChannels, dryDelaySize, false, false, true);
    dryDelayRing.clear();
    dryDelayWriteHead = 0;

    engageGain = 1.0f;
    engageInitialised = false;

    recordCapacity = juce::jmax(1, static_cast<int>(sampleRate * DspConstants::kMaxRecordSeconds));
    recordBuffer.setSize(numChannels, recordCapacity, false, false, true);
    recordBuffer.clear();
    recordWritePos = 0;
    recordedSamples.store(0, std::memory_order_release);

    lastBlocAEnabled = false;
    lastBlocBEnabled = false;
    lastReportedLatency = -1;
    lastHostIsPlaying = false;
    expectedNextPpqPosition = -1.0;
    blocBSamplesSinceAnchor = 0;
    blocBEngaged = false;

    blocCSamplesSinceJump = 0;
    blocCWindowIndex = -1;
    blocCCurrentReversed = false;
    blocCNeedsHardSnap = false;
    blocCFreeRun = 0;
    lastBlocCEnabled = false;
    smoothedBlocCSmooth.reset(sampleRate, DspConstants::kParamSmoothingSeconds);
    smoothedBlocCSmooth.setCurrentAndTargetValue(0.0f);

    updateLatencyReport(false);
}

void SlowTimeAudioProcessor::releaseResources()
{
}

bool SlowTimeAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    return true;
}

void SlowTimeAudioProcessor::updateLatencyReport(bool blocAEnabled)
{
    // Bloc B (ChopVibeEngine) adds no latency in either direction -- see the
    // ChopVibeEngine class comment for why that's a deliberate choice (it's
    // what Deja Vu/HalfTime and the rest of this plugin category do, and two
    // alternatives that DID add latency were tried and rejected). Only Bloc
    // A's phase-vocoder engine contributes latency.
    const int latency = blocAEnabled ? cachedBlocALatencySamples : 0;

    if (latency != lastReportedLatency)
    {
        lastReportedLatency = latency;
        setLatencySamples(latency);
    }
}

void SlowTimeAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    if (numSamples <= 0)
        return;

    const bool blocAEnabled = apvts.getRawParameterValue(ParamIDs::blocAEnabled)->load() >= 0.5f;
    const bool blocBEnabled = apvts.getRawParameterValue(ParamIDs::blocBEnabled)->load() >= 0.5f;
    const auto vibeMode = static_cast<VibeMode>(static_cast<int>(apvts.getRawParameterValue(ParamIDs::vibeMode)->load()));
    const auto loopDivision = static_cast<LoopDivision>(static_cast<int>(apvts.getRawParameterValue(ParamIDs::loopDivision)->load()));
    const auto vibeParams = vibeModeToParams(vibeMode);

    const bool blocCEnabled = apvts.getRawParameterValue(ParamIDs::blocCEnabled)->load() >= 0.5f;
    const auto blocCDivision = static_cast<LoopDivision>(static_cast<int>(apvts.getRawParameterValue(ParamIDs::blocCDivision)->load()));
    const auto blocCPattern = static_cast<ReversePattern>(static_cast<int>(apvts.getRawParameterValue(ParamIDs::blocCPattern)->load()));
    const float blocCChance = apvts.getRawParameterValue(ParamIDs::blocCChance)->load();

    // Capture the RAW input into the dry delay ring before anything touches
    // the buffer -- this is the dry signal the Mix / Fade stages blend against.
    for (int ch = 0; ch < dryDelayRing.getNumChannels() && ch < buffer.getNumChannels(); ++ch)
    {
        const float* in = buffer.getReadPointer(ch);
        for (int i = 0; i < numSamples; ++i)
        {
            const auto idx = static_cast<int>((dryDelayWriteHead + i) % static_cast<juce::int64>(dryDelaySize));
            dryDelayRing.setSample(ch, idx, in[i]);
        }
    }
    dryDelayWriteHead += numSamples;

    if (blocAEnabled != lastBlocAEnabled)
    {
        // No crossfade yet -- hard reset on enable/disable transition to avoid
        // stale-state clicks (validate sonoridade first; click-free switching
        // is a later milestone per the established dev order).
        stretchEngine.reset();
        lastBlocAEnabled = blocAEnabled;
    }

    const auto hostSync = queryHostSync(*this);
    const int divisionSamplesNow = loopDivisionToSamples(loopDivision, hostSync, currentSampleRate);

    // A transport start, or the playhead jumping (seek, loop-back) shows up
    // as either isPlaying flipping false->true, or this block's ppq position
    // landing somewhere other than where continuous playback would have put
    // it. Either one means whatever chunk is currently loaded is stale and
    // must be replaced right away, instead of waiting for the next scheduled
    // boundary (that's what previously caused stale audio to keep playing
    // for a while after pressing Play).
    bool transportJumped = false;
    if (hostSync.hasPpq)
    {
        if (hostSync.isPlaying && ! lastHostIsPlaying)
        {
            transportJumped = true;
        }
        else if (hostSync.isPlaying && expectedNextPpqPosition >= 0.0)
        {
            // Tolerance has to clear routine host PPQ jitter (which scales
            // with the block) while still catching any genuine seek/loop
            // (>= a beat). See kTransportJumpToleranceQn for what a too-tight
            // value did to the whole Bloc B grid.
            const double blockQn = (static_cast<double>(numSamples) / currentSampleRate)
                                   * (hostSync.bpm / 60.0);
            const double tolerance = juce::jmax(2.0 * blockQn, DspConstants::kTransportJumpToleranceQn);

            if (std::abs(hostSync.ppqPosition - expectedNextPpqPosition) > tolerance)
                transportJumped = true;
        }
    }
    // Transport just started rolling (play from stop) -- used to reset the
    // play-to-pause capture below. Computed before lastHostIsPlaying updates.
    const bool transportStarted = hostSync.isPlaying && ! lastHostIsPlaying;
    lastHostIsPlaying = hostSync.isPlaying;

    const bool justEnabled = blocBEnabled && ! lastBlocBEnabled;
    // Only "live" (crossfade in place) if we were already properly engaged;
    // if Bloc B is still armed/waiting from a prior enable or jump, a
    // division change just updates what boundary it's waiting for -- see the
    // main Bloc B block below.
    const bool loopDivisionChangedLive = blocBEnabled && lastBlocBEnabled
                                         && loopDivision != lastLoopDivision && blocBEngaged;
    lastBlocBEnabled = blocBEnabled;
    lastLoopDivision = loopDivision;

    updateLatencyReport(blocAEnabled);

    const float pitchSemitones = apvts.getRawParameterValue(ParamIDs::pitchSemitones)->load();
    const float fineCents      = apvts.getRawParameterValue(ParamIDs::fineCents)->load();
    const float sourceFreqHz   = apvts.getRawParameterValue(ParamIDs::sourceFreqHz)->load();

    // Retune calculator: how many semitones to shift so a track referenced at
    // sourceFreqHz (e.g. 432) lands on standard concert pitch (440).
    const float freqOffsetSemitones = 12.0f * std::log2(DspConstants::kStandardConcertPitchHz / sourceFreqHz);
    const float targetSemitones = pitchSemitones + fineCents / 100.0f + freqOffsetSemitones;

    smoothedPitchSemitones.setTargetValue(targetSemitones);
    smoothedPitchSemitones.skip(juce::jmax(0, numSamples - 1));
    const float appliedSemitones = smoothedPitchSemitones.getNextValue();

    if (blocAEnabled)
    {
        const auto algoMode = static_cast<AlgoMode>(static_cast<int>(apvts.getRawParameterValue(ParamIDs::algoMode)->load()));

        smoothedTonalityLimitHz.setTargetValue(tonalityLimitHzForMode(algoMode));
        smoothedTonalityLimitHz.skip(juce::jmax(0, numSamples - 1));
        const float appliedTonalityLimitHz = smoothedTonalityLimitHz.getNextValue();

        stretchEngine.setPitchSemitones(appliedSemitones, appliedTonalityLimitHz);
        stretchEngine.process(buffer, numSamples);
    }

    // Keep the rolling history buffer warm whenever the host transport is
    // actually running (or when the host gives no transport info at all,
    // e.g. Standalone -- default to always-on rather than silently breaking).
    // Gating on isPlaying avoids capturing/counting through dead air while
    // paused, which previously meant a fresh boundary could land on a chunk
    // of pure silence right as playback resumed.
    const bool transportActive = ! hostSync.hasPosition || hostSync.isPlaying;

    // Bloc B chops Bloc A's OUTPUT, which is delayed by Bloc A's phase-vocoder
    // latency, yet the host reports the playhead in real time. Left uncorrected,
    // Bloc B would place its reset grid against real-time PPQ while the audio it
    // is actually cutting is that many samples behind -- so the whole chop grid
    // sits off the beat (worst at a play/seek, where Bloc A's warm-up feeds a
    // thin ramp into the chopper: the "little bit at the start" that drifts off
    // tempo). Shifting the PPQ that Bloc B uses for boundary detection back by
    // Bloc A's latency makes it cut the delayed audio at its true musical
    // position; host PDC (which compensates that same latency) then lands the
    // result on the grid. Zero when Bloc A is off. Only boundary detection is
    // shifted -- transport-jump detection stays on the raw playhead. Bloc C
    // (reverse) sits downstream of the same latency, so it shares this grid.
    HostSyncInfo gridSync = hostSync;
    if (blocAEnabled && hostSync.hasPpq && hostSync.bpm > 0.0)
    {
        const double blocALatencyQn = (static_cast<double>(cachedBlocALatencySamples) / currentSampleRate)
                                      * (hostSync.bpm / 60.0);
        gridSync.ppqPosition -= blocALatencyQn;
    }

    if (transportActive)
    {
        if (blocBEnabled)
        {
            chopVibeEngine.setRatio(vibeParams.timeRatio);

            // Knob 0..100 -> envelope depth 0..1 (see ChopVibeEngine::setSmooth).
            const float smoothPercent = apvts.getRawParameterValue(ParamIDs::smooth)->load();
            smoothedSmooth.setTargetValue(smoothPercent / 100.0f);
            smoothedSmooth.skip(juce::jmax(0, numSamples - 1));
            chopVibeEngine.setSmooth(smoothedSmooth.getNextValue());

            // History must be complete up to "now" before the engine anchors
            // its live read pointer against it. (No clearing on a jump: all
            // Vibe modes are slowdowns now, which read FORWARD from the anchor,
            // so pre-jump history is never touched anyway -- and the previous
            // clear-AFTER-push wiped the very block we were about to read,
            // which is exactly what produced the short late/off-grid start.)
            chopVibeEngine.pushInput(buffer, 0, numSamples);

            if (justEnabled || transportJumped)
            {
                // Don't anchor mid-grid: disarm and wait for the next clean
                // boundary below instead of snapping to "right now", which is
                // almost never an exact grid line. Anchoring immediately here
                // used to mean the very first (or post-jump) window played
                // back starting at whatever odd phase the transport happened
                // to land on, audibly off-tempo, until it self-corrected at
                // the next natural boundary a division later.
                blocBEngaged = false;
                blocBSamplesSinceAnchor = 0;
            }
            else if (loopDivisionChangedLive)
            {
                // Already properly engaged and playing on-grid; a live
                // division change crossfades in place through Smooth instead
                // of waiting, same as before.
                chopVibeEngine.resetToLiveBoundary(numSamples, divisionSamplesNow, /*hardSnap*/ false);
                blocBSamplesSinceAnchor = 0;
            }

            // Re-anchor exactly on the host's real bar/beat grid whenever a
            // boundary falls inside this block. With no PPQ (e.g. Standalone),
            // there's no real grid to wait for: engage right away if armed,
            // otherwise fall back to a free-running division-length counter
            // so the read pointer can't drift indefinitely behind the signal.
            int splitIndex = gridSync.hasPpq
                ? samplesUntilNextBoundaryWithinBlock(gridSync, loopDivision, numSamples, currentSampleRate)
                : -1;

            if (splitIndex < 0 && ! blocBEngaged && gridSync.hasPpq && gridSync.bpm > 0.0)
            {
                // Arming after a jump/enable: a jump doesn't happen on an
                // audio block boundary, so the ppq we land on almost never
                // falls within samplesUntilNextBoundaryWithinBlock's tight
                // (near-zero) exact-hit tolerance -- typically it's a
                // handful of samples past the ideal grid line. That's not
                // "missed it, wait a whole division for the next one" (what
                // a forward-only search concludes) -- it's "already
                // basically sitting on it". Snap to the nearest multiple of
                // the division, on whichever side it falls, if we're within
                // about a block's worth of it. Without this, e.g. a DAW
                // section loop that wraps back to an exact bar line still
                // produced a near-full-division silent gap on every pass,
                // because the post-wrap ppq was a few samples past 0.
                const double divisionQnNow = loopDivisionToQuarterNotes(loopDivision, gridSync.numerator, gridSync.denominator);
                if (divisionQnNow > 0.0)
                {
                    const double blockQn = (static_cast<double>(numSamples) / currentSampleRate) * (gridSync.bpm / 60.0);
                    const double snapToleranceQn = juce::jmax(2.0 * blockQn, DspConstants::kTransportJumpToleranceQn);
                    const double nearestK = std::round(gridSync.ppqPosition / divisionQnNow);
                    const double nearestBoundaryPpq = nearestK * divisionQnNow;
                    if (std::abs(gridSync.ppqPosition - nearestBoundaryPpq) <= snapToleranceQn)
                        splitIndex = 0;
                }
            }

            if (splitIndex < 0 && ! hostSync.hasPpq)
            {
                if (! blocBEngaged)
                    splitIndex = 0;
                else if (blocBSamplesSinceAnchor + numSamples >= divisionSamplesNow)
                    splitIndex = juce::jlimit(0, numSamples,
                                              static_cast<int>(divisionSamplesNow - blocBSamplesSinceAnchor));
            }

            if (splitIndex >= 0)
            {
                if (blocBEngaged && splitIndex > 0)
                    chopVibeEngine.process(buffer, 0, splitIndex);

                // hardSnap whenever we're coming from armed (no valid current
                // window to blend from) or a transport jump (the old window
                // describes the wrong part of the timeline now); otherwise
                // this is the loopDivisionChangedLive case, already reset above.
                chopVibeEngine.resetToLiveBoundary(numSamples - splitIndex, divisionSamplesNow,
                                                   /*hardSnap*/ transportJumped || ! blocBEngaged);
                blocBEngaged = true;
                blocBSamplesSinceAnchor = 0;

                if (splitIndex < numSamples)
                    chopVibeEngine.process(buffer, splitIndex, numSamples - splitIndex);

                blocBSamplesSinceAnchor += numSamples - splitIndex;
            }
            else if (blocBEngaged)
            {
                chopVibeEngine.process(buffer, 0, numSamples);
                blocBSamplesSinceAnchor += numSamples;
            }
            // else: armed but no clean boundary in this block yet -- leave
            // buffer as Bloc A's output (or raw dry) untouched and keep
            // waiting; pushInput above already keeps history warm for when
            // the boundary arrives.
        }
        else
        {
            // Not actively chopping right now, but keep history warm for an
            // instant, zero-latency engage later.
            chopVibeEngine.pushInput(buffer, 0, numSamples);
        }
    }

    // --- Bloc C: grid-synced reverse of the Bloc A -> B output ---
    // Reverse reads BACKWARD through recorded history, so it needs a full
    // window of fresh post-jump audio before it may reverse (freshness guard).
    // The engine is always reversed; a per-sample gate blends the buffer
    // between dry (normal windows) and reversed (pattern-selected windows),
    // which also avoids the read-pointer clamp artifact a "forward-normal"
    // engine pass would hit at each block end.
    {
        const bool justEnabledC = blocCEnabled && ! lastBlocCEnabled;
        const bool divisionChangedC = blocCDivision != lastBlocCDivision;
        lastBlocCEnabled = blocCEnabled;
        lastBlocCDivision = blocCDivision;

        // Anything that invalidates the current window -- a transport jump, a
        // fresh enable, or a new grid division -- rides the gate back down to
        // dry and re-engages at the next clean boundary. Cutting instead of
        // ramping (an instant gate reset, which is what this used to do) was an
        // audible click on every one of those parameter moves. A division
        // change is the worst case: the running window's fade-out envelope is
        // sized for the OLD division, so the new grid's boundary lands while
        // that envelope is still mid-fade -- a step straight to the next
        // window unless we ramp out first.
        if (transportJumped || justEnabledC || divisionChangedC)
        {
            if (transportJumped)
                blocCSamplesSinceJump = 0;   // history invalid; enable/division keep theirs
            blocCWindowIndex = -1;
            blocCCurrentReversed = false;
            blocCNeedsHardSnap = true;
            blocCFreeRun = 0;
            blocCGate.setTargetValue(0.0f);
        }

        if (transportActive)
        {
            // Keep history warm even while disabled, so enabling mid-playback
            // can reverse as soon as enough fresh audio exists.
            reverseEngine.pushInput(buffer, 0, numSamples);
            if (hostSync.isPlaying || ! hostSync.hasPosition)
                blocCSamplesSinceJump += numSamples;
        }

        // Stay active while the gate is still open so a DISABLE fades out
        // instead of snapping the reversed window straight back to dry.
        const bool gateOpen = blocCGate.isSmoothing() || blocCGate.getCurrentValue() > 1.0e-4f;

        if ((blocCEnabled || gateOpen) && transportActive)
        {
            const float cSmoothPercent = apvts.getRawParameterValue(ParamIDs::blocCSmooth)->load();
            smoothedBlocCSmooth.setTargetValue(cSmoothPercent / 100.0f);
            smoothedBlocCSmooth.skip(juce::jmax(0, numSamples - 1));
            reverseEngine.setSmooth(smoothedBlocCSmooth.getNextValue());
            reverseEngine.setReversed(true); // always reverse; the gate selects windows

            const int blocCDivSamples = loopDivisionToSamples(blocCDivision, hostSync, currentSampleRate);
            const int chans = buffer.getNumChannels();
            scratchC.setSize(chans, numSamples, false, false, true);

            auto blendSeg = [&](int start, int len)
            {
                if (len <= 0)
                    return;
                reverseEngine.process(scratchC, start, len);
                for (int i = start; i < start + len; ++i)
                {
                    const float g = blocCGate.getNextValue();
                    for (int ch = 0; ch < chans; ++ch)
                    {
                        const float dry = buffer.getSample(ch, i);
                        const float rev = scratchC.getSample(ch, i);
                        buffer.setSample(ch, i, dry * (1.0f - g) + rev * g);
                    }
                }
            };

            if (! blocCEnabled)
            {
                // Disabled but the gate is still open: ride it down to dry over
                // the ramp, after which this branch stops running entirely.
                blocCGate.setTargetValue(0.0f);
                blendSeg(0, numSamples);
            }
            else
            {
                int splitIndex = gridSync.hasPpq
                ? samplesUntilNextBoundaryWithinBlock(gridSync, blocCDivision, numSamples, currentSampleRate)
                : -1;
            if (splitIndex < 0 && ! gridSync.hasPpq
                && blocCFreeRun + numSamples >= blocCDivSamples)
                splitIndex = juce::jlimit(0, numSamples,
                                          static_cast<int>(blocCDivSamples - blocCFreeRun));

            if (splitIndex >= 0)
            {
                // Segment before the boundary belongs to the current window.
                blocCGate.setTargetValue(blocCCurrentReversed ? 1.0f : 0.0f);
                blendSeg(0, splitIndex);

                // New window at the boundary -- decide reverse (pattern + guard).
                ++blocCWindowIndex;
                bool rev = false;
                if (blocCSamplesSinceJump >= blocCDivSamples)
                {
                    switch (blocCPattern)
                    {
                        case ReversePattern::Always:    rev = true; break;
                        case ReversePattern::PingPong:  rev = (blocCWindowIndex % 2) != 0; break;
                        case ReversePattern::EveryTwo:  rev = (blocCWindowIndex % 2) == 0; break;
                        case ReversePattern::EveryFour: rev = (blocCWindowIndex % 4) == 0; break;
                        case ReversePattern::Random:    rev = blocCRandom.nextFloat() < (blocCChance / 100.0f); break;
                    }
                }

                reverseEngine.resetToLiveBoundary(numSamples - splitIndex, blocCDivSamples,
                                                  /*hardSnap*/ blocCNeedsHardSnap);
                blocCNeedsHardSnap = false;
                blocCCurrentReversed = rev;
                blocCGate.setTargetValue(rev ? 1.0f : 0.0f);
                blocCFreeRun = 0;

                blendSeg(splitIndex, numSamples - splitIndex);
                blocCFreeRun += numSamples - splitIndex;
                }
                else
                {
                    blocCGate.setTargetValue(blocCCurrentReversed ? 1.0f : 0.0f);
                    blendSeg(0, numSamples);
                    blocCFreeRun += numSamples;
                }
            }
        }
    }

    // buffer now holds the fully processed WET signal (Bloc A -> B -> C).

    // --- Band: band-pass on the wet signal only ---
    const float nyquistGuard = static_cast<float>(currentSampleRate) * 0.49f;
    const float bandLowCut  = juce::jlimit(20.0f, nyquistGuard,
        apvts.getRawParameterValue(ParamIDs::bandLowCutHz)->load());
    const float bandHighCut = juce::jlimit(20.0f, nyquistGuard,
        apvts.getRawParameterValue(ParamIDs::bandHighCutHz)->load());

    smoothedBandLowCutHz.setTargetValue(bandLowCut);
    smoothedBandLowCutHz.skip(juce::jmax(0, numSamples - 1));
    smoothedBandHighCutHz.setTargetValue(bandHighCut);
    smoothedBandHighCutHz.skip(juce::jmax(0, numSamples - 1));

    bandHighPass.setCutoffFrequency(smoothedBandLowCutHz.getNextValue());
    bandLowPass.setCutoffFrequency(smoothedBandHighCutHz.getNextValue());

    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        bandHighPass.process(ctx);
        bandLowPass.process(ctx);
    }

    // --- Aligned dry: read the dry path back by the wet path's latency ---
    const int dryDelaySamples = blocAEnabled ? cachedBlocALatencySamples : 0;
    const juce::int64 dryBlockStart = dryDelayWriteHead - numSamples;
    dryBuffer.setSize(buffer.getNumChannels(), numSamples, false, false, true);
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        const int ringCh = juce::jmin(ch, dryDelayRing.getNumChannels() - 1);
        for (int i = 0; i < numSamples; ++i)
        {
            const juce::int64 readAbs = dryBlockStart + i - dryDelaySamples;
            const auto idx = static_cast<int>(((readAbs % dryDelaySize) + dryDelaySize) % dryDelaySize);
            dryBuffer.setSample(ch, i, dryDelayRing.getSample(ringCh, idx));
        }
    }

    // --- Fade In/Out (engage envelope) ---
    const float fadeInBars  = apvts.getRawParameterValue(ParamIDs::fadeInBars)->load();
    const float fadeOutBars = apvts.getRawParameterValue(ParamIDs::fadeOutBars)->load();
    const double barSeconds = static_cast<double>(hostSync.numerator)
                              * (4.0 / static_cast<double>(hostSync.denominator))
                              * (60.0 / hostSync.bpm);
    const float fadeInSamples  = static_cast<float>(fadeInBars  * barSeconds * currentSampleRate);
    const float fadeOutSamples = static_cast<float>(fadeOutBars * barSeconds * currentSampleRate);
    const float upStep   = fadeInSamples  > 1.0f ? 1.0f / fadeInSamples  : 1.0f;
    const float downStep = fadeOutSamples > 1.0f ? 1.0f / fadeOutSamples : 1.0f;

    // Always engaged for now; Fade In/Out will drive this envelope once wired
    // to a trigger (the Bypass toggle that used to drive it was removed).
    const float engageTarget = 1.0f;
    if (! engageInitialised)
    {
        engageGain = engageTarget;
        engageInitialised = true;
    }

    for (int i = 0; i < numSamples; ++i)
    {
        if (engageGain < engageTarget)
            engageGain = juce::jmin(engageTarget, engageGain + upStep);
        else if (engageGain > engageTarget)
            engageGain = juce::jmax(engageTarget, engageGain - downStep);

        const float g = engageGain;

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            const float dryS = dryBuffer.getSample(ch, i);
            const float wetS = buffer.getSample(ch, i);
            // Fade In/Out crossfades the full processed (wet) signal to dry.
            buffer.setSample(ch, i, dryS * (1.0f - g) + wetS * g);
        }
    }

    expectedNextPpqPosition = (hostSync.hasPpq && hostSync.bpm > 0.0)
        ? hostSync.ppqPosition + (static_cast<double>(numSamples) / currentSampleRate) * (hostSync.bpm / 60.0)
        : -1.0;

    // --- Play-to-pause capture of the final output (drag-to-DAW) ---
    // Reset on transport start, then append the finished output while playing,
    // up to the pre-allocated cap. The valid length is published last with a
    // release store so the editor thread only ever sees fully-written samples.
    if (transportStarted)
    {
        recordWritePos = 0;
        recordedSamples.store(0, std::memory_order_release);
    }

    const bool recordEnabled = apvts.getRawParameterValue(ParamIDs::recordEnabled)->load() >= 0.5f;
    if (recordEnabled && hostSync.isPlaying && recordWritePos < recordCapacity)
    {
        const int toCopy = juce::jmin(numSamples, recordCapacity - recordWritePos);
        const int chans = juce::jmin(recordBuffer.getNumChannels(), buffer.getNumChannels());
        for (int ch = 0; ch < chans; ++ch)
            recordBuffer.copyFrom(ch, recordWritePos, buffer, ch, 0, toCopy);
        recordWritePos += toCopy;
        recordedSamples.store(recordWritePos, std::memory_order_release);
    }
}

int SlowTimeAudioProcessor::getRecordingSnapshot(juce::AudioBuffer<float>& dest) const
{
    const int n = recordedSamples.load(std::memory_order_acquire);
    const int chans = recordBuffer.getNumChannels();
    if (n <= 0 || chans <= 0)
    {
        dest.setSize(0, 0);
        return 0;
    }

    dest.setSize(chans, n, false, false, true);
    for (int ch = 0; ch < chans; ++ch)
        dest.copyFrom(ch, 0, recordBuffer, ch, 0, n);
    return n;
}

juce::AudioProcessorEditor* SlowTimeAudioProcessor::createEditor()
{
    return new SlowTimeAudioProcessorEditor(*this);
}

void SlowTimeAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = apvts.copyState(); auto xml = state.createXml())
        copyXmlToBinary(*xml, destData);
}

void SlowTimeAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlowTimeAudioProcessor();
}
