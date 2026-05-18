#include "PresetSelector.hpp"

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QSizePolicy>
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
  presetComboBox->setMinimumContentsLength(16);
  presetComboBox->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  presetComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

  savePresetButton = new QPushButton("Save Current Preset", this);
  savePresetButton->setObjectName("secondaryButton");
  savePresetButton->setMinimumWidth(180);
  savePresetButton->setSizePolicy(QSizePolicy::MinimumExpanding,
                                  QSizePolicy::Fixed);

  layout->addWidget(label);
  layout->addWidget(presetComboBox);
  layout->addWidget(savePresetButton);
}

void PresetSelector::addPreset(const QString &name, const QString &id) {
  presetComboBox->addItem(name, id);
  presetComboBox->setItemData(presetComboBox->count() - 1, name,
                              Qt::ToolTipRole);
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
