#include "helpers/test_helpers.h"
#include <dsp/PluginProcessor.h>
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
    for (int i = 0; i < dsplay::numAlgorithms; ++i)
        if (juce::String (dsplay::createAlgorithms()[static_cast<size_t> (i)]->getDescriptor().name) == name)
            return i;

    FAIL ("No algorithm named " << name);
    return -1;
}

void selectAlgorithm (PluginProcessor& plugin, int index)
{
    auto* parameter = plugin.getValueTreeState().getParameter (PluginProcessor::algorithmParamId);
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (static_cast<float> (index)));
}

// Sets a knob from a real-world value using the selected algorithm's range.
void setKnob (PluginProcessor& plugin, int knob, float realWorldValue)
{
    const auto& range = plugin.getSelectedDescriptor().parameters[static_cast<size_t> (knob)].range;
    plugin.getValueTreeState()
        .getParameter (PluginProcessor::knobParamId (knob))
        ->setValueNotifyingHost (range.convertTo0to1 (realWorldValue));
}

float knobValue (PluginProcessor& plugin, int knob)
{
    return plugin.getValueTreeState().getParameter (PluginProcessor::knobParamId (knob))->getValue();
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

        for (int i = 0; i < descriptor.numParameters; ++i)
        {
            const auto& parameter = descriptor.parameters[static_cast<size_t> (i)];
            INFO ("Parameter: " << parameter.name);

            CHECK (juce::String (parameter.name).isNotEmpty());
            CHECK (parameter.range.start < parameter.range.end);
            CHECK (parameter.defaultValue >= parameter.range.start);
            CHECK (parameter.defaultValue <= parameter.range.end);
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

            const auto labelText = [editor] (int knob)
            {
                auto* label = dynamic_cast<juce::Label*> (editor->findChildWithID (PluginEditor::knobLabelId (knob)));
                REQUIRE (label != nullptr);
                return label->getText();
            };
            const auto sliderEnabled = [editor] (int knob)
            {
                auto* slider = editor->findChildWithID (PluginEditor::knobSliderId (knob));
                REQUIRE (slider != nullptr);
                return slider->isEnabled();
            };

            const auto lpf        = algorithmIndexOf ("Low-pass filter");
            const auto compressor = algorithmIndexOf ("Compressor");

            selectAlgorithm (plugin, lpf);
            const auto& lpfDescriptor = plugin.getAlgorithm (lpf).getDescriptor();
            CHECK (labelText (0) == lpfDescriptor.parameters[0].name);
            CHECK (sliderEnabled (0));
            CHECK (labelText (PluginEditor::numKnobs - 1).isEmpty());
            CHECK_FALSE (sliderEnabled (PluginEditor::numKnobs - 1));

            selectAlgorithm (plugin, compressor);
            const auto& compressorDescriptor = plugin.getAlgorithm (compressor).getDescriptor();
            for (int knob = 0; knob < PluginEditor::numKnobs; ++knob)
            {
                CHECK (labelText (knob) == compressorDescriptor.parameters[static_cast<size_t> (knob)].name);
                CHECK (sliderEnabled (knob));
            }
        });
}

TEST_CASE ("Each algorithm remembers its own knob values", "[algorithms]")
{
    PluginProcessor plugin;
    const auto      lpf        = algorithmIndexOf ("Low-pass filter");
    const auto      compressor = algorithmIndexOf ("Compressor");

    selectAlgorithm (plugin, lpf);
    plugin.syncKnobsToSelectedAlgorithm();
    setKnob (plugin, 0, 250.f); // cutoff
    const auto lpfKnob0 = knobValue (plugin, 0);

    selectAlgorithm (plugin, compressor);
    plugin.syncKnobsToSelectedAlgorithm();
    CHECK (knobValue (plugin, 0)
           == Catch::Approx (plugin.getAlgorithm (compressor).getDescriptor().parameters[0].defaultNormalised()));
    setKnob (plugin, 0, -40.f); // threshold
    const auto compressorKnob0 = knobValue (plugin, 0);
    CHECK (compressorKnob0 != Catch::Approx (lpfKnob0));

    selectAlgorithm (plugin, lpf);
    plugin.syncKnobsToSelectedAlgorithm();
    CHECK (knobValue (plugin, 0) == Catch::Approx (lpfKnob0));

    selectAlgorithm (plugin, compressor);
    plugin.syncKnobsToSelectedAlgorithm();
    CHECK (knobValue (plugin, 0) == Catch::Approx (compressorKnob0));
}

