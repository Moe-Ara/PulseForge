#pragma once

#include <QString>
#include <QWidget>
#include <string>

class QComboBox;
class QPushButton;

class PresetSelector : public QWidget {
public:
  explicit PresetSelector(QWidget *parent = nullptr);

  void addPreset(const QString &name, const QString &id);
  void clearPresets();
  std::string currentPresetId() const;
  QComboBox *comboBox() const;
  QPushButton *saveButton() const;

private:
  QComboBox *presetComboBox = nullptr;
  QPushButton *savePresetButton = nullptr;
};
