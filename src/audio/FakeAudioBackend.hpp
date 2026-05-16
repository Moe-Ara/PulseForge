#pragma once
#include "../core/Logger.hpp"
#include "IAudioBackend.hpp"
#include <vector>

class FakeAudioBackend : public IAudioBackend {
public:
  bool initialize() override {
    Logger::info("Fake audio backend initialized");
    return true;
  }
  std::vector<AudioDevice> listOutputDevices() override {
    return {{"Speaker-1", "Built-in Speaker", "speaker-1", true},
            {"Headphones-1", "Headphones", "headphones-1", false}};
  }
  bool createProcessingSink(const std::string &displayName) override {
    Logger::info("Creating processing sink: " + displayName);
    return true;
  }

  bool removeProcessingSink() override {
    Logger::info("Removing processing sink");
    return true;
  }

  bool setTargetDevice(const std::string &deviceId) override {
    Logger::info("Target device set to: " + deviceId);
    return true;
  }

  bool applyEffectChain(const EffectChain &chain) override {
    Logger::info("Applying effect chain");

    for (const auto &effect : chain.effects) {
      Logger::info("Effect: " + effect.type);
    }

    return true;
  }

  bool enable() override {
    Logger::info("Enhancement enabled");
    return true;
  }

  bool disable() override {
    Logger::info("Enhancement disabled");
    return true;
  }

  bool isEnabled() const override {
    return false;
  }
};
