#pragma once
#include "../audio/IAudioBackend.hpp"
#include "../presets/Preset.hpp"
#include "../system/AutoStartManager.hpp"

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
  bool enableAutoStart();
  bool disableAutoStart();
  bool isAutoStartEnabled() const;

private:
  IAudioBackend &backend;
  AutoStartManager autoStartManager;
};
