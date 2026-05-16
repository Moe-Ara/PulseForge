#include "PipeWireBackend.hpp"

#include "../core/Logger.hpp"

#include <pipewire/keys.h>
#include <pipewire/pipewire.h>
#include <spa/utils/string.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <string>

static std::string trim(std::string value)
{
  while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' '))
  {
    value.pop_back();
  }

  return value;
}
static std::string runCommand(const std::string &command)
{
  std::array<char, 256> buffer{};
  std::string result;

  FILE *pipe = popen(command.c_str(), "r");
  if (!pipe)
  {
    return "";
  }

  while (fgets(buffer.data(), buffer.size(), pipe) != nullptr)
  {
    result += buffer.data();
  }

  pclose(pipe);
  return result;
}
PipeWireBackend::PipeWireBackend()
{
  pw_init(nullptr, nullptr);
}

PipeWireBackend::~PipeWireBackend()
{
  if (registry)
  {
    pw_proxy_destroy(reinterpret_cast<pw_proxy *>(registry));
  }

  if (core)
  {
    pw_core_disconnect(core);
  }

  if (context)
  {
    pw_context_destroy(context);
  }

  if (loop)
  {
    pw_main_loop_destroy(loop);
  }

  pw_deinit();
}

bool PipeWireBackend::initialize()
{
  loop = pw_main_loop_new(nullptr);
  if (!loop)
  {
    Logger::error("Failed to create PipeWire main loop");
    return false;
  }

  context = pw_context_new(pw_main_loop_get_loop(loop), nullptr, 0);
  if (!context)
  {
    Logger::error("Failed to create PipeWire context");
    return false;
  }

  core = pw_context_connect(context, nullptr, 0);
  if (!core)
  {
    Logger::error("Failed to connect to PipeWire");
    return false;
  }

  registry = pw_core_get_registry(core, PW_VERSION_REGISTRY, 0);
  if (!registry)
  {
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

  while (!syncDone)
  {
    pw_loop_iterate(pw_main_loop_get_loop(loop), -1);
  }

  Logger::info("PipeWire backend initialized");
  return true;
}

std::vector<AudioDevice> PipeWireBackend::listOutputDevices()
{
  return outputDevices;
}

bool PipeWireBackend::setTargetDevice(const std::string &deviceId)
{
  for (const auto &device : outputDevices)
  {
    if (device.id == deviceId)
    {
      selectedDeviceId = deviceId;
      Logger::info("Selected target device: " + device.name + " [" + device.id + "]");
      return true;
    }
  }

  Logger::warn("Could not select device. Unknown id: " + deviceId);
  return false;
}

bool PipeWireBackend::isEnabled() const
{
  return enabled;
}

bool PipeWireBackend::setDefaultSinkToVirtual()
{
  std::string command = "pactl get-default-sink";
  std::string currentDefaultSink = runCommand(command);
  if (currentDefaultSink.empty())
  {
    Logger::error("Failed to get current default sink. Command output was empty.");
    return false;
  }
  previousDefaultSinkName = trim(currentDefaultSink);

  Logger::info("Previous default sink: " + previousDefaultSinkName);
  command = "pactl set-default-sink " + virtualSinkName;
  int result = std::system(command.c_str());
  if (result != 0)
  {
    Logger::error("Failed to set default sink to virtual. Command exited with code: " + std::to_string(result));
    return false;
  }
  Logger::info("Set default sink to virtual: " + virtualSinkDisplayName);
  return true;
}

bool PipeWireBackend::restorePreviousDefaultSink()
{
  if (previousDefaultSinkName.empty())
  {
    Logger::warn("No previous default sink to restore.");
    return false;
  }
  std::string command = "pactl set-default-sink " + previousDefaultSinkName;
  int result = std::system(command.c_str());
  if (result != 0)
  {
    Logger::error("Failed to restore previous default sink. Command exited with code: " + std::to_string(result));
    return false;
  }
  Logger::info("Restored previous default sink with name: " + previousDefaultSinkName);
  previousDefaultSinkName.clear();
  return true;
}

void PipeWireBackend::onCoreDone(void *data, uint32_t, int seq)
{
  auto *self = static_cast<PipeWireBackend *>(data);

  if (seq == self->pendingSeq)
  {
    self->syncDone = true;
  }
}

void PipeWireBackend::onRegistryGlobal(
    void *data,
    uint32_t id,
    uint32_t,
    const char *type,
    uint32_t,
    const struct spa_dict *props)
{
  auto *self = static_cast<PipeWireBackend *>(data);

  if (!props || !type)
  {
    return;
  }

  if (std::string(type) != PW_TYPE_INTERFACE_Node)
  {
    return;
  }

  const char *mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
  if (!mediaClass || std::string(mediaClass) != "Audio/Sink")
  {
    return;
  }

  const char *nodeName = spa_dict_lookup(props, PW_KEY_NODE_NAME);
  const char *nodeDescription = spa_dict_lookup(props, PW_KEY_NODE_DESCRIPTION);
  const char *nick = spa_dict_lookup(props, PW_KEY_NODE_NICK);
  const char *factoryName = spa_dict_lookup(props, PW_KEY_FACTORY_NAME);

  const std::string name = nodeName ? nodeName : "";
  if (nodeName && std::string(nodeName) == self->virtualSinkName)
  {
    return;
  }
  if (nodeDescription && std::string(nodeDescription).find("PulseForge") != std::string::npos)
  {
    return;
  }
  // Skip monitor/virtual/internal nodes for now
  if (name.find("monitor") != std::string::npos ||
      name.find("Monitor") != std::string::npos ||
      name.find("null") != std::string::npos ||
      name.find("Null") != std::string::npos)
  {
    return;
  }

  AudioDevice device;
  device.id = std::to_string(id);

  if (nodeDescription)
  {
    device.name = nodeDescription;
  }
  else if (nick)
  {
    device.name = nick;
  }
  else if (nodeName)
  {
    device.name = nodeName;
  }
  else
  {
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
      (nodeName ? nodeName : "unknown") +
      " | factory: " +
      (factoryName ? factoryName : "unknown"));
}

bool PipeWireBackend::createVirtualSink(const std::string &name)
{
  if (virtualSinkModuleId != -1)
  {
    Logger::warn("Virtual sink already created with id: " + std::to_string(virtualSinkModuleId));
    return true;
  }
  const std::string command =
      "pactl load-module module-null-sink "
      "sink_name=" +
      virtualSinkName + " "
                        "sink_properties=device.description=\"" +
      name + "\"";

  const std::string result = runCommand(command);
  if (result.empty())
  {
    Logger::error("Failed to create virtual sink. Command output was empty.");
    return false;
  }
  try
  {
    virtualSinkModuleId = std::stoi(result);
  }
  catch (...)
  {
    Logger::error("Failed to parse module ID from command output: " + result);
    return false;
  } // silence unused variable warning
  enabled = true;
  Logger::info("Created virtual sink: " + name);
  Logger::info("Module ID: " + std::to_string(virtualSinkModuleId));
  return true;
}

bool PipeWireBackend::removeVirtualSink(const std::string &name)
{
  if (virtualSinkModuleId == -1)
  {
    Logger::warn("No virtual sink to remove.");
    return true;
  }
  const std::string command = "pactl unload-module " + std::to_string(virtualSinkModuleId);
  const int result = std::system(command.c_str());
  if (result != 0)
  {
    Logger::error("Failed to remove virtual sink. Command exited with code: " + std::to_string(result));
    return false;
  }
  Logger::info("Removed virtual sink with module ID: " + std::to_string(virtualSinkModuleId));
  virtualSinkModuleId = -1;
  enabled = false;
  return true;
}

bool PipeWireBackend::applyEffectChain(const EffectChain &)
{
  Logger::warn("applyEffectChain not implemented yet");
  return false;
}

bool PipeWireBackend::enable()
{
  if (!createVirtualSink(virtualSinkDisplayName))
  {
    return false;
  }
  if (!setDefaultSinkToVirtual())
  {
    return false;
  }
  selectedSinkName = previousDefaultSinkName;

  return createLoopbackToSelectedSink();
}

bool PipeWireBackend::disable()
{
  removeLoopback();
  restorePreviousDefaultSink();
  return removeVirtualSink(virtualSinkDisplayName);
}
bool PipeWireBackend::createLoopbackToSelectedSink()
{
  if (loopbackModuleId != -1)
  {
    Logger::warn("Loopback module already created with id: " + std::to_string(loopbackModuleId));
    return true;
  }
  if (selectedSinkName.empty())
  {
    Logger::warn("No target sink selected for loopback.");
    return false;
  }
  const std::string command =
      "pactl load-module module-loopback "
      "sink=" +
      selectedSinkName + " "
                         "source=" +
      virtualSinkName + ".monitor";
  const std::string result = runCommand(command);
  if (result.empty())
  {
    Logger::error("Failed to create loopback module. Command output was empty.");
    return false;
  }
  try
  {
    loopbackModuleId = std::stoi(trim(result));
  }
  catch (...)
  {
    Logger::error("Failed to parse loopback module id: " + result);
    return false;
  }
  Logger::info("Created loopback to sink: " + selectedSinkName);
  Logger::info("Loopback module ID: " + std::to_string(loopbackModuleId));
  return true;
}
bool PipeWireBackend::removeLoopback()
{
  if (loopbackModuleId == -1)
  {
    Logger::warn("No loopback module to remove.");
    return true;
  }
  const std::string command = "pactl unload-module " + std::to_string(loopbackModuleId);
  const int result = std::system(command.c_str());
  if (result != 0)
  {
    Logger::error("Failed to remove loopback module. Command exited with code: " + std::to_string(result));
    return false;
  }
  Logger::info("Removed loopback module with ID: " + std::to_string(loopbackModuleId));
  loopbackModuleId = -1;
  return true;
}