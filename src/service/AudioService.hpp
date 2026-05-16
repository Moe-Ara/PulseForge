#pragma once
#include "../audio/IAudioBackend.hpp"
#include "../presets/Preset.hpp"
class AudioService {
public:
  explicit AudioService(IAudioBackend &backend);

  bool initialize();
  std::vector<AudioDevice> getOutputDevices();

  bool enableEnhancement();
  bool disableEnhancement();

  bool selectOutputDevice(const std::string &deviceId);
  bool applyPreset(const Preset &preset);

private:
  IAudioBackend &backend;
};
