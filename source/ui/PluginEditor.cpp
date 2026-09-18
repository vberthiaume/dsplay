#include "PluginEditor.h"

// NOLINTNEXTLINE
PluginEditor::PluginEditor (PluginProcessor& p) : AudioProcessorEditor (&p), processorRef (p)
{
    auto& apvts = processorRef.getValueTreeState();

    // Algorithm selector. Items must be added before the attachment so it can select the current one.
    algorithmLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (algorithmLabel);

    for (int i = 0; i < dsplay::numAlgorithms; ++i)
        algorithmBox.addItem (processorRef.getAlgorithm (i).getDescriptor().name, i + 1);

    addAndMakeVisible (algorithmBox);
    algorithmAttachment = std::make_unique<ComboBoxAttachment> (apvts, PluginProcessor::algorithmParamId, algorithmBox);

    // Knobs. The attachment installs the parameter's text conversion on the slider, so the text box shows the mapped
    // value with units rather than the raw 0..1 position.
    for (int i = 0; i < numKnobs; ++i)
    {
        const auto index  = static_cast<size_t> (i);
        auto&      slider = knobSliders[index];
        auto&      label  = knobLabels[index];

        slider.setComponentID (knobSliderId (i));
        slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        addAndMakeVisible (slider);

        label.setComponentID (knobLabelId (i));
        label.setJustificationType (juce::Justification::centred);
        label.setFont (juce::FontOptions { fontSize });
        addAndMakeVisible (label);

        knobAttachments[index] = std::make_unique<SliderAttachment> (apvts, PluginProcessor::knobParamId (i), slider);
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
    apvts.addParameterListener (PluginProcessor::algorithmParamId, this);

    setSize (width, height);
}

PluginEditor::~PluginEditor()
{
    processorRef.getValueTreeState().removeParameterListener (PluginProcessor::algorithmParamId, this);
    cancelPendingUpdate();
}

void PluginEditor::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (parameterID, newValue);

    // Parameter changes can arrive from any thread; only touch components on the message thread.
    if (juce::MessageManager::getInstance()->isThisTheMessageThread())
        updateKnobsForSelectedAlgorithm();
    else
        triggerAsyncUpdate();
}

void PluginEditor::updateKnobsForSelectedAlgorithm()
{
    const auto& descriptor = processorRef.getSelectedDescriptor();

    for (int i = 0; i < numKnobs; ++i)
    {
        const auto index = static_cast<size_t> (i);
        const auto used  = i < descriptor.numParameters;

        knobLabels[index].setText (used ? descriptor.parameters[index].name : "", juce::dontSendNotification);
        knobSliders[index].setEnabled (used);
        knobSliders[index].updateText();
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

    const auto knobW = area.getWidth() / numKnobs;

    for (int i = 0; i < numKnobs; ++i)
    {
        const auto index = static_cast<size_t> (i);
        auto       cell  = area.removeFromLeft (knobW);

        knobLabels[index].setBounds (cell.removeFromTop (labelH));
        knobSliders[index].setBounds (cell);
    }
}
