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
    return {{"Speaker-1", "Built-in Speaker", true},
            {"Headphones-1", "Headphones", false}};
  }
  bool createVirtualSink(const std::string &name) override {
    Logger::info("Creating virtual sink: " + name);
    return true;
  }

  bool removeVirtualSink(const std::string &name) override {
    Logger::info("Removing virtual sink: " + name);
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
};
