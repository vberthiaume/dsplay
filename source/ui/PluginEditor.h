#pragma once

#include "../dsp/PluginProcessor.h"
#include "melatonin_inspector/melatonin_inspector.h"

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class PluginEditor : public juce::AudioProcessorEditor,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::AsyncUpdater
{
public:
    static constexpr auto width { 620 };
    static constexpr auto height { 260 };
    static constexpr auto fontSize { 16.f };
    static constexpr int  numKnobs { PluginProcessor::numKnobs };

    // Component IDs, so tests (and the inspector) can find the controls.
    static juce::String knobSliderId (int index) { return "knobSlider" + juce::String (index + 1); }
    static juce::String knobLabelId (int index) { return "knobLabel" + juce::String (index + 1); }

    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override { updateKnobsForSelectedAlgorithm(); }

    // Relabels the knobs and enables/disables them to match the selected algorithm's descriptor.
    void updateKnobsForSelectedAlgorithm();

    using SliderAttachment   = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    PluginProcessor& processorRef;

    juce::Label                         algorithmLabel { {}, "Algorithm" };
    juce::ComboBox                      algorithmBox;
    std::unique_ptr<ComboBoxAttachment> algorithmAttachment;

    std::array<juce::Slider, numKnobs>                      knobSliders;
    std::array<juce::Label, numKnobs>                       knobLabels;
    std::array<std::unique_ptr<SliderAttachment>, numKnobs> knobAttachments;

    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton                      inspectButton { "Inspect" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
