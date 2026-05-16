#pragma once

#include "AudioDevice.hpp"
#include "EffectChain.hpp"
#include "IAudioBackend.hpp"
#include <pipewire/pipewire.h>
#include <spa/utils/dict.h>
#include <string>
#include <vector>

class PipeWireBackend : public IAudioBackend
{
public:
  PipeWireBackend();
  ~PipeWireBackend() override;

  bool initialize() override;
  std::vector<AudioDevice> listOutputDevices() override;

  bool createVirtualSink(const std::string &name) override;
  bool removeVirtualSink(const std::string &name) override;

  bool applyEffectChain(const EffectChain &chain) override;
  bool enable() override;
  bool disable() override;
  bool setTargetDevice(const std::string &deviceId) override;
  bool isEnabled() const override;
  bool setDefaultSinkToVirtual();
  bool restorePreviousDefaultSink();
private:
  static void onRegistryGlobal(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props);
  static void onCoreDone(void *data, uint32_t id, int seq);


private:
  pw_main_loop *loop = nullptr;
  pw_context *context = nullptr;
  pw_core *core = nullptr;
  pw_registry *registry = nullptr;

  std::vector<AudioDevice> outputDevices;

private:
  int pendingSeq = 0;
  bool syncDone = false;
  std::string selectedDeviceId;
  spa_hook registryListener{};
  spa_hook coreListener{};

  bool enabled = false;
  int virtualSinkModuleId = -1;
  std::string virtualSinkName = "pulseforge_enhanced";
  std::string virtualSinkDisplayName = "PulseForge Enhanced";
  std::string previousDefaultSinkName{};
};
