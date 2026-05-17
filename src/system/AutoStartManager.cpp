#include "AutoStartManager.hpp"

#include "../core/AppConfig.hpp"
#include "../core/Logger.hpp"

#include <QProcess>
#include <QStringList>

#include <string>

namespace {

struct ProcessResult {
  int exitCode = -1;
  bool timedOut = false;
  QString output;
};

ProcessResult runSystemctl(const QStringList &arguments) {
  QProcess process;
  process.setProgram(QString::fromUtf8(AppConfig::systemctlExecutable.data()));
  process.setArguments(arguments);
  process.setProcessChannelMode(QProcess::MergedChannels);
  process.start();

  ProcessResult result;
  if (!process.waitForStarted(AppConfig::processTimeoutMs)) {
    result.output = process.errorString();
    return result;
  }

  if (!process.waitForFinished(AppConfig::processTimeoutMs)) {
    result.timedOut = true;
    process.kill();
    process.waitForFinished(AppConfig::processKillWaitMs);
  }

  result.exitCode = process.exitCode();
  result.output = QString::fromLocal8Bit(process.readAll()).trimmed();
  return result;
}

} // namespace

bool AutoStartManager::enableAutoStart() const {
  const ProcessResult reloadResult = runSystemctl({"--user", "daemon-reload"});
  if (reloadResult.exitCode != 0) {
    Logger::warn("systemctl --user daemon-reload failed before enabling "
                 "PulseForge autostart. Exit code: " +
                 std::to_string(reloadResult.exitCode) + ". Output: " +
                 reloadResult.output.toStdString());
  }

  return runSystemctlUser("enable");
}

bool AutoStartManager::disableAutoStart() const {
  return runSystemctlUser("disable");
}

bool AutoStartManager::isAutoStartEnabled() const {
  const ProcessResult result =
      runSystemctl({"--user", "is-enabled",
                    QString::fromUtf8(AppConfig::systemdServiceName.data())});
  return result.exitCode == 0 && result.output == "enabled";
}

bool AutoStartManager::runSystemctlUser(const char *action) const {
  const ProcessResult result =
      runSystemctl({"--user", action,
                    QString::fromUtf8(AppConfig::systemdServiceName.data())});
  if (result.exitCode == 0) {
    Logger::info(std::string("systemctl --user ") + action + " " +
                 std::string(AppConfig::systemdServiceName) + " succeeded.");
    return true;
  }

  Logger::warn(std::string("systemctl --user ") + action + " " +
               std::string(AppConfig::systemdServiceName) +
               " failed. Exit code: " +
               std::to_string(result.exitCode) +
               (result.timedOut ? " (timed out). Output: " : ". Output: ") +
               result.output.toStdString());
  return false;
}
