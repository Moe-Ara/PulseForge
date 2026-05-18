#include "EffectControls.hpp"

#include <QLabel>
#include <QSizePolicy>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>
#include <cstddef>
#include <iterator>
#include <utility>

namespace {

constexpr int kSliderMinimum = 0;
constexpr int kSliderMaximum = 100;
constexpr std::size_t kPreampIndex = 5;
constexpr std::size_t kLimiterIndex = 6;

float normalizedValue(const QSlider *slider) {
  return static_cast<float>(slider->value() - kSliderMinimum) /
         static_cast<float>(kSliderMaximum - kSliderMinimum);
}

float sliderRangeValue(const QSlider *slider, float minimum, float maximum) {
  return minimum + normalizedValue(slider) * (maximum - minimum);
}

} // namespace

EffectControls::EffectControls(QWidget *parent) : QWidget(parent) {
  setObjectName("effectsPanel");
  setMinimumWidth(240);
  setMaximumWidth(300);
  setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(24, 20, 24, 20);
  layout->setSpacing(8);

  addControl("Fidelity", 0);
  addControl("Ambience", 0);
  addControl("3D surround", 0);
  addControl("Dynamic boost", 0);
  addControl("Bass", 0);
  addControl("Preamp", 50);
  addControl("Limiter", 100);
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
    updateControlLabel(i);
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
      bass * 6.5f - fidelity * 0.4f,
      bass * 4.5f - fidelity * 0.3f,
      bass * 1.4f - surround * 1.2f - ambience * 0.8f,
      ambience * 1.8f - surround * 0.8f,
      ambience * 2.2f + dynamicBoost * 0.8f,
      fidelity * 3.0f + dynamicBoost * 1.1f,
      fidelity * 4.0f + surround * 2.2f + dynamicBoost * 1.0f,
      fidelity * 3.4f + surround * 2.8f,
      fidelity * 2.2f + surround * 2.0f,
  };
}

float EffectControls::preampDb() const {
  if (sliders.size() <= kPreampIndex) {
    return 0.0f;
  }
  return sliderRangeValue(sliders.at(kPreampIndex), -8.0f, 4.0f);
}

float EffectControls::limiterCeilingDb() const {
  if (sliders.size() <= kLimiterIndex) {
    return 0.0f;
  }
  return sliderRangeValue(sliders.at(kLimiterIndex), -12.0f, -1.0f);
}

void EffectControls::setValuesChangedHandler(std::function<void()> handler) {
  valuesChangedHandler = std::move(handler);
}

void EffectControls::addControl(const QString &name, int value) {
  auto *label = new QLabel(name, this);
  label->setObjectName("effectLabel");
  label->setWordWrap(false);
  label->setMinimumHeight(20);
  controlNames.push_back(name);
  labels.push_back(label);

  auto *slider = new QSlider(Qt::Horizontal, this);
  slider->setObjectName("effectSlider");
  slider->setRange(kSliderMinimum, kSliderMaximum);
  slider->setValue(value);
  slider->setFixedHeight(20);
  slider->setMinimumWidth(180);
  sliders.push_back(slider);
  updateControlLabel(sliders.size() - 1);

  connect(slider, &QSlider::valueChanged, this, [this]() {
    const auto *changedSlider = qobject_cast<QSlider *>(sender());
    const auto iterator =
        std::find(sliders.begin(), sliders.end(), changedSlider);
    if (iterator != sliders.end()) {
      updateControlLabel(static_cast<std::size_t>(
          std::distance(sliders.begin(), iterator)));
    }
    if (valuesChangedHandler && !suppressNotifications) {
      valuesChangedHandler();
    }
  });

  layout()->addWidget(label);
  layout()->addWidget(slider);
}

void EffectControls::updateControlLabel(std::size_t index) {
  if (index >= labels.size() || index >= controlNames.size()) {
    return;
  }

  if (index == kPreampIndex) {
    labels.at(index)->setText(
        QString("Preamp %1 dB").arg(preampDb(), 0, 'f', 1));
    return;
  }

  if (index == kLimiterIndex) {
    labels.at(index)->setText(
        QString("Limiter %1 dB").arg(limiterCeilingDb(), 0, 'f', 1));
    return;
  }

  labels.at(index)->setText(controlNames.at(index));
}
