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
  return {24, 10, 6, 8, 8};
}

std::vector<int> effectValuesForPreset(const Preset &preset) {
  if (preset.id == "gaming") {
    return {42, 14, 34, 24, 12};
  }
  if (preset.id == "music") {
    return {36, 24, 14, 16, 22};
  }
  if (preset.id == "movie") {
    return {32, 34, 30, 24, 30};
  }
  if (preset.id == "voice") {
    return {48, 4, 0, 10, 0};
  }
  if (preset.id == "bass-boost") {
    return {22, 12, 8, 16, 54};
  }
  if (preset.id == "clarity") {
    return {62, 8, 4, 12, 0};
  }
  return defaultEffectValues();
}

Preset flat() {
  return makePreset("flat", "Flat", "Neutral response at unity loudness.",
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 0.0f},
                    0.0f, 0.0f);
}

Preset gaming() {
  return makePreset("gaming", "Gaming",
                    "Tighter lows and clearer positional detail.",
                    {-1.5f, -1.0f, -0.5f, 1.0f, 1.8f,
                     2.2f, 1.6f, 0.8f, 0.2f},
                    -0.2f, 0.0f,
                    compressor(-10.0f, 1.25f, 8.0f, 140.0f));
}

Preset music() {
  return makePreset("music", "Music", "Gentle warmth with a little air.",
                    {1.2f, 0.8f, 0.2f, 0.0f, 0.2f,
                     0.7f, 1.0f, 0.8f, 0.3f},
                    0.0f, 0.0f,
                    compressor(-9.0f, 1.15f, 18.0f, 180.0f));
}

Preset movie() {
  return makePreset("movie", "Movie", "Fuller low end with clearer dialog.",
                    {2.0f, 1.4f, 0.2f, 0.6f, 1.2f,
                     1.4f, 0.7f, 0.2f, 0.0f},
                    -0.5f, 0.0f,
                    compressor(-12.0f, 1.35f, 12.0f, 220.0f));
}

Preset voice() {
  return makePreset("voice", "Voice", "Focused speech and reduced rumble.",
                    {-4.0f, -3.0f, -1.5f, 0.8f, 2.0f,
                     2.2f, 0.8f, -0.8f, -1.5f},
                    0.0f, 0.0f,
                    compressor(-14.0f, 1.6f, 6.0f, 150.0f));
}

Preset bassBoost() {
  return makePreset("bass-boost", "Bass Boost",
                    "More low-end weight without crushing volume.",
                    {3.0f, 2.4f, 1.0f, 0.2f, -0.6f,
                     -0.2f, 0.5f, 0.2f, 0.0f},
                    -0.5f, 0.0f,
                    compressor(-11.0f, 1.3f, 14.0f, 200.0f));
}

Preset clarity() {
  return makePreset("clarity", "Clarity",
                    "Cleaner presence with restrained brightness.",
                    {-1.5f, -1.0f, -0.3f, 0.8f, 1.4f,
                     1.8f, 1.6f, 0.9f, 0.2f},
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
  const float preampDb = 0.35f + dynamicBoost * 0.85f;
  std::optional<Effect> compressorEffect = std::nullopt;
  if (dynamicBoost > 0.05f) {
    compressorEffect =
        compressor(-9.0f - dynamicBoost * 7.0f,
                   1.08f + dynamicBoost * 0.52f, 9.0f, 170.0f);
  }

  return Preset{"custom-eq", "Custom EQ", "User controlled equalizer curve.",
                chainFor(safeGains, safeFrequencies, preampDb, 0.0f,
                         compressorEffect)};
}

} // namespace PresetFactory
