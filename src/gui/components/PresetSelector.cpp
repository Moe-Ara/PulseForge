#include "PresetSelector.hpp"

#include <QComboBox>
#include <QLabel>
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

  layout->addWidget(label);
  layout->addWidget(presetComboBox);
}

void PresetSelector::addPreset(const QString &name, const QString &id) {
  presetComboBox->addItem(name, id);
}

std::string PresetSelector::currentPresetId() const {
  return presetComboBox->currentData().toString().toStdString();
}

QComboBox *PresetSelector::comboBox() const {
  return presetComboBox;
}
