#pragma once
#include <QMainWindow>
#include <QString>
#include <vector>

class DeviceSelector;
class EnhancementToggle;
class EffectControls;
class EqualizerPanel;
class PresetSelector;
class QCheckBox;
class QPushButton;
class StatusIndicator;
class TrayManager;
class QCloseEvent;

#include "../presets/PresetStore.hpp"
#include "../service/AudioService.hpp"
#include "../system/SettingsStore.hpp"

struct Preset;

class MainWindow : public QMainWindow {
public:
  explicit MainWindow(AudioService &audioService, QWidget *parent = nullptr);
  ~MainWindow() override;

  void startHiddenInTray();

protected:
  void closeEvent(QCloseEvent *event) override;

private:
  void setupUi();
  void setupTray();
  void loadDevices();
  void loadPresets();
  void setupConnections();
  void restoreSettings();
  void saveSettings() const;
  void saveEnhancementEnabled(bool enabled) const;
  void showFirstRunDialog();
  void applyPresetLive(const Preset &preset, bool updateEqualizerSliders);
  void applyCurrentEqualizerCurveLive();
  std::vector<float> combinedEqualizerGains() const;
  bool selectPresetById(const QString &presetId);
  void setEnhancementActive(bool active, const QString &message);
  void setEnhancementOperationBusy(bool busy);
  void requestToggleEnhancement();
  void requestEnableEnhancement();
  void requestDisableEnhancement(const QString &successMessage);
  void requestQuit();
  void beginQuitCleanup();
  void minimizeToTray();
  void showFromTray();
  void setAutoStartEnabledFromUi(bool enabled);
  void setAutoStartChecked(bool enabled);

private:
  AudioService &audioService;
  PresetStore presetStore;
  SettingsStore settingsStore;
  TrayManager *trayManager = nullptr;
  DeviceSelector *deviceSelector = nullptr;
  EnhancementToggle *enhancementToggle = nullptr;
  EffectControls *effectControls = nullptr;
  EqualizerPanel *equalizerPanel = nullptr;
  PresetSelector *presetSelector = nullptr;
  StatusIndicator *statusIndicator = nullptr;
  QCheckBox *autoStartCheckBox = nullptr;
  QCheckBox *startMinimizedCheckBox = nullptr;
  QCheckBox *minimizeToTrayCheckBox = nullptr;
  QPushButton *minimizeButton = nullptr;
  QPushButton *closeButton = nullptr;
  bool customCurveActive = false;
  bool restoringSettings = false;
  bool enhancementOperationInProgress = false;
  bool quitRequested = false;
  bool quitCleanupInProgress = false;
};
