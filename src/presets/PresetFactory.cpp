#include "PresetFactory.hpp"

#include "dsp/DspConfig.hpp"

#include <array>
#include <cstddef>
#include <optional>
#include <utility>

namespace PresetFactory {
namespace {

using PresetGains = std::array<float, DspConfig::eqBandCount>;

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

EffectChain chainFor(const PresetGains &gains, float preampDb,
                     float limiterCeilingDb,
                     std::optional<Effect> compressorEffect) {
  EffectChain chain;
  chain.effects.reserve(DspConfig::eqFrequencies.size() + 3);
  chain.effects.push_back(preamp(preampDb));

  for (std::size_t i = 0; i < DspConfig::eqFrequencies.size(); ++i) {
    chain.effects.push_back({"eq_band",
                             {{"freq", DspConfig::eqFrequencies.at(i)},
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
                chainFor(gains, preampDb, limiterCeilingDb,
                         std::move(compressorEffect))};
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

Preset flat() {
  return makePreset("flat", "Flat", "Neutral response with safe headroom.",
                    {0.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                     0.0f, 0.0f, 0.0f, 0.0f},
                    -1.0f, -1.0f);
}

Preset gaming() {
  return makePreset("gaming", "Gaming", "Footstep clarity without harsh bass.",
                    {-2.0f, -1.0f, 0.0f, 2.0f, 3.0f,
                     4.0f, 3.0f, 2.0f, 1.0f},
                    -3.0f, -1.0f,
                    compressor(-18.0f, 2.0f, 5.0f, 90.0f));
}

Preset music() {
  return makePreset("music", "Music", "Balanced warmth and sparkle.",
                    {2.0f, 1.5f, 0.5f, 0.0f, 0.0f,
                     1.0f, 1.5f, 1.0f, 0.5f},
                    -2.5f, -1.0f,
                    compressor(-14.0f, 1.4f, 12.0f, 140.0f));
}

Preset movie() {
  return makePreset("movie", "Movie", "Fuller low end with clearer dialog.",
                    {3.0f, 2.0f, 0.5f, 1.0f, 2.0f,
                     2.0f, 1.0f, 0.5f, 0.0f},
                    -4.0f, -1.0f,
                    compressor(-20.0f, 2.4f, 8.0f, 180.0f));
}

Preset voice() {
  return makePreset("voice", "Voice", "Focused speech and reduced rumble.",
                    {-6.0f, -4.0f, -2.0f, 1.0f, 3.0f,
                     3.0f, 1.0f, -1.0f, -2.0f},
                    -2.0f, -1.0f,
                    compressor(-22.0f, 2.8f, 4.0f, 120.0f));
}

Preset bassBoost() {
  return makePreset("bass-boost", "Bass Boost",
                    "Extra punch with protected output.",
                    {5.0f, 4.0f, 2.0f, 0.5f, -1.0f,
                     0.0f, 1.0f, 0.5f, 0.0f},
                    -5.0f, -1.0f,
                    compressor(-16.0f, 1.8f, 10.0f, 160.0f));
}

Preset clarity() {
  return makePreset("clarity", "Clarity", "Crisper detail with lighter lows.",
                    {-2.0f, -1.5f, -0.5f, 1.0f, 2.0f,
                     3.0f, 3.0f, 2.0f, 1.0f},
                    -3.0f, -1.0f,
                    compressor(-15.0f, 1.5f, 6.0f, 100.0f));
}

Preset equalizer(const std::vector<float> &gains) {
  PresetGains safeGains{};
  for (std::size_t i = 0; i < safeGains.size() && i < gains.size(); ++i) {
    safeGains.at(i) = gains.at(i);
  }

  return makePreset("custom-eq", "Custom EQ",
                    "User controlled equalizer curve.", safeGains, -2.0f,
                    -1.0f, compressor(-12.0f, 1.6f, 8.0f, 120.0f));
}

} // namespace PresetFactory
