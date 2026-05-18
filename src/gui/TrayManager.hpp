#pragma once

#include <QObject>
#include <functional>

class QAction;
class QMenu;
class QSystemTrayIcon;
class QWidget;

class TrayManager : public QObject {
public:
  explicit TrayManager(QWidget *parent = nullptr);

  bool isAvailable() const;
  void setEnabledState(bool enabled);
  void setAutoStartState(bool enabled);
  void setBusy(bool busy);
  void showStillRunningMessageOnce();

  void setShowCallback(std::function<void()> callback);
  void setToggleEnhancementCallback(std::function<void()> callback);
  void setDisableEnhancementCallback(std::function<void()> callback);
  void setAutoStartCallback(std::function<void(bool)> callback);
  void setQuitCallback(std::function<void()> callback);

private:
  void updateActions();

private:
  QSystemTrayIcon *trayIcon = nullptr;
  QMenu *trayMenu = nullptr;
  QAction *showAction = nullptr;
  QAction *toggleEnhancementAction = nullptr;
  QAction *disableEnhancementAction = nullptr;
  QAction *autoStartAction = nullptr;
  QAction *quitAction = nullptr;

  bool enabled = false;
  bool autoStartEnabled = false;
  bool busy = false;
  bool trayMessageShown = false;

  std::function<void()> showCallback;
  std::function<void()> toggleEnhancementCallback;
  std::function<void()> disableEnhancementCallback;
  std::function<void(bool)> autoStartCallback;
  std::function<void()> quitCallback;
};
