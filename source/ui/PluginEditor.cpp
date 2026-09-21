#include "PluginEditor.h"

// NOLINTNEXTLINE
PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (&p), processorRef (p)
{
    algorithmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (algorithmLabel);

    algorithmBox.setComponentID (algorithmBoxId);
    for (int i = 0; i < dsplay::numAlgorithms; ++i)
        algorithmBox.addItem (processorRef.getAlgorithm (i).getDescriptor().name, i + 1);

    algorithmBox.setSelectedItemIndex (processorRef.getSelectedAlgorithmIndex(), juce::dontSendNotification);
    algorithmBox.onChange = [this]
    {
        processorRef.setSelectedAlgorithm (algorithmBox.getSelectedItemIndex());
        updateKnobsForSelectedAlgorithm();
    };
    addAndMakeVisible (algorithmBox);

    for (std::size_t i = 0; i < numKnobs; ++i)
    {
        auto& [slider, label] = knobs[i];

        slider.setComponentID (knobSliderId (i));
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        slider.onValueChange
            = [this, i] { processorRef.setKnobValue (i, static_cast<float> (knobs[i].slider.getValue())); };
        addAndMakeVisible (slider);

        label.setComponentID (knobLabelId (i));
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions { fontSize });
        addAndMakeVisible (label);
    }

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

    g.setColour (juce::Colours::white.withAlpha (0.6f));
    g.setFont (juce::FontOptions { fontSize * 0.75f });
    const auto footer = juce::String() + PRODUCT_NAME_WITHOUT_VERSION + " v" VERSION + " (" + CMAKE_BUILD_TYPE + ")";
    g.drawText (footer, getLocalBounds().reduced (10), juce::Justification::bottomLeft, false);
}

void PluginEditor::resized()
{
    constexpr auto margin   = 10;
    constexpr auto headerH  = 30;
    constexpr auto footerH  = 30;
    constexpr auto labelH   = 20;
    constexpr auto labelW   = 80;
    constexpr auto comboW   = 200;
    constexpr auto inspectW = 80;

    auto area = getLocalBounds().reduced (margin);

    auto header = area.removeFromTop (headerH);
    algorithmLabel.setBounds (header.removeFromLeft (labelW));
    algorithmBox.setBounds (header.removeFromLeft (comboW).reduced (0, 2));
    inspectButton.setBounds (header.removeFromRight (inspectW).reduced (0, 2));

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
