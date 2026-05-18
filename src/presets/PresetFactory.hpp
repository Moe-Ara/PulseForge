#pragma once

#include "Preset.hpp"

#include <optional>
#include <string>
#include <vector>

namespace PresetFactory {

std::vector<Preset> builtInPresets();
std::optional<Preset> presetById(const std::string &id);
std::vector<float> gainsForPreset(const Preset &preset);
std::vector<float> defaultFrequencies();
std::vector<int> effectValuesForPreset(const Preset &preset);
std::vector<int> defaultEffectValues();

Preset flat();
Preset gaming();
Preset music();
Preset movie();
Preset voice();
Preset bassBoost();
Preset clarity();
Preset competitiveFps();
Preset warm();
Preset bright();
Preset equalizer(const std::vector<float> &gains);
Preset equalizer(const std::vector<float> &gains,
                 const std::vector<float> &frequencies);
Preset equalizer(const std::vector<float> &gains,
                 const std::vector<float> &frequencies,
                 const std::vector<int> &effectValues);
Preset equalizer(const std::vector<float> &gains,
                 const std::vector<float> &frequencies,
                 const std::vector<int> &effectValues, float preampDb,
                 float limiterCeilingDb);

} // namespace PresetFactory
