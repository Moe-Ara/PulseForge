// IAudioBackend.hpp
#pragma once
#include "AudioDevice.hpp"
#include "EffectChain.hpp"
#include <string>
#include <vector>

class IAudioBackend
{
public:
  virtual ~IAudioBackend() = default;

  virtual bool initialize() = 0;

  virtual std::vector<AudioDevice> listOutputDevices() = 0;

  virtual bool createVirtualSink(const std::string &name) = 0;
  virtual bool removeVirtualSink(const std::string &name) = 0;

  virtual bool setTargetDevice(const std::string &deviceId) = 0;

  virtual bool applyEffectChain(const EffectChain &chain) = 0;

  virtual bool enable() = 0;
  virtual bool disable() = 0;
  virtual bool isEnabled() const = 0;
};
