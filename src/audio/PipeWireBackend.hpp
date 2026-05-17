#pragma once

#include "AudioConfig.hpp"
#include "AudioDevice.hpp"
#include "EffectChain.hpp"
#include "IAudioBackend.hpp"
#include "PipeWireRouting.hpp"
#include "dsp/AudioProcessor.hpp"
#include "system/ProcessRunner.hpp"
#include "system/RuntimeStateStore.hpp"

#include <mutex>
#include <pipewire/pipewire.h>
#include <spa/utils/dict.h>
#include <string>
#include <vector>
#include <mutex>

class PipeWireBackend : public IAudioBackend
{
public:
  PipeWireBackend();
  ~PipeWireBackend() override;

  bool initialize() override;
  std::vector<AudioDevice> listOutputDevices() override;

  bool createProcessingSink(const std::string &displayName) override;
  bool removeProcessingSink() override;

  bool applyEffectChain(const EffectChain &chain) override;
  bool enable() override;
  bool disable() override;
  bool setTargetDevice(const std::string &deviceId) override;
  bool isEnabled() const override;

private:
  static void onRegistryGlobal(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props);
  static void onCoreDone(void *data, uint32_t id, int seq);

  bool createVirtualSink();
  bool removeVirtualSink();
  bool createLoopback();
  bool removeLoopback();
  bool rememberDefaultSink();
  bool setDefaultSink(const std::string &sinkName);
  bool restorePreviousDefaultSink();
  bool cleanupStaleModules();
  bool saveRuntimeState() const;
  bool clearRuntimeState() const;
  bool restoreDefaultSinkFromRuntimeState();
  std::string resolveTargetSinkName() const;

private:
  mutable std::mutex mutex;

  pw_main_loop *loop = nullptr;
  pw_context *context = nullptr;
  pw_core *core = nullptr;
  pw_registry *registry = nullptr;

  std::vector<AudioDevice> outputDevices;

  int pendingSeq = 0;
  bool syncDone = false;
  spa_hook registryListener{};
  spa_hook coreListener{};

  bool enabled = false;
  bool usingAudioProcessor = false;
  std::string virtualSinkName = std::string(AudioConfig::virtualSinkName);
  std::string virtualSinkDisplayName =
      std::string(AudioConfig::virtualSinkDisplayName);
  std::string monitorSourceName = std::string(AudioConfig::monitorSourceName);
  std::string previousDefaultSinkName;
  std::string selectedSinkName;

  int virtualSinkModuleId = -1;
  int loopbackModuleId = -1;
  EffectChain currentEffectChain;
  AudioProcessor audioProcessor;
  ProcessRunner processRunner;
  RuntimeStateStore runtimeStateStore;
  PipeWireRouting routing{processRunner};
};
