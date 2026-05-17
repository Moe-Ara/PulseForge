#pragma once

#include "../core/AppConfig.hpp"

#include <string>
#include <vector>

struct ProcessResult {
  int exitCode = -1;
  std::string output;
  bool timedOut = false;
};

class ProcessRunner {
public:
  explicit ProcessRunner(int timeoutMs = AppConfig::processTimeoutMs);

  ProcessResult run(const std::vector<std::string> &arguments) const;
  bool runChecked(const std::vector<std::string> &arguments,
                  const std::string &errorMessage) const;

  static std::string trim(std::string value);

private:
  std::string resolveExecutable(const std::string &name) const;

  int timeoutMs;
};
