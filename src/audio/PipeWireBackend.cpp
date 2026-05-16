#include "PipeWireBackend.hpp"

#include "../core/Logger.hpp"

#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <spa/utils/string.h>
#include <array>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {

std::string trim(std::string value) {
  while (!value.empty() &&
         (value.back() == '\n' || value.back() == '\r' ||
          value.back() == ' ')) {
    value.pop_back();
  }

  return value;
}

std::string runCommand(const std::string &command) {
  std::array<char, 256> buffer{};
  std::string result;

  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return "";
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
    result += buffer.data();
  }

  pclose(pipe);
  return result;
}

std::string resolveExecutable(const std::string &name) {
  if (name.find('/') != std::string::npos) {
    return name;
  }

  const char *pathValue = std::getenv("PATH");
  const std::string path = pathValue ? pathValue : "";
  std::stringstream pathStream(path);
  std::string directory;
  while (std::getline(pathStream, directory, ':')) {
    const std::filesystem::path candidate =
        std::filesystem::path(directory) / name;
    if (access(candidate.c_str(), X_OK) == 0) {
      return candidate.string();
    }
  }

  const std::array<std::filesystem::path, 5> fallbackDirectories = {
      "/usr/bin", "/bin", "/usr/local/bin", "/usr/sbin",
      "/run/current-system/sw/bin"};
  for (const auto &directory : fallbackDirectories) {
    const std::filesystem::path candidate = directory / name;
    if (access(candidate.c_str(), X_OK) == 0) {
      return candidate.string();
    }
  }

  return name;
}

struct ProcessResult {
  int exitCode = -1;
  std::string output;
};

ProcessResult runProcess(const std::vector<std::string> &arguments) {
  ProcessResult result;
  if (arguments.empty()) {
    Logger::error("Cannot run an empty process command.");
    return result;
  }

  std::vector<std::string> resolvedArguments = arguments;
  resolvedArguments.front() = resolveExecutable(resolvedArguments.front());

  int pipeFds[2];

  if (pipe(pipeFds) != 0) {
    Logger::error("Failed to create process pipe: " + std::string(std::strerror(errno)));
    return result;
  }

  const pid_t pid = fork();
  if (pid == -1) {
    close(pipeFds[0]);
    close(pipeFds[1]);
    Logger::error("Failed to fork process: " + std::string(std::strerror(errno)));
    return result;
  }

  if (pid == 0) {
    dup2(pipeFds[1], STDOUT_FILENO);
    dup2(pipeFds[1], STDERR_FILENO);
    close(pipeFds[0]);
    close(pipeFds[1]);

    std::vector<char *> argv;
    argv.reserve(resolvedArguments.size() + 1);
    for (const auto &argument : resolvedArguments) {
      argv.push_back(const_cast<char *>(argument.c_str()));
    }
    argv.push_back(nullptr);

    execvp(argv.front(), argv.data());
    const std::string error =
        "execvp failed for " + resolvedArguments.front() + ": " +
        std::string(std::strerror(errno)) + "\n";
    write(STDERR_FILENO, error.c_str(), error.size());
    _exit(127);
  }

  close(pipeFds[1]);

  std::array<char, 512> buffer{};
  ssize_t bytesRead = 0;
  while ((bytesRead = read(pipeFds[0], buffer.data(), buffer.size())) > 0) {
    result.output.append(buffer.data(), static_cast<std::size_t>(bytesRead));
  }
  close(pipeFds[0]);

  int status = 0;
  waitpid(pid, &status, 0);

  if (WIFEXITED(status)) {
    result.exitCode = WEXITSTATUS(status);
  }

  return result;
}

bool runProcessChecked(const std::vector<std::string> &arguments,
                       const std::string &error) {
  const ProcessResult result = runProcess(arguments);
  if (result.exitCode == 0) {
    return true;
  }

  Logger::error(error + " Command exited with code: " +
                std::to_string(result.exitCode) + ". Output: " +
                trim(result.output));
  return false;
}

const char *stringOrUnknown(const char *value) {
  return value ? value : "unknown";
}

std::string getenvOrEmpty(const char *name) {
  const char *value = std::getenv(name);
  return value ? value : "";
}

} // namespace

PipeWireBackend::PipeWireBackend() {
  pw_init(nullptr, nullptr);
}

