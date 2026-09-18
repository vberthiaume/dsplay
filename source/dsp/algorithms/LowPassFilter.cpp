#include "LowPassFilter.h"

namespace dsplay
{
const AlgorithmDescriptor LowPassFilter::descriptor {
    .name          = "Low-pass filter",
    .numParameters = numUsedParameters,
    .parameters    = { {
        { .name         = "Cutoff",
          .range        = makeSkewedRange (20.f, 20000.f, 1000.f),
          .defaultValue = 1000.f,
          .suffix       = " Hz",
          .decimals     = 0 },
        { .name         = "Resonance",
          .range        = makeSkewedRange (0.5f, 10.f, 2.f),
          .defaultValue = 0.707f,
          .suffix       = " Q",
          .decimals     = 2 },
        { .name = "Gain", .range = { -24.f, 24.f }, .defaultValue = 0.f, .suffix = " dB", .decimals = 1 },
    } },
};

void LowPassFilter::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    juce::ignoreUnused (maxBlockSize);
    sampleRate = newSampleRate;
    channels.assign (static_cast<size_t> (numChannels), ChannelState {});

    smoothedCutoff.reset (sampleRate, smoothingSeconds);
    smoothedResonance.reset (sampleRate, smoothingSeconds);
    smoothedGain.reset (sampleRate, smoothingSeconds);

    smoothedCutoff.setCurrentAndTargetValue (descriptor.parameters[cutoff].defaultValue);
    smoothedResonance.setCurrentAndTargetValue (descriptor.parameters[resonance].defaultValue);
    smoothedGain.setCurrentAndTargetValue (1.f);
}

void LowPassFilter::reset() noexcept
{
    for (auto& channel : channels)
        channel = {};

    smoothedCutoff.setCurrentAndTargetValue (smoothedCutoff.getTargetValue());
    smoothedResonance.setCurrentAndTargetValue (smoothedResonance.getTargetValue());
    smoothedGain.setCurrentAndTargetValue (smoothedGain.getTargetValue());
}

void LowPassFilter::setParameter (int index, float value) noexcept
{
    switch (index)
    {
        case cutoff: smoothedCutoff.setTargetValue (value); break;
        case resonance: smoothedResonance.setTargetValue (value); break;
        case gain: smoothedGain.setTargetValue (juce::Decibels::decibelsToGain (value)); break;
        default: break;
    }
}

void LowPassFilter::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = std::min (buffer.getNumChannels(), static_cast<int> (channels.size()));

    // Per-block coefficient update. skip() advances the smoothers by a whole block so the ramp time stays in
    // seconds regardless of block size.
    const auto maxCutoff = static_cast<float> (sampleRate * 0.49);
    const auto fc        = std::min (smoothedCutoff.skip (numSamples), maxCutoff);
    const auto q         = smoothedResonance.skip (numSamples);

    const auto g  = std::tan (juce::MathConstants<float>::pi * fc / static_cast<float> (sampleRate));
    const auto k  = 1.f / q;
    const auto a1 = 1.f / (1.f + g * (g + k));
    const auto a2 = g * a1;
    const auto a3 = g * a2;

    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* data  = buffer.getWritePointer (ch);
        auto& state = channels[static_cast<size_t> (ch)];

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

    const auto startGain = smoothedGain.getCurrentValue();
    const auto endGain   = smoothedGain.skip (numSamples);
    buffer.applyGainRamp (0, numSamples, startGain, endGain);
}
} // namespace dsplay
