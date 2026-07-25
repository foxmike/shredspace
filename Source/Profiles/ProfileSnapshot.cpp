#include "ProfileSnapshot.h"

#include <algorithm>

namespace shredspace
{

void ProfileSnapshotHandoff::publish (const ProfileSnapshotData& snapshot) noexcept
{
    const auto inactiveIndex = 1 - activeIndex.load (std::memory_order_relaxed);
    slots[static_cast<std::size_t> (inactiveIndex)] = snapshot;
    activeIndex.store (inactiveIndex, std::memory_order_release);
    generation.fetch_add (1, std::memory_order_release);
}

const ProfileSnapshotData& ProfileSnapshotHandoff::acquire() const noexcept
{
    [[maybe_unused]] const auto observedGeneration = generation.load (std::memory_order_acquire);
    const auto index = activeIndex.load (std::memory_order_acquire);
    return slots[static_cast<std::size_t> (index)];
}

void buildSnapshot (const ProfileDocument& document,
                    const juce::StringArray& parameterIds,
                    ProfileSnapshotData& out)
{
    out = {};

    const auto parameterCount = std::min (static_cast<std::size_t> (parameterIds.size()),
                                          kMaxSnapshotParameters);
    out.baseParameterCount = static_cast<int> (parameterCount);

    for (std::size_t parameterIndex = 0; parameterIndex < parameterCount; ++parameterIndex)
    {
        const auto value = document.base_parameter_values.find (
            parameterIds[static_cast<int> (parameterIndex)]);

        if (value != document.base_parameter_values.end())
            out.baseParameterValues[parameterIndex] = value->second;
    }

    for (std::size_t macroIndex = 0; macroIndex < kMacroCount; ++macroIndex)
    {
        const auto mapping = document.macro_mapping.find (kMacroNames[macroIndex]);
        if (mapping == document.macro_mapping.end())
            continue;

        auto& destinationCount = out.macroDestinationCounts[macroIndex];

        for (const auto& source : mapping->second)
        {
            const auto destinationIndex = parameterIds.indexOf (source.destination_parameter_id);
            if (destinationIndex < 0
                || destinationIndex >= static_cast<int> (kMaxSnapshotParameters)
                || destinationCount >= static_cast<int> (kMaxDestinationsPerMacro))
                continue;

            auto& destination =
                out.macroDestinations[macroIndex][static_cast<std::size_t> (destinationCount)];
            destination.destinationIndex = destinationIndex;
            destination.deltaMin = source.delta_min;
            destination.deltaMax = source.delta_max;
            destination.curve = source.curve;
            ++destinationCount;
        }
    }

    const auto sceneCount = std::min (document.scene_values.size(), kSceneCount);
    for (std::size_t sceneIndex = 0; sceneIndex < sceneCount; ++sceneIndex)
    {
        const auto& source = document.scene_values[sceneIndex];
        auto& destination = out.scenes[sceneIndex];

        for (std::size_t macroIndex = 0; macroIndex < kMacroCount; ++macroIndex)
        {
            const auto target = source.macro_targets.find (kMacroNames[macroIndex]);
            if (target != source.macro_targets.end())
                destination.macroTargets[macroIndex] = target->second;
        }

        destination.mixTarget = source.mix_target.value;
        destination.routingTarget = static_cast<int> (source.routing_target.value);
        destination.holdBehaviour = static_cast<int> (source.hold_behavior);
    }
}

} // namespace shredspace
