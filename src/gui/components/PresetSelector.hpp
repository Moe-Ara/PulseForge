#pragma once

#include <QString>
#include <QWidget>
#include <string>

class QComboBox;

class PresetSelector : public QWidget {
public:
  explicit PresetSelector(QWidget *parent = nullptr);

  void addPreset(const QString &name, const QString &id);
  std::string currentPresetId() const;
  QComboBox *comboBox() const;

private:
  QComboBox *presetComboBox = nullptr;
};
