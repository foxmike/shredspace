#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <type_traits>

namespace shredspace
{

/**
 * Fuses fixed-rate analyzer outputs into per-voice adaptive destination
 * deltas. The controller never writes base parameter values; callers layer
 * the returned deltas into their internal effective values.
 */
class AdaptiveController
{
public:
    enum class Mode
    {
        Off,
        Level,
        Articulation
    };

    /** POD payload published by process() and scaled by getDeltasForVoice(). */
    struct AdaptiveDeltas
    {
        float wetDbDelta;
        float t60OctaveDelta;
        float lpfOctaveDelta;
        float widthDelta;
        float softeningDelta;
        float diffusionDelta;
    };

    static constexpr double analysisSampleRate = 48000.0;
    static constexpr float maximumFeedback = 0.985f;

    AdaptiveController() noexcept = default;
    AdaptiveController (const AdaptiveController&) = delete;
    AdaptiveController& operator= (const AdaptiveController&) = delete;
    AdaptiveController (AdaptiveController&&) = delete;
    AdaptiveController& operator= (AdaptiveController&&) = delete;

    /**
     * Precomputes every destination's independent one-pole coefficients.
     * Defaults match the intelligence controls' engineering defaults.
     */
    void prepare (float contractionAttackMs = 8.0f,
                  float contractionReleaseMs = 220.0f,
                  float bloomAttackMs = 220.0f,
                  float bloomReleaseMs = 800.0f) noexcept;

    /** Clears all smoothing, telemetry, and published delta state. */
    void reset() noexcept;

    void setMode (Mode newMode) noexcept;
    [[nodiscard]] Mode getMode() const noexcept;

    /**
     * Advances the controller by a block of samples at analysisSampleRate.
     * Level mode intentionally consumes inputActivity only.
     */
    void process (float density,
                  float smoothedOnsetActivity,
                  float sustain,
                  float phraseGap,
                  float inputActivity,
                  int numAnalysisSamples) noexcept;

    /** Returns the latest destination deltas scaled by adaptiveRole in [0, 1]. */
    [[nodiscard]] AdaptiveDeltas getDeltasForVoice (float adaptiveRole) const noexcept;

    [[nodiscard]] float getContraction() const noexcept;
    [[nodiscard]] float getBloom() const noexcept;
    [[nodiscard]] std::uint64_t getPublishedGeneration() const noexcept;

    /** DEFINITION and BLOOM macro scalars, normalized to [0, 1]. */
    void setDefinition (float amount) noexcept;
    void setBloomAmount (float amount) noexcept;

    /** Updates response timing without clearing the running envelopes. */
    void setTimeConstants (float contractionAttackMs,
                           float contractionReleaseMs,
                           float bloomAttackMs,
                           float bloomHoldMs,
                           float bloomReleaseMs) noexcept;

    void setFusionWeights (float densityWeight,
                           float onsetWeight,
                           float sustainWeight,
                           float gapWeight) noexcept;

    /**
     * Sets full-role destination depths. Linear-domain amounts use normalized
     * units (for example 0.35 means 35 percent width).
     */
    void setDestinationDepths (float wetReductionDb,
                               float wetLiftDb,
                               float tailReductionOctaves,
                               float tailExpansionOctaves,
                               float toneDarkeningOctaves,
                               float toneOpeningOctaves,
                               float widthReduction,
                               float widthExpansion,
                               float softeningIncrease,
                               float diffusionIncrease) noexcept;

    [[nodiscard]] static float computeEffectiveT60 (float t60BaseSeconds,
                                                     float t60OctaveDelta) noexcept;
    [[nodiscard]] static float computeEffectiveLpf (float lpfBaseHz,
                                                    float lpfOctaveDelta) noexcept;
    [[nodiscard]] static float computeEffectiveWidth (float widthBase,
                                                      float widthDelta) noexcept;
    [[nodiscard]] static float computeEffectiveSoftening (float softeningBase,
                                                          float softeningDelta) noexcept;
    [[nodiscard]] static float computeEffectiveDiffusion (float diffusionBase,
                                                          float diffusionDelta) noexcept;
    [[nodiscard]] static float wetDbToLinear (float wetBaseDb,
                                              float wetDbDelta) noexcept;

