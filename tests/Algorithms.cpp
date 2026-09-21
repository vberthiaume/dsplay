#include "helpers/test_helpers.h"
#include <dsp/PluginProcessor.h>
#include <dsp/algorithms/LowPassFilter.h>
#include <dsp/algorithms/Compressor.h>
#include <ui/PluginEditor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
constexpr double sampleRate { 44100.0 };
constexpr int    blockSize { 512 };
constexpr int    numChannels { 2 };

int algorithmIndexOf (const char* name)
{
    const auto algorithms = dsplay::createAlgorithms();

    for (int i = 0; i < dsplay::numAlgorithms; ++i)
        if (juce::String (algorithms[static_cast<std::size_t> (i)]->getDescriptor().name) == name)
            return i;

    FAIL ("No algorithm named " << name);
    return -1;
}

template <typename Enum>
void setKnob (PluginProcessor& plugin, Enum parameter, float value)
{
    plugin.setKnobValue (static_cast<std::size_t> (std::to_underlying (parameter)), value);
}

void fillSine (juce::AudioBuffer<float>& buffer, float frequency, float amplitude, double& phase)
{
    const auto increment = juce::MathConstants<double>::twoPi * frequency / sampleRate;

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto sample = amplitude * static_cast<float> (std::sin (phase));
        phase             = std::fmod (phase + increment, juce::MathConstants<double>::twoPi);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample (ch, i, sample);
    }
}

// Runs a sine through the plugin for `settleSeconds` so smoothing and detectors reach steady state, then returns the
// RMS of the following `measureSeconds` of output. Measuring over many blocks keeps the result meaningful even when a
// single block holds less than one period of the test tone.
float steadyStateRms (PluginProcessor& plugin,
                      float            frequency,
                      float            amplitude,
                      double           settleSeconds  = 0.5,
                      double           measureSeconds = 0.5)
{
    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer         midi;
    double                   phase { 0.0 };

    const auto settleBlocks  = static_cast<int> (settleSeconds * sampleRate / blockSize);
    const auto measureBlocks = static_cast<int> (measureSeconds * sampleRate / blockSize);

    for (int block = 0; block < settleBlocks; ++block)
    {
        fillSine (buffer, frequency, amplitude, phase);
        plugin.processBlock (buffer, midi);
    }

    auto sumOfSquares = 0.0;

    for (int block = 0; block < measureBlocks; ++block)
    {
        fillSine (buffer, frequency, amplitude, phase);
        plugin.processBlock (buffer, midi);

        for (int i = 0; i < blockSize; ++i)
            sumOfSquares += static_cast<double> (buffer.getSample (0, i)) * buffer.getSample (0, i);
    }

    return static_cast<float> (std::sqrt (sumOfSquares / (measureBlocks * blockSize)));
}
} // namespace

TEST_CASE ("Algorithm descriptors are well-formed", "[algorithms]")
{
    for (const auto& algorithm : dsplay::createAlgorithms())
    {
        const auto& descriptor = algorithm->getDescriptor();
        INFO ("Algorithm: " << descriptor.name);

        CHECK (juce::String (descriptor.name).isNotEmpty());
        CHECK (descriptor.numParameters >= 1);
        CHECK (descriptor.numParameters <= dsplay::maxParameters);

        for (std::size_t i = 0; i < descriptor.numParameters; ++i)
        {
            const auto& parameter = descriptor.parameters[i];
            INFO ("Parameter: " << parameter.name);

            CHECK (juce::String (parameter.name).isNotEmpty());
            CHECK (parameter.min < parameter.max);
            CHECK (parameter.defaultValue >= parameter.min);
            CHECK (parameter.defaultValue <= parameter.max);

            if (parameter.skewCentre > 0.f)
            {
                CHECK (parameter.skewCentre > parameter.min);
                CHECK (parameter.skewCentre < parameter.max);
            }

            // The atomics start at the descriptor defaults.
            CHECK (algorithm->getParameter (i) == Catch::Approx (parameter.defaultValue));
        }
    }
}

