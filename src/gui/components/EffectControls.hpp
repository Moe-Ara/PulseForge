#pragma once

#include <QWidget>

#include <functional>
#include <vector>

class QSlider;

class EffectControls : public QWidget {
public:
  explicit EffectControls(QWidget *parent = nullptr);

  std::vector<float> tonalGains() const;
  void setValuesChangedHandler(std::function<void()> handler);

private:
  void addControl(const QString &name, int value);

  std::vector<QSlider *> sliders;
  std::function<void()> valuesChangedHandler;
  bool suppressNotifications = false;
};
