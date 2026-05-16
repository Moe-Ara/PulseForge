#pragma once

#include "CardContainer.hpp"

#include <functional>
#include <vector>

class QSlider;

class EqualizerPanel : public CardContainer {
public:
  explicit EqualizerPanel(QWidget *parent = nullptr);

  std::vector<float> gains() const;
  void setGains(const std::vector<float> &gains);
  void setGainsChangedHandler(std::function<void(const std::vector<float> &)> handler);

private:
  std::vector<float> currentGains;
  std::vector<QSlider *> sliders;
  std::function<void(const std::vector<float> &)> gainsChangedHandler;
};
