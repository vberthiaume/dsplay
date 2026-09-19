#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "../util/RealtimeAttributes.h"
#include "AlgorithmRegistry.h"

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class PluginProcessor : public juce::AudioProcessor, private juce::Timer
{
public:
    static constexpr int numKnobs { dsplay::maxParameters };

    // Parameter IDs. The knobs are generic 0..1 parameters whose meaning depends on the selected algorithm.
    static constexpr const char* algorithmParamId { "algorithm" };
    static juce::String          knobParamId (int index) { return "knob" + juce::String (index + 1); }

    PluginProcessor();
    ~PluginProcessor() override;

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

    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    // Index of the algorithm currently selected by the "algorithm" parameter.
    [[nodiscard]] int                      getSelectedAlgorithmIndex() const noexcept;
    [[nodiscard]] const dsplay::Algorithm& getAlgorithm (const int index) const
    {
        return *algorithms[static_cast<size_t> (index)];
    }
    [[nodiscard]] const dsplay::AlgorithmDescriptor& getSelectedDescriptor() const noexcept;

    // Each algorithm remembers its own knob values. When the selected algorithm changes, this stores the current knob
    // values for the previous algorithm and restores the ones saved for the new algorithm. Normally driven by a timer
    // on the message thread; exposed so tests can drive it deterministically.
    void syncKnobsToSelectedAlgorithm();

private:
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    juce::String                                        knobValueToText (int knob, float normalised) const;
    float                                               knobTextToValue (int knob, const juce::String& text) const;

    void timerCallback() override { syncKnobsToSelectedAlgorithm(); }

    static constexpr int   knobSyncIntervalMs { 30 };
    static constexpr float defaultAlgorithmIndex { 0.f };

    using KnobValues = std::array<float, numKnobs>;

    // Must be constructed before apvts: the parameter layout is built from the algorithm descriptors.
    std::array<std::unique_ptr<dsplay::Algorithm>, dsplay::numAlgorithms> algorithms { dsplay::createAlgorithms() };

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<float>*                       algorithmParam { nullptr };
    std::array<std::atomic<float>*, numKnobs> knobParams {};

    // Audio thread only.
    int activeAlgorithm { -1 };

    // Message thread only (see syncKnobsToSelectedAlgorithm).
    std::array<KnobValues, dsplay::numAlgorithms> storedKnobs {};
    int                                           lastSyncedAlgorithm { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};
