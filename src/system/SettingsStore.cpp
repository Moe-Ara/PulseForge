#include "SettingsStore.hpp"

#include "../core/AppConfig.hpp"

#include <QSettings>
#include <QVariantList>

#include <cstddef>

namespace {

QSettings settings() {
  return QSettings(QString::fromUtf8(AppConfig::organizationName.data()),
                   QString::fromUtf8(AppConfig::applicationName.data()));
}

QVariantList floatsToVariantList(const std::vector<float> &floats) {
  QVariantList values;
  values.reserve(static_cast<int>(floats.size()));
  for (float value : floats) {
    values.push_back(value);
  }
  return values;
}

std::vector<float> floatsFromVariantList(const QVariantList &values) {
  std::vector<float> floats;
  floats.reserve(static_cast<std::size_t>(values.size()));
  for (const auto &value : values) {
    floats.push_back(value.toFloat());
  }
  return floats;
}

QVariantList intsToVariantList(const std::vector<int> &ints) {
  QVariantList values;
  values.reserve(static_cast<int>(ints.size()));
  for (int value : ints) {
    values.push_back(value);
  }
  return values;
}

std::vector<int> intsFromVariantList(const QVariantList &values) {
  std::vector<int> ints;
  ints.reserve(static_cast<std::size_t>(values.size()));
  for (const auto &value : values) {
    ints.push_back(value.toInt());
  }
  return ints;
}

} // namespace

QString SettingsStore::selectedOutputSinkName() const {
  return settings().value("audio/outputSinkName").toString();
}

void SettingsStore::setSelectedOutputSinkName(const QString &sinkName) const {
  QSettings store = settings();
  store.setValue("audio/outputSinkName", sinkName);
}

QString SettingsStore::lastPresetId() const {
  return settings().value("presets/lastPresetId", "flat").toString();
}

void SettingsStore::setLastPresetId(const QString &presetId) const {
  QSettings store = settings();
  store.setValue("presets/lastPresetId", presetId);
}

std::vector<float> SettingsStore::customEqGains() const {
  return floatsFromVariantList(
      settings().value("presets/customEqGains").toList());
}

void SettingsStore::setCustomEqGains(const std::vector<float> &gains) const {
  QSettings store = settings();
  store.setValue("presets/customEqGains", floatsToVariantList(gains));
}

std::vector<float> SettingsStore::customEqFrequencies() const {
  return floatsFromVariantList(
      settings().value("presets/customEqFrequencies").toList());
}

void SettingsStore::setCustomEqFrequencies(
    const std::vector<float> &frequencies) const {
  QSettings store = settings();
  store.setValue("presets/customEqFrequencies",
                 floatsToVariantList(frequencies));
}

std::vector<int> SettingsStore::customEffectValues() const {
  return intsFromVariantList(
      settings().value("presets/customEffectValues").toList());
}

void SettingsStore::setCustomEffectValues(const std::vector<int> &values) const {
  QSettings store = settings();
  store.setValue("presets/customEffectValues", intsToVariantList(values));
}

bool SettingsStore::enhancementEnabled() const {
  return settings().value("audio/enhancementEnabled", false).toBool();
}

void SettingsStore::setEnhancementEnabled(bool enabled) const {
  QSettings store = settings();
  store.setValue("audio/enhancementEnabled", enabled);
}

bool SettingsStore::startMinimized() const {
  return settings().value("window/startMinimized", false).toBool();
}

void SettingsStore::setStartMinimized(bool enabled) const {
  QSettings store = settings();
  store.setValue("window/startMinimized", enabled);
}

bool SettingsStore::minimizeToTray() const {
  return settings().value("window/minimizeToTray", true).toBool();
}

void SettingsStore::setMinimizeToTray(bool enabled) const {
  QSettings store = settings();
  store.setValue("window/minimizeToTray", enabled);
}

bool SettingsStore::startOnLoginPreference() const {
  return settings().value("startup/startOnLogin", false).toBool();
}

void SettingsStore::setStartOnLoginPreference(bool enabled) const {
  QSettings store = settings();
  store.setValue("startup/startOnLogin", enabled);
}

bool SettingsStore::firstRunComplete() const {
  return settings().value("firstRun/complete", false).toBool();
}

void SettingsStore::setFirstRunComplete(bool complete) const {
  QSettings store = settings();
  store.setValue("firstRun/complete", complete);
}

void SettingsStore::sync() const {
  QSettings store = settings();
  store.sync();
}
