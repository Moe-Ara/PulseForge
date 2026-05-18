#include "RuntimeStateStore.hpp"

#include "../core/AppConfig.hpp"
#include "../core/Logger.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <utility>

RuntimeStateStore::RuntimeStateStore() : statePath(defaultPath()) {}

RuntimeStateStore::RuntimeStateStore(std::string path)
    : statePath(std::move(path)) {}

std::string RuntimeStateStore::defaultPath() {
  const char *runtimeDir =
      std::getenv(AppConfig::runtimeDirEnvironmentVariable.data());
  if (runtimeDir && *runtimeDir) {
    return (std::filesystem::path(runtimeDir) /
            AppConfig::runtimeStateRelativePath)
        .string();
  }

  return std::string(AppConfig::runtimeStateFallbackPath);
}

bool RuntimeStateStore::load(RuntimeState &state) const {
  std::ifstream stream(statePath);
  if (!stream.is_open()) {
    return false;
  }

  RuntimeState loaded;
  std::string line;
  while (std::getline(stream, line)) {
    try {
      if (line.rfind("virtual_sink_module=", 0) == 0) {
        loaded.virtualSinkModuleId = std::stoi(line.substr(20));
      } else if (line.rfind("loopback_module=", 0) == 0) {
        loaded.loopbackModuleId = std::stoi(line.substr(16));
      } else if (line.rfind("previous_default=", 0) == 0) {
        loaded.previousDefaultSinkName = line.substr(17);
      } else if (line.rfind("previous_default_source=", 0) == 0) {
        loaded.previousDefaultSourceName = line.substr(24);
      } else if (line.rfind("processing_sink=", 0) == 0) {
        loaded.processingSinkName = line.substr(16);
      } else if (line.rfind("source_changed=", 0) == 0) {
        loaded.defaultSourceChangedByPulseForge = line.substr(15) == "1";
      }
    } catch (...) {
    }
  }

  state = std::move(loaded);
  return true;
}

bool RuntimeStateStore::save(const RuntimeState &state) const {
  std::filesystem::path path(statePath);
  std::error_code error;
  std::filesystem::create_directories(path.parent_path(), error);
  if (error) {
    Logger::error("Failed to create PulseForge runtime state directory: " +
                  error.message());
    return false;
  }

  std::ofstream stream(path);
  if (!stream.is_open()) {
    Logger::error("Failed to write PulseForge runtime state.");
    return false;
  }

  stream << "virtual_sink_module=" << state.virtualSinkModuleId << '\n';
  stream << "loopback_module=" << state.loopbackModuleId << '\n';
  stream << "previous_default=" << state.previousDefaultSinkName << '\n';
  stream << "previous_default_source=" << state.previousDefaultSourceName
         << '\n';
  stream << "processing_sink=" << state.processingSinkName << '\n';
  stream << "source_changed="
         << (state.defaultSourceChangedByPulseForge ? "1" : "0") << '\n';
  return true;
}

bool RuntimeStateStore::clear() const {
  std::error_code error;
  std::filesystem::remove(statePath, error);
  if (error) {
    Logger::warn("Failed to remove PulseForge runtime state: " +
                 error.message());
    return false;
  }
  return true;
}

const std::string &RuntimeStateStore::path() const {
  return statePath;
}
