#pragma once
#include <string>
#include <unordered_map>
#include <vector>

struct Effect {
  std::string type;
  std::unordered_map<std::string, float> parameters;
};
struct EffectChain {
  std::vector<Effect> effects;
};