PipeWireBackend::~PipeWireBackend() {
  restorePreviousDefaultSink();
  removeFilterSink();
  clearRuntimeState();

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
  cleanupStaleFilterSink();
  restoreDefaultSinkFromRuntimeState();

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
  return outputDevices;
}

bool PipeWireBackend::setTargetDevice(const std::string &deviceId) {
  for (const auto &device : outputDevices) {
    if (device.id == deviceId) {
      selectedSinkName = device.sinkName;
      Logger::info("Selected target device: " + device.name + " [" +
                   device.sinkName + "]");
      if (filterModuleId != -1) {
        return createOrReloadFilterSink();
      }
      return true;
    }
  }

  Logger::warn("Could not select device. Unknown id: " + deviceId);
  return false;
}

bool PipeWireBackend::isEnabled() const {
  return enabled;
}

bool PipeWireBackend::routeDefaultSinkToProcessingSink() {
  const std::string currentDefaultSink = runCommand("pactl get-default-sink");
  if (currentDefaultSink.empty()) {
    Logger::error(
        "Failed to get current default sink. Command output was empty.");
    return false;
  }

  const std::string defaultSinkName = trim(currentDefaultSink);
  if (defaultSinkName != virtualSinkName) {
    previousDefaultSinkName = defaultSinkName;
  }

  Logger::info("Previous default sink: " + previousDefaultSinkName);

  if (!runProcessChecked({"pactl", "set-default-sink", virtualSinkName},
                         "Failed to route default sink to PulseForge.")) {
    return false;
  }

  Logger::info("Set default sink to virtual: " + virtualSinkDisplayName);
  defaultSinkRouted = true;
  return saveRuntimeState();
}

