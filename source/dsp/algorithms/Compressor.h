#pragma once

#include "../Algorithm.h"

namespace dsplay
{
// Feed-forward compressor with a stereo-linked peak detector, soft knee, and log-domain attack/release
// smoothing of the gain reduction. Follows the design in Giannoulis, Massberg & Reiss, "Digital Dynamic Range
// Compressor Design - A Tutorial and Analysis" (JAES 2012).
class Compressor final : public Algorithm
{
public:
    enum Parameter
    {
        threshold = 0,
        ratio,
        attack,
        release,
        makeup,
        numUsedParameters
    };

    static constexpr float kneeWidthDb { 6.f };

    [[nodiscard]] const AlgorithmDescriptor& getDescriptor() const noexcept override { return descriptor; }

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
    void reset() noexcept RTSAN_NONBLOCKING override;
    void setParameter (int index, float value) noexcept RTSAN_NONBLOCKING override;
    void process (juce::AudioBuffer<float>& buffer) noexcept RTSAN_NONBLOCKING override;

private:
    // Static gain computer: returns the gain reduction in dB (>= 0) for an input level in dB.
    [[nodiscard]] float computeGainReductionDb (float inputDb) const noexcept;

    static const AlgorithmDescriptor descriptor;
    static constexpr double          smoothingSeconds { 0.05 };
    static constexpr float           silenceDb { -120.f };

    double sampleRate { 44100.0 };

    float thresholdDb { -20.f };
    float ratioValue { 4.f };
    float attackCoeff { 0.f };
    float releaseCoeff { 0.f };
    float gainReductionDb { 0.f }; // smoothed detector state, shared by all channels (stereo-linked)

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedMakeupGain;
};
} // namespace dsplay
