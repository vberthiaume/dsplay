#include <dsp/PluginProcessor.h>
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

namespace
{
constexpr double sampleRate { 44100.0 };
constexpr int    blockSize { 512 };
constexpr int    numChannels { 2 };

// A host whose transport is running.
struct PlayingHead final : juce::AudioPlayHead
{
    [[nodiscard]] juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo info;
        info.setIsPlaying (true);
        return info;
    }
};

// Runs `numBlocks` blocks of silence through the plugin and reports whether the last one came out silent.
bool lastBlockIsSilent (PluginProcessor& plugin, int numBlocks)
{
    juce::AudioBuffer<float> buffer (numChannels, blockSize);
    juce::MidiBuffer         midi;

    for (int i = 0; i < numBlocks; ++i)
    {
        buffer.clear();
        plugin.processBlock (buffer, midi);
    }

    return buffer.getMagnitude (0, blockSize) == 0.f;
}
} // namespace

TEST_CASE ("Drum loop replaces silent input and wraps around", "[loop]")
{
    PluginProcessor plugin;
    plugin.setLoopEnabled (true);
    plugin.prepareToPlay (sampleRate, blockSize);

    CHECK_FALSE (lastBlockIsSilent (plugin, 1));

    // Past the end of the file the source must have wrapped rather than gone quiet.
    const auto blocksPerLoop = static_cast<int> (plugin.getLoopLengthInSamples() / blockSize);
    CHECK_FALSE (lastBlockIsSilent (plugin, blocksPerLoop + 1));
}

TEST_CASE ("Drum loop yields to the toggle and to a running host transport", "[loop]")
{
    PluginProcessor plugin;
    plugin.prepareToPlay (sampleRate, blockSize);

    CHECK (lastBlockIsSilent (plugin, 1));

    plugin.setLoopEnabled (true);
    PlayingHead head;
    plugin.setPlayHead (&head);
    CHECK (lastBlockIsSilent (plugin, 1));

    plugin.setPlayHead (nullptr);
    CHECK_FALSE (lastBlockIsSilent (plugin, 1));
}

TEST_CASE ("Drum loop is resampled to the device rate", "[loop]")
{
    PluginProcessor plugin;
    plugin.prepareToPlay (sampleRate, blockSize);
    const auto nativeLength = static_cast<double> (plugin.getLoopLengthInSamples());

    plugin.prepareToPlay (sampleRate * 2, blockSize);
    CHECK (static_cast<double> (plugin.getLoopLengthInSamples()) == Catch::Approx (nativeLength * 2).margin (1.0));
}
