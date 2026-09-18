#pragma once

#include "../Algorithm.h"

namespace dsplay
{
// Resonant low-pass filter using Zavalishin's topology-preserving-transform state-variable filter (TPT SVF).
// Coefficients are recomputed once per block from smoothed cutoff/resonance values; the SVF stays stable under
// those changes where a recomputed biquad would not.
class LowPassFilter final : public Algorithm
{
public:
    enum Parameter
    {
        cutoff = 0,
        resonance,
        gain,
        numUsedParameters
    };

    [[nodiscard]] const AlgorithmDescriptor& getDescriptor() const noexcept override { return descriptor; }

    void prepare (double sampleRate, int maxBlockSize, int numChannels) override;
    void reset() noexcept RTSAN_NONBLOCKING override;
    void setParameter (int index, float value) noexcept RTSAN_NONBLOCKING override;
    void process (juce::AudioBuffer<float>& buffer) noexcept RTSAN_NONBLOCKING override;

private:
    struct ChannelState
    {
        float ic1eq { 0.f };
        float ic2eq { 0.f };
    };

    static const AlgorithmDescriptor descriptor;
    static constexpr double          smoothingSeconds { 0.05 };

    double                    sampleRate { 44100.0 };
    std::vector<ChannelState> channels;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedResonance;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         smoothedGain;
};
} // namespace dsplay
