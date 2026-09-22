#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../util/RealtimeAttributes.h"

//TODO: is this the best spot for this?
#include "algorithms/LowPassFilter.h"
#include "algorithms/Compressor.h"

namespace dsplay
{
// The list of algorithms available in the playground
constexpr int numAlgorithms { 2 };

inline std::array<std::unique_ptr<Algorithm>, numAlgorithms> createAlgorithms()
{
    // The order here is the order in the combo box and the index stored in the "algorithm" parameter, so append new algorithms at the end to keep saved sessions valid.
    return { std::make_unique<LowPassFilter>(), std::make_unique<Compressor>() };
}
} // namespace dsplay

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class PluginProcessor : public juce::AudioProcessor
{
public:
    static constexpr auto numKnobs { dsplay::maxParameters };

    PluginProcessor();
    ~PluginProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>& buffer,
                       juce::MidiBuffer&         midiMessages) noexcept RTSAN_NONBLOCKING override;

    juce::AudioProcessorEditor* createEditor() override;
    bool                        hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override
    {
#if JucePlugin_WantsMidiInput
        return true;
#else
        return false;
#endif
    }

    bool producesMidi() const override
    {
#if JucePlugin_ProducesMidiOutput
        return true;
#else
        return false;
#endif
    }

    bool isMidiEffect() const override
    {
#if JucePlugin_IsMidiEffect
        return true;
#else
        return false;
#endif
    }

    double getTailLengthSeconds() const override { return 0.0; }

    int                getNumPrograms() override;
    int                getCurrentProgram() override;
    void               setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override; // NOLINT
    void               changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // The UI talks to the processor only through these. The selected algorithm and its knob values are atomics, so
    // they can be set from the message thread while processBlock reads them.
    void              setSelectedAlgorithm (int index) noexcept;
    [[nodiscard]] int getSelectedAlgorithmIndex() const noexcept { return selectedAlgorithm.load(); }

    [[nodiscard]] const dsplay::Algorithm& getAlgorithm (int index) const
    {
        return *algorithms[static_cast<std::size_t> (index)];
    }
    [[nodiscard]] const dsplay::AlgorithmDescriptor& getSelectedDescriptor() const noexcept
    {
        return getAlgorithm (getSelectedAlgorithmIndex()).getDescriptor();
    }

    void                setKnobValue (std::size_t knob, float value) noexcept;
    [[nodiscard]] float getKnobValue (std::size_t knob) const noexcept;

private:
    std::array<std::unique_ptr<dsplay::Algorithm>, dsplay::numAlgorithms> algorithms { dsplay::createAlgorithms() };

    std::atomic<int> selectedAlgorithm { 0 };

    // Audio thread only: which algorithm processed the previous block, to detect switches.
    int activeAlgorithm { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
