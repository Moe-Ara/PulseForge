#include "EffectControls.hpp"

#include <QLabel>
#include <QSlider>
#include <QVBoxLayout>
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

  addControl("Fidelity", 28);
  addControl("Ambience", 24);
  addControl("3D surround", 24);
  addControl("Dynamic boost", 26);
  addControl("Bass", 42);
  layout->addStretch();
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
      bass * 2.6f,
      bass * 1.8f,
      bass * 0.7f - surround * 0.3f,
      ambience * 0.7f - surround * 0.2f,
      ambience * 0.8f + dynamicBoost * 0.4f,
      fidelity * 1.1f + dynamicBoost * 0.4f,
      fidelity * 1.4f + surround * 0.8f,
      fidelity * 0.9f + surround * 1.1f,
      fidelity * 0.4f + surround * 0.7f,
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
