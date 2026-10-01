#include "TappedDelayFir.hpp"

namespace dsplay
{
const AlgorithmDescriptor TappedDelayFir::descriptor {
    .name          = "Tapped Delay FIR Filter",
    .numParameters = std::to_underlying (Parameter::count),
    .parameters    = { {
        { .name = "Gain", .min = 0.f, .max = 24.f, .defaultValue = 0.f, .suffix = " dB", .decimals = 1 },
    } },
};

void TappedDelayFir::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    reset();
}

void TappedDelayFir::reset() noexcept {}

void TappedDelayFir::process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept
{
    auto&      block       = context.getOutputBlock();
    const auto numSamples  = block.getNumSamples();
    const auto numChannels = block.getNumChannels();

    if (numChannels == 0)
        return;

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        const auto gainLinear = juce::Decibels::decibelsToGain (-gainReductionDb) * smoothedMakeupGain.getNextValue();

        for (std::size_t ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (ch)[i] *= gainLinear;
    }
}
} // namespace dsplay
