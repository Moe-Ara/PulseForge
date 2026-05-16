#pragma once

#include <QString>
#include <vector>

struct SavedPreset {
  QString id;
  QString name;
  std::vector<float> gains;
};

class PresetStore {
public:
  std::vector<SavedPreset> loadPresets() const;
  bool savePreset(const QString &name, const std::vector<float> &gains) const;
  bool loadPreset(const QString &id, SavedPreset &preset) const;

private:
  QString idForName(const QString &name) const;
};
