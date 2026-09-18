#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "../util/RealtimeAttributes.h"

namespace dsplay
{
// Every algorithm exposes at most this many parameters, one per rotary slider in the UI.
constexpr int maxParameters { 5 };

// Describes one algorithm parameter. The UI shows `name`, the host and the slider text box show the mapped value
// formatted with `decimals` digits followed by `suffix` (e.g. "1200.0 Hz", "4.0:1").
struct ParameterDescriptor
{
    const char*                    name { "" };
    juce::NormalisableRange<float> range {};
    float                          defaultValue { 0.f };
    const char*                    suffix { "" };
    int                            decimals { 1 };

    [[nodiscard]] float defaultNormalised() const { return range.convertTo0to1 (defaultValue); }
};

struct AlgorithmDescriptor
{
    const char*                                    name { "" };
    int                                            numParameters { 0 };
    std::array<ParameterDescriptor, maxParameters> parameters {};
};

// Builds a range whose midpoint on the slider maps to `centre`, i.e. a log-like sweep for frequencies and times.
inline juce::NormalisableRange<float> makeSkewedRange (float start, float end, float centre)
{
    juce::NormalisableRange<float> range { start, end };
    range.setSkewForCentre (centre);
    return range;
}

// Base class for every DSP algorithm in the playground.
//
// Lifecycle: prepare() is called from prepareToPlay() and may allocate. reset() clears internal state and is called
// on the audio thread when the algorithm becomes active. setParameter() receives the real-world (mapped) value of
// the given parameter slot and process() is then called once per block. Both must be realtime-safe.
class Algorithm
{
public:
    Algorithm()          = default;
    virtual ~Algorithm() = default;

    Algorithm (const Algorithm&)            = delete;
    Algorithm& operator= (const Algorithm&) = delete;
    Algorithm (Algorithm&&)                 = delete;
    Algorithm& operator= (Algorithm&&)      = delete;

    [[nodiscard]] virtual const AlgorithmDescriptor& getDescriptor() const noexcept = 0;

    virtual void prepare (double sampleRate, int maxBlockSize, int numChannels)        = 0;
    virtual void reset() noexcept RTSAN_NONBLOCKING                                    = 0;
    virtual void setParameter (int index, float value) noexcept RTSAN_NONBLOCKING      = 0;
    virtual void process (juce::AudioBuffer<float>& buffer) noexcept RTSAN_NONBLOCKING = 0;
};
} // namespace dsplay
