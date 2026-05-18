#include "DeviceSelector.hpp"

#include <QComboBox>
#include <QLabel>
#include <QSizePolicy>
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
  deviceComboBox->setMinimumContentsLength(22);
  deviceComboBox->setSizeAdjustPolicy(
      QComboBox::AdjustToMinimumContentsLengthWithIcon);
  deviceComboBox->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

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
    const int itemIndex = deviceComboBox->count() - 1;
    deviceComboBox->setItemData(itemIndex,
                                QString::fromStdString(device.sinkName),
                                Qt::UserRole + 1);
    deviceComboBox->setItemData(itemIndex, QString::fromStdString(device.name),
                                Qt::ToolTipRole);
    if (device.isDefault) {
      deviceComboBox->setCurrentIndex(itemIndex);
    }
  }

  const bool hasDevices = deviceComboBox->count() > 0;
  deviceComboBox->setEnabled(hasDevices);
  emptyLabel->setVisible(!hasDevices);
}

bool DeviceSelector::selectDeviceBySinkName(const QString &sinkName) {
  if (sinkName.isEmpty()) {
    return false;
  }

  for (int i = 0; i < deviceComboBox->count(); ++i) {
    if (deviceComboBox->itemData(i, Qt::UserRole + 1).toString() == sinkName) {
      deviceComboBox->setCurrentIndex(i);
      return true;
    }
  }

  return false;
}

std::string DeviceSelector::currentDeviceId() const {
  return deviceComboBox->currentData().toString().toStdString();
}

QString DeviceSelector::currentSinkName() const {
  return deviceComboBox->currentData(Qt::UserRole + 1).toString();
}

QComboBox *DeviceSelector::comboBox() const {
  return deviceComboBox;
}
