#pragma once

#include "../Algorithm.h"

namespace dsplay
{
// Feed-forward compressor with a stereo-linked peak detector, soft knee, and log-domain attack/release smoothing of the
// gain reduction. Follows the design in Giannoulis, Massberg & Reiss, "Digital Dynamic Range Compressor Design - A
// Tutorial and Analysis" (JAES 2012).
class Compressor final : public Algorithm
{
public:
    enum class Parameter : std::uint8_t
    {
        threshold,
        ratio,
        attack,
        release,
        makeup,
        count
    };

    static constexpr float kneeWidthDb { 6.f };

    Compressor() : Algorithm (descriptor) {}

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() noexcept RTSAN_NONBLOCKING override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept RTSAN_NONBLOCKING override;

private:
    // Static gain computer: returns the gain reduction in dB (>= 0) for an input level in dB, using the threshold and
    // ratio captured for the current block.
    [[nodiscard]] float computeGainReductionDb (float inputDb) const noexcept;

    // One-pole coefficient for a time constant given in milliseconds.
    [[nodiscard]] float timeToCoefficient (float milliseconds) const noexcept;

    static const AlgorithmDescriptor descriptor;
    static constexpr double          smoothingSeconds { 0.05 };
    static constexpr double          defaultSampleRate { 44100.0 };
    static constexpr float           silenceDb { -120.f };

    double sampleRate { defaultSampleRate };

    // Per-block copies of the parameters used inside the sample loop.
    float thresholdDb { 0.f };
    float ratio { 1.f };

    float gainReductionDb { 0.f }; // smoothed detector state, shared by all channels (stereo-linked)

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMakeupGain;
};
} // namespace dsplay
