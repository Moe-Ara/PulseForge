#include "PresetFactory.hpp"

#include "dsp/DspConfig.hpp"

#include <array>
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <optional>
#include <string>
#include <utility>

namespace PresetFactory {
namespace {

using PresetGains = std::array<float, DspConfig::eqBandCount>;
using PresetFrequencies = std::array<float, DspConfig::eqBandCount>;

Effect preamp(float db) {
  return {"preamp", {{"db", db}}};
}

Effect limiter(float ceilingDb) {
  return {"limiter", {{"ceiling", ceilingDb}}};
}

Effect compressor(float thresholdDb, float ratio, float attackMs,
                  float releaseMs) {
  return {"compressor",
          {{"threshold", thresholdDb},
           {"ratio", ratio},
           {"attack_ms", attackMs},
           {"release_ms", releaseMs}}};
}

Effect stereoWidth(float width) {
  return {"stereo_width", {{"width", width}}};
}

PresetFrequencies builtInFrequencies() {
  PresetFrequencies frequencies{};
  std::copy(DspConfig::eqFrequencies.begin(), DspConfig::eqFrequencies.end(),
            frequencies.begin());
  return frequencies;
}

float normalizedEffectValue(const std::vector<int> &values,
                            std::size_t index) {
  if (index >= values.size()) {
    return 0.0f;
  }
  return std::clamp(static_cast<float>(values.at(index)), 0.0f, 100.0f) /
         100.0f;
}

EffectChain chainFor(const PresetGains &gains,
                     const PresetFrequencies &frequencies, float preampDb,
                     float limiterCeilingDb,
                     std::optional<Effect> compressorEffect) {
  EffectChain chain;
  chain.effects.reserve(DspConfig::eqBandCount + 3);
  chain.effects.push_back(preamp(preampDb));

  for (std::size_t i = 0; i < frequencies.size(); ++i) {
    chain.effects.push_back({"eq_band",
                             {{"freq", frequencies.at(i)},
                              {"gain", gains.at(i)},
                              {"q", 1.0f}}});
  }

  if (compressorEffect) {
    chain.effects.push_back(*compressorEffect);
  }
  chain.effects.push_back(limiter(limiterCeilingDb));
  return chain;
}

Preset makePreset(const std::string &id, const std::string &name,
                  const std::string &description,
                  const PresetGains &gains, float preampDb,
                  float limiterCeilingDb,
                  std::optional<Effect> compressorEffect = std::nullopt) {
  return Preset{id, name, description,
                chainFor(gains, builtInFrequencies(), preampDb,
                         limiterCeilingDb, std::move(compressorEffect))};
}

} // namespace

std::vector<Preset> builtInPresets() {
  return {flat(), gaming(), music(), movie(), voice(), bassBoost(), clarity()};
}

std::optional<Preset> presetById(const std::string &id) {
  for (const auto &preset : builtInPresets()) {
    if (preset.id == id) {
      return preset;
    }
  }

  return std::nullopt;
}

std::vector<float> gainsForPreset(const Preset &preset) {
  std::vector<float> gains;
  gains.reserve(DspConfig::eqFrequencies.size());

  for (const auto &effect : preset.chain.effects) {
    if (effect.type != "eq_band") {
      continue;
    }

    const auto gain = effect.parameters.find("gain");
    gains.push_back(gain == effect.parameters.end() ? 0.0f : gain->second);
  }

  if (gains.size() < DspConfig::eqFrequencies.size()) {
    gains.resize(DspConfig::eqFrequencies.size(), 0.0f);
  }
  return gains;
}

std::vector<float> defaultFrequencies() {
  return {DspConfig::eqFrequencies.begin(), DspConfig::eqFrequencies.end()};
}

std::vector<int> defaultEffectValues() {
  return {50, 24, 14, 34, 24, 50, 100};
}

std::vector<int> effectValuesForPreset(const Preset &preset) {
  if (preset.id == "gaming") {
    return {66, 18, 46, 42, 10, 50, 100};
  }
  if (preset.id == "music") {
    return {56, 34, 24, 54, 34, 50, 100};
  }
  if (preset.id == "movie") {
    return {48, 42, 42, 58, 44, 48, 100};
  }
  if (preset.id == "voice") {
    return {76, 6, 0, 36, 0, 50, 100};
  }
  if (preset.id == "bass-boost") {
    return {38, 20, 10, 48, 76, 46, 100};
  }
  if (preset.id == "clarity") {
    return {88, 10, 4, 42, 4, 50, 100};
  }
  return defaultEffectValues();
}

Preset flat() {
  return makePreset("flat", "Flat", "Neutral response at unity loudness.",
                    {0.4f, 0.2f, -0.7f, -0.3f, 0.2f,
                     0.8f, 1.2f, 1.1f, 0.7f},
                    0.0f, 0.0f);
}

Preset gaming() {
  return makePreset("gaming", "Gaming",
                    "Tighter lows and clearer positional detail.",
                    {-2.4f, -2.0f, -1.5f, 0.6f, 2.0f,
                     3.6f, 3.2f, 1.8f, 0.6f},
                    -0.2f, 0.0f,
                    compressor(-10.0f, 1.25f, 8.0f, 140.0f));
}

Preset music() {
  return makePreset("music", "Music", "Gentle warmth with a little air.",
                    {2.0f, 1.3f, -1.0f, -0.4f, 0.5f,
                     1.6f, 2.4f, 2.3f, 1.4f},
                    0.0f, 0.0f,
                    compressor(-9.0f, 1.15f, 18.0f, 180.0f));
}

Preset movie() {
  return makePreset("movie", "Movie", "Fuller low end with clearer dialog.",
                    {3.2f, 2.1f, -1.2f, 0.1f, 1.6f,
                     2.6f, 1.9f, 1.1f, 0.5f},
                    -0.5f, 0.0f,
                    compressor(-12.0f, 1.35f, 12.0f, 220.0f));
}

Preset voice() {
  return makePreset("voice", "Voice", "Focused speech and reduced rumble.",
                    {-6.0f, -4.2f, -2.4f, 0.8f, 2.6f,
                     3.8f, 1.8f, -1.4f, -2.2f},
                    0.0f, 0.0f,
                    compressor(-14.0f, 1.6f, 6.0f, 150.0f));
}

Preset bassBoost() {
  return makePreset("bass-boost", "Bass Boost",
                    "More low-end weight without crushing volume.",
                    {5.2f, 3.8f, 0.4f, -0.8f, -0.8f,
                     0.2f, 1.2f, 0.9f, 0.4f},
                    -0.5f, 0.0f,
                    compressor(-11.0f, 1.3f, 14.0f, 200.0f));
}

Preset clarity() {
  return makePreset("clarity", "Clarity",
                    "Cleaner presence with restrained brightness.",
                    {-2.6f, -2.0f, -1.4f, 0.4f, 1.8f,
                     3.6f, 4.0f, 2.5f, 1.2f},
                    0.0f, 0.0f,
                    compressor(-10.0f, 1.2f, 8.0f, 150.0f));
}

Preset equalizer(const std::vector<float> &gains) {
  return equalizer(gains, defaultFrequencies());
}

Preset equalizer(const std::vector<float> &gains,
                 const std::vector<float> &frequencies) {
  return equalizer(gains, frequencies, defaultEffectValues());
}

Preset equalizer(const std::vector<float> &gains,
                 const std::vector<float> &frequencies,
                 const std::vector<int> &effectValues) {
  return equalizer(gains, frequencies, effectValues, 0.0f, 0.0f);
}

Preset equalizer(const std::vector<float> &gains,
                 const std::vector<float> &frequencies,
                 const std::vector<int> &effectValues, float preampDb,
                 float limiterCeilingDb) {
  PresetGains safeGains{};
  for (std::size_t i = 0; i < safeGains.size() && i < gains.size(); ++i) {
    safeGains.at(i) = gains.at(i);
  }

  PresetFrequencies safeFrequencies = builtInFrequencies();
  for (std::size_t i = 0; i < safeFrequencies.size() && i < frequencies.size();
       ++i) {
    const float frequency = frequencies.at(i);
    if (std::isfinite(frequency)) {
      safeFrequencies.at(i) =
          std::clamp(frequency, DspConfig::minEditableEqFrequencyHz,
                     DspConfig::maxEditableEqFrequencyHz);
    }
  }

  const float dynamicBoost = normalizedEffectValue(effectValues, 3);
  const float fidelity = normalizedEffectValue(effectValues, 0);
  const float ambience = normalizedEffectValue(effectValues, 1);
  const float surround = normalizedEffectValue(effectValues, 2);
  const float bass = normalizedEffectValue(effectValues, 4);
  const float totalPreampDb =
      std::clamp(1.0f + dynamicBoost * 2.4f + fidelity * 0.8f +
                     bass * 0.45f + preampDb,
                 -12.0f, 12.0f);
  const float safeLimiterCeilingDb =
      std::clamp(limiterCeilingDb, DspConfig::minLimiterCeilingDb,
                 DspConfig::maxLimiterCeilingDb);
  std::optional<Effect> compressorEffect = std::nullopt;
  if (dynamicBoost > 0.05f) {
    compressorEffect =
        compressor(-12.0f - dynamicBoost * 14.0f,
                   1.25f + dynamicBoost * 1.65f, 3.0f, 95.0f);
  }

  EffectChain chain =
      chainFor(safeGains, safeFrequencies, totalPreampDb,
               safeLimiterCeilingDb, compressorEffect);
  const float width = 1.0f + surround * 1.15f + ambience * 0.45f;
  chain.effects.push_back(stereoWidth(width));

  return Preset{"custom-eq", "Custom EQ", "User controlled equalizer curve.",
                chain};
}

} // namespace PresetFactory
