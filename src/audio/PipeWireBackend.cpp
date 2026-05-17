#include "PipeWireBackend.hpp"

#include "../core/AppConfig.hpp"
#include "../core/Logger.hpp"

#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <spa/utils/string.h>

#include <string>

namespace {

const char *stringOrUnknown(const char *value) {
  return value ? value : "unknown";
}

bool outputContainsModule(const std::string &output, int moduleId,
                          const std::string &moduleName,
                          const std::string &marker) {
  const std::string prefix = std::to_string(moduleId) + "\t";
  const auto pos = output.find(prefix);
  if (pos == std::string::npos) {
    return false;
  }

  const auto lineEnd = output.find('\n', pos);
  const std::string line =
      output.substr(pos, lineEnd == std::string::npos ? std::string::npos
                                                      : lineEnd - pos);
  return line.find(moduleName) != std::string::npos &&
         line.find(marker) != std::string::npos;
}

} // namespace

PipeWireBackend::PipeWireBackend() {
  pw_init(nullptr, nullptr);
}

PipeWireBackend::~PipeWireBackend() {
  disable();

  if (registry) {
    pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));
  }
  if (core) {
    pw_core_disconnect(core);
  }
  if (context) {
    pw_context_destroy(context);
  }
  if (loop) {
    pw_main_loop_destroy(loop);
  }

  pw_deinit();
}

bool PipeWireBackend::initialize() {
  std::lock_guard<std::mutex> lock(mutex);

  restoreDefaultSinkFromRuntimeState();
  cleanupStaleModules();

  loop = pw_main_loop_new(nullptr);
  if (!loop) {
    Logger::error("Failed to create PipeWire main loop");
    return false;
  }

  context = pw_context_new(pw_main_loop_get_loop(loop), nullptr, 0);
  if (!context) {
    Logger::error("Failed to create PipeWire context");
    return false;
  }

  core = pw_context_connect(context, nullptr, 0);
  if (!core) {
    Logger::error("Failed to connect to PipeWire");
    return false;
  }

  registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
  if (!registry) {
    Logger::error("Failed to get PipeWire registry");
    return false;
  }

  static const pw_registry_events registryEvents = {
      .version = PW_VERSION_REGISTRY_EVENTS,
      .global = &PipeWireBackend::onRegistryGlobal,
  };

  static const pw_core_events coreEvents = {
      .version = PW_VERSION_CORE_EVENTS,
      .done = &PipeWireBackend::onCoreDone,
  };

  pw_registry_add_listener(registry, &registryListener, &registryEvents, this);
  pw_core_add_listener(core, &coreListener, &coreEvents, this);

  syncDone = false;
  pendingSeq = pw_core_sync(core, PW_ID_CORE, 0);
  while (!syncDone) {
    pw_loop_iterate(pw_main_loop_get_loop(loop), -1);
  }

  Logger::info("PipeWire backend initialized");
  return true;
}

std::vector<AudioDevice> PipeWireBackend::listOutputDevices() {
  std::lock_guard<std::mutex> lock(mutex);
  return outputDevices;
}

bool PipeWireBackend::createProcessingSink(const std::string &displayName) {
  std::lock_guard<std::mutex> lock(mutex);
  virtualSinkDisplayName = displayName;
  return createVirtualSink();
}

bool PipeWireBackend::removeProcessingSink() {
  std::lock_guard<std::mutex> lock(mutex);
  const bool loopbackRemoved = removeLoopback();
  const bool sinkRemoved = removeVirtualSink();
  const bool removed = loopbackRemoved && sinkRemoved;
  if (removed) {
    enabled = false;
    usingAudioProcessor = false;
  }
  return removed;
}

bool PipeWireBackend::applyEffectChain(const EffectChain &chain) {
  std::lock_guard<std::mutex> lock(mutex);
  currentEffectChain = chain;
  audioProcessor.setEffectChain(chain);
  Logger::info("Stored effect chain with " +
               std::to_string(chain.effects.size()) +
               " effect(s). AudioProcessor maps gain, EQ, compressor, and "
               "limiter effects.");
  if (enabled && !usingAudioProcessor) {
    Logger::warn(
        "DSP settings updated, but live processing is using loopback fallback.");
  }
  return true;
}

bool PipeWireBackend::enable() {
  std::lock_guard<std::mutex> lock(mutex);

  if (enabled) {
    Logger::info("PulseForge enhancement is already enabled.");
    return true;
  }

  const std::string targetSink = resolveTargetSinkName();
  if (targetSink.empty()) {
    Logger::error("No target sink available for PulseForge loopback.");
    return false;
  }
  selectedSinkName = targetSink;

  if (!createVirtualSink()) {
    return false;
  }

  if (!rememberDefaultSink()) {
    removeVirtualSink();
    return false;
  }

  if (!setDefaultSink(virtualSinkName)) {
    restorePreviousDefaultSink();
    removeVirtualSink();
    return false;
  }

  bool usingProcessor = audioProcessor.start(monitorSourceName, selectedSinkName);
  if (!usingProcessor) {
    Logger::warn("AudioProcessor failed to start. Falling back to module-loopback.");
  }

  if (!usingProcessor && !createLoopback()) {
    restorePreviousDefaultSink();
    removeVirtualSink();
    clearRuntimeState();
    usingAudioProcessor = false;
    return false;
  }

  enabled = true;
  usingAudioProcessor = usingProcessor;
  saveRuntimeState();
  Logger::info("PulseForge routing enabled: apps -> " + virtualSinkName +
               (usingProcessor ? " -> AudioProcessor -> "
                               : " -> loopback -> ") +
               selectedSinkName);
  return true;
}

