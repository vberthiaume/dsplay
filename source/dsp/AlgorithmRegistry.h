#pragma once

#include "algorithms/LowPassFilter.h"
#include "algorithms/Compressor.h"

namespace dsplay
{
// The list of algorithms available in the playground. The order here is the order in the combo box and the index
// stored in the "algorithm" parameter, so append new algorithms at the end to keep saved sessions valid.
constexpr int numAlgorithms { 2 };

inline std::array<std::unique_ptr<Algorithm>, numAlgorithms> createAlgorithms()
{
    return { std::make_unique<LowPassFilter>(), std::make_unique<Compressor>() };
}
} // namespace dsplay
