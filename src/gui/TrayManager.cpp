#include "TrayManager.hpp"

#include "../core/AppConfig.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QFile>
#include <QIcon>
#include <QMenu>
#include <QSystemTrayIcon>
#include <utility>

namespace {

QIcon resolveTrayIcon() {
  QIcon icon =
      QIcon::fromTheme(QString::fromUtf8(AppConfig::desktopFileName.data()));
  if (!icon.isNull()) {
    return icon;
  }

  const QString developmentIconPath =
      QCoreApplication::applicationDirPath() +
      QStringLiteral("/../data/icons/pulseforge.svg");
  if (QFile::exists(developmentIconPath)) {
    icon = QIcon(developmentIconPath);
  }

  if (icon.isNull()) {
    icon = QApplication::windowIcon();
  }

  return icon;
}

} // namespace

TrayManager::TrayManager(QWidget *parent) : QObject(parent) {
  trayIcon = new QSystemTrayIcon(resolveTrayIcon(), this);
  trayIcon->setToolTip("PulseForge audio enhancer");

  trayMenu = new QMenu(parent);
  showAction = trayMenu->addAction("Show PulseForge");
  toggleEnhancementAction = trayMenu->addAction("Enable Enhancement");
  disableEnhancementAction =
      trayMenu->addAction("Restore Default Output / Disable");
  disableEnhancementAction->setToolTip(
      "Stop enhancement and restore the previous output device");
  autoStartAction = trayMenu->addAction("Start on Login");
  autoStartAction->setCheckable(true);
  autoStartAction->setToolTip("Start PulseForge when you log in");
  trayMenu->addSeparator();
  quitAction = trayMenu->addAction("Quit");
  quitAction->setToolTip(
      "Quit PulseForge after restoring normal audio routing");

  trayIcon->setContextMenu(trayMenu);

  connect(showAction, &QAction::triggered, this, [this]() {
    if (showCallback) {
      showCallback();
    }
  });
  connect(toggleEnhancementAction, &QAction::triggered, this, [this]() {
    if (toggleEnhancementCallback) {
      toggleEnhancementCallback();
    }
  });
  connect(disableEnhancementAction, &QAction::triggered, this, [this]() {
    if (disableEnhancementCallback) {
      disableEnhancementCallback();
    }
  });
  connect(autoStartAction, &QAction::toggled, this, [this](bool checked) {
    if (autoStartCallback) {
      autoStartCallback(checked);
    }
  });
  connect(quitAction, &QAction::triggered, this, [this]() {
    if (quitCallback) {
      quitCallback();
    }
  });
  connect(trayIcon, &QSystemTrayIcon::activated, this,
          [this](QSystemTrayIcon::ActivationReason reason) {
            if (reason == QSystemTrayIcon::DoubleClick ||
                reason == QSystemTrayIcon::Trigger) {
              if (showCallback) {
                showCallback();
              }
            }
          });

  updateActions();
  trayIcon->show();
}

bool TrayManager::isAvailable() const {
  return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayManager::setEnabledState(bool active) {
  enabled = active;
  updateActions();
}

void TrayManager::setAutoStartState(bool active) {
  autoStartEnabled = active;
  updateActions();
}

void TrayManager::setBusy(bool value) {
  busy = value;
  updateActions();
}

void TrayManager::showStillRunningMessageOnce() {
  if (trayMessageShown || !trayIcon || !isAvailable()) {
    return;
  }

  trayMessageShown = true;
  trayIcon->showMessage("PulseForge is still running",
                        "Use the tray icon to show the window or quit.",
                        QSystemTrayIcon::Information, 3500);
}

void TrayManager::setShowCallback(std::function<void()> callback) {
  showCallback = std::move(callback);
}

void TrayManager::setToggleEnhancementCallback(std::function<void()> callback) {
  toggleEnhancementCallback = std::move(callback);
}

void TrayManager::setDisableEnhancementCallback(std::function<void()> callback) {
  disableEnhancementCallback = std::move(callback);
}

void TrayManager::setAutoStartCallback(std::function<void(bool)> callback) {
  autoStartCallback = std::move(callback);
}

void TrayManager::setQuitCallback(std::function<void()> callback) {
  quitCallback = std::move(callback);
}

void TrayManager::updateActions() {
  if (toggleEnhancementAction) {
    toggleEnhancementAction->setText(enabled ? "Disable Enhancement"
                                             : "Enable Enhancement");
    toggleEnhancementAction->setEnabled(!busy);
  }

  if (disableEnhancementAction) {
    disableEnhancementAction->setEnabled(enabled && !busy);
  }

  if (autoStartAction) {
    const bool signalsBlocked = autoStartAction->blockSignals(true);
    autoStartAction->setChecked(autoStartEnabled);
    autoStartAction->blockSignals(signalsBlocked);
  }

  if (quitAction) {
    quitAction->setEnabled(true);
  }
}
