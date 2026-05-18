#include "RuntimeDependencyChecker.hpp"

#include "ProcessRunner.hpp"

QString RuntimeDependencyChecker::Result::userMessage() const {
  QString message =
      "PulseForge requires PipeWire, WirePlumber, pactl, and the "
      "PipeWire Pulse compatibility server.\n\n";

  if (!missing.isEmpty()) {
    message += "Missing or unavailable:\n";
    for (const QString &item : missing) {
      message += "  - " + item + "\n";
    }
    message += "\n";
  }

  if (!warnings.isEmpty()) {
    message += "Warnings:\n";
    for (const QString &item : warnings) {
      message += "  - " + item + "\n";
    }
    message += "\n";
  }

  message +=
      "On Fedora/Bazzite these are usually installed by default. If needed, "
      "install pipewire, pipewire-pulseaudio, wireplumber, and pactl, then log "
      "out and back in.";
  return message;
}

RuntimeDependencyChecker::Result RuntimeDependencyChecker::check() const {
  Result result;

  if (!commandAvailable("pactl")) {
    result.missing << "pactl command";
  } else {
    const ProcessRunner runner;
    const ProcessResult pactlInfo = runner.run({"pactl", "info"});
    const QString output = QString::fromStdString(pactlInfo.output);
    if (pactlInfo.exitCode != 0) {
      result.missing << "PipeWire Pulse/PulseAudio compatibility server";
    } else if (!output.contains("PipeWire", Qt::CaseInsensitive)) {
      result.warnings
          << "pactl is available, but the active Pulse server does not report "
             "PipeWire. PulseForge is intended for PipeWire sessions.";
    }
  }

  if (commandAvailable("systemctl")) {
    const ProcessRunner runner;
    const ProcessResult pipewire =
        runner.run({"systemctl", "--user", "is-active", "pipewire.service"});
    if (pipewire.exitCode != 0) {
      result.missing << "PipeWire user service";
    }

    const ProcessResult pipewirePulse = runner.run(
        {"systemctl", "--user", "is-active", "pipewire-pulse.service"});
    if (pipewirePulse.exitCode != 0) {
      result.missing << "pipewire-pulse user service";
    }

    const ProcessResult wireplumber =
        runner.run({"systemctl", "--user", "is-active", "wireplumber.service"});
    if (wireplumber.exitCode != 0) {
      result.missing << "WirePlumber user service";
    }
  } else {
    result.warnings
        << "systemctl was not found; start-on-login integration will be "
           "unavailable.";
  }

  result.ok = result.missing.isEmpty();
  return result;
}

bool RuntimeDependencyChecker::commandAvailable(const QString &command) const {
  const ProcessRunner runner;
  const ProcessResult result = runner.run({"sh", "-c", "command -v " +
                                                        command.toStdString()});
  return result.exitCode == 0;
}
