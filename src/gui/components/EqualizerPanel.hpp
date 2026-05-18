#pragma once

#include "CardContainer.hpp"

#include <functional>
#include <vector>

class QSlider;
class QSpinBox;
class QGridLayout;

class EqualizerPanel : public CardContainer {
public:
  explicit EqualizerPanel(QWidget *parent = nullptr);

  std::vector<float> gains() const;
  std::vector<float> frequencies() const;
  void setGains(const std::vector<float> &gains);
  void setFrequencies(const std::vector<float> &frequencies);
  void setGainsChangedHandler(
      std::function<void(const std::vector<float> &)> handler);
  void setCompactMode(bool compact);

private:
  std::vector<float> currentGains;
  std::vector<float> currentFrequencies;
  std::vector<QSlider *> sliders;
  std::vector<QSpinBox *> frequencyInputs;
  std::function<void(const std::vector<float> &)> gainsChangedHandler;
  QWidget *equalizerBody = nullptr;
  QGridLayout *equalizerLayout = nullptr;
  bool suppressChangeNotifications = false;
  bool compactMode = false;
};
