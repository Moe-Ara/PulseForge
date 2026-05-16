#include "PresetSelector.hpp"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

PresetSelector::PresetSelector(QWidget *parent) : QWidget(parent) {
  setObjectName("selectorBlock");

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto *label = new QLabel("Preset", this);
  label->setObjectName("fieldLabel");

  presetComboBox = new QComboBox(this);
  presetComboBox->setObjectName("fieldCombo");

  savePresetButton = new QPushButton("Save Current Preset", this);
  savePresetButton->setObjectName("secondaryButton");

  layout->addWidget(label);
  layout->addWidget(presetComboBox);
  layout->addWidget(savePresetButton);
}

void PresetSelector::addPreset(const QString &name, const QString &id) {
  presetComboBox->addItem(name, id);
}

void PresetSelector::clearPresets() {
  presetComboBox->clear();
}

std::string PresetSelector::currentPresetId() const {
  return presetComboBox->currentData().toString().toStdString();
}

QComboBox *PresetSelector::comboBox() const {
  return presetComboBox;
}

QPushButton *PresetSelector::saveButton() const {
  return savePresetButton;
}
