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

void makeGainsClipSafe(PresetGains &gains) {
  float totalPositiveBoostDb = 0.0f;
  for (float &gain : gains) {
    gain = std::clamp(gain, DspConfig::minEffectiveEqGainDb,
                      DspConfig::maxEffectiveEqGainDb);
    totalPositiveBoostDb += std::max(0.0f, gain);
  }

  if (totalPositiveBoostDb <= DspConfig::maxTotalPositiveEqBoostDb ||
      totalPositiveBoostDb <= DspConfig::tinySignal) {
    return;
  }

  const float scale =
      DspConfig::maxTotalPositiveEqBoostDb / totalPositiveBoostDb;
  for (float &gain : gains) {
    if (gain > 0.0f) {
      gain *= scale;
    }
  }
}

EffectChain chainFor(const PresetGains &gains,
                     const PresetFrequencies &frequencies, float preampDb,
                     float limiterCeilingDb,
                     std::optional<Effect> compressorEffect) {
  EffectChain chain;
  chain.effects.reserve(DspConfig::eqBandCount + 3);
  chain.effects.push_back(preamp(preampDb));

  for (std::size_t i = 0; i < frequencies.size(); ++i) {
    const float q =
        i < DspConfig::eqDefaultQ.size() ? DspConfig::eqDefaultQ.at(i) : 0.9f;
    chain.effects.push_back({"eq_band",
                             {{"freq", frequencies.at(i)},
                              {"gain", gains.at(i)},
                              {"q", q}}});
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
  PresetGains safeGains = gains;
  makeGainsClipSafe(safeGains);
  return Preset{id, name, description,
                chainFor(safeGains, builtInFrequencies(), preampDb,
                         limiterCeilingDb, std::move(compressorEffect))};
}

} // namespace

std::vector<Preset> builtInPresets() {
  return {flat(),          gaming(), music(), movie(), voice(),
          bassBoost(),     clarity(), competitiveFps(), warm(), bright()};
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
  return {42, 18, 10, 24, 18, 66, 100};
}

std::vector<int> effectValuesForPreset(const Preset &preset) {
  if (preset.id == "gaming") {
    return {54, 14, 34, 30, 8, 64, 100};
  }
  if (preset.id == "music") {
    return {46, 26, 16, 36, 24, 64, 100};
  }
  if (preset.id == "movie") {
    return {40, 34, 30, 40, 32, 60, 100};
  }
  if (preset.id == "voice") {
    return {60, 4, 0, 26, 0, 66, 100};
  }
  if (preset.id == "bass-boost") {
    return {30, 14, 8, 32, 54, 58, 100};
  }
  if (preset.id == "clarity") {
    return {70, 8, 2, 30, 2, 66, 100};
  }
  if (preset.id == "competitive-fps") {
    return {68, 6, 8, 26, 0, 68, 100};
  }
  if (preset.id == "warm") {
    return {28, 24, 10, 18, 34, 62, 100};
  }
  if (preset.id == "bright") {
    return {76, 10, 4, 24, 0, 68, 100};
  }
  return defaultEffectValues();
}

Preset flat() {
  return makePreset("flat", "Flat", "Neutral response at unity loudness.",
                    {0.2f, 0.1f, -0.4f, -0.2f, 0.1f,
                     0.4f, 0.6f, 0.6f, 0.3f},
                    -1.0f, -1.0f);
}

Preset gaming() {
  return makePreset("gaming", "Gaming",
                    "Tighter lows and clearer positional detail.",
                    {-1.8f, -1.4f, -1.2f, 0.3f, 1.3f,
                     2.4f, 2.2f, 1.2f, 0.4f},
                    -1.5f, -1.0f,
                    compressor(-12.0f, 1.2f, 10.0f, 170.0f));
}

Preset music() {
  return makePreset("music", "Music", "Gentle warmth with a little air.",
                    {1.1f, 0.7f, -0.8f, -0.3f, 0.3f,
                     0.9f, 1.3f, 1.2f, 0.7f},
                    -1.2f, -1.0f,
                    compressor(-11.0f, 1.12f, 18.0f, 220.0f));
}

