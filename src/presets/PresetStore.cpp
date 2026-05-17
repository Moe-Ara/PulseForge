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

std::vector<float> defaultFrequencies() {
  return {DspConfig::eqFrequencies.begin(), DspConfig::eqFrequencies.end()};
}

std::vector<float> floatsFromVariantList(const QVariantList &values) {
  std::vector<float> floats;
  floats.reserve(values.size());
  for (const auto &value : values) {
    floats.push_back(value.toFloat());
  }
  return floats;
}

QVariantList floatsToVariantList(const std::vector<float> &floats) {
  QVariantList values;
  for (float value : floats) {
    values.push_back(value);
  }
  return values;
}

std::vector<int> defaultEffectValues() {
  return {50, 24, 14, 34, 24, 50, 100};
}

std::vector<int> intsFromVariantList(const QVariantList &values) {
  std::vector<int> ints;
  ints.reserve(values.size());
  for (const auto &value : values) {
    ints.push_back(value.toInt());
  }
  return ints;
}

QVariantList intsToVariantList(const std::vector<int> &ints) {
  QVariantList values;
  for (int value : ints) {
    values.push_back(value);
  }
  return values;
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

    preset.gains = floatsFromVariantList(appSettings.value("gains").toList());
    preset.frequencies =
        floatsFromVariantList(appSettings.value("frequencies").toList());
    if (preset.frequencies.size() != DspConfig::eqBandCount) {
      preset.frequencies = defaultFrequencies();
    }
    preset.effectValues =
        intsFromVariantList(appSettings.value("effectValues").toList());
    if (preset.effectValues.size() < 5) {
      preset.effectValues = defaultEffectValues();
    } else if (preset.effectValues.size() < defaultEffectValues().size()) {
      const std::vector<int> defaults = defaultEffectValues();
      for (std::size_t index = preset.effectValues.size();
           index < defaults.size(); ++index) {
        preset.effectValues.push_back(defaults.at(index));
      }
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
                             const std::vector<float> &gains,
                             const std::vector<float> &frequencies,
                             const std::vector<int> &effectValues) const {
  if (name.trimmed().isEmpty() || gains.size() != DspConfig::eqBandCount ||
      frequencies.size() != DspConfig::eqBandCount ||
      effectValues.size() < defaultEffectValues().size()) {
    return false;
  }

  std::vector<SavedPreset> presets = loadPresets();
  const QString id = idForName(name);

  SavedPreset savedPreset{id, name.trimmed(), gains, frequencies, effectValues};
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

    const auto &preset = presets.at(static_cast<std::size_t>(i));
    appSettings.setValue("gains", floatsToVariantList(preset.gains));
    appSettings.setValue(
        "frequencies", floatsToVariantList(preset.frequencies));
    appSettings.setValue("effectValues",
                         intsToVariantList(preset.effectValues));
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
