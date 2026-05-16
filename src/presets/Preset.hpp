#pragma once
#include "../audio/EffectChain.hpp"
#include <string>

struct Preset {
  std::string id;
  std::string name;
  std::string description;
  EffectChain chain;
};
