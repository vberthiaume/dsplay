#include "PluginProcessor.h"
#include "../ui/PluginEditor.h"

namespace
{
constexpr int parameterVersionHint { 1 };

const juce::Identifier storedKnobsTreeType { "storedKnobs" };

juce::Identifier storedKnobProperty (int algorithm, int knob)
{
    return { "a" + juce::String (algorithm) + "k" + juce::String (knob) };
}
} // namespace

PluginProcessor::PluginProcessor() // NOLINT
: AudioProcessor (BusesProperties()
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts { *this, nullptr, "Parameters", createParameterLayout() },
  algorithmParam { apvts.getRawParameterValue (algorithmParamId) }
{
    for (int knob = 0; knob < numKnobs; ++knob)
        knobParams[static_cast<size_t> (knob)] = apvts.getRawParameterValue (knobParamId (knob));

    // Seed every algorithm's remembered knob values with its defaults (unused slots stay at 0).
    for (int algorithm = 0; algorithm < dsplay::numAlgorithms; ++algorithm)
    {
        const auto& descriptor = getAlgorithm (algorithm).getDescriptor();
        auto&       values     = storedKnobs[static_cast<size_t> (algorithm)];

        for (int knob = 0; knob < descriptor.numParameters; ++knob)
            values[static_cast<size_t> (knob)] = descriptor.parameters[static_cast<size_t> (knob)].defaultNormalised();
    }

    lastSyncedAlgorithm = getSelectedAlgorithmIndex();
    startTimer (knobSyncIntervalMs);
}

PluginProcessor::~PluginProcessor() { stopTimer(); }

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    juce::StringArray algorithmNames;
    for (const auto& algorithm : algorithms)
        algorithmNames.add (algorithm->getDescriptor().name);

    layout.add (
        std::make_unique<juce::AudioParameterChoice> (juce::ParameterID { algorithmParamId, parameterVersionHint },
                                                      "Algorithm",
                                                      algorithmNames,
                                                      static_cast<int> (defaultAlgorithmIndex)));

    const auto& defaultDescriptor = algorithms[static_cast<size_t> (defaultAlgorithmIndex)]->getDescriptor();

    for (int knob = 0; knob < numKnobs; ++knob)
    {
        const auto defaultValue = knob < defaultDescriptor.numParameters
                                      ? defaultDescriptor.parameters[static_cast<size_t> (knob)].defaultNormalised()
                                      : 0.f;

        const auto attributes = juce::AudioParameterFloatAttributes()
                                    .withStringFromValueFunction ([this, knob] (float value, int)
                                                                  { return knobValueToText (knob, value); })
                                    .withValueFromStringFunction ([this, knob] (const juce::String& text)
                                                                  { return knobTextToValue (knob, text); });

        layout.add (
            std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { knobParamId (knob), parameterVersionHint },
                                                         "Knob " + juce::String (knob + 1),
                                                         juce::NormalisableRange<float> { 0.f, 1.f },
                                                         defaultValue,
                                                         attributes));
    }

    return layout;
}

int PluginProcessor::getSelectedAlgorithmIndex() const noexcept
{
    const auto index = static_cast<int> (algorithmParam->load());
    return std::clamp (index, 0, dsplay::numAlgorithms - 1);
}

const dsplay::AlgorithmDescriptor& PluginProcessor::getSelectedDescriptor() const noexcept
{
    return getAlgorithm (getSelectedAlgorithmIndex()).getDescriptor();
}

juce::String PluginProcessor::knobValueToText (int knob, float normalised) const
{
    const auto& descriptor = getSelectedDescriptor();

    if (knob >= descriptor.numParameters)
        return "-";

    const auto& parameter = descriptor.parameters[static_cast<size_t> (knob)];
    return juce::String (parameter.range.convertFrom0to1 (normalised), parameter.decimals) + parameter.suffix;
}

float PluginProcessor::knobTextToValue (int knob, const juce::String& text) const
{
    const auto& descriptor = getSelectedDescriptor();

    if (knob >= descriptor.numParameters)
        return 0.f;

    const auto& parameter = descriptor.parameters[static_cast<size_t> (knob)];
    const auto  value     = parameter.range.snapToLegalValue (text.trimStart().getFloatValue());
    return parameter.range.convertTo0to1 (value);
}

