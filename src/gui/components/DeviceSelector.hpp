#pragma once

#include <QWidget>
#include <string>
#include <vector>

#include "../../audio/AudioDevice.hpp"

class QComboBox;
class QLabel;

class DeviceSelector : public QWidget {
public:
  explicit DeviceSelector(QWidget *parent = nullptr);

  void setDevices(const std::vector<AudioDevice> &devices);
  bool selectDeviceBySinkName(const QString &sinkName);
  std::string currentDeviceId() const;
  QString currentSinkName() const;
  QComboBox *comboBox() const;

private:
  QComboBox *deviceComboBox = nullptr;
  QLabel *emptyLabel = nullptr;
};