TEST_CASE ("Editor relabels knobs when the algorithm changes", "[editor][algorithms]")
{
    runWithinPluginEditor (
        [] (PluginProcessor& plugin)
        {
            auto* editor = plugin.getActiveEditor();
            REQUIRE (editor != nullptr);

            auto* comboBox = dynamic_cast<juce::ComboBox*> (editor->findChildWithID (PluginEditor::algorithmBoxId));
            REQUIRE (comboBox != nullptr);

            const auto labelText = [editor] (std::size_t knob)
            {
                auto* label = dynamic_cast<juce::Label*> (editor->findChildWithID (PluginEditor::knobLabelId (knob)));
                REQUIRE (label != nullptr);
                return label->getText();
            };
            const auto sliderEnabled = [editor] (std::size_t knob)
            {
                auto* slider = editor->findChildWithID (PluginEditor::knobSliderId (knob));
                REQUIRE (slider != nullptr);
                return slider->isEnabled();
            };

            const auto lpf        = algorithmIndexOf ("Low-pass filter");
            const auto compressor = algorithmIndexOf ("Compressor");

            comboBox->setSelectedItemIndex (lpf, juce::sendNotificationSync);
            CHECK (plugin.getSelectedAlgorithmIndex() == lpf);

            const auto& lpfDescriptor = plugin.getAlgorithm (lpf).getDescriptor();
            CHECK (labelText (0) == lpfDescriptor.parameters[0].name);
            CHECK (sliderEnabled (0));
            CHECK (labelText (PluginEditor::numKnobs - 1).isEmpty());
            CHECK_FALSE (sliderEnabled (PluginEditor::numKnobs - 1));

            comboBox->setSelectedItemIndex (compressor, juce::sendNotificationSync);
            CHECK (plugin.getSelectedAlgorithmIndex() == compressor);

            const auto& compressorDescriptor = plugin.getAlgorithm (compressor).getDescriptor();
            for (std::size_t knob = 0; knob < PluginEditor::numKnobs; ++knob)
            {
                CHECK (labelText (knob) == compressorDescriptor.parameters[knob].name);
                CHECK (sliderEnabled (knob));
            }
        });
}

TEST_CASE ("Low-pass filter attenuates high frequencies and passes low ones", "[algorithms][dsp]")
{
    using Parameter = dsplay::LowPassFilter::Parameter;

    PluginProcessor plugin;
    plugin.prepareToPlay (sampleRate, blockSize);
    plugin.setSelectedAlgorithm (algorithmIndexOf ("Low-pass filter"));

    setKnob (plugin, Parameter::cutoff, 200.f);
    setKnob (plugin, Parameter::resonance, 0.707f);
    setKnob (plugin, Parameter::gain, 0.f);

    constexpr auto amplitude = 0.5f;
    const auto     inputRms  = amplitude / juce::MathConstants<float>::sqrt2;

    const auto highRms = steadyStateRms (plugin, 10000.f, amplitude);
    CHECK (highRms < inputRms * 0.01f); // > 40 dB down, five and a half octaves above cutoff at 12 dB/oct

    const auto lowRms = steadyStateRms (plugin, 20.f, amplitude);
    CHECK (lowRms == Catch::Approx (inputRms).epsilon (0.05));
}

TEST_CASE ("Compressor reduces loud signals and leaves quiet ones alone", "[algorithms][dsp]")
{
    using Parameter = dsplay::Compressor::Parameter;

    PluginProcessor plugin;
    plugin.prepareToPlay (sampleRate, blockSize);
    plugin.setSelectedAlgorithm (algorithmIndexOf ("Compressor"));

    setKnob (plugin, Parameter::threshold, -30.f);
    setKnob (plugin, Parameter::ratio, 20.f);
    setKnob (plugin, Parameter::attack, 1.f);
    setKnob (plugin, Parameter::release, 50.f);
    setKnob (plugin, Parameter::makeup, 0.f);

    const auto loudAmplitude = 1.f;
    const auto loudInputRms  = loudAmplitude / juce::MathConstants<float>::sqrt2;
    const auto loudRms       = steadyStateRms (plugin, 1000.f, loudAmplitude);
    const auto reductionDb   = juce::Decibels::gainToDecibels (loudRms / loudInputRms);
    CHECK (reductionDb < -20.f); // 30 dB over threshold at 20:1 leaves ~1.5 dB, so ~28 dB of reduction

    const auto quietAmplitude = juce::Decibels::decibelsToGain (-50.f);
    const auto quietInputRms  = quietAmplitude / juce::MathConstants<float>::sqrt2;
    const auto quietRms       = steadyStateRms (plugin, 1000.f, quietAmplitude);
    CHECK (quietRms == Catch::Approx (quietInputRms).epsilon (0.02));
}

TEST_CASE ("Switching algorithms while processing is realtime-safe", "[algorithms][rtsan]")
{
    PluginProcessor plugin;
    plugin.prepareToPlay (sampleRate, blockSize);

    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer         midi;
    double                   phase { 0.0 };

    for (int block = 0; block < 20; ++block)
    {
        plugin.setSelectedAlgorithm (block % dsplay::numAlgorithms);
        fillSine (buffer, 440.f, 0.5f, phase);
        plugin.processBlock (buffer, midi);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < blockSize; ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));
    }
}
