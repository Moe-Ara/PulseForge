#pragma once

#include "Preset.hpp"

#include <optional>
#include <string>
#include <vector>

namespace PresetFactory {

std::vector<Preset> builtInPresets();
std::optional<Preset> presetById(const std::string &id);
std::vector<float> gainsForPreset(const Preset &preset);

Preset flat();
Preset gaming();
Preset music();
Preset movie();
Preset voice();
Preset bassBoost();
Preset clarity();
Preset equalizer(const std::vector<float> &gains);

} // namespace PresetFactory
