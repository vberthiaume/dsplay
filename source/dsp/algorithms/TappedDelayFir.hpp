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
        tapSize,
        count
    };

    TappedDelayFir() : Algorithm (descriptor) {}

    void prepare (const juce::dsp::ProcessSpec& spec) override;
    void reset() noexcept RTSAN_NONBLOCKING override;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept RTSAN_NONBLOCKING override;

protected:
    void onParameterChanged (std::size_t index, float value) noexcept override;

private:
    static constexpr double          defaultSampleRate { 44100.0 };
    double                           sampleRate { defaultSampleRate };
    static const AlgorithmDescriptor descriptor;

    // static constexpr auto                          tapSize { 50 };
    static constexpr size_t                        numTapChannels { 2 };
    std::array<std::vector<float>, numTapChannels> tap {};
    size_t                                         curTap { 0 };

    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> smoothedGain;

    void        resizeTapsAndReset();
    std::size_t getTapSize() const { return static_cast<std::size_t> (getParameter (Parameter::tapSize)); }
};
} // namespace dsplay
