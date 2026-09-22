#pragma once

#include <juce_dsp/juce_dsp.h>

#include "../util/RealtimeAttributes.h"

namespace dsplay
{
// Every algorithm exposes at most this many parameters, one per rotary slider in the UI.
constexpr std::size_t maxParameters { 5 };

// Describes one algorithm parameter in real-world units. The UI configures a slider from it: `min`/`max` become the
// range, a non-zero `skewCentre` is the value shown at the slider's midpoint (log-like sweep for Hz and ms), and the
// text box shows the value with `decimals` digits followed by `suffix`.
struct ParameterDescriptor
{
    const char* name { "" };
    float       min { 0.f };
    float       max { 1.f };
    float       skewCentre { 0.f };
    float       defaultValue { 0.f };
    const char* suffix { "" };
    int         decimals { 1 };
};

struct AlgorithmDescriptor
{
    const char*                                    name { "" };
    std::size_t                                    numParameters { 0 };
    std::array<ParameterDescriptor, maxParameters> parameters {};
};

// Base class for every DSP algorithm in the playground.
//
// Parameters are stored here as real-world values in atomics: the UI writes them with setParameter() from the message
// thread and the algorithm reads them with getParameter() at the top of process(). Subclasses declare a scoped enum
// naming their parameters and pass it straight to getParameter(), so the DSP code never sees a bare index.
//
// Lifecycle follows juce::dsp::ProcessorBase: prepare() may allocate, reset() clears state and is called on the audio
// thread when the algorithm becomes active, process() runs once per block. reset() and process() must be realtime-safe.
class Algorithm : public juce::dsp::ProcessorBase
{
public:
    explicit Algorithm (const AlgorithmDescriptor& descriptorToUse) : descriptor (descriptorToUse)
    {
        for (std::size_t i = 0; i < maxParameters; ++i)
            values[i].store (descriptor.parameters[i].defaultValue, std::memory_order_relaxed);
    }

    ~Algorithm() override = default;

    Algorithm (const Algorithm&)            = delete;
    Algorithm& operator= (const Algorithm&) = delete;
    Algorithm (Algorithm&&)                 = delete;
    Algorithm& operator= (Algorithm&&)      = delete;

    [[nodiscard]] const AlgorithmDescriptor& getDescriptor() const noexcept { return descriptor; }

    void setParameter (std::size_t index, float value) noexcept
    {
        values[index].store (value, std::memory_order_relaxed);
    }

    [[nodiscard]] float getParameter (std::size_t index) const noexcept
    {
        return values[index].load (std::memory_order_relaxed);
    }

    template <typename Enum>
        requires std::is_enum_v<Enum>
    [[nodiscard]] float getParameter (Enum parameter) const noexcept
    {
        return getParameter (static_cast<std::size_t> (std::to_underlying (parameter)));
    }

    // Tighten the ProcessorBase contract: these run on the audio thread.
    void reset() noexcept RTSAN_NONBLOCKING override                                                            = 0;
    void process (const juce::dsp::ProcessContextReplacing<float>& context) noexcept RTSAN_NONBLOCKING override = 0;

private:
    const AlgorithmDescriptor&                    descriptor;
    std::array<std::atomic<float>, maxParameters> values {};
};
} // namespace dsplay
