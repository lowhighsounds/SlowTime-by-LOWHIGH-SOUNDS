#include "VibeModeMap.h"
#include <cmath>

VibeModeParams vibeModeToParams(VibeMode mode)
{
    float ratio = 1.0f;
    switch (mode)
    {
        // "1.5x" is a mode NAME, not the literal stretch ratio. Measured
        // against real Deja Vu, this mode plays at 3/4 speed (ratio 4/3),
        // NOT 2/3 (ratio 1.5): its pitch sits ~2 semitones higher (-5 st, a
        // fourth, vs -7 st a fifth), and 3/4 speed is exactly what turns a
        // straight groove into triplets -- source 8ths at 0.5-beat spacing
        // stretch by 4/3 to 0.667-beat spacing = triplet quarters. Ratio 1.5
        // gave neither the right pitch nor the triplet feel, which is why it
        // sounded like a pitch-only variant of 2x. (PDF page 1: "-5 semitons
        // ... convertendo grooves retos em tercinas".)
        case VibeMode::Ratio1_5x:   ratio = 4.0f / 3.0f;  break;
        case VibeMode::Ratio2x:     ratio = 2.0f;         break;  // octave down
        case VibeMode::Ratio4x:     ratio = 4.0f;         break;  // two octaves down
    }

    const float pitchSemitones = -12.0f * std::log2(ratio);
    return { ratio, pitchSemitones };
}
