#include "EffectControls.hpp"

#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <utility>

namespace {

constexpr int kSliderMinimum = 0;
constexpr int kSliderMaximum = 100;

float normalizedValue(const QSlider *slider) {
  return static_cast<float>(slider->value() - kSliderMinimum) /
         static_cast<float>(kSliderMaximum - kSliderMinimum);
}

} // namespace

EffectControls::EffectControls(QWidget *parent) : QWidget(parent) {
  setObjectName("effectsPanel");

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(26, 22, 26, 22);
  layout->setSpacing(13);

  addControl("Fidelity", 0);
  addControl("Ambience", 0);
  addControl("3D surround", 0);
  addControl("Dynamic boost", 0);
  addControl("Bass", 0);
  layout->addStretch();
}

std::vector<int> EffectControls::values() const {
  std::vector<int> currentValues;
  currentValues.reserve(sliders.size());
  for (const auto *slider : sliders) {
    currentValues.push_back(slider->value());
  }
  return currentValues;
}

void EffectControls::setValues(const std::vector<int> &values) {
  const std::size_t count = std::min(values.size(), sliders.size());
  suppressNotifications = true;
  for (std::size_t i = 0; i < count; ++i) {
    sliders.at(i)->setValue(std::clamp(values.at(i), kSliderMinimum,
                                       kSliderMaximum));
  }
  suppressNotifications = false;
}

std::vector<float> EffectControls::tonalGains() const {
  if (sliders.size() < 5) {
    return std::vector<float>(9, 0.0f);
  }

  const float fidelity = normalizedValue(sliders.at(0));
  const float ambience = normalizedValue(sliders.at(1));
  const float surround = normalizedValue(sliders.at(2));
  const float dynamicBoost = normalizedValue(sliders.at(3));
  const float bass = normalizedValue(sliders.at(4));

  return {
      bass * 5.0f,
      bass * 3.5f,
      bass * 1.5f - surround * 1.0f,
      ambience * 1.8f - surround * 0.6f,
      ambience * 2.0f + dynamicBoost * 1.2f,
      fidelity * 2.4f + dynamicBoost * 1.4f,
      fidelity * 3.2f + surround * 2.0f,
      fidelity * 2.4f + surround * 2.8f,
      fidelity * 1.4f + surround * 1.8f,
  };
}

void EffectControls::setValuesChangedHandler(std::function<void()> handler) {
  valuesChangedHandler = std::move(handler);
}

void EffectControls::addControl(const QString &name, int value) {
  auto *label = new QLabel(name, this);
  label->setObjectName("effectLabel");

  auto *slider = new QSlider(Qt::Horizontal, this);
  slider->setObjectName("effectSlider");
  slider->setRange(kSliderMinimum, kSliderMaximum);
  slider->setValue(value);
  slider->setFixedHeight(22);
  sliders.push_back(slider);

  connect(slider, &QSlider::valueChanged, this, [this]() {
    if (valuesChangedHandler && !suppressNotifications) {
      valuesChangedHandler();
    }
  });

  layout()->addWidget(label);
  layout()->addWidget(slider);
}
