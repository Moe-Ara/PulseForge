#include "EqualizerPanel.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QObject>
#include <QSlider>
#include <QStringList>
#include <algorithm>
#include <array>
#include <utility>

EqualizerPanel::EqualizerPanel(QWidget *parent)
    : CardContainer("Equalizer", "Subtle tuning for clarity and warmth.",
                    parent) {
  auto *equalizer = new QWidget(this);
  equalizer->setObjectName("equalizerPanel");

  auto *layout = new QGridLayout(equalizer);
  layout->setContentsMargins(14, 12, 14, 12);
  layout->setHorizontalSpacing(10);
  layout->setVerticalSpacing(8);

  const QStringList gainScale = {"+12", "+6", "0", "-6", "-12"};
  for (int i = 0; i < gainScale.size(); ++i) {
    auto *scaleLabel = new QLabel(gainScale.at(i), this);
    scaleLabel->setObjectName(i == 2 ? "zeroDbLabel" : "dbLabel");
    scaleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    layout->addWidget(scaleLabel, i, 0);
  }

  const QStringList bandNames = {"110", "220", "400", "750", "1.5k",
                                 "3k",  "6k",  "12k", "16k"};
  constexpr std::array<int, 9> bandValues = {2, 1, -1, 2, 5, 6, 4, 3, 2};
  currentGains.reserve(bandValues.size());

  for (int i = 0; i < bandNames.size(); ++i) {
    const int bandValue = bandValues.at(static_cast<std::size_t>(i));
    currentGains.push_back(static_cast<float>(bandValue));

    auto *gainLabel = new QLabel(QString("%1").arg(bandValue), this);
    gainLabel->setObjectName("gainLabel");
    gainLabel->setAlignment(Qt::AlignCenter);

    auto *slider = new QSlider(Qt::Vertical, this);
    slider->setObjectName("eqSlider");
    slider->setRange(-12, 12);
    slider->setValue(bandValue);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(6);
    slider->setFixedHeight(150);
    sliders.push_back(slider);

    auto *bandLabel = new QLabel(bandNames.at(i), this);
    bandLabel->setObjectName("bandLabel");
    bandLabel->setAlignment(Qt::AlignCenter);

    QObject::connect(slider, &QSlider::valueChanged, gainLabel,
                     [this, gainLabel, index = i](int value) {
                       gainLabel->setText(QString("%1").arg(value));
                       currentGains.at(index) = static_cast<float>(value);
                       if (gainsChangedHandler) {
                         gainsChangedHandler(currentGains);
                       }
                     });

    layout->addWidget(gainLabel, 0, i + 1);
    layout->addWidget(slider, 1, i + 1, 3, 1, Qt::AlignHCenter);
    layout->addWidget(bandLabel, 4, i + 1);
  }

  contentLayout()->addWidget(equalizer);
}

std::vector<float> EqualizerPanel::gains() const {
  return currentGains;
}

void EqualizerPanel::setGains(const std::vector<float> &gains) {
  const std::size_t count = std::min(gains.size(), sliders.size());
  for (std::size_t i = 0; i < count; ++i) {
    sliders.at(i)->setValue(static_cast<int>(gains.at(i)));
  }
}

void EqualizerPanel::setGainsChangedHandler(
    std::function<void(const std::vector<float> &)> handler) {
  gainsChangedHandler = std::move(handler);
}
