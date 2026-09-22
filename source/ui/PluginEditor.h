#pragma once

#include "../dsp/PluginProcessor.h"

#if JUCE_DEBUG
#include "melatonin_inspector/melatonin_inspector.h"
#endif

// NOLINTNEXTLINE(cppcoreguidelines-special-member-functions)
class PluginEditor : public juce::AudioProcessorEditor
{
public:
    static constexpr auto width { 620 };
    static constexpr auto height { 260 };
    static constexpr auto fontSize { 16.f };
    static constexpr auto numKnobs { PluginProcessor::numKnobs };

    // Component IDs, so tests (and the inspector) can find the controls.
    static constexpr const char* algorithmBoxId { "algorithmSelector" };
    static constexpr const char* loopButtonId { "loopButton" };
    static constexpr const char* bypassButtonId { "bypassButton" };
    static juce::String          knobSliderId (std::size_t index) { return "knobSlider" + juce::String (index + 1); }
    static juce::String          knobLabelId (std::size_t index) { return "knobLabel" + juce::String (index + 1); }

    explicit PluginEditor (PluginProcessor& processorToUse);
    ~PluginEditor() override = default;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    static constexpr int   margin { 10 };
    static constexpr int   textBoxWidth { 90 };
    static constexpr int   textBoxHeight { 20 };
    static constexpr float footerAlpha { 0.6f };
    static constexpr float footerFontScale { 0.75f };

    struct Knob
    {
        juce::Slider slider;
        juce::Label  label;
    };

    // Configures every knob (range, skew, suffix, label, enabled state, current value) from the selected algorithm's descriptor. Called on construction and whenever the combo box changes.
    void updateKnobsForSelectedAlgorithm();

    PluginProcessor& processorRef;

    juce::Label    algorithmLabel { {}, "Algorithm" };
    juce::ComboBox algorithmSelector;

    std::array<Knob, numKnobs> knobs;

    juce::ToggleButton bypassButton { "Bypass" };
    juce::TextButton   loopButton { "Play loop" };

#if JUCE_DEBUG
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton                      inspectButton { "Inspect" };
#endif

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
