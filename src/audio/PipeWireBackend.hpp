#pragma once

#include "AudioDevice.hpp"
#include "EffectChain.hpp"
#include "IAudioBackend.hpp"
#include <pipewire/pipewire.h>
#include <spa/utils/dict.h>
#include <string>
#include <vector>

class PipeWireBackend : public IAudioBackend {
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
  bool routeDefaultSinkToProcessingSink();
  bool restorePreviousDefaultSink();

  static void onRegistryGlobal(void *data, uint32_t id, uint32_t permissions,
                               const char *type, uint32_t version,
                               const struct spa_dict *props);
  static void onCoreDone(void *data, uint32_t id, int seq);
  bool createOrReloadFilterSink();
  bool removeFilterSink();
  bool createFilterSinkWithPactl();
  bool createFilterSinkWithPipeWireDaemon();
  bool removeFilterChainDaemon();
  bool cleanupStaleFilterSink();
  bool saveRuntimeState() const;
  bool clearRuntimeState() const;
  bool restoreDefaultSinkFromRuntimeState();
  bool verifyFilterSinkRouting() const;
  bool targetSinkIsVisibleToPactl() const;
  bool waitForProcessingSink() const;
  bool writeFilterChainDaemonConfig() const;
  std::vector<std::string> buildFilterChainModuleArgs() const;
  std::string buildFilterChainModuleArgsForLog() const;
  std::string buildFilterChainDaemonConfig() const;
  std::string buildFilterGraphArgs() const;
  std::string buildCapturePropsArgs() const;
  std::string buildPlaybackPropsArgs() const;
  std::string buildParamEqFilters() const;
  std::string runtimeStatePath() const;
  std::string filterChainConfigPath() const;
  std::string resolveTargetSinkName() const;

private:
  pw_main_loop *loop = nullptr;
  pw_context *context = nullptr;
  pw_core *core = nullptr;
  pw_registry *registry = nullptr;

  std::vector<AudioDevice> outputDevices;

private:
  int pendingSeq = 0;
  bool syncDone = false;
  spa_hook registryListener{};
  spa_hook coreListener{};

  bool enabled = false;
  bool defaultSinkRouted = false;
  std::string virtualSinkName = "pulseforge_enhanced";
  std::string virtualSinkDisplayName = "PulseForge Enhanced";
  std::string filterOutputNodeName = "pulseforge_enhanced_output";
  std::string previousDefaultSinkName{};

  int filterModuleId = -1;
  int filterProcessId = -1;
  std::string selectedSinkName;
  EffectChain currentEffectChain;
};
