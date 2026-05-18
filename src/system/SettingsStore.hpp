#pragma once

#include <QString>
#include <vector>

class SettingsStore {
public:
  QString selectedOutputSinkName() const;
  void setSelectedOutputSinkName(const QString &sinkName) const;

  QString lastPresetId() const;
  void setLastPresetId(const QString &presetId) const;

  std::vector<float> customEqGains() const;
  void setCustomEqGains(const std::vector<float> &gains) const;

  std::vector<float> customEqFrequencies() const;
  void setCustomEqFrequencies(const std::vector<float> &frequencies) const;

  std::vector<int> customEffectValues() const;
  void setCustomEffectValues(const std::vector<int> &values) const;

  bool enhancementEnabled() const;
  void setEnhancementEnabled(bool enabled) const;

  bool startMinimized() const;
  void setStartMinimized(bool enabled) const;

  bool minimizeToTray() const;
  void setMinimizeToTray(bool enabled) const;

  bool startOnLoginPreference() const;
  void setStartOnLoginPreference(bool enabled) const;

  bool firstRunComplete() const;
  void setFirstRunComplete(bool complete) const;

  void sync() const;
};
