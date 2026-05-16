#pragma once
#include "../audio/IAudioBackend.hpp"
#include "../presets/Preset.hpp"

#include <string>
#include <vector>

class AudioService {
public:
  explicit AudioService(IAudioBackend &backend);

  bool initialize();
  std::vector<AudioDevice> getOutputDevices();

  bool enableEnhancement();
  bool disableEnhancement();

  bool selectOutputDevice(const std::string &deviceId);
  bool applyPreset(const Preset &preset);
  bool isEnabled() const;

private:
  IAudioBackend &backend;
  bool enabled = false;
};
