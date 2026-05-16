#pragma once

#include "Preset.hpp"

#include <vector>

namespace PresetFactory {

Preset gaming();
Preset equalizer(const std::vector<float> &gains);

} // namespace PresetFactory
