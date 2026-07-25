#include "SceneManager.h"
#include <cmath>
#include <algorithm>

namespace shredspace
{

SceneManager::SceneManager() noexcept
{
}

SceneManager::SceneManager (RoutingEngine* routing, DelayTimeController* delayTime) noexcept
    : routingEngine_ (routing), delayTimeController_ (delayTime)
{
}

void SceneManager::registerEngines (RoutingEngine* routing, DelayTimeController* delayTime) noexcept
{
    routingEngine_ = routing;
    delayTimeController_ = delayTime;
}

void SceneManager::prepare (double sampleRate, int maxBlockSize) noexcept
{
    sampleRate_ = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlockSize_ = std::max (1, maxBlockSize);

    // Initial discrete target apply
    if (routingEngine_ != nullptr)
    {
        routingEngine_->setTopology (static_cast<RoutingEngine::Topology> (activeRouting_));
    }
}

void SceneManager::setSceneValues (SceneId sceneId, const SceneValues& values) noexcept
{
    scenes_[static_cast<std::size_t> (sceneId)] = values;
    updateDiscreteTargets();
}

void SceneManager::setSceneValues (const std::array<SceneValues, kSceneCount>& allSceneValues) noexcept
{
    scenes_ = allSceneValues;
    updateDiscreteTargets();
}

void SceneManager::setSceneValues (const ProfileSnapshotData& snapshot) noexcept
{
    for (std::size_t i = 0; i < kSceneCount; ++i)
    {
        SceneValues sv;
        for (std::size_t m = 0; m < 8; ++m)
            sv.macroTargets[m] = snapshot.scenes[i].macroTargets[m];
        sv.mixTarget = snapshot.scenes[i].mixTarget;
        sv.routingTarget = snapshot.scenes[i].routingTarget;
        sv.holdBehaviour = snapshot.scenes[i].holdBehaviour;
        
        scenes_[i] = sv;
    }
    updateDiscreteTargets();
}

void SceneManager::setMorphPosition (SceneId a, SceneId b, float lambda) noexcept
{
    sceneA_ = a;
    sceneB_ = b;
    lambda_ = std::clamp (lambda, 0.0f, 1.0f);

    updateDiscreteTargets();
}

void SceneManager::setMorphPosition (int sceneSelect, float morphPercent) noexcept
{
    int indexA = std::clamp (sceneSelect, 0, 3);
    int indexB = std::clamp (indexA + 1, 0, 3);
    
    setMorphPosition (static_cast<SceneId> (indexA),
                      static_cast<SceneId> (indexB),
                      std::clamp (morphPercent * 0.01f, 0.0f, 1.0f));
}

void SceneManager::setMorphPosition (float lambda) noexcept
{
    setMorphPosition (sceneA_, sceneB_, lambda);
}

float SceneManager::getSceneDelta (MacroId macro, float baseValue) const noexcept
{
    return getMorphedTarget (macro) - baseValue;
}

float SceneManager::getSceneDelta (MacroId macro) const noexcept
{
    return getMorphedTarget (macro);
}

float SceneManager::getMixDelta (float baseValue) const noexcept
{
    return getMorphedMixTarget() - baseValue;
}

float SceneManager::getMixDelta() const noexcept
{
    return getMorphedMixTarget();
}

float SceneManager::getMorphedTarget (MacroId macro) const noexcept
{
    const auto idx = static_cast<std::size_t> (macro);
    float p_A = scenes_[static_cast<std::size_t> (sceneA_)].macroTargets[idx];
    float p_B = scenes_[static_cast<std::size_t> (sceneB_)].macroTargets[idx];

    if (macro == MacroId::TIME || macro == MacroId::TAIL)
    {
        return morphLogarithmic (p_A, p_B, lambda_);
    }
    return morphLinear (p_A, p_B, lambda_);
}

float SceneManager::getMorphedMixTarget() const noexcept
{
    float p_A = scenes_[static_cast<std::size_t> (sceneA_)].mixTarget;
    float p_B = scenes_[static_cast<std::size_t> (sceneB_)].mixTarget;
    return morphLinear (p_A, p_B, lambda_);
}

void SceneManager::updateDiscreteTargets() noexcept
{
    const auto newActiveScene = (lambda_ < 0.5f) ? sceneA_ : sceneB_;
    activeScene_ = newActiveScene;

    const auto& sceneData = scenes_[static_cast<std::size_t> (activeScene_)];

    // Update routing target
    const auto newRouting = sceneData.routingTarget;
    if (newRouting != activeRouting_)
    {
        activeRouting_ = newRouting;
        if (routingEngine_ != nullptr)
        {
            routingEngine_->setTopology (static_cast<RoutingEngine::Topology> (activeRouting_));
        }
    }

    // Update hold behavior
    activeHoldBehaviour_ = sceneData.holdBehaviour;
}

} // namespace shredspace
