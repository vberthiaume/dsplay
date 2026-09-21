#include "LowPassFilter.h"

namespace dsplay
{
const AlgorithmDescriptor LowPassFilter::descriptor {
    .name          = "Low-pass filter",
    .numParameters = std::to_underlying (Parameter::count),
    .parameters    = { {
        { .name         = "Cutoff",
          .min          = 20.f,
          .max          = 20000.f,
          .skewCentre   = 1000.f,
          .defaultValue = 1000.f,
          .suffix       = " Hz",
          .decimals     = 0 },
        { .name         = "Resonance",
          .min          = 0.5f,
          .max          = 10.f,
          .skewCentre   = 2.f,
          .defaultValue = 0.707f,
          .suffix       = " Q",
          .decimals     = 2 },
        { .name = "Gain", .min = -24.f, .max = 24.f, .defaultValue = 0.f, .suffix = " dB", .decimals = 1 },
    } },
};

void LowPassFilter::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    channels.assign (spec.numChannels, ChannelState {});

    smoothedCutoff.reset (sampleRate, smoothingSeconds);
    smoothedResonance.reset (sampleRate, smoothingSeconds);
    smoothedGain.reset (sampleRate, smoothingSeconds);

    reset();
}

void LowPassFilter::reset() noexcept
{
    for (auto& channel : channels)
        channel = {};

    // Snap the smoothers to the current parameter values so a freshly selected algorithm doesn't ramp from stale state.
    smoothedCutoff.setCurrentAndTargetValue (getParameter (Parameter::cutoff));
    smoothedResonance.setCurrentAndTargetValue (getParameter (Parameter::resonance));
    smoothedGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (getParameter (Parameter::gain)));
}

void LowPassFilter::process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept
{
    auto&      block       = context.getOutputBlock();
    const auto numSamples  = static_cast<int> (block.getNumSamples());
    const auto numChannels = std::min (block.getNumChannels(), channels.size());

    smoothedCutoff.setTargetValue (getParameter (Parameter::cutoff));
    smoothedResonance.setTargetValue (getParameter (Parameter::resonance));
    smoothedGain.setTargetValue (juce::Decibels::decibelsToGain (getParameter (Parameter::gain)));

    // Per-block coefficient update. skip() advances the smoothers by a whole block so the ramp time stays in seconds
    // regardless of block size.
    const auto maxCutoff = static_cast<float> (sampleRate * 0.49);
    const auto fc        = std::min (smoothedCutoff.skip (numSamples), maxCutoff);
    const auto q         = smoothedResonance.skip (numSamples);

    const auto g  = std::tan (juce::MathConstants<float>::pi * fc / static_cast<float> (sampleRate));
    const auto k  = 1.f / q;
    const auto a1 = 1.f / (1.f + g * (g + k));
    const auto a2 = g * a1;
    const auto a3 = g * a2;

    for (std::size_t ch = 0; ch < numChannels; ++ch)
    {
        auto* data  = block.getChannelPointer (ch);
        auto& state = channels[ch];

        for (int i = 0; i < numSamples; ++i)
        {
            const auto v0 = data[i];
            const auto v3 = v0 - state.ic2eq;
            const auto v1 = a1 * state.ic1eq + a2 * v3;
            const auto v2 = state.ic2eq + a2 * state.ic1eq + a3 * v3;

            state.ic1eq = 2.f * v1 - state.ic1eq;
            state.ic2eq = 2.f * v2 - state.ic2eq;

            data[i] = v2;
        }
    }

    block.multiplyBy (smoothedGain);
}
} // namespace dsplay
