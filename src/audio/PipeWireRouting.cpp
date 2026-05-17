#include "PipeWireRouting.hpp"

#include "AudioConfig.hpp"
#include "../core/Logger.hpp"

#include <string>

PipeWireRouting::PipeWireRouting(ProcessRunner &runner)
    : processRunner(runner) {}

bool PipeWireRouting::createVirtualSink(const std::string &sinkName,
                                        const std::string &displayName,
                                        int &moduleId) const {
  const ProcessResult result = processRunner.run(
      {std::string(AudioConfig::pactlExecutable), "load-module",
       std::string(AudioConfig::nullSinkModule), "sink_name=" + sinkName,
       "sink_properties=device.description=" + displayName});

  if (result.exitCode != 0) {
    Logger::error("Failed to create PulseForge virtual sink. Exit code: " +
                  std::to_string(result.exitCode) + ". Output: " +
                  ProcessRunner::trim(result.output));
    return false;
  }

  try {
    moduleId = std::stoi(ProcessRunner::trim(result.output));
  } catch (...) {
    Logger::error("Could not parse virtual sink module id from output: " +
                  ProcessRunner::trim(result.output));
    return false;
  }

  return true;
}

bool PipeWireRouting::createLoopback(const std::string &sourceName,
                                     const std::string &sinkName,
                                     int &moduleId) const {
  const ProcessResult result = processRunner.run(
      {std::string(AudioConfig::pactlExecutable), "load-module",
       std::string(AudioConfig::loopbackModule), "source=" + sourceName,
       "sink=" + sinkName,
       "latency_msec=" + std::to_string(AudioConfig::loopbackLatencyMs)});

  if (result.exitCode != 0) {
    Logger::error("Failed to create PulseForge loopback. Exit code: " +
                  std::to_string(result.exitCode) + ". Output: " +
                  ProcessRunner::trim(result.output));
    return false;
  }

  try {
    moduleId = std::stoi(ProcessRunner::trim(result.output));
  } catch (...) {
    Logger::error("Could not parse loopback module id from output: " +
                  ProcessRunner::trim(result.output));
    return false;
  }

  return true;
}

bool PipeWireRouting::unloadModule(int moduleId,
                                   const std::string &failureMessage) const {
  return processRunner.runChecked(
      {std::string(AudioConfig::pactlExecutable), "unload-module",
       std::to_string(moduleId)},
      failureMessage);
}

bool PipeWireRouting::getDefaultSink(std::string &sinkName) const {
  const ProcessResult result =
      processRunner.run({std::string(AudioConfig::pactlExecutable),
                         "get-default-sink"});
  sinkName = ProcessRunner::trim(result.output);
  if (result.exitCode != 0 || sinkName.empty()) {
    Logger::error("Failed to get current default sink. Output: " + sinkName);
    return false;
  }

  return true;
}

bool PipeWireRouting::setDefaultSink(const std::string &sinkName) const {
  if (!processRunner.runChecked({std::string(AudioConfig::pactlExecutable),
                                 "set-default-sink", sinkName},
                                "Failed to set default sink.")) {
    return false;
  }

  Logger::info("Set default sink: " + sinkName);
  return true;
}

bool PipeWireRouting::listModules(std::string &modules) const {
  const ProcessResult result =
      processRunner.run({std::string(AudioConfig::pactlExecutable), "list",
                         "modules", "short"});
  modules = result.output;
  if (result.exitCode != 0) {
    Logger::warn("Could not list pactl modules for stale cleanup. Output: " +
                 ProcessRunner::trim(result.output));
    return false;
  }

  return true;
}
