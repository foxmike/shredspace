#include "Analysis/AdaptiveController.h"

#include <algorithm>
#include <cmath>

namespace shredspace
{

void AdaptiveController::prepare (float contractionAttackMs,
                                  float contractionReleaseMs,
                                  float bloomAttackMs,
                                  float bloomReleaseMs) noexcept
{
    // Each call writes distinct coefficient storage for every destination.
    prepareDestination (wetSmoother_,
                        contractionAttackMs,
                        contractionReleaseMs,
                        bloomAttackMs,
                        bloomReleaseMs);
    prepareDestination (tailSmoother_,
                        contractionAttackMs,
                        contractionReleaseMs,
                        bloomAttackMs,
                        bloomReleaseMs);
    prepareDestination (toneSmoother_,
                        contractionAttackMs,
                        contractionReleaseMs,
                        bloomAttackMs,
                        bloomReleaseMs);
    prepareDestination (widthSmoother_,
                        contractionAttackMs,
                        contractionReleaseMs,
                        bloomAttackMs,
                        bloomReleaseMs);
    prepareDestination (softeningSmoother_,
                        contractionAttackMs,
                        contractionReleaseMs,
                        bloomAttackMs,
                        bloomReleaseMs);
    prepareDestination (diffusionSmoother_,
                        contractionAttackMs,
                        contractionReleaseMs,
                        bloomAttackMs,
                        bloomReleaseMs);

    reset();
}

void AdaptiveController::reset() noexcept
{
    clearAllDestinations();

    published_[0] = {};
    published_[1] = {};
    activePublishedIndex_.store (0, std::memory_order_relaxed);
    publishedGeneration_.store (0, std::memory_order_relaxed);
    contractionTelemetry_.store (0.0f, std::memory_order_relaxed);
    bloomTelemetry_.store (0.0f, std::memory_order_relaxed);
    bloomHoldSamplesRemaining_ = 0;
    heldBloomTarget_ = 0.0f;
}

void AdaptiveController::setMode (Mode newMode) noexcept
{
    mode_.store (newMode, std::memory_order_release);

    if (newMode == Mode::Off)
    {
        contractionTelemetry_.store (0.0f, std::memory_order_relaxed);
        bloomTelemetry_.store (0.0f, std::memory_order_relaxed);
    }
}

AdaptiveController::Mode AdaptiveController::getMode() const noexcept
{
    return mode_.load (std::memory_order_acquire);
}

void AdaptiveController::process (float density,
                                  float smoothedOnsetActivity,
                                  float sustain,
                                  float phraseGap,
                                  float inputActivity,
                                  int numAnalysisSamples) noexcept
{
    const auto mode = mode_.load (std::memory_order_acquire);

    if (mode == Mode::Off)
    {
        clearAllDestinations();
        bloomHoldSamplesRemaining_ = 0;
        heldBloomTarget_ = 0.0f;
        contractionTelemetry_.store (0.0f, std::memory_order_relaxed);
        bloomTelemetry_.store (0.0f, std::memory_order_relaxed);
        publish (Mode::Off, {});
        return;
    }

    if (mode == Mode::Level)
    {
        // Traditional level ducking: analyzer articulation inputs are not read.
        clearDestination (tailSmoother_);
        clearDestination (toneSmoother_);
        clearDestination (widthSmoother_);
        clearDestination (softeningSmoother_);
        clearDestination (diffusionSmoother_);
        wetSmoother_.bloom.value = 0.0f;
        bloomHoldSamplesRemaining_ = 0;
        heldBloomTarget_ = 0.0f;

        const float contractionTarget = clamp01 (inputActivity) * clamp01 (definition_);
        const float wetContraction =
            advance (wetSmoother_.contraction, contractionTarget, numAnalysisSamples);

        AdaptiveDeltas deltas {};
        deltas.wetDbDelta = -wetContraction * std::max (0.0f, wetReductionDb_);

        contractionTelemetry_.store (wetContraction, std::memory_order_relaxed);
        bloomTelemetry_.store (0.0f, std::memory_order_relaxed);
        publish (Mode::Level, deltas);
        return;
    }

    const float C_target =
        std::clamp (densityWeight_ * density
                        + onsetWeight_ * smoothedOnsetActivity,
                    0.0f,
                    1.0f);
    const float contraction = C_target * clamp01 (definition_);

    const float rawBloomTarget =
        std::clamp (sustainWeight_ * sustain + gapWeight_ * phraseGap,
                    0.0f,
                    1.0f)
        * (1.0f - contraction);
    const float scaledBloomTarget =
        rawBloomTarget * clamp01 (bloomAmount_);

    if (scaledBloomTarget >= heldBloomTarget_)
    {
        heldBloomTarget_ = scaledBloomTarget;
        bloomHoldSamplesRemaining_ = static_cast<int> (
            bloomHoldMs_ * 0.001f * static_cast<float> (analysisSampleRate));
    }
    else if (bloomHoldSamplesRemaining_ > 0)
    {
        bloomHoldSamplesRemaining_ =
            std::max (0, bloomHoldSamplesRemaining_ - numAnalysisSamples);
    }
    else
    {
        heldBloomTarget_ = scaledBloomTarget;
    }

    const float bloom = heldBloomTarget_;

    const float wetC =
        advance (wetSmoother_.contraction, contraction, numAnalysisSamples);
    const float wetB = advance (wetSmoother_.bloom, bloom, numAnalysisSamples);
    const float tailC =
        advance (tailSmoother_.contraction, contraction, numAnalysisSamples);
    const float tailB = advance (tailSmoother_.bloom, bloom, numAnalysisSamples);
    const float toneC =
        advance (toneSmoother_.contraction, contraction, numAnalysisSamples);
    const float toneB = advance (toneSmoother_.bloom, bloom, numAnalysisSamples);
    const float widthC =
        advance (widthSmoother_.contraction, contraction, numAnalysisSamples);
    const float widthB = advance (widthSmoother_.bloom, bloom, numAnalysisSamples);
    const float softeningC =
        advance (softeningSmoother_.contraction, contraction, numAnalysisSamples);
    const float diffusionB =
        advance (diffusionSmoother_.bloom, bloom, numAnalysisSamples);

    AdaptiveDeltas deltas {};
    // dB remains in dB here. Conversion to linear happens only after layering.
    deltas.wetDbDelta =
        -wetC * std::max (0.0f, wetReductionDb_)
        + wetB * std::max (0.0f, wetLiftDb_);
    deltas.t60OctaveDelta =
        -tailC * std::max (0.0f, tailReductionOctaves_)
        + tailB * std::max (0.0f, tailExpansionOctaves_);
    deltas.lpfOctaveDelta =
        -toneC * std::max (0.0f, toneDarkeningOctaves_)
        + toneB * std::max (0.0f, toneOpeningOctaves_);
    deltas.widthDelta =
        -widthC * std::max (0.0f, widthReduction_)
        + widthB * std::max (0.0f, widthExpansion_);
    deltas.softeningDelta =
        softeningC * std::max (0.0f, softeningIncrease_);
    deltas.diffusionDelta =
        diffusionB * std::max (0.0f, diffusionIncrease_);

    contractionTelemetry_.store (contraction, std::memory_order_relaxed);
    bloomTelemetry_.store (bloom, std::memory_order_relaxed);
    publish (Mode::Articulation, deltas);
}

AdaptiveController::AdaptiveDeltas
AdaptiveController::getDeltasForVoice (float adaptiveRole) const noexcept
{
    if (mode_.load (std::memory_order_acquire) == Mode::Off)
        return {};

    const auto activeIndex =
        activePublishedIndex_.load (std::memory_order_acquire);
    const auto state = published_[activeIndex];

    if (state.mode == Mode::Off)
        return {};

    const float role = clamp01 (adaptiveRole);
    AdaptiveDeltas result {};
    result.wetDbDelta = role * state.deltas.wetDbDelta;

    if (state.mode == Mode::Level)
        return result;

    result.t60OctaveDelta = role * state.deltas.t60OctaveDelta;
    result.lpfOctaveDelta = role * state.deltas.lpfOctaveDelta;
    result.widthDelta = role * state.deltas.widthDelta;
    result.softeningDelta = role * state.deltas.softeningDelta;
    result.diffusionDelta = role * state.deltas.diffusionDelta;
    return result;
}

float AdaptiveController::getContraction() const noexcept
{
    return contractionTelemetry_.load (std::memory_order_relaxed);
}

float AdaptiveController::getBloom() const noexcept
{
    return bloomTelemetry_.load (std::memory_order_relaxed);
}

std::uint64_t AdaptiveController::getPublishedGeneration() const noexcept
{
    return publishedGeneration_.load (std::memory_order_acquire);
}

void AdaptiveController::setDefinition (float amount) noexcept
{
    definition_ = clamp01 (amount);
}

void AdaptiveController::setBloomAmount (float amount) noexcept
{
    bloomAmount_ = clamp01 (amount);
}

void AdaptiveController::setTimeConstants (float contractionAttackMs,
                                           float contractionReleaseMs,
                                           float bloomAttackMs,
                                           float bloomHoldMs,
                                           float bloomReleaseMs) noexcept
{
    prepareDestination (wetSmoother_, contractionAttackMs, contractionReleaseMs,
                        bloomAttackMs, bloomReleaseMs);
    prepareDestination (tailSmoother_, contractionAttackMs, contractionReleaseMs,
                        bloomAttackMs, bloomReleaseMs);
    prepareDestination (toneSmoother_, contractionAttackMs, contractionReleaseMs,
                        bloomAttackMs, bloomReleaseMs);
    prepareDestination (widthSmoother_, contractionAttackMs, contractionReleaseMs,
                        bloomAttackMs, bloomReleaseMs);
    prepareDestination (softeningSmoother_, contractionAttackMs, contractionReleaseMs,
                        bloomAttackMs, bloomReleaseMs);
    prepareDestination (diffusionSmoother_, contractionAttackMs, contractionReleaseMs,
                        bloomAttackMs, bloomReleaseMs);
    bloomHoldMs_ = std::max (0.0f, bloomHoldMs);
}

void AdaptiveController::setFusionWeights (float densityWeight,
                                           float onsetWeight,
                                           float sustainWeight,
                                           float gapWeight) noexcept
{
    densityWeight_ = std::max (0.0f, densityWeight);
    onsetWeight_ = std::max (0.0f, onsetWeight);
    sustainWeight_ = std::max (0.0f, sustainWeight);
    gapWeight_ = std::max (0.0f, gapWeight);
}

void AdaptiveController::setDestinationDepths (
    float wetReductionDb,
    float wetLiftDb,
    float tailReductionOctaves,
    float tailExpansionOctaves,
    float toneDarkeningOctaves,
    float toneOpeningOctaves,
    float widthReduction,
    float widthExpansion,
    float softeningIncrease,
    float diffusionIncrease) noexcept
{
    wetReductionDb_ = std::max (0.0f, wetReductionDb);
    wetLiftDb_ = std::max (0.0f, wetLiftDb);
    tailReductionOctaves_ = std::max (0.0f, tailReductionOctaves);
    tailExpansionOctaves_ = std::max (0.0f, tailExpansionOctaves);
    toneDarkeningOctaves_ = std::max (0.0f, toneDarkeningOctaves);
    toneOpeningOctaves_ = std::max (0.0f, toneOpeningOctaves);
    widthReduction_ = std::max (0.0f, widthReduction);
    widthExpansion_ = std::max (0.0f, widthExpansion);
    softeningIncrease_ = std::max (0.0f, softeningIncrease);
    diffusionIncrease_ = std::max (0.0f, diffusionIncrease);
}

float AdaptiveController::computeEffectiveT60 (float t60BaseSeconds,
                                                float t60OctaveDelta) noexcept
{
    return t60BaseSeconds * std::exp2 (t60OctaveDelta);
}

float AdaptiveController::computeEffectiveLpf (float lpfBaseHz,
                                               float lpfOctaveDelta) noexcept
{
    return lpfBaseHz * std::exp2 (lpfOctaveDelta);
}

float AdaptiveController::computeEffectiveWidth (float widthBase,
                                                 float widthDelta) noexcept
{
    return std::clamp (widthBase + widthDelta, 0.0f, 2.0f);
}

float AdaptiveController::computeEffectiveSoftening (float softeningBase,
                                                     float softeningDelta) noexcept
{
    return std::clamp (softeningBase + softeningDelta, 0.0f, 1.0f);
}

float AdaptiveController::computeEffectiveDiffusion (float diffusionBase,
                                                     float diffusionDelta) noexcept
{
    return std::clamp (diffusionBase + diffusionDelta, 0.0f, 0.40f);
}

float AdaptiveController::wetDbToLinear (float wetBaseDb,
                                         float wetDbDelta) noexcept
{
    return std::pow (10.0f, (wetBaseDb + wetDbDelta) / 20.0f);
}

float AdaptiveController::computeCappedFeedback (
    float delayTimeSeconds,
    float t60BaseSeconds,
    float t60OctaveDelta) noexcept
{
    const float t60Effective =
        computeEffectiveT60 (t60BaseSeconds, t60OctaveDelta);

    if (! std::isfinite (t60Effective) || t60Effective <= 0.0f)
        return 0.0f;

    const float delay = std::max (0.0f, delayTimeSeconds);
    const float gRequested =
        std::pow (10.0f, -3.0f * delay / t60Effective);
    return std::min (gRequested, maximumFeedback);
}

float AdaptiveController::clamp01 (float value) noexcept
{
    return std::clamp (value, 0.0f, 1.0f);
}

float AdaptiveController::coefficientForMilliseconds (float milliseconds) noexcept
{
    const float seconds = std::max (milliseconds, 0.001f) * 0.001f;
    return std::exp (-1.0f
                     / (seconds * static_cast<float> (analysisSampleRate)));
}

void AdaptiveController::prepareDestination (DestinationSmoother& destination,
                                             float contractionAttackMs,
                                             float contractionReleaseMs,
                                             float bloomAttackMs,
                                             float bloomReleaseMs) noexcept
{
    destination.contraction.attackCoefficient =
        coefficientForMilliseconds (contractionAttackMs);
    destination.contraction.releaseCoefficient =
        coefficientForMilliseconds (contractionReleaseMs);
    destination.bloom.attackCoefficient =
        coefficientForMilliseconds (bloomAttackMs);
    destination.bloom.releaseCoefficient =
        coefficientForMilliseconds (bloomReleaseMs);
}

void AdaptiveController::clearDestination (
    DestinationSmoother& destination) noexcept
{
    destination.contraction.value = 0.0f;
    destination.bloom.value = 0.0f;
}

float AdaptiveController::advance (OnePole& smoother,
                                   float target,
                                   int numAnalysisSamples) noexcept
{
    if (numAnalysisSamples <= 0)
        return smoother.value;

    const float coefficient = target > smoother.value
                                  ? smoother.attackCoefficient
                                  : smoother.releaseCoefficient;
    const float blockCoefficient =
        std::pow (coefficient, static_cast<float> (numAnalysisSamples));
    smoother.value = target + blockCoefficient * (smoother.value - target);
    return smoother.value;
}

void AdaptiveController::clearAllDestinations() noexcept
{
    clearDestination (wetSmoother_);
    clearDestination (tailSmoother_);
    clearDestination (toneSmoother_);
    clearDestination (widthSmoother_);
    clearDestination (softeningSmoother_);
    clearDestination (diffusionSmoother_);
}

void AdaptiveController::publish (Mode mode,
                                  const AdaptiveDeltas& deltas) noexcept
{
    const auto activeIndex =
        activePublishedIndex_.load (std::memory_order_relaxed);
    const auto nextIndex = 1U - activeIndex;
    published_[nextIndex] = { deltas, mode };
    activePublishedIndex_.store (nextIndex, std::memory_order_release);
    publishedGeneration_.fetch_add (1, std::memory_order_release);
}

} // namespace shredspace