bool PipeWireBackend::disable() {
  std::lock_guard<std::mutex> lock(mutex);

  audioProcessor.stop();
  const bool loopbackRemoved = removeLoopback();
  const bool defaultRestored = restorePreviousDefaultSink();
  const bool sinkRemoved = removeVirtualSink();
  clearRuntimeState();
  enabled = false;
  usingAudioProcessor = false;

  return loopbackRemoved && defaultRestored && sinkRemoved;
}

bool PipeWireBackend::setTargetDevice(const std::string &deviceId) {
  std::lock_guard<std::mutex> lock(mutex);

  for (const auto &device : outputDevices) {
    if (device.id != deviceId) {
      continue;
    }

    selectedSinkName = device.sinkName;
    Logger::info("Selected target device: " + device.name + " [" +
                 device.sinkName + "]");

    if (enabled) {
      audioProcessor.stop();
      if (!removeLoopback()) {
        return false;
      }
      const bool processorStarted =
          audioProcessor.start(monitorSourceName, selectedSinkName);
      if (!processorStarted) {
        Logger::warn(
            "AudioProcessor failed to restart after device change. Falling back to module-loopback.");
      }
      const bool routed = processorStarted || createLoopback();
      if (routed) {
        usingAudioProcessor = processorStarted;
        saveRuntimeState();
      } else {
        restorePreviousDefaultSink();
        removeVirtualSink();
        clearRuntimeState();
        enabled = false;
        usingAudioProcessor = false;
      }
      return routed;
    }

    return true;
  }

  Logger::warn("Could not select device. Unknown id: " + deviceId);
  return false;
}

bool PipeWireBackend::isEnabled() const {
  std::lock_guard<std::mutex> lock(mutex);
  return enabled;
}

void PipeWireBackend::onCoreDone(void *data, uint32_t, int seq) {
  auto *self = static_cast<PipeWireBackend *>(data);
  if (seq == self->pendingSeq) {
    self->syncDone = true;
  }
}

void PipeWireBackend::onRegistryGlobal(
    void *data,
    uint32_t id,
    uint32_t,
    const char *type,
    uint32_t,
    const struct spa_dict *props) {
  auto *self = static_cast<PipeWireBackend *>(data);
  if (!props || !type || std::string(type) != PW_TYPE_INTERFACE_Node) {
    return;
  }

  const char *mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
  if (!mediaClass || std::string(mediaClass) != "Audio/Sink") {
    return;
  }

  const char *nodeName = spa_dict_lookup(props, PW_KEY_NODE_NAME);
  const char *nodeDescription = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
  const char *nick = spa_dict_lookup(props, PW_KEY_NODE_NICK);
  const char *factoryName = spa_dict_lookup(props, PW_KEY_FACTORY_NAME);

  const std::string name = nodeName ? nodeName : "";
  if (nodeName && std::string(nodeName) == self->virtualSinkName) {
    return;
  }
  if (nodeDescription &&
      std::string(nodeDescription)
              .find(std::string(AppConfig::applicationName)) !=
          std::string::npos) {
    return;
  }
  if (name.find("monitor") != std::string::npos ||
      name.find("Monitor") != std::string::npos ||
      name.find("null") != std::string::npos ||
      name.find("Null") != std::string::npos) {
    return;
  }

  AudioDevice device;
  device.id = std::to_string(id);
  device.sinkName = nodeName ? nodeName : "";
  if (nodeDescription) {
    device.name = nodeDescription;
  } else if (nick) {
    device.name = nick;
  } else if (nodeName) {
    device.name = nodeName;
  } else {
    device.name = "Unknown Output";
  }
  device.isDefault = false;

  self->outputDevices.push_back(device);
  Logger::info("Found output device: " + device.id + " - " + device.name +
               " | node: " + stringOrUnknown(nodeName) +
               " | factory: " + stringOrUnknown(factoryName));
}

bool PipeWireBackend::createVirtualSink() {
  if (virtualSinkModuleId != -1) {
    return true;
  }

  if (!routing.createVirtualSink(virtualSinkName, virtualSinkDisplayName,
                                 virtualSinkModuleId)) {
    return false;
  }

  Logger::info("Created PulseForge virtual sink module: " +
               std::to_string(virtualSinkModuleId));
  return true;
}

