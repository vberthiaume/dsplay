#include "PluginEditor.h"

// NOLINTNEXTLINE
PluginEditor::PluginEditor (PluginProcessor& processorToUse)
: AudioProcessorEditor (&processorToUse), processorRef (processorToUse)
{
    algorithmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (algorithmLabel);

    algorithmSelector.setComponentID (algorithmBoxId);
    for (int i = 0; i < processorRef.getNumAlgorithms(); ++i)
        algorithmSelector.addItem (processorRef.getAlgorithm (i).getDescriptor().name, i + 1);

    algorithmSelector.setSelectedItemIndex (processorRef.getSelectedAlgorithmIndex(), juce::dontSendNotification);
    algorithmSelector.onChange = [this]
    {
        processorRef.setSelectedAlgorithm (algorithmSelector.getSelectedItemIndex());
        updateKnobsForSelectedAlgorithm();
    };
    addAndMakeVisible (algorithmSelector);

    for (std::size_t i = 0; i < numKnobs; ++i)
    {
        auto& [slider, label] = knobs[i];

        slider.setComponentID (knobSliderId (i));
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, textBoxWidth, textBoxHeight);
        slider.onValueChange
            = [this, i] { processorRef.setKnobValue (i, static_cast<float> (knobs[i].slider.getValue())); };
        addAndMakeVisible (slider);

        label.setComponentID (knobLabelId (i));
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions { fontSize });
        addAndMakeVisible (label);
    }

    bypassButton.setComponentID (bypassButtonId);
    bypassButton.setToggleState (processorRef.isBypassed(), juce::dontSendNotification);
    bypassButton.onClick = [this] { processorRef.setBypassed (bypassButton.getToggleState()); };
    addAndMakeVisible (bypassButton);

    loopButton.setComponentID (loopButtonId);
    loopButton.setClickingTogglesState (true);
    loopButton.setToggleState (processorRef.isLoopEnabled(), juce::dontSendNotification);
    loopButton.onClick = [this] { processorRef.setLoopEnabled (loopButton.getToggleState()); };
    addAndMakeVisible (loopButton);

#if JUCE_DEBUG
    addAndMakeVisible (inspectButton);
    inspectButton.onClick = [&]
    {
        if (! inspector)
        {
            inspector          = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this] { inspector.reset(); };
        }

        inspector->setVisible (true);
    };
#endif

    updateKnobsForSelectedAlgorithm();
    setSize (width, height);
}

void PluginEditor::updateKnobsForSelectedAlgorithm()
{
    const auto& descriptor = processorRef.getSelectedDescriptor();

    for (std::size_t i = 0; i < numKnobs; ++i)
    {
        auto& [slider, label] = knobs[i];
        const auto used       = i < descriptor.numParameters;

        if (! used)
        {
            label.setText ("", juce::dontSendNotification);
            slider.setEnabled (false);
            slider.textFromValueFunction = [] (double) { return juce::String ("-"); };
            slider.updateText();
            continue;
        }

        const auto& parameter = descriptor.parameters[i];

        label.setText (parameter.name, juce::dontSendNotification);
        slider.setEnabled (true);
        slider.textFromValueFunction = nullptr;
        slider.setRange (parameter.min, parameter.max);

        if (parameter.skewCentre > 0.f)
            slider.setSkewFactorFromMidPoint (parameter.skewCentre);
        else
            slider.setSkewFactor (1.0);

        slider.setTextValueSuffix (parameter.suffix);
        slider.setNumDecimalPlacesToDisplay (parameter.decimals);

        // Each algorithm keeps its own values, so switching back restores where the knobs were.
        slider.setValue (processorRef.getKnobValue (i), juce::dontSendNotification);
    }
}

void PluginEditor::paint (juce::Graphics& g)
{
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    g.setColour (juce::Colours::white.withAlpha (footerAlpha));
    g.setFont (juce::FontOptions { fontSize * footerFontScale });
    const auto footer = juce::String() + PRODUCT_NAME_WITHOUT_VERSION + " v" VERSION + " (" + CMAKE_BUILD_TYPE + ")";
    g.drawText (footer, getLocalBounds().reduced (margin), juce::Justification::bottomLeft, false);
}

void PluginEditor::resized()
{
    constexpr auto headerH = 30;
    constexpr auto footerH = 30;
    constexpr auto labelH  = 20;
    constexpr auto labelW  = 80;
    constexpr auto comboW  = 200;
    constexpr auto buttonW = 80;

    auto area = getLocalBounds().reduced (margin);

    auto header = area.removeFromTop (headerH);
    algorithmLabel.setBounds (header.removeFromLeft (labelW));
    algorithmSelector.setBounds (header.removeFromLeft (comboW).reduced (0, 2));
    header.removeFromLeft (margin);
    bypassButton.setBounds (header.removeFromLeft (buttonW));
    loopButton.setBounds (header.removeFromRight (buttonW).reduced (0, 2));
#if JUCE_DEBUG
    inspectButton.setBounds (header.removeFromRight (buttonW).reduced (0, 2));
#endif

    area.removeFromBottom (footerH);
    area.removeFromTop (margin);

    const auto knobW = area.getWidth() / static_cast<int> (numKnobs);

    for (auto& [slider, label] : knobs)
    {
        auto cell = area.removeFromLeft (knobW);
        label.setBounds (cell.removeFromTop (labelH));
        slider.setBounds (cell);
    }
}
