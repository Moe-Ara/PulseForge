#pragma once

#include <QString>
#include <QWidget>

#include <functional>
#include <vector>

class QLabel;
class QSlider;

class EffectControls : public QWidget {
public:
  explicit EffectControls(QWidget *parent = nullptr);

  std::vector<int> values() const;
  void setValues(const std::vector<int> &values);
  std::vector<float> tonalGains() const;
  float preampDb() const;
  float limiterCeilingDb() const;
  void setValuesChangedHandler(std::function<void()> handler);
  void setCompactMode(bool compact);

private:
  void addControl(const QString &name, int value);
  void updateControlLabel(std::size_t index);

  std::vector<QString> controlNames;
  std::vector<QLabel *> labels;
  std::vector<QSlider *> sliders;
  std::function<void()> valuesChangedHandler;
  bool suppressNotifications = false;
  bool compactMode = false;
};
