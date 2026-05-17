#pragma once
#include <QMainWindow>
#include <QString>
#include <vector>

class DeviceSelector;
class EnhancementToggle;
class EffectControls;
class EqualizerPanel;
class PresetSelector;
class StatusIndicator;

#include "../presets/PresetStore.hpp"
#include "../service/AudioService.hpp"

struct Preset;

class MainWindow : public QMainWindow {
public:
  explicit MainWindow(AudioService &audioService, QWidget *parent = nullptr);

private:
  void setupUi();
  void loadDevices();
  void loadPresets();
  void setupConnections();
  void restoreSettings();
  void saveSettings() const;
  void saveEnhancementEnabled(bool enabled) const;
  void applyPresetLive(const Preset &preset, bool updateEqualizerSliders);
  void applyCurrentEqualizerCurveLive();
  std::vector<float> combinedEqualizerGains() const;
  bool selectPresetById(const QString &presetId);
  void setEnhancementActive(bool active, const QString &message);

private:
  AudioService &audioService;
  PresetStore presetStore;
  DeviceSelector *deviceSelector = nullptr;
  EnhancementToggle *enhancementToggle = nullptr;
  EffectControls *effectControls = nullptr;
  EqualizerPanel *equalizerPanel = nullptr;
  PresetSelector *presetSelector = nullptr;
  StatusIndicator *statusIndicator = nullptr;
  bool customCurveActive = false;
  bool restoringSettings = false;
};
