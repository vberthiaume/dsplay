#include "PluginProcessor.h"
#include "../ui/PluginEditor.h"

PluginProcessor::PluginProcessor() // NOLINT
: AudioProcessor (BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
                      .withInput ("Input", juce::AudioChannelSet::stereo(), true)
#endif
                      .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
#endif
  )
{
}

void PluginProcessor::setSelectedAlgorithm (int index) noexcept
{
    selectedAlgorithm.store (std::clamp (index, 0, dsplay::numAlgorithms - 1));
}

void PluginProcessor::setKnobValue (std::size_t knob, float value) noexcept
{
    algorithms[static_cast<std::size_t> (getSelectedAlgorithmIndex())]->setParameter (knob, value);
}

float PluginProcessor::getKnobValue (std::size_t knob) const noexcept
{
    return getAlgorithm (getSelectedAlgorithmIndex()).getParameter (knob);
}

int PluginProcessor::getNumPrograms()
{
    return 1;
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
    const juce::dsp::ProcessSpec spec {
        .sampleRate       = sampleRate,
        .maximumBlockSize = static_cast<juce::uint32> (samplesPerBlock),
        .numChannels = static_cast<juce::uint32> (std::max (getTotalNumInputChannels(), getTotalNumOutputChannels())),
    };

    for (auto& algorithm : algorithms)
        algorithm->prepare (spec);

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
    auto&      algorithm = *algorithms[static_cast<std::size_t> (selected)];

    if (selected != activeAlgorithm)
    {
        algorithm.reset();
        activeAlgorithm = selected;
    }

    juce::dsp::AudioBlock<float> block (buffer);
    algorithm.process (juce::dsp::ProcessContextReplacing<float> (block));
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) // NOLINT
{
    // Deliberately empty: this is a standalone playground, nothing is persisted between runs.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Deliberately empty, see getStateInformation().
    juce::ignoreUnused (data, sizeInBytes);
}

juce::AudioProcessorEditor*         PluginProcessor::createEditor() { return new PluginEditor (*this); } // NOLINT
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PluginProcessor(); }               // NOLINT