    /**
     * Recomputes feedback from the effective T60 and always applies the
     * stability cap. Passing a zero delta computes from the base T60.
     */
    [[nodiscard]] static float computeCappedFeedback (
        float delayTimeSeconds,
        float t60BaseSeconds,
        float t60OctaveDelta = 0.0f) noexcept;

private:
    struct OnePole
    {
        float value {};
        float attackCoefficient {};
        float releaseCoefficient {};
    };

    struct DestinationSmoother
    {
        OnePole contraction;
        OnePole bloom;
    };

    struct PublishedState
    {
        AdaptiveDeltas deltas;
        Mode mode;
    };

    static_assert (std::is_trivial_v<AdaptiveDeltas>);
    static_assert (std::is_standard_layout_v<AdaptiveDeltas>);
    static_assert (std::is_trivially_copyable_v<AdaptiveDeltas>);
    static_assert (std::is_trivial_v<PublishedState>);
    static_assert (std::is_standard_layout_v<PublishedState>);
    static_assert (std::is_trivially_copyable_v<PublishedState>);

    [[nodiscard]] static float clamp01 (float value) noexcept;
    [[nodiscard]] static float coefficientForMilliseconds (float milliseconds) noexcept;
    static void prepareDestination (DestinationSmoother& destination,
                                    float contractionAttackMs,
                                    float contractionReleaseMs,
                                    float bloomAttackMs,
                                    float bloomReleaseMs) noexcept;
    static void clearDestination (DestinationSmoother& destination) noexcept;
    [[nodiscard]] static float advance (OnePole& smoother,
                                        float target,
                                        int numAnalysisSamples) noexcept;
    void clearAllDestinations() noexcept;
    void publish (Mode mode, const AdaptiveDeltas& deltas) noexcept;

    std::array<PublishedState, 2> published_ {};
    std::atomic<unsigned int> activePublishedIndex_ { 0 };
    std::atomic<std::uint64_t> publishedGeneration_ { 0 };
    std::atomic<Mode> mode_ { Mode::Off };

    // Scalar telemetry crosses threads independently from the POD snapshots.
    std::atomic<float> contractionTelemetry_ { 0.0f };
    std::atomic<float> bloomTelemetry_ { 0.0f };

    DestinationSmoother wetSmoother_;
    DestinationSmoother tailSmoother_;
    DestinationSmoother toneSmoother_;
    DestinationSmoother widthSmoother_;
    DestinationSmoother softeningSmoother_;
    DestinationSmoother diffusionSmoother_;

    float definition_ { 1.0f };
    float bloomAmount_ { 1.0f };

    float densityWeight_ { 0.55f };
    float onsetWeight_ { 0.45f };
    float sustainWeight_ { 0.65f };
    float gapWeight_ { 0.35f };

    // Destination defaults mirror the frozen intelligence parameter defaults.
    float wetReductionDb_ { 8.0f };
    float wetLiftDb_ { 4.0f };
    float tailReductionOctaves_ { 1.0f };
    float tailExpansionOctaves_ { 0.75f };
    float toneDarkeningOctaves_ { 0.75f };
    float toneOpeningOctaves_ { 0.35f };
    float widthReduction_ { 0.35f };
    float widthExpansion_ { 0.30f };
    float softeningIncrease_ { 0.45f };
    float diffusionIncrease_ { 0.12f };
    float bloomHoldMs_ { 350.0f };
    int bloomHoldSamplesRemaining_ {};
    float heldBloomTarget_ {};
};

} // namespace shredspace
