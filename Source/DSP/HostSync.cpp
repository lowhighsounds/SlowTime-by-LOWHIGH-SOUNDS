#include "HostSync.h"
#include <cmath>

HostSyncInfo queryHostSync(juce::AudioProcessor& processor)
{
    HostSyncInfo info;

    if (auto* playHead = processor.getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            info.bpm = pos->getBpm().orFallback(120.0);
            if (auto sig = pos->getTimeSignature())
            {
                info.numerator = sig->numerator;
                info.denominator = sig->denominator;
            }
            info.isPlaying = pos->getIsPlaying();
            info.hasPosition = true;

            if (auto ppq = pos->getPpqPosition())
            {
                info.ppqPosition = *ppq;
                info.hasPpq = true;
            }
            if (auto barStart = pos->getPpqPositionOfLastBarStart())
            {
                info.ppqPositionOfLastBarStart = *barStart;
                info.hasBarStart = true;
            }
        }
    }

    if (info.bpm <= 0.0)
        info.bpm = 120.0;
    if (info.numerator <= 0)
        info.numerator = 4;
    if (info.denominator <= 0)
        info.denominator = 4;

    return info;
}

double loopDivisionToQuarterNotes(LoopDivision division, int numerator, int denominator)
{
    const double beatQuarterNotes = 4.0 / static_cast<double>(denominator);

    switch (division)
    {
        case LoopDivision::Sixteenth:      return beatQuarterNotes * 0.25;
        case LoopDivision::EighthTriplet:  return beatQuarterNotes * (1.0 / 3.0);
        case LoopDivision::Eighth:         return beatQuarterNotes * 0.5;
        case LoopDivision::Quarter:        return beatQuarterNotes * 1.0;
        case LoopDivision::Half:           return beatQuarterNotes * 2.0;
        case LoopDivision::OneBar:         return beatQuarterNotes * numerator;
        case LoopDivision::TwoBars:        return beatQuarterNotes * numerator * 2;
        case LoopDivision::FourBars:       return beatQuarterNotes * numerator * 4;
    }
    return beatQuarterNotes;
}

int loopDivisionToSamples(LoopDivision division, const HostSyncInfo& hostSync, double sampleRate)
{
    const double quarterNotes = loopDivisionToQuarterNotes(division, hostSync.numerator, hostSync.denominator);
    const double seconds = quarterNotes * (60.0 / hostSync.bpm);
    return juce::jmax(1, static_cast<int>(std::round(seconds * sampleRate)));
}

int samplesUntilNextBoundaryWithinBlock(const HostSyncInfo& hostSync, LoopDivision division,
                                         int numSamples, double sampleRate)
{
    if (! hostSync.hasPpq || hostSync.bpm <= 0.0)
        return -1;

    const double divisionQn = loopDivisionToQuarterNotes(division, hostSync.numerator, hostSync.denominator);
    if (divisionQn <= 0.0)
        return -1;

    // Always reference the absolute timeline origin (ppq 0), not "the start
    // of the current bar". ppqPositionOfLastBarStart resets every single bar,
    // so for a division longer than one bar (TwoBars, FourBars) using it as
    // the modulo reference made the target boundary jump forward by a full
    // bar every time the bar advanced -- the delta to it never actually
    // reached zero, so those two divisions never re-triggered past the very
    // first boundary. Divisions that are <= one bar are unaffected: the bar
    // grid and the ppq-0 grid coincide for them (a bar length is always an
    // exact multiple of those division lengths), so this is a strict fix,
    // not a behaviour change, for every division except TwoBars/FourBars.
    const double relative = hostSync.ppqPosition;

    // Smallest boundary at-or-after `relative` (a tiny tolerance either side
    // of an exact multiple counts as "at" it, so a block landing precisely on
    // the grid triggers now instead of being skipped for a whole extra
    // division). Using floor(x + eps) + 1 here instead always skipped the
    // boundary exactly at `relative` itself -- confirmed by ear and by a
    // reset-boundary log: the very first pass, which starts exactly at
    // ppq 0.0, hit this on every boundary landing on a clean multiple (14,
    // 28 at 140 BPM/2 qn divisions), doubling those windows. Every later pass
    // starts from a hard reset a hair off 0.0 (float accumulation), so it
    // never lands exactly on a multiple again and the bug goes quiet --
    // which is why it only ever showed up on the first time through.
    const double toleranceQn = 1.0e-6;
    const double k = std::ceil((relative - toleranceQn) / divisionQn);
    const double nextBoundaryPpq = k * divisionQn;

    const double ppqDelta = nextBoundaryPpq - hostSync.ppqPosition;
    const double samplesDelta = ppqDelta * (60.0 / hostSync.bpm) * sampleRate;

    if (samplesDelta >= 0.0 && samplesDelta < static_cast<double>(numSamples))
    {
        // Round-to-nearest can push a delta that's a hair under numSamples
        // (e.g. 479.6 of 480) up to numSamples itself -- one past the last
        // valid in-block index. Clamp it into this block (firing a fraction
        // of a sample early) rather than deferring to the next block: deferral
        // was tried and is unsafe in general -- it relies on the next block's
        // ppq landing back near delta 0 for the *same* boundary, which only
        // happens when the division length happens to be an exact multiple of
        // the block size. For the (common) case where it isn't, by the next
        // block `ceil()` has already moved on to targeting the boundary
        // *after* this one, permanently skipping it. Clamping instead
        // guarantees every boundary is always handled somewhere -- in the
        // rare exact-alignment case this can fire the same boundary again on
        // the next block too (one sample apart, so audibly identical), but it
        // can never lose one outright, which matters far more.
        return juce::jlimit(0, numSamples - 1, static_cast<int>(std::round(samplesDelta)));
    }

    return -1;
}
