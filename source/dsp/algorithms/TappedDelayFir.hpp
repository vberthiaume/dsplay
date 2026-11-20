#pragma once

#include "../Algorithm.h"

namespace dsplay
{
class TappedDelayFir final : public Algorithm
{
public:
    static constexpr size_t maxTaps { 100 };
    static constexpr double defaultSampleRate { 44100.0 };
    static constexpr size_t numTapChannels { 2 };

    enum class Parameter : std::uint8_t
    {
        gain,
        tapSize,
        count
    };

    TappedDelayFir() : Algorithm (descriptor) {}

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() noexcept RTSAN_NONBLOCKING override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept RTSAN_NONBLOCKING override;

private:
    static const AlgorithmDescriptor descriptor;

    std::array<std::array<float, maxTaps>, numTapChannels> tap {};
    size_t                                                 curTap { 0 };

    juce::SmoothedValue<float> smoothedGain;

    [[nodiscard]] std::size_t getTapSize() const
    {
        return static_cast<std::size_t> (getParameter (Parameter::tapSize));
    }
};
} // namespace dsplay