bool PipeWireBackend::restorePreviousDefaultSink() {
  if (previousDefaultSinkName.empty()) {
    return true;
  }

  if (!runProcessChecked({"pactl", "set-default-sink", previousDefaultSinkName},
                         "Failed to restore previous default sink.")) {
    return false;
  }

  Logger::info("Restored previous default sink with name: " +
               previousDefaultSinkName);
  previousDefaultSinkName.clear();
  defaultSinkRouted = false;
  return true;
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

  if (!props || !type) {
    return;
  }

  if (std::string(type) != PW_TYPE_INTERFACE_Node) {
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
      std::string(nodeDescription).find("PulseForge") != std::string::npos) {
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

  Logger::info(
      "Found output device: " +
      device.id +
      " - " +
      device.name +
      " | node: " +
      stringOrUnknown(nodeName) +
      " | factory: " +
      stringOrUnknown(factoryName));
}

bool PipeWireBackend::createProcessingSink(const std::string &displayName) {
  virtualSinkDisplayName = displayName;
  return createOrReloadFilterSink();
}

bool PipeWireBackend::removeProcessingSink() {
  return removeFilterSink();
}

bool PipeWireBackend::applyEffectChain(const EffectChain &chain) {
  currentEffectChain = chain;
  Logger::info("Stored effect chain with " +
               std::to_string(chain.effects.size()) + " effect(s)");

  if (filterModuleId == -1) {
    return true;
  }

  return createOrReloadFilterSink();
}

bool PipeWireBackend::enable() {
  if (!createProcessingSink(virtualSinkDisplayName)) {
    return false;
  }

  if (!routeDefaultSinkToProcessingSink()) {
    return false;
  }

  enabled = true;
  return true;
}

bool PipeWireBackend::disable() {
  restorePreviousDefaultSink();
  const bool removed = removeProcessingSink();
  clearRuntimeState();
  return removed;
}

bool PipeWireBackend::createOrReloadFilterSink() {
  const bool wasEnabled = enabled;

  if (filterModuleId != -1 && !removeFilterSink()) {
    return false;
  }

  selectedSinkName = resolveTargetSinkName();
  if (selectedSinkName.empty()) {
    Logger::error("No target sink available for filter-chain playback.");
    return false;
  }

  const std::string filterChainArgs = buildFilterChainArgs();
  Logger::info("Loading PipeWire filter-chain args: " + filterChainArgs);

  const ProcessResult loadResult = runProcess(
      {"pw-cli", "load-module", "libpipewire-module-filter-chain",
       filterChainArgs});
  if (loadResult.exitCode != 0) {
    Logger::error(
        "Failed to create PipeWire filter-chain sink. Exit code: " +
        std::to_string(loadResult.exitCode) + ". Output: " +
        trim(loadResult.output));
    if (loadResult.exitCode == 127) {
      Logger::error(
          "PulseForge could not find pw-cli. Install your distribution's PipeWire tools package, such as pipewire-bin, pipewire-tools, or pipewire-utils.");
    }
    return false;
  }

  const std::string moduleIdOutput = trim(loadResult.output);
  if (moduleIdOutput.empty()) {
    Logger::error(
        "pw-cli reported success but did not print a module id; refusing to enable because PulseForge could not clean it up reliably.");
    return false;
  }

  try {
    filterModuleId = std::stoi(moduleIdOutput);
  } catch (...) {
    Logger::error("Could not parse filter-chain module id from output: " +
                  moduleIdOutput);
    return false;
  }

  Logger::info("Created PipeWire filter-chain sink routed to: " +
               selectedSinkName);
  Logger::info("Filter-chain module ID: " + std::to_string(filterModuleId));

  if (!verifyFilterSinkRouting()) {
    Logger::warn("Could not verify target.object routing. Session manager may still connect it asynchronously.");
  }

  if (wasEnabled) {
    if (!runProcessChecked({"pactl", "set-default-sink", virtualSinkName},
                           "Failed to keep PulseForge as default after reload.")) {
      return false;
    }
    enabled = true;
    saveRuntimeState();
  }

  return true;
}

bool PipeWireBackend::removeFilterSink() {
  if (filterModuleId == -1) {
    return true;
  }

  if (!runProcessChecked({"pw-cli", "destroy", std::to_string(filterModuleId)},
                         "Failed to remove filter-chain module.")) {
    return false;
  }

  Logger::info("Removed filter-chain module with ID: " +
               std::to_string(filterModuleId));
  filterModuleId = -1;
  enabled = false;
  return true;
}

bool PipeWireBackend::cleanupStaleFilterSink() {
  std::ifstream state(runtimeStatePath());
  if (!state.is_open()) {
    return true;
  }

  int staleModuleId = -1;
  std::string line;
  while (std::getline(state, line)) {
    if (line.rfind("module=", 0) == 0) {
      try {
        staleModuleId = std::stoi(line.substr(7));
      } catch (...) {
        staleModuleId = -1;
      }
    }
  }

  if (staleModuleId == -1) {
    return clearRuntimeState();
  }

  const ProcessResult info =
      runProcess({"pw-cli", "info", std::to_string(staleModuleId)});
  if (info.exitCode != 0) {
    return true;
  }

  if (info.output.find("libpipewire-module-filter-chain") ==
          std::string::npos &&
      info.output.find("PulseForge") == std::string::npos) {
    Logger::warn("Stored module id no longer looks like PulseForge. Leaving it alone.");
    return false;
  }

  if (!runProcessChecked({"pw-cli", "destroy", std::to_string(staleModuleId)},
                         "Failed to remove stale PulseForge filter-chain module.")) {
    return false;
  }

  Logger::info("Removed stale PulseForge filter-chain module: " +
               std::to_string(staleModuleId));
  return true;
}

bool PipeWireBackend::saveRuntimeState() const {
  std::filesystem::path path(runtimeStatePath());
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) {
    Logger::error("Failed to create PulseForge runtime state directory: " +
                  error.message());
    return false;
  }

  std::ofstream state(path);
  if (!state.is_open()) {
    Logger::error("Failed to write PulseForge runtime state.");
    return false;
  }

  state << "module=" << filterModuleId << '\n';
  state << "previous_default=" << previousDefaultSinkName << '\n';
  state << "processing_sink=" << virtualSinkName << '\n';
  return true;
}

bool PipeWireBackend::clearRuntimeState() const {
  std::error_code error;
  std::filesystem::remove(runtimeStatePath(), error);
  if (error) {
    Logger::warn("Failed to remove PulseForge runtime state: " + error.message());
    return false;
  }
  return true;
}

bool PipeWireBackend::restoreDefaultSinkFromRuntimeState() {
  std::ifstream state(runtimeStatePath());
  if (!state.is_open()) {
    return true;
  }

  std::string previousDefault;
  std::string line;
  while (std::getline(state, line)) {
    if (line.rfind("previous_default=", 0) == 0) {
      previousDefault = line.substr(17);
    }
  }

  if (previousDefault.empty()) {
    return clearRuntimeState();
  }

  const std::string currentDefault = trim(runCommand("pactl get-default-sink"));
  if (currentDefault == virtualSinkName) {
    if (!runProcessChecked({"pactl", "set-default-sink", previousDefault},
                           "Failed to restore default sink from crash state.")) {
      return false;
    }
    Logger::info("Restored default sink from previous PulseForge session: " +
                 previousDefault);
  }

  return clearRuntimeState();
}

