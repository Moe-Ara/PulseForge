#pragma once
#include <QMainWindow>
#include <queue>
#include "components/EnhancementToggle.hpp"

class QPushButton;
class QComboBox;
class QLabel;

#include "../presets/Preset.hpp"
#include "../service/AudioService.hpp"

class MainWindow : public QMainWindow {
public:
  explicit MainWindow(AudioService &audioService, QWidget *parent = nullptr);

private:
  void setupUi();
  void loadDevices();
  void setupConnections();

  Preset createGamingPreset() const;

private:
  AudioService &audioService;
  QLabel *statusLabel = nullptr;
  QComboBox *deviceComboBox = nullptr;
  QComboBox *presetComboBox = nullptr;
  QPushButton *enableButton = nullptr;
  QPushButton *disableButton = nullptr;
  QPushButton *applyPresetButton = nullptr;
  EnhancementToggle *enhancementToggle = nullptr;
};