bool PipeWireBackend::removeVirtualSink() {
  if (virtualSinkModuleId == -1) {
    return true;
  }

  if (!routing.unloadModule(virtualSinkModuleId,
                            "Failed to remove PulseForge virtual sink.")) {
    return false;
  }

  Logger::info("Removed PulseForge virtual sink module: " +
               std::to_string(virtualSinkModuleId));
  virtualSinkModuleId = -1;
  return true;
}

bool PipeWireBackend::createLoopback() {
  if (loopbackModuleId != -1) {
    return true;
  }

  selectedSinkName = resolveTargetSinkName();
  if (selectedSinkName.empty()) {
    Logger::error("No target sink available for PulseForge loopback.");
    return false;
  }

  // MVP routing uses PipeWire's loopback. AudioProcessor will replace this
  // bridge when PulseForge owns DSP processing directly.
  if (!routing.createLoopback(monitorSourceName, selectedSinkName,
                              loopbackModuleId)) {
    return false;
  }

  Logger::info("Created PulseForge loopback module: " +
               std::to_string(loopbackModuleId) + " [" + monitorSourceName +
               " -> " + selectedSinkName + "]");
  return true;
}

bool PipeWireBackend::removeLoopback() {
  if (loopbackModuleId == -1) {
    return true;
  }

  if (!routing.unloadModule(loopbackModuleId,
                            "Failed to remove PulseForge loopback.")) {
    return false;
  }

  Logger::info("Removed PulseForge loopback module: " +
               std::to_string(loopbackModuleId));
  loopbackModuleId = -1;
  return true;
}

bool PipeWireBackend::rememberDefaultSink() {
  std::string defaultSink;
  if (!routing.getDefaultSink(defaultSink)) {
    return false;
  }

  if (defaultSink != virtualSinkName) {
    previousDefaultSinkName = defaultSink;
  }

  Logger::info("Previous default sink: " + previousDefaultSinkName);
  return true;
}

bool PipeWireBackend::setDefaultSink(const std::string &sinkName) {
  if (!routing.setDefaultSink(sinkName)) {
    return false;
  }
  return true;
}

bool PipeWireBackend::restorePreviousDefaultSink() {
  if (previousDefaultSinkName.empty()) {
    return true;
  }

  const bool restored = setDefaultSink(previousDefaultSinkName);
  if (restored) {
    Logger::info("Restored previous default sink: " + previousDefaultSinkName);
    previousDefaultSinkName.clear();
  }
  return restored;
}

bool PipeWireBackend::cleanupStaleModules() {
  RuntimeState state;
  if (!runtimeStateStore.load(state)) {
    return true;
  }

  std::string modules;
  if (!routing.listModules(modules)) {
    return false;
  }

  bool ok = true;
  if (state.loopbackModuleId != -1 &&
      outputContainsModule(modules, state.loopbackModuleId,
                           std::string(AudioConfig::loopbackModule),
                           monitorSourceName)) {
    ok = routing.unloadModule(state.loopbackModuleId,
                              "Failed to remove stale PulseForge loopback.") &&
         ok;
  }

  if (state.virtualSinkModuleId != -1 &&
      outputContainsModule(modules, state.virtualSinkModuleId,
                           std::string(AudioConfig::nullSinkModule),
                           virtualSinkName)) {
    ok = routing.unloadModule(
             state.virtualSinkModuleId,
             "Failed to remove stale PulseForge virtual sink.") &&
         ok;
  }

  clearRuntimeState();
  return ok;
}

bool PipeWireBackend::saveRuntimeState() const {
  return runtimeStateStore.save(
      {virtualSinkModuleId, loopbackModuleId, previousDefaultSinkName,
       virtualSinkName});
}

bool PipeWireBackend::clearRuntimeState() const {
  return runtimeStateStore.clear();
}

bool PipeWireBackend::restoreDefaultSinkFromRuntimeState() {
  RuntimeState state;
  if (!runtimeStateStore.load(state)) {
    return true;
  }

  if (state.previousDefaultSinkName.empty()) {
    return true;
  }

  std::string currentDefault;
  if (routing.getDefaultSink(currentDefault) &&
      currentDefault == virtualSinkName) {
    if (!setDefaultSink(state.previousDefaultSinkName)) {
      return false;
    }
    Logger::info("Restored default sink from previous PulseForge session: " +
                 state.previousDefaultSinkName);
  }

  return true;
}

std::string PipeWireBackend::resolveTargetSinkName() const {
  if (!selectedSinkName.empty() && selectedSinkName != virtualSinkName) {
    return selectedSinkName;
  }

  if (!previousDefaultSinkName.empty() &&
      previousDefaultSinkName != virtualSinkName) {
    return previousDefaultSinkName;
  }

  std::string defaultSink;
  if (!routing.getDefaultSink(defaultSink)) {
    return {};
  }
  if (defaultSink != virtualSinkName) {
    return defaultSink;
  }

  for (const auto &device : outputDevices) {
    if (!device.sinkName.empty() && device.sinkName != virtualSinkName) {
      return device.sinkName;
    }
  }

  return {};
}
