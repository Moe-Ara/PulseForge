#pragma once
#include <QMainWindow>
#include <QString>

class DeviceSelector;
class EnhancementToggle;
class EqualizerPanel;
class PresetSelector;
class StatusIndicator;

#include "../presets/PresetStore.hpp"
#include "../service/AudioService.hpp"

class MainWindow : public QMainWindow {
public:
  explicit MainWindow(AudioService &audioService, QWidget *parent = nullptr);

private:
  void setupUi();
  void loadDevices();
  void loadPresets();
  void setupConnections();
  void setEnhancementActive(bool active, const QString &message);

private:
  AudioService &audioService;
  PresetStore presetStore;
  DeviceSelector *deviceSelector = nullptr;
  EnhancementToggle *enhancementToggle = nullptr;
  EqualizerPanel *equalizerPanel = nullptr;
  PresetSelector *presetSelector = nullptr;
  StatusIndicator *statusIndicator = nullptr;
};
