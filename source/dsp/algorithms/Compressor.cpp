#include "Compressor.h"

namespace dsplay
{
const AlgorithmDescriptor Compressor::descriptor {
    .name          = "Compressor",
    .numParameters = numUsedParameters,
    .parameters    = { {
        { .name = "Threshold", .range = { -60.f, 0.f }, .defaultValue = -20.f, .suffix = " dB", .decimals = 1 },
        { .name         = "Ratio",
             .range        = makeSkewedRange (1.f, 20.f, 4.f),
             .defaultValue = 4.f,
             .suffix       = ":1",
             .decimals     = 1 },
        { .name         = "Attack",
             .range        = makeSkewedRange (0.1f, 100.f, 10.f),
             .defaultValue = 10.f,
             .suffix       = " ms",
             .decimals     = 1 },
        { .name         = "Release",
             .range        = makeSkewedRange (5.f, 1000.f, 100.f),
             .defaultValue = 100.f,
             .suffix       = " ms",
             .decimals     = 0 },
        { .name = "Makeup", .range = { 0.f, 24.f }, .defaultValue = 0.f, .suffix = " dB", .decimals = 1 },
    } },
};

namespace
{
    // One-pole coefficient for a time constant given in milliseconds.
    float timeToCoefficient (float milliseconds, double sampleRate) noexcept
    {
        const auto samples = static_cast<float> (sampleRate) * milliseconds * 0.001f;
        return std::exp (-1.f / std::max (samples, 1.f));
    }
} // namespace

void Compressor::prepare (double newSampleRate, int maxBlockSize, int numChannels)
{
    juce::ignoreUnused (maxBlockSize, numChannels);
    sampleRate = newSampleRate;

    attackCoeff  = timeToCoefficient (descriptor.parameters[attack].defaultValue, sampleRate);
    releaseCoeff = timeToCoefficient (descriptor.parameters[release].defaultValue, sampleRate);

    smoothedMakeupGain.reset (sampleRate, smoothingSeconds);
    smoothedMakeupGain.setCurrentAndTargetValue (1.f);
    gainReductionDb = 0.f;
}

void Compressor::reset() noexcept
{
    gainReductionDb = 0.f;
    smoothedMakeupGain.setCurrentAndTargetValue (smoothedMakeupGain.getTargetValue());
}

void Compressor::setParameter (int index, float value) noexcept
{
    switch (index)
    {
        case threshold: thresholdDb = value; break;
        case ratio: ratioValue = value; break;
        case attack: attackCoeff = timeToCoefficient (value, sampleRate); break;
        case release: releaseCoeff = timeToCoefficient (value, sampleRate); break;
        case makeup: smoothedMakeupGain.setTargetValue (juce::Decibels::decibelsToGain (value)); break;
        default: break;
    }
}

void Compressor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const auto numSamples  = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    if (numChannels == 0)
        return;

    auto* const* data = buffer.getArrayOfWritePointers();

    for (int i = 0; i < numSamples; ++i)
    {
        // Stereo-linked peak detection: the loudest channel drives the gain for all of them.
        auto peak = 0.f;
        for (int ch = 0; ch < numChannels; ++ch)
            peak = std::max (peak, std::abs (data[ch][i]));

        const auto inputDb = juce::Decibels::gainToDecibels (peak, silenceDb);

        //calculate gain reduction
        const auto targetDb = [this, inputDb]()
        {
            const auto overshoot = inputDb - thresholdDb;
            const auto halfKnee  = kneeWidthDb * 0.5f;

            if (overshoot <= -halfKnee)
                return 0.f;

            const auto slope = 1.f / ratioValue - 1.f; // negative: how much output drops per dB of overshoot

            if (overshoot >= halfKnee)
                return -slope * overshoot;

            // Inside the knee: quadratic interpolation between the two straight segments.
            const auto x = overshoot + halfKnee;
            return -slope * x * x / (2.f * kneeWidthDb);
        }();

        // Ballistics in the log domain: attack when reduction grows, release when it shrinks.
        const auto coeff = targetDb > gainReductionDb ? attackCoeff : releaseCoeff;
        gainReductionDb  = coeff * gainReductionDb + (1.f - coeff) * targetDb;

        const auto gainLinear = juce::Decibels::decibelsToGain (-gainReductionDb) * smoothedMakeupGain.getNextValue();

        for (int ch = 0; ch < numChannels; ++ch)
            data[ch][i] *= gainLinear;
    }
}
} // namespace dsplay
