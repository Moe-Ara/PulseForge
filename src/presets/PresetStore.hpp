#pragma once

#include <QString>
#include <vector>

struct SavedPreset {
  QString id;
  QString name;
  std::vector<float> gains;
  std::vector<float> frequencies;
  std::vector<int> effectValues;
};

class PresetStore {
public:
  std::vector<SavedPreset> loadPresets() const;
  bool savePreset(const QString &name, const std::vector<float> &gains,
                  const std::vector<float> &frequencies,
                  const std::vector<int> &effectValues) const;
  bool loadPreset(const QString &id, SavedPreset &preset) const;

private:
  QString idForName(const QString &name) const;
};
