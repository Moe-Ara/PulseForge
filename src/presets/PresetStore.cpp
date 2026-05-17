#include "PresetStore.hpp"

#include "core/AppConfig.hpp"
#include "dsp/DspConfig.hpp"

#include <QRegularExpression>
#include <QSettings>
#include <QVariantList>
#include <cstddef>

namespace {

QSettings settings() {
  return QSettings(QString::fromUtf8(AppConfig::organizationName.data()),
                   QString::fromUtf8(AppConfig::applicationName.data()));
}

} // namespace

std::vector<SavedPreset> PresetStore::loadPresets() const {
  QSettings appSettings = settings();
  const int size = appSettings.beginReadArray("presets");

  std::vector<SavedPreset> presets;
  presets.reserve(static_cast<std::size_t>(size));

  for (int i = 0; i < size; ++i) {
    appSettings.setArrayIndex(i);

    SavedPreset preset;
    preset.id = appSettings.value("id").toString();
    preset.name = appSettings.value("name").toString();

    const QVariantList values = appSettings.value("gains").toList();
    preset.gains.reserve(values.size());
    for (const auto &value : values) {
      preset.gains.push_back(value.toFloat());
    }

    if (!preset.id.isEmpty() && !preset.name.isEmpty() &&
        preset.gains.size() == DspConfig::eqBandCount) {
      presets.push_back(preset);
    }
  }

  appSettings.endArray();
  return presets;
}

bool PresetStore::savePreset(const QString &name,
                             const std::vector<float> &gains) const {
  if (name.trimmed().isEmpty() || gains.size() != DspConfig::eqBandCount) {
    return false;
  }

  std::vector<SavedPreset> presets = loadPresets();
  const QString id = idForName(name);

  SavedPreset savedPreset{id, name.trimmed(), gains};
  bool replaced = false;
  for (auto &preset : presets) {
    if (preset.id == id) {
      preset = savedPreset;
      replaced = true;
      break;
    }
  }

  if (!replaced) {
    presets.push_back(savedPreset);
  }

  QSettings appSettings = settings();
  appSettings.beginWriteArray("presets");

  for (int i = 0; i < static_cast<int>(presets.size()); ++i) {
    appSettings.setArrayIndex(i);
    appSettings.setValue("id", presets.at(static_cast<std::size_t>(i)).id);
    appSettings.setValue("name", presets.at(static_cast<std::size_t>(i)).name);

    QVariantList values;
    for (float gain : presets.at(static_cast<std::size_t>(i)).gains) {
      values.push_back(gain);
    }
    appSettings.setValue("gains", values);
  }

  appSettings.endArray();
  appSettings.sync();
  return appSettings.status() == QSettings::NoError;
}

bool PresetStore::loadPreset(const QString &id, SavedPreset &preset) const {
  for (const auto &storedPreset : loadPresets()) {
    if (storedPreset.id == id) {
      preset = storedPreset;
      return true;
    }
  }

  return false;
}

QString PresetStore::idForName(const QString &name) const {
  QString id = name.trimmed().toLower();
  id.replace(QRegularExpression("[^a-z0-9]+"), "-");
  id = id.remove(QRegularExpression("^-+|-+$"));
  return "custom:" + (id.isEmpty() ? "preset" : id);
}
