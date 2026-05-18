#pragma once

#include <string>

struct RuntimeState {
  int virtualSinkModuleId = -1;
  int loopbackModuleId = -1;
  std::string previousDefaultSinkName;
  std::string previousDefaultSourceName;
  std::string processingSinkName;
  bool defaultSourceChangedByPulseForge = false;
};

class RuntimeStateStore {
public:
  RuntimeStateStore();
  explicit RuntimeStateStore(std::string path);

  bool load(RuntimeState &state) const;
  bool save(const RuntimeState &state) const;
  bool clear() const;

  const std::string &path() const;
  static std::string defaultPath();

private:
  std::string statePath;
};
