#include "PipeWireBackend.hpp"

#include "../core/Logger.hpp"

#include <pipewire/impl-module.h>
#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <spa/utils/string.h>
#include <array>
#include <cerrno>
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

std::string shellQuote(const std::string &value) {
  std::string quoted = "'";
  for (char character : value) {
    if (character == '\'') {
      quoted += "'\\''";
    } else {
      quoted += character;
    }
  }
  quoted += "'";
  return quoted;
}

} // namespace

PipeWireBackend::PipeWireBackend() {
  pw_init(nullptr, nullptr);
}

PipeWireBackend::~PipeWireBackend() {
  restorePreviousDefaultSink();
  removeFilterSink();
  clearRuntimeState();
  stopLoopThread();

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

  startLoopThread();

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
  const ProcessResult currentDefaultSink =
      runProcess({"pactl", "get-default-sink"});
  if (currentDefaultSink.exitCode != 0 || currentDefaultSink.output.empty()) {
    Logger::error(
        "Failed to get current default sink. Output: " +
        trim(currentDefaultSink.output));
    return false;
  }

  const std::string defaultSinkName = trim(currentDefaultSink.output);
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
  if (nativeFilterModule && !removeFilterSink()) {
    return false;
  }

  selectedSinkName = resolveTargetSinkName();
  if (selectedSinkName.empty()) {
    Logger::error("No target sink available for filter-chain playback.");
    return false;
  }

  if (!targetSinkIsVisibleToPactl()) {
    return false;
  }

  if (!createFilterSinkWithPactl() && !createFilterSinkWithNativePipeWireModule()) {
    if (wasEnabled) {
      restorePreviousDefaultSink();
    }
    return false;
  }

  Logger::info("Created PipeWire filter-chain sink routed to: " +
               selectedSinkName);

  if (!connectFilterOutputToTargetSink()) {
    Logger::error(
        "PulseForge could not connect the filter-chain output to the selected device. Leaving enhancement disabled to avoid silence.");
    removeFilterSink();
    if (wasEnabled) {
      restorePreviousDefaultSink();
    }
    return false;
  }

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

bool PipeWireBackend::createFilterSinkWithPactl() {
  const std::vector<std::string> filterChainArgs = buildFilterChainModuleArgs();
  Logger::info("Loading PipeWire filter-chain through pactl with args: " +
               buildFilterChainModuleArgsForLog());

  std::vector<std::string> pactlArgs = {"pactl", "load-module",
                                        "module-filter-chain"};
  pactlArgs.insert(pactlArgs.end(), filterChainArgs.begin(),
                   filterChainArgs.end());

  const ProcessResult loadResult = runProcess(pactlArgs);
  if (loadResult.exitCode != 0) {
    const std::string output = trim(loadResult.output);
    Logger::error(
        "Failed to create PipeWire filter-chain sink. Exit code: " +
        std::to_string(loadResult.exitCode) + ". Output: " + output);
    if (loadResult.exitCode == 127) {
      Logger::error(
          "PulseForge could not find pactl. Install your distribution's PulseAudio/PipeWire Pulse control package, such as pulseaudio-utils or pipewire-pulse.");
    }
    if (loadResult.exitCode == 1 &&
        output.find("No such entity") != std::string::npos) {
      Logger::error(
          "pactl reported 'No such entity'. If the target sink above is visible, this usually means the PulseAudio/PipeWire Pulse server does not expose module-filter-chain. PulseForge needs that module for the pactl-based lifecycle.");
      Logger::warn(
          "Falling back to an in-process native PipeWire filter-chain module for this session.");
    }
    return false;
  }

  const std::string moduleIdOutput = trim(loadResult.output);
  if (moduleIdOutput.empty()) {
    Logger::error(
        "pactl reported success but did not print a module id; refusing to enable because PulseForge could not clean it up reliably.");
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
  if (!waitForProcessingSink()) {
    Logger::error(
        "pactl loaded module-filter-chain, but the PulseForge sink did not appear.");
    removeFilterSink();
    return false;
  }

  return true;
}

bool PipeWireBackend::createFilterSinkWithNativePipeWireModule() {
  if (!context) {
    Logger::error("Cannot load native PipeWire filter-chain module without a PipeWire context.");
    return false;
  }

  const std::string moduleArgs = buildNativeFilterChainModuleArgs();
  Logger::info("Loading native PipeWire filter-chain module with args: " +
               moduleArgs);

  stopLoopThread();
  nativeFilterModule = pw_context_load_module(
      context, "libpipewire-module-filter-chain", moduleArgs.c_str(), nullptr);
  if (!nativeFilterModule) {
    startLoopThread();
    Logger::error(
        "Failed to load native libpipewire-module-filter-chain. Make sure the PipeWire filter-chain module is installed.");
    return false;
  }
  startLoopThread();

  Logger::info("Loaded native PipeWire filter-chain module in PulseForge.");
  if (!waitForProcessingSink()) {
    Logger::error(
        "Native PipeWire filter-chain module loaded, but the PulseForge sink did not appear.");
    removeNativeFilterModule();
    return false;
  }

  return saveRuntimeState();
}

bool PipeWireBackend::removeFilterSink() {
  if (filterModuleId == -1 && !nativeFilterModule) {
    return true;
  }

  if (filterModuleId != -1) {
    if (!runProcessChecked({"pactl", "unload-module", std::to_string(filterModuleId)},
                           "Failed to unload filter-chain module.")) {
      return false;
    }

    Logger::info("Removed filter-chain module with ID: " +
                 std::to_string(filterModuleId));
    filterModuleId = -1;
  }

  if (!removeNativeFilterModule()) {
    return false;
  }

  enabled = false;
  return true;
}

bool PipeWireBackend::removeNativeFilterModule() {
  if (!nativeFilterModule) {
    return true;
  }

  stopLoopThread();
  pw_impl_module_destroy(nativeFilterModule);
  nativeFilterModule = nullptr;
  startLoopThread();
  Logger::info("Destroyed native PipeWire filter-chain module.");
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

  bool cleaned = true;

  if (staleModuleId == -1) {
    return clearRuntimeState() && cleaned;
  }

  const ProcessResult info =
      runProcess({"pactl", "list", "modules", "short"});
  if (info.exitCode != 0) {
    return cleaned;
  }

  const std::string moduleLinePrefix = std::to_string(staleModuleId) + "\t";
  const auto staleModulePos = info.output.find(moduleLinePrefix);
  if (staleModulePos == std::string::npos ||
      info.output.find("module-filter-chain", staleModulePos) ==
          std::string::npos ||
      info.output.find(virtualSinkName, staleModulePos) == std::string::npos) {
    Logger::warn("Stored module id no longer looks like PulseForge. Leaving it alone.");
    return false;
  }

  if (!runProcessChecked({"pactl", "unload-module", std::to_string(staleModuleId)},
                         "Failed to remove stale PulseForge filter-chain module.")) {
    cleaned = false;
  }

  Logger::info("Removed stale PulseForge filter-chain module: " +
               std::to_string(staleModuleId));
  clearRuntimeState();
  return cleaned;
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

  const ProcessResult currentDefault = runProcess({"pactl", "get-default-sink"});
  if (trim(currentDefault.output) == virtualSinkName) {
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

bool PipeWireBackend::targetSinkIsVisibleToPactl() const {
  const ProcessResult sinks = runProcess({"pactl", "list", "sinks", "short"});
  if (sinks.exitCode != 0) {
    Logger::warn("Could not preflight target sink visibility with pactl. Output: " +
                 trim(sinks.output));
    return true;
  }

  std::istringstream lines(sinks.output);
  std::string line;
  while (std::getline(lines, line)) {
    std::istringstream fields(line);
    std::string index;
    std::string sinkName;
    fields >> index >> sinkName;
    if (sinkName == selectedSinkName) {
      return true;
    }
  }

  Logger::error("Selected target sink is not visible to pactl: " +
                selectedSinkName);
  Logger::error(
      "PulseForge cannot safely set target.object until the selected output device appears in `pactl list sinks short`.");
  return false;
}

bool PipeWireBackend::connectFilterOutputToTargetSink() const {
  const std::array<std::pair<std::string, std::string>, 2> stereoLinks = {
      std::make_pair(filterOutputNodeName + ":output_FL",
                     selectedSinkName + ":playback_FL"),
      std::make_pair(filterOutputNodeName + ":output_FR",
                     selectedSinkName + ":playback_FR")};

  for (const auto &[source, destination] : stereoLinks) {
    const ProcessResult link = runProcess({"pw-link", "-w", source, destination});
    const std::string output = trim(link.output);

    if (link.exitCode == 0) {
      Logger::info("Linked " + source + " -> " + destination);
      continue;
    }

    if (output.find("File exists") != std::string::npos ||
        output.find("already linked") != std::string::npos) {
      Logger::info("PipeWire link already exists: " + source + " -> " +
                   destination);
      continue;
    }

    if (link.exitCode == 127) {
      Logger::warn(
          "pw-link is not available; relying on the session manager to connect PulseForge to the selected sink.");
      return true;
    }

    Logger::error("Failed to link " + source + " -> " + destination +
                  ". Output: " + output);
    return false;
  }

  return true;
}

bool PipeWireBackend::waitForProcessingSink() const {
  for (int attempt = 0; attempt < 20; ++attempt) {
    const ProcessResult sinks = runProcess({"pactl", "list", "sinks", "short"});
    if (sinks.exitCode == 0 && sinks.output.find(virtualSinkName) != std::string::npos) {
      return true;
    }
    usleep(100000);
  }

  return false;
}

std::vector<std::string> PipeWireBackend::buildFilterChainModuleArgs() const {
  return {"node.description=" + virtualSinkDisplayName,
          "media.name=" + virtualSinkDisplayName,
          "filter.graph=" + buildFilterGraphArgs(),
          "capture.props=" + buildCapturePropsArgs(),
          "playback.props=" + buildPlaybackPropsArgs()};
}

std::string PipeWireBackend::buildFilterChainModuleArgsForLog() const {
  std::ostringstream args;
  const auto moduleArgs = buildFilterChainModuleArgs();
  for (std::size_t index = 0; index < moduleArgs.size(); ++index) {
    if (index > 0) {
      args << " ";
    }

    const std::string &argument = moduleArgs[index];
    const std::size_t separator = argument.find('=');
    if (separator == std::string::npos) {
      args << shellQuote(argument);
      continue;
    }

    args << argument.substr(0, separator + 1)
         << shellQuote(argument.substr(separator + 1));
  }

  return args.str();
}

std::string PipeWireBackend::buildNativeFilterChainModuleArgs() const {
  std::ostringstream args;
  args << "{ "
       << "node.description = \"" << virtualSinkDisplayName << "\" "
       << "media.name = \"" << virtualSinkDisplayName << "\" "
       << "filter.graph = " << buildFilterGraphArgs() << " "
       << "capture.props = " << buildCapturePropsArgs() << " "
       << "playback.props = " << buildPlaybackPropsArgs() << " "
       << "}";

  return args.str();
}

std::string PipeWireBackend::buildFilterGraphArgs() const {
  const std::string eqFilters = buildParamEqFilters();

  std::ostringstream args;
  args << "{ "
       << "nodes = [ { type = builtin name = eq label = param_eq config = { filters1 = [ "
       << eqFilters << " ] filters2 = [ "
       << eqFilters << " ] } } ] "
       << "inputs = [ \"eq:In 1\" \"eq:In 2\" ] "
       << "outputs = [ \"eq:Out 1\" \"eq:Out 2\" ] "
       << "}";

  return args.str();
}

std::string PipeWireBackend::buildCapturePropsArgs() const {
  std::ostringstream args;
  args << "{ "
       << "node.name = \"" << virtualSinkName << "\" "
       << "node.description = \"" << virtualSinkDisplayName << "\" "
       << "media.class = Audio/Sink "
       << "audio.channels = 2 "
       << "audio.position = [ FL FR ] "
       << "}";

  return args.str();
}

std::string PipeWireBackend::buildPlaybackPropsArgs() const {
  std::ostringstream args;
  args << "{ "
       << "node.name = \"" << filterOutputNodeName << "\" "
       << "node.passive = true "
       << "target.object = \"" << selectedSinkName << "\" "
       << "node.target = \"" << selectedSinkName << "\" "
       << "audio.channels = 2 "
       << "audio.position = [ FL FR ] "
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

  const ProcessResult currentDefault = runProcess({"pactl", "get-default-sink"});
  return trim(currentDefault.output);
}

void PipeWireBackend::startLoopThread() {
  if (loopThreadRunning || !loop) {
    return;
  }

  loopThreadRunning = true;
  loopThread = std::thread([this]() {
    while (loopThreadRunning) {
      pw_loop_iterate(pw_main_loop_get_loop(loop), 100);
    }
  });
}

void PipeWireBackend::stopLoopThread() {
  if (!loopThreadRunning) {
    return;
  }

  loopThreadRunning = false;
  if (loopThread.joinable()) {
    loopThread.join();
  }
}