TEST_CASE ("Knob values survive a state save and restore", "[algorithms][state]")
{
    const auto        lpf        = algorithmIndexOf ("Low-pass filter");
    const auto        compressor = algorithmIndexOf ("Compressor");
    juce::MemoryBlock state;
    float             lpfKnob0 {};
    float             compressorKnob0 {};

    {
        PluginProcessor plugin;
        selectAlgorithm (plugin, lpf);
        plugin.syncKnobsToSelectedAlgorithm();
        setKnob (plugin, 0, 250.f);
        lpfKnob0 = knobValue (plugin, 0);

        selectAlgorithm (plugin, compressor);
        plugin.syncKnobsToSelectedAlgorithm();
        setKnob (plugin, 0, -40.f);
        compressorKnob0 = knobValue (plugin, 0);

        plugin.getStateInformation (state);
    }

    PluginProcessor restored;
    restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));

    CHECK (restored.getSelectedAlgorithmIndex() == compressor);
    CHECK (knobValue (restored, 0) == Catch::Approx (compressorKnob0));

    selectAlgorithm (restored, lpf);
    restored.syncKnobsToSelectedAlgorithm();
    CHECK (knobValue (restored, 0) == Catch::Approx (lpfKnob0));
}

TEST_CASE ("Low-pass filter attenuates high frequencies and passes low ones", "[algorithms][dsp]")
{
    PluginProcessor plugin;
    plugin.prepareToPlay (sampleRate, blockSize);
    selectAlgorithm (plugin, algorithmIndexOf ("Low-pass filter"));

    setKnob (plugin, 0, 200.f);  // cutoff
    setKnob (plugin, 1, 0.707f); // resonance
    setKnob (plugin, 2, 0.f);    // gain

    constexpr auto amplitude = 0.5f;
    const auto     inputRms  = amplitude / juce::MathConstants<float>::sqrt2;

    const auto highRms = steadyStateRms (plugin, 10000.f, amplitude);
    CHECK (highRms < inputRms * 0.01f); // > 40 dB down, two octaves and a half above cutoff at 12 dB/oct

    const auto lowRms = steadyStateRms (plugin, 20.f, amplitude);
    CHECK (lowRms == Catch::Approx (inputRms).epsilon (0.05));
}

TEST_CASE ("Compressor reduces loud signals and leaves quiet ones alone", "[algorithms][dsp]")
{
    PluginProcessor plugin;
    plugin.prepareToPlay (sampleRate, blockSize);
    selectAlgorithm (plugin, algorithmIndexOf ("Compressor"));

    setKnob (plugin, 0, -30.f); // threshold
    setKnob (plugin, 1, 20.f);  // ratio
    setKnob (plugin, 2, 1.f);   // attack
    setKnob (plugin, 3, 50.f);  // release
    setKnob (plugin, 4, 0.f);   // makeup

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
        selectAlgorithm (plugin, block % dsplay::numAlgorithms);
        fillSine (buffer, 440.f, 0.5f, phase);
        plugin.processBlock (buffer, midi);

        for (int ch = 0; ch < numChannels; ++ch)
            for (int i = 0; i < blockSize; ++i)
                REQUIRE (std::isfinite (buffer.getSample (ch, i)));
    }
}
