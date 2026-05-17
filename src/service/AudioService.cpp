#include "AudioService.hpp"
#include "../core/Logger.hpp"

AudioService::AudioService(IAudioBackend &backend) : backend(backend) {}

bool AudioService::initialize() {
  return backend.initialize();
}

std::vector<AudioDevice> AudioService::getOutputDevices() {
  return backend.listOutputDevices();
}

bool AudioService::enableEnhancement() {
  return backend.enable();
}

bool AudioService::disableEnhancement() {
  return backend.disable();
}

bool AudioService::selectOutputDevice(const std::string &deviceId) {
  return backend.setTargetDevice(deviceId);
}

bool AudioService::applyPreset(const Preset &preset) {
  Logger::info("Applying preset: " + preset.name);
  return backend.applyEffectChain(preset.chain);
}

bool AudioService::isEnabled() const {
  return backend.isEnabled();
}
