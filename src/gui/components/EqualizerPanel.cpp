#include "EqualizerPanel.hpp"

#include "dsp/DspConfig.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QObject>
#include <QSlider>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {

QString bandLabelFor(float frequency) {
  if (frequency >= 1000.0f) {
    const float kilohertz = frequency / 1000.0f;
    if (std::abs(kilohertz - static_cast<int>(kilohertz)) < 0.01f) {
      return QString("%1k").arg(static_cast<int>(kilohertz));
    }
    return QString("%1k").arg(kilohertz, 0, 'f', 1);
  }

  return QString::number(static_cast<int>(frequency));
}

constexpr int kSliderMinimumDb = -12;
constexpr int kSliderMaximumDb = 12;
constexpr int kSliderTickIntervalDb = 6;
constexpr int kSliderHeight = 150;

} // namespace

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

  currentGains.reserve(DspConfig::eqBandCount);

  for (std::size_t band = 0; band < DspConfig::eqFrequencies.size(); ++band) {
    const int i = static_cast<int>(band);
    const int bandValue = 0;
    currentGains.push_back(static_cast<float>(bandValue));

    auto *gainLabel = new QLabel(QString("%1").arg(bandValue), this);
    gainLabel->setObjectName("gainLabel");
    gainLabel->setAlignment(Qt::AlignCenter);

    auto *slider = new QSlider(Qt::Vertical, this);
    slider->setObjectName("eqSlider");
    slider->setRange(kSliderMinimumDb, kSliderMaximumDb);
    slider->setValue(bandValue);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(kSliderTickIntervalDb);
    slider->setFixedHeight(kSliderHeight);
    sliders.push_back(slider);

    auto *bandLabel =
        new QLabel(bandLabelFor(DspConfig::eqFrequencies.at(band)), this);
    bandLabel->setObjectName("bandLabel");
    bandLabel->setAlignment(Qt::AlignCenter);

    QObject::connect(slider, &QSlider::valueChanged, gainLabel,
                     [this, gainLabel, index = i](int value) {
                       gainLabel->setText(QString("%1").arg(value));
                       currentGains.at(index) = static_cast<float>(value);
                       if (gainsChangedHandler &&
                           !suppressChangeNotifications) {
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
  suppressChangeNotifications = true;
  for (std::size_t i = 0; i < count; ++i) {
    sliders.at(i)->setValue(static_cast<int>(gains.at(i)));
  }
  suppressChangeNotifications = false;
}

void EqualizerPanel::setGainsChangedHandler(
    std::function<void(const std::vector<float> &)> handler) {
  gainsChangedHandler = std::move(handler);
}