bool PipeWireBackend::verifyFilterSinkRouting() const {
  if (selectedSinkName.empty()) {
    return false;
  }

  const ProcessResult dump = runProcess({"pw-dump"});
  if (dump.exitCode != 0 || dump.output.empty()) {
    return false;
  }

  const auto outputNodePos = dump.output.find(filterOutputNodeName);
  if (outputNodePos == std::string::npos) {
    return false;
  }

  const auto targetPos = dump.output.find(selectedSinkName, outputNodePos);
  return targetPos != std::string::npos;
}

std::string PipeWireBackend::buildFilterChainArgs() const {
  std::ostringstream args;
  args << "{ "
       << "node.description = \"" << virtualSinkDisplayName << "\" "
       << "media.name = \"" << virtualSinkDisplayName << "\" "
       << "filter.graph = { "
       << "nodes = [ { type = builtin name = eq label = param_eq config = { filters = [ "
       << buildParamEqFilters()
       << " ] } } ] "
       << "inputs = [ \"eq:In 1\" \"eq:In 2\" ] "
       << "outputs = [ \"eq:Out 1\" \"eq:Out 2\" ] "
       << "} "
       << "capture.props = { "
       << "node.name = \"" << virtualSinkName << "\" "
       << "node.description = \"" << virtualSinkDisplayName << "\" "
       << "media.class = Audio/Sink "
       << "audio.channels = 2 "
       << "audio.position = [ FL FR ] "
       << "} "
       << "playback.props = { "
       << "node.name = \"" << filterOutputNodeName << "\" "
       << "node.passive = true "
       << "target.object = \"" << selectedSinkName << "\" "
       << "node.target = \"" << selectedSinkName << "\" "
       << "audio.channels = 2 "
       << "audio.position = [ FL FR ] "
       << "} "
       << "}";

  return args.str();
}

std::string PipeWireBackend::buildParamEqFilters() const {
  std::vector<std::string> filters;

  for (const auto &effect : currentEffectChain.effects) {
    if (effect.type == "eq_band") {
      const auto freq = effect.parameters.find("freq");
      const auto gain = effect.parameters.find("gain");
      const auto q = effect.parameters.find("q");

      if (freq == effect.parameters.end() || gain == effect.parameters.end()) {
        continue;
      }

      std::ostringstream filter;
      filter << "{ type = bq_peaking freq = " << freq->second
             << " gain = " << gain->second
             << " q = " << (q == effect.parameters.end() ? 1.0f : q->second)
             << " }";
      filters.push_back(filter.str());
    } else if (effect.type == "eq") {
      const auto bass = effect.parameters.find("bass");
      const auto treble = effect.parameters.find("treble");

      if (bass != effect.parameters.end()) {
        const float gain = (bass->second - 1.0f) * 8.0f;
        std::ostringstream filter;
        filter << "{ type = bq_lowshelf freq = 120 gain = " << gain
               << " q = 0.7 }";
        filters.push_back(filter.str());
      }

      if (treble != effect.parameters.end()) {
        const float gain = (treble->second - 1.0f) * 8.0f;
        std::ostringstream filter;
        filter << "{ type = bq_highshelf freq = 6000 gain = " << gain
               << " q = 0.7 }";
        filters.push_back(filter.str());
      }
    }
  }

  if (filters.empty()) {
    filters.push_back("{ type = bq_peaking freq = 1000 gain = 0 q = 1.0 }");
  }

  std::ostringstream result;
  for (const auto &filter : filters) {
    result << filter << " ";
  }

  return result.str();
}

std::string PipeWireBackend::runtimeStatePath() const {
  const std::string runtimeDir = getenvOrEmpty("XDG_RUNTIME_DIR");
  if (!runtimeDir.empty()) {
    return runtimeDir + "/pulseforge/runtime-state";
  }

  return "/tmp/pulseforge-runtime-state";
}

std::string PipeWireBackend::resolveTargetSinkName() const {
  if (!selectedSinkName.empty()) {
    return selectedSinkName;
  }

  if (!previousDefaultSinkName.empty()) {
    return previousDefaultSinkName;
  }

  return trim(runCommand("pactl get-default-sink"));
}
