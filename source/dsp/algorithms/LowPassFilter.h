#pragma once

#include "../Algorithm.h"

namespace dsplay
{
// Resonant low-pass filter using Zavalishin's topology-preserving-transform state-variable filter (TPT SVF).
// Coefficients are recomputed once per block from smoothed cutoff/resonance values; the SVF stays stable under those
// changes where a recomputed biquad would not.
class LowPassFilter final : public Algorithm
{
public:
    enum class Parameter : std::uint8_t
    {
        cutoff,
        resonance,
        gain,
        count
    };

    LowPassFilter() : Algorithm (descriptor) {}

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() noexcept RTSAN_NONBLOCKING override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept RTSAN_NONBLOCKING override;

private:
    struct ChannelState
    {
        float ic1eq { 0.f };
        float ic2eq { 0.f };
    };

    static const AlgorithmDescriptor descriptor;
    static constexpr double          smoothingSeconds { 0.05 };
    static constexpr double          defaultSampleRate { 44100.0 };

    double                    sampleRate { defaultSampleRate };
    std::vector<ChannelState> channels;

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedCutoff;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Multiplicative> smoothedResonance;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear>         smoothedGain;
};
} // namespace dsplay
