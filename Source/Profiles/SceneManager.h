#pragma once

#include <array>
#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>
#include "../Parameters/MacroEngine.h"
#include "../DSP/RoutingEngine.h"
#include "../DSP/DelayTimeController.h"
#include "ProfileSnapshot.h"

namespace shredspace
{

enum class SceneId
{
    A = 0,
    B = 1,
    C = 2,
    D = 3
};

struct SceneValues
{
    std::array<float, 8> macroTargets {};
    float mixTarget = 0.0f;
    int routingTarget = 0;
    int holdBehaviour = 0;
};

// Global morph helper functions as allowed in allowed_symbols
inline float morphLinear (float p_A, float p_B, float lambda) noexcept
{
    return (1.0f - lambda) * p_A + lambda * p_B;
}

inline float morphLogarithmic (float p_A, float p_B, float lambda) noexcept
{
    if (p_A > 0.0f && p_B > 0.0f)
        return p_A * std::pow (p_B / p_A, lambda);
    return morphLinear (p_A, p_B, lambda);
}

class SceneManager
{
public:
    SceneManager() noexcept;
    SceneManager (RoutingEngine* routing, DelayTimeController* delayTime) noexcept;
    ~SceneManager() = default;

    void registerEngines (RoutingEngine* routing, DelayTimeController* delayTime) noexcept;

    void prepare (double sampleRate, int maxBlockSize) noexcept;

    void setSceneValues (SceneId sceneId, const SceneValues& values) noexcept;
    void setSceneValues (const std::array<SceneValues, kSceneCount>& allSceneValues) noexcept;
    void setSceneValues (const ProfileSnapshotData& snapshot) noexcept;

    void setMorphPosition (SceneId a, SceneId b, float lambda) noexcept;
    void setMorphPosition (int sceneSelect, float morphPercent) noexcept;
    void setMorphPosition (float lambda) noexcept;

    [[nodiscard]] float getSceneDelta (MacroId macro, float baseValue) const noexcept;
    [[nodiscard]] float getSceneDelta (MacroId macro) const noexcept;

    [[nodiscard]] float getMixDelta (float baseValue) const noexcept;
    [[nodiscard]] float getMixDelta() const noexcept;

    [[nodiscard]] SceneId getActiveSceneId() const noexcept { return activeScene_; }
    [[nodiscard]] int getActiveSceneIndex() const noexcept { return static_cast<int> (activeScene_); }
    [[nodiscard]] int getActiveRouting() const noexcept { return activeRouting_; }
    [[nodiscard]] int getActiveHoldBehaviour() const noexcept { return activeHoldBehaviour_; }

private:
    [[nodiscard]] float getMorphedTarget (MacroId macro) const noexcept;
    [[nodiscard]] float getMorphedMixTarget() const noexcept;

    void updateDiscreteTargets() noexcept;

    RoutingEngine* routingEngine_ { nullptr };
    DelayTimeController* delayTimeController_ { nullptr };

    std::array<SceneValues, kSceneCount> scenes_ {};
    
    SceneId sceneA_ { SceneId::A };
    SceneId sceneB_ { SceneId::B };
    float lambda_ { 0.0f };

    SceneId activeScene_ { SceneId::A };
    int activeRouting_ { 0 };
    int activeHoldBehaviour_ { 0 };

    double sampleRate_ { 44100.0 };
    int maxBlockSize_ { 256 };
};

} // namespace shredspace
