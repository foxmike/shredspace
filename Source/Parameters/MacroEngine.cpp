// This engine never writes APVTS state; it only reads base values and produces
// internal deltas and effective values.
#include "MacroEngine.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{
namespace
{
constexpr double kMacroSmoothingSeconds = 0.030;
} // namespace

void MacroEngine::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    maximumBlockSize = std::max (1, maxBlockSize);

    for (std::size_t macroIndex = 0; macroIndex < kMacroCount; ++macroIndex)
    {
        auto& smoother = macroPositions[macroIndex];
        smoother.setCurrentAndTargetValue (std::clamp (macroTargets[macroIndex], 0.0f, 1.0f));
        smoother.reset (currentSampleRate, kMacroSmoothingSeconds);
    }

    prepared = true;
}

bool MacroEngine::setDestinationTable (MacroId macro, MacroDestinationTable table)
{
    const auto macroIndex = toIndex (macro);
    if (macroIndex >= kMacroCount || table.size() > kMaxMacroDestinations)
        return false;

    destinationCounts[macroIndex] = table.size();

    for (std::size_t index = 0; index < table.size(); ++index)
    {
        const auto& source = table[index];
        destinations[macroIndex][index] = {
            source.delta_min,
            source.delta_max,
            source.curve > 0.0f ? source.curve : 1.0f
        };
        deltas[macroIndex][index] = 0.0f;
    }

    for (std::size_t index = table.size(); index < kMaxMacroDestinations; ++index)
    {
        destinations[macroIndex][index] = {};
        deltas[macroIndex][index] = 0.0f;
    }

    return true;
}

void MacroEngine::setMacroValue (MacroId macro, float normalizedValue) noexcept
{
    const auto macroIndex = toIndex (macro);
    if (macroIndex >= kMacroCount)
        return;

    const auto target = std::clamp (normalizedValue, 0.0f, 1.0f);
    macroTargets[macroIndex] = target;

    if (prepared)
        macroPositions[macroIndex].setTargetValue (target);
    else
        macroPositions[macroIndex].setCurrentAndTargetValue (target);
}

void MacroEngine::processDeltas (int numSamples) noexcept
{
    const auto samplesToAdvance = std::clamp (numSamples, 0, maximumBlockSize);

    for (std::size_t macroIndex = 0; macroIndex < kMacroCount; ++macroIndex)
    {
        auto& smoother = macroPositions[macroIndex];
        const auto normalizedValue = samplesToAdvance > 0
                                         ? smoother.skip (samplesToAdvance)
                                         : smoother.getCurrentValue();
        const auto count = destinationCounts[macroIndex];

        for (std::size_t destinationIndex = 0;
             destinationIndex < count;
             ++destinationIndex)
        {
            const auto& destination = destinations[macroIndex][destinationIndex];
            const auto clampedU = std::clamp (normalizedValue, 0.0f, 1.0f);
            const auto curve = destination.curve > 0.0f ? destination.curve : 1.0f;
            const auto curvedU = std::pow (clampedU, curve);
            deltas[macroIndex][destinationIndex] =
                destination.deltaMin
                + curvedU * (destination.deltaMax - destination.deltaMin);
        }
    }
}

float MacroEngine::getMacroDelta (MacroId macro,
                                  std::size_t destinationIndex) const noexcept
{
    const auto macroIndex = toIndex (macro);
    if (macroIndex >= kMacroCount
        || destinationIndex >= destinationCounts[macroIndex])
        return 0.0f;

    return deltas[macroIndex][destinationIndex];
}

float computeEffectiveValue (ParameterDomain domain,
                             float base,
                             float macroDelta,
                             float sceneDelta,
                             float adaptiveDelta,
                             float minimum,
                             float maximum) noexcept
{
    if (minimum > maximum)
        std::swap (minimum, maximum);

    switch (domain)
    {
        case ParameterDomain::linear:
            return std::clamp (base + macroDelta + sceneDelta + adaptiveDelta,
                               minimum,
                               maximum);

        case ParameterDomain::decibels:
        {
            const auto decibels = std::clamp (base + macroDelta + sceneDelta + adaptiveDelta,
                                               minimum,
                                               maximum);
            return std::pow (10.0f, decibels / 20.0f);
        }

        case ParameterDomain::octaveScaled:
            return std::clamp (base * std::exp2 (macroDelta + sceneDelta + adaptiveDelta),
                               minimum,
                               maximum);
    }

    return std::clamp (base, minimum, maximum);
}

} // namespace shredspace
