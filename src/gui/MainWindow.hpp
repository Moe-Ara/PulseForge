#pragma once
#include <QMainWindow>
#include <QString>

class DeviceSelector;
class EnhancementToggle;
class PresetSelector;
class StatusIndicator;

#include "../presets/Preset.hpp"
#include "../service/AudioService.hpp"

class MainWindow : public QMainWindow {
public:
  explicit MainWindow(AudioService &audioService, QWidget *parent = nullptr);

private:
  void setupUi();
  void loadDevices();
  void setupConnections();
  void setEnhancementActive(bool active, const QString &message);

  Preset createGamingPreset() const;

private:
  AudioService &audioService;
  DeviceSelector *deviceSelector = nullptr;
  EnhancementToggle *enhancementToggle = nullptr;
  PresetSelector *presetSelector = nullptr;
  StatusIndicator *statusIndicator = nullptr;
};
