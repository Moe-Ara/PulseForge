#include "AutoStartManager.hpp"

#include "../core/AppConfig.hpp"
#include "../core/Logger.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QIODevice>
#include <QProcess>
#include <QStringList>
#include <QTextStream>

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

QString quoteSystemdExecPath(QString path) {
  path.replace("\\", "\\\\");
  path.replace("\"", "\\\"");
  return "\"" + path + "\"";
}

} // namespace

bool AutoStartManager::enableAutoStart() const {
  if (!ensureUserServiceFile()) {
    return false;
  }

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

bool AutoStartManager::ensureUserServiceFile() const {
  const QString serviceDirectory =
      QDir::homePath() + QStringLiteral("/.config/systemd/user");
  QDir directory(serviceDirectory);
  if (!directory.exists() && !directory.mkpath(QStringLiteral("."))) {
    Logger::warn("Could not create user systemd directory: " +
                 serviceDirectory.toStdString());
    return false;
  }

  const QString servicePath =
      serviceDirectory + QStringLiteral("/") +
      QString::fromUtf8(AppConfig::systemdServiceName.data());
  QFile serviceFile(servicePath);
  if (!serviceFile.open(QIODevice::WriteOnly | QIODevice::Text |
                        QIODevice::Truncate)) {
    Logger::warn("Could not write PulseForge user service: " +
                 serviceFile.errorString().toStdString());
    return false;
  }

  const QString executable =
      quoteSystemdExecPath(QCoreApplication::applicationFilePath());
  QTextStream stream(&serviceFile);
  stream << "[Unit]\n"
         << "Description=PulseForge Audio Enhancer\n"
         << "After=graphical-session.target pipewire.service "
            "pipewire-pulse.service wireplumber.service\n"
         << "Wants=pipewire.service pipewire-pulse.service "
            "wireplumber.service\n\n"
         << "[Service]\n"
         << "Type=simple\n"
         << "ExecStart=" << executable << " --start-minimized\n"
         << "Restart=on-failure\n"
         << "RestartSec=2\n\n"
         << "[Install]\n"
         << "WantedBy=default.target\n";

  Logger::info("Wrote PulseForge user service: " + servicePath.toStdString());
  return true;
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
