#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

#include <juce_core/juce_core.h>

#include "ProfileSchema.h"

namespace shredspace
{

inline constexpr std::size_t kMaxSnapshotParameters = 128;
inline constexpr std::size_t kMacroCount = 8;
inline constexpr std::size_t kMaxDestinationsPerMacro = 16;
inline constexpr std::size_t kSceneCount = 4;

struct ProfileSnapshotData
{
    struct MacroDestinationData
    {
        int destinationIndex = -1;
        float deltaMin = 0.0f;
        float deltaMax = 0.0f;
        float curve = 0.0f;
    };

    struct SceneData
    {
        std::array<float, kMacroCount> macroTargets {};
        float mixTarget = 0.0f;
        int routingTarget = 0;
        int holdBehaviour = 0;
    };

    std::array<float, kMaxSnapshotParameters> baseParameterValues {};
    int baseParameterCount = 0;
    std::array<std::array<MacroDestinationData, kMaxDestinationsPerMacro>, kMacroCount> macroDestinations {};
    std::array<int, kMacroCount> macroDestinationCounts {};
    std::array<SceneData, kSceneCount> scenes {};
};

// Strings and metadata never enter the snapshot; these assertions enforce its
// numeric, fixed-storage cross-thread representation.
static_assert (std::is_trivially_copyable_v<ProfileSnapshotData>);
static_assert (std::is_standard_layout_v<ProfileSnapshotData>);
static_assert (kMacroCount == 8);

class ProfileSnapshotHandoff
{
public:
    void publish (const ProfileSnapshotData& snapshot) noexcept;
    [[nodiscard]] const ProfileSnapshotData& acquire() const noexcept;

private:
    std::array<ProfileSnapshotData, 2> slots {};
    std::atomic<int> activeIndex { 0 };
    std::atomic<std::uint32_t> generation { 0 };
};

void buildSnapshot (const ProfileDocument& document,
                    const juce::StringArray& parameterIds,
                    ProfileSnapshotData& out);

} // namespace shredspace
