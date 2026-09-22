#include "Compressor.h"

namespace dsplay
{
const AlgorithmDescriptor Compressor::descriptor {
    .name          = "Compressor",
    .numParameters = std::to_underlying (Parameter::count),
    .parameters    = { {
        { .name = "Threshold", .min = -60.f, .max = 0.f, .defaultValue = -20.f, .suffix = " dB", .decimals = 1 },
        { .name         = "Ratio",
          .min          = 1.f,
          .max          = 20.f,
          .skewCentre   = 4.f,
          .defaultValue = 4.f,
          .suffix       = ":1",
          .decimals     = 1 },
        { .name         = "Attack",
          .min          = 0.1f,
          .max          = 100.f,
          .skewCentre   = 10.f,
          .defaultValue = 10.f,
          .suffix       = " ms",
          .decimals     = 1 },
        { .name         = "Release",
          .min          = 5.f,
          .max          = 1000.f,
          .skewCentre   = 100.f,
          .defaultValue = 100.f,
          .suffix       = " ms",
          .decimals     = 0 },
        { .name = "Makeup", .min = 0.f, .max = 24.f, .defaultValue = 0.f, .suffix = " dB", .decimals = 1 },
    } },
};

void Compressor::prepare (const juce::dsp::ProcessSpec& spec)
{
    sampleRate = spec.sampleRate;
    smoothedMakeupGain.reset (sampleRate, smoothingSeconds);
    reset();
}

void Compressor::reset() noexcept
{
    gainReductionDb = 0.f;
    smoothedMakeupGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (getParameter (Parameter::makeup)));
}

float Compressor::timeToCoefficient (float milliseconds) const noexcept
{
    constexpr auto millisecondsPerSecond = 1000.f;
    const auto     samples               = static_cast<float> (sampleRate) * milliseconds / millisecondsPerSecond;
    return std::exp (-1.f / std::max (samples, 1.f));
}

float Compressor::computeGainReductionDb (float inputDb) const noexcept
{
    const auto overshoot = inputDb - thresholdDb;

    if (overshoot <= -halfKneeWidthDb)
        return 0.f;

    const auto slope = 1.f / ratio - 1.f; // negative: how much output drops per dB of overshoot

    if (overshoot >= halfKneeWidthDb)
        return -slope * overshoot;

    // Inside the knee: quadratic interpolation between the two straight segments.
    const auto x = overshoot + halfKneeWidthDb;
    return -slope * x * x * kneeCurvature;
}

void Compressor::process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept
{
    auto&      block       = context.getOutputBlock();
    const auto numSamples  = block.getNumSamples();
    const auto numChannels = block.getNumChannels();

    if (numChannels == 0)
        return;

    thresholdDb             = getParameter (Parameter::threshold);
    ratio                   = getParameter (Parameter::ratio);
    const auto attackCoeff  = timeToCoefficient (getParameter (Parameter::attack));
    const auto releaseCoeff = timeToCoefficient (getParameter (Parameter::release));
    smoothedMakeupGain.setTargetValue (juce::Decibels::decibelsToGain (getParameter (Parameter::makeup)));

    for (std::size_t i = 0; i < numSamples; ++i)
    {
        // Stereo-linked peak detection: the loudest channel drives the gain for all of them.
        auto peak = 0.f;
        for (std::size_t ch = 0; ch < numChannels; ++ch)
            peak = std::max (peak, std::abs (block.getSample (static_cast<int> (ch), static_cast<int> (i))));

        const auto inputDb  = juce::Decibels::gainToDecibels (peak, silenceDb);
        const auto targetDb = computeGainReductionDb (inputDb);

        // Ballistics in the log domain: attack when reduction grows, release when it shrinks.
        const auto coeff = targetDb > gainReductionDb ? attackCoeff : releaseCoeff;
        gainReductionDb  = coeff * gainReductionDb + (1.f - coeff) * targetDb;

        const auto gainLinear = juce::Decibels::decibelsToGain (-gainReductionDb) * smoothedMakeupGain.getNextValue();

        for (std::size_t ch = 0; ch < numChannels; ++ch)
            block.getChannelPointer (ch)[i] *= gainLinear;
    }
}
} // namespace dsplay
