#pragma once

#include "../Algorithm.h"

namespace dsplay
{
class TappedDelayFir final : public Algorithm
{
public:
    enum class Parameter : std::uint8_t
    {
        gain,
        count
    };

    TappedDelayFir() : Algorithm (descriptor) {}

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() noexcept RTSAN_NONBLOCKING override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept RTSAN_NONBLOCKING override;

private:
    static constexpr double          defaultSampleRate { 44100.0 };
    double                           sampleRate { defaultSampleRate };
    static const AlgorithmDescriptor descriptor;

    static constexpr auto                                  tapSize { 5 };
    static constexpr size_t                                numTapChannels { 2 };
    std::array<std::array<float, tapSize>, numTapChannels> tap {};
    size_t                                                 curTap { 0 };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;
};
} // namespace dsplay
