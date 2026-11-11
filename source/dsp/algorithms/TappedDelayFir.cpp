#include "TappedDelayFir.hpp"

namespace dsplay
{
const AlgorithmDescriptor TappedDelayFir::descriptor {
    .name          = "Tapped Delay FIR Filter",
    .numParameters = std::to_underlying (Parameter::count),
    .parameters    = { {
        { .name = "Gain", .min = 0.f, .max = 24.f, .defaultValue = 0.f, .suffix = " dB", .decimals = 1 },
    }, },
};

void TappedDelayFir::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    smoothedGain.reset (sampleRate, smoothingSeconds);
    reset();
}

void TappedDelayFir::reset() noexcept
{
    for (auto& channelTap : tap)
        channelTap.fill (0.f);

    curTap = 0;
}

void TappedDelayFir::process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept
{
    auto&      block       = context.getOutputBlock();
    const auto numSamples  = block.getNumSamples();
    const auto numChannels = block.getNumChannels();
    jassert (numChannels == numTapChannels);

    smoothedGain.setTargetValue (juce::Decibels::decibelsToGain (getParameter (Parameter::gain)));

    if (numChannels == 0)
        return;

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        //get the current gain
        const auto gainLinear = smoothedGain.getNextValue();

        //for each channel
        for (std::size_t ch = 0; ch < numChannels; ++ch)
        {
            //get a reference to our current sample
            auto& curSample = block.getChannelPointer (ch)[i];

            //add current sample value to the tap
            tap.at (ch).at (curTap) = curSample;

            //calculate filtered sample value from the current tap
            curSample = 0.f;
            for (std::size_t t = 0; t < tapSize; ++t)
                curSample += tap.at (ch).at (t) / tapSize;

            //apply the gain
            curSample *= gainLinear;
        }

        curTap = (curTap + 1) % tapSize;
    }
}
} // namespace dsplay
