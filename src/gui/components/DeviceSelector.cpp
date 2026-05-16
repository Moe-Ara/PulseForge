#include "DeviceSelector.hpp"

#include <QComboBox>
#include <QLabel>
#include <QVBoxLayout>

DeviceSelector::DeviceSelector(QWidget *parent) : QWidget(parent) {
  setObjectName("selectorBlock");

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(8);

  auto *label = new QLabel("Output device", this);
  label->setObjectName("fieldLabel");

  deviceComboBox = new QComboBox(this);
  deviceComboBox->setObjectName("fieldCombo");

  emptyLabel = new QLabel("No PipeWire output devices found", this);
  emptyLabel->setObjectName("hintLabel");
  emptyLabel->setVisible(false);

  layout->addWidget(label);
  layout->addWidget(deviceComboBox);
  layout->addWidget(emptyLabel);
}

void DeviceSelector::setDevices(const std::vector<AudioDevice> &devices) {
  deviceComboBox->clear();

  for (const auto &device : devices) {
    deviceComboBox->addItem(QString::fromStdString(device.name),
                            QString::fromStdString(device.id));
    if (device.isDefault) {
      deviceComboBox->setCurrentIndex(deviceComboBox->count() - 1);
    }
  }

  const bool hasDevices = deviceComboBox->count() > 0;
  deviceComboBox->setEnabled(hasDevices);
  emptyLabel->setVisible(!hasDevices);
}

std::string DeviceSelector::currentDeviceId() const {
  return deviceComboBox->currentData().toString().toStdString();
}

QComboBox *DeviceSelector::comboBox() const {
  return deviceComboBox;
}
