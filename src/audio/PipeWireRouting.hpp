#pragma once

#include "system/ProcessRunner.hpp"

#include <string>

class PipeWireRouting {
public:
  explicit PipeWireRouting(ProcessRunner &processRunner);

  bool createVirtualSink(const std::string &sinkName,
                         const std::string &displayName, int &moduleId) const;
  bool createLoopback(const std::string &sourceName,
                      const std::string &sinkName, int &moduleId) const;
  bool unloadModule(int moduleId, const std::string &failureMessage) const;
  bool getDefaultSink(std::string &sinkName) const;
  bool setDefaultSink(const std::string &sinkName) const;
  bool listModules(std::string &modules) const;

private:
  ProcessRunner &processRunner;
};