Preset movie() {
  return makePreset("movie", "Movie", "Fuller low end with clearer dialog.",
                    {1.8f, 1.2f, -0.9f, 0.0f, 1.0f,
                     1.7f, 1.2f, 0.7f, 0.3f},
                    -1.8f, -1.0f,
                    compressor(-13.0f, 1.25f, 14.0f, 240.0f));
}

Preset voice() {
  return makePreset("voice", "Voice", "Focused speech and reduced rumble.",
                    {-4.0f, -3.0f, -1.8f, 0.5f, 1.7f,
                     2.4f, 1.0f, -1.2f, -1.8f},
                    -1.2f, -1.0f,
                    compressor(-15.0f, 1.45f, 8.0f, 180.0f));
}

Preset bassBoost() {
  return makePreset("bass-boost", "Bass Boost",
                    "More low-end weight without crushing volume.",
                    {3.2f, 2.3f, 0.0f, -0.8f, -0.6f,
                     0.0f, 0.7f, 0.5f, 0.2f},
                    -2.0f, -1.0f,
                    compressor(-12.0f, 1.3f, 16.0f, 240.0f));
}

Preset clarity() {
  return makePreset("clarity", "Clarity",
                    "Cleaner presence with restrained brightness.",
                    {-1.8f, -1.4f, -1.1f, 0.2f, 1.2f,
                     2.4f, 2.6f, 1.6f, 0.7f},
                    -1.5f, -1.0f,
                    compressor(-12.0f, 1.18f, 10.0f, 180.0f));
}

Preset competitiveFps() {
  return makePreset("competitive-fps", "Competitive FPS",
                    "Maximum footstep focus with restrained bass.",
                    {-3.2f, -2.6f, -1.8f, 0.2f, 1.6f,
                     2.6f, 2.0f, 0.6f, -0.2f},
                    -1.4f, -1.0f,
                    compressor(-13.0f, 1.18f, 8.0f, 150.0f));
}

Preset warm() {
  return makePreset("warm", "Warm",
                    "Smooth relaxed listening with fuller lows.",
                    {1.4f, 1.0f, -0.7f, -0.8f, -0.3f,
                     0.2f, 0.3f, 0.2f, 0.0f},
                    -1.4f, -1.0f,
                    compressor(-12.0f, 1.12f, 20.0f, 260.0f));
}

Preset bright() {
  return makePreset("bright", "Bright",
                    "Air and detail without sharp upper mids.",
                    {-1.0f, -0.9f, -1.2f, -0.3f, 0.6f,
                     1.2f, 2.1f, 2.2f, 1.0f},
                    -1.6f, -1.0f,
                    compressor(-12.0f, 1.12f, 12.0f, 200.0f));
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
  makeGainsClipSafe(safeGains);

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
      std::clamp(-1.5f + dynamicBoost * 1.6f + fidelity * 0.25f +
                     bass * 0.2f + preampDb,
                 -12.0f, 6.0f);
  const float safeLimiterCeilingDb =
      std::clamp(limiterCeilingDb, DspConfig::minLimiterCeilingDb,
                 -1.0f);
  std::optional<Effect> compressorEffect = std::nullopt;
  if (dynamicBoost > 0.05f) {
    compressorEffect =
        compressor(-8.0f - dynamicBoost * 3.0f,
                   1.08f + dynamicBoost * 0.32f, 18.0f, 260.0f);
  }

  EffectChain chain =
      chainFor(safeGains, safeFrequencies, totalPreampDb,
               safeLimiterCeilingDb, compressorEffect);
  const float width = 1.0f + surround * 0.65f + ambience * 0.22f;
  chain.effects.push_back(stereoWidth(width));

  return Preset{"custom-eq", "Custom EQ", "User controlled equalizer curve.",
                chain};
}

} // namespace PresetFactory
