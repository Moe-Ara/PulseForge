#include "AudioService.hpp"
#include "../core/Logger.hpp"
#include <exception>
#include <vector>

AudioService::AudioService(IAudioBackend &backend) : backend(backend) {}

bool AudioService::initialize() { return backend.initialize(); }

std::vector<AudioDevice> AudioService::getOutputDevices() {
  return backend.listOutputDevices();
}

bool AudioService::enableEnhancement() {
  backend.createVirtualSink("BazziteSound Enhanced");
  return backend.enable();
}

bool AudioService::disableEnhancement() {
  backend.disable();
  return backend.removeVirtualSink("BazziteSound Enhanced");
}

bool AudioService::selectOutputDevice(const std::string &deviceId) {
  return backend.setTargetDevice(deviceId);
}

bool AudioService::applyPreset(const Preset &preset) {
  Logger::info("Applying preset: " + preset.name);
  return backend.applyEffectChain(preset.chain);
}
