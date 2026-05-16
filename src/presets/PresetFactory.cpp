#include "PresetFactory.hpp"

#include <array>
#include <cstddef>

namespace PresetFactory {

Preset gaming() {
  return Preset{"gaming",
                "Gaming",
                "Boost clarity and footsteps.",
                {{{"eq", {{"bass", 1.1f}, {"treble", 1.25f}}},
                  {"compressor", {{"threshold", -18.0f}, {"ratio", 2.0f}}},
                  {"limiter", {{"ceiling", -1.0f}}}}}};
}

Preset equalizer(const std::vector<float> &gains) {
  constexpr std::array<float, 9> frequencies = {110.0f, 220.0f, 400.0f,
                                                750.0f, 1500.0f, 3000.0f,
                                                6000.0f, 12000.0f, 16000.0f};

  EffectChain chain;

  for (std::size_t i = 0; i < frequencies.size() && i < gains.size(); ++i) {
    chain.effects.push_back({"eq_band",
                             {{"freq", frequencies.at(i)},
                              {"gain", gains.at(i)},
                              {"q", 1.0f}}});
  }

  return Preset{"custom-eq", "Custom EQ",
                "User controlled equalizer curve.", chain};
}

} // namespace PresetFactory