void PluginProcessor::syncKnobsToSelectedAlgorithm()
{
    const auto selected = getSelectedAlgorithmIndex();

    if (selected == lastSyncedAlgorithm)
        return;

    auto& previous = storedKnobs[static_cast<size_t> (lastSyncedAlgorithm)];
    for (int knob = 0; knob < numKnobs; ++knob)
        previous[static_cast<size_t> (knob)] = knobParams[static_cast<size_t> (knob)]->load();

    const auto& next = storedKnobs[static_cast<size_t> (selected)];
    for (int knob = 0; knob < numKnobs; ++knob)
        apvts.getParameter (knobParamId (knob))->setValueNotifyingHost (next[static_cast<size_t> (knob)]);

    lastSyncedAlgorithm = selected;
}

int PluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram() { return 0; }

void PluginProcessor::setCurrentProgram (int index) { juce::ignoreUnused (index); }

const juce::String PluginProcessor::getProgramName (int index) // NOLINT
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName) // NOLINT
{
    juce::ignoreUnused (index, newName);
}

void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock) // NOLINT
{
    const auto numChannels = std::max (getTotalNumInputChannels(), getTotalNumOutputChannels());

    for (auto& algorithm : algorithms)
        algorithm->prepare (sampleRate, samplesPerBlock, numChannels);

    // Force a reset of whichever algorithm runs first.
    activeAlgorithm = -1;
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) noexcept
{
    juce::ignoreUnused (midiMessages);

    juce::ScopedNoDenormals noDenormals;
    const auto              totalNumInputChannels  = getTotalNumInputChannels();
    const auto              totalNumOutputChannels = getTotalNumOutputChannels();

    // Output channels without a matching input may contain garbage; clear them before processing.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // Hard switch: the newly selected algorithm starts from a clean state.
    const auto selected  = getSelectedAlgorithmIndex();
    auto&      algorithm = *algorithms[static_cast<size_t> (selected)];

    if (selected != activeAlgorithm)
    {
        algorithm.reset();
        activeAlgorithm = selected;
    }

    // Map the generic 0..1 knobs onto the algorithm's real-world parameter ranges.
    const auto& descriptor = algorithm.getDescriptor();

    for (int knob = 0; knob < descriptor.numParameters; ++knob)
    {
        const auto normalised = knobParams[static_cast<size_t> (knob)]->load();
        const auto value      = descriptor.parameters[static_cast<size_t> (knob)].range.convertFrom0to1 (normalised);
        algorithm.setParameter (knob, value);
    }

    algorithm.process (buffer);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) // NOLINT
{
    // Make sure the remembered values for the current algorithm are up to date before saving.
    auto& current = storedKnobs[static_cast<size_t> (lastSyncedAlgorithm)];
    for (int knob = 0; knob < numKnobs; ++knob)
        current[static_cast<size_t> (knob)] = knobParams[static_cast<size_t> (knob)]->load();

    auto state  = apvts.copyState();
    auto stored = state.getOrCreateChildWithName (storedKnobsTreeType, nullptr);

    for (int algorithm = 0; algorithm < dsplay::numAlgorithms; ++algorithm)
        for (int knob = 0; knob < numKnobs; ++knob)
            stored.setProperty (storedKnobProperty (algorithm, knob),
                                storedKnobs[static_cast<size_t> (algorithm)][static_cast<size_t> (knob)],
                                nullptr);

    if (const auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);

    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    apvts.replaceState (juce::ValueTree::fromXml (*xml));

    const auto stored = apvts.state.getChildWithName (storedKnobsTreeType);

    if (stored.isValid())
        for (int algorithm = 0; algorithm < dsplay::numAlgorithms; ++algorithm)
            for (int knob = 0; knob < numKnobs; ++knob)
            {
                auto& value = storedKnobs[static_cast<size_t> (algorithm)][static_cast<size_t> (knob)];
                value       = stored.getProperty (storedKnobProperty (algorithm, knob), value);
            }

    lastSyncedAlgorithm = getSelectedAlgorithmIndex();
}

juce::AudioProcessorEditor*         PluginProcessor::createEditor() { return new PluginEditor (*this); } // NOLINT
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }               // NOLINT
