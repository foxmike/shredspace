// This engine never writes APVTS state; it only reads base values and produces
// internal deltas and effective values.
#pragma once

#include <array>
#include <cstddef>
#include <span>

#include <juce_audio_basics/juce_audio_basics.h>

#include "../Profiles/ProfileSchema.h"

namespace shredspace
{

enum class MacroId
{
    TIME,
    SPACE,
    DEFINITION,
    BLOOM,
    TAIL,
    FOCUS,
    WIDTH,
    CHARACTER
};

enum class ParameterDomain
{
    linear,
    decibels,
    octaveScaled
};

inline constexpr std::size_t kMaxMacroDestinations = 16;

using MacroDestinationTable = std::span<const MacroDestination>;

/**
 * Applies profile-supplied macro mappings without changing base parameters.
 *
 * Intended profile coordination:
 * - TIME drives the DelayTimeController; TAIL sets nominal T60.
 * - SPACE coordinates voice levels, wet prominence, diffusion, and bloom
 *   availability.
 * - DEFINITION coordinates contraction strength destinations.
 * - BLOOM coordinates bloom strength destinations.
 * - FOCUS coordinates HPF, LPF, guard, and softening.
 * - WIDTH coordinates pan, width, offset, cross-feedback, modulation phase,
 *   and LF mono.
 * - CHARACTER coordinates saturation, modulation depth, drift, diffusion,
 *   attack softness, and bandwidth.
 *
 * Destination IDs remain profile data. They are copied only while installing
 * a table and are never inspected by the real-time evaluation path.
 */
class MacroEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    bool setDestinationTable (MacroId macro, MacroDestinationTable destinations);
    void setMacroValue (MacroId macro, float normalizedValue) noexcept;

    /** Advances smoothing and evaluates all installed deltas once per block. */
    void processDeltas (int numSamples) noexcept;

    /** Returns a delta by its position in the installed profile table. */
    [[nodiscard]] float getMacroDelta (MacroId macro,
                                       std::size_t destinationIndex) const noexcept;

private:
    static constexpr std::size_t kMacroCount = 8;

    struct CompiledDestination
    {
        float deltaMin = 0.0f;
        float deltaMax = 0.0f;
        float curve = 1.0f;
    };

    using Smoother = juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>;

    static constexpr std::size_t toIndex (MacroId macro) noexcept
    {
        return static_cast<std::size_t> (macro);
    }

    std::array<Smoother, kMacroCount> macroPositions {};
    std::array<float, kMacroCount> macroTargets {};
    std::array<std::array<CompiledDestination, kMaxMacroDestinations>, kMacroCount> destinations {};
    std::array<std::array<float, kMaxMacroDestinations>, kMacroCount> deltas {};
    std::array<std::size_t, kMacroCount> destinationCounts {};
    int maximumBlockSize = 1;
    double currentSampleRate = 44100.0;
    bool prepared = false;
};

/**
 * Layers base, macro, scene, and adaptive values in the destination domain.
 * Decibel results are smoothed and clamped in dB before this conversion point.
 */
[[nodiscard]] float computeEffectiveValue (ParameterDomain domain,
                                           float base,
                                           float macroDelta,
                                           float sceneDelta,
                                           float adaptiveDelta,
                                           float minimum,
                                           float maximum) noexcept;

} // namespace shredspace
