#include "EqualizerPanel.hpp"

#include "dsp/DspConfig.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QObject>
#include <QSizePolicy>
#include <QSlider>
#include <QSpinBox>
#include <QStyle>
#include <QStringList>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {

constexpr int kSliderMinimumDb = -12;
constexpr int kSliderMaximumDb = 12;
constexpr int kSliderTickIntervalDb = 6;
constexpr int kSliderHeight = 128;
constexpr int kCompactSliderHeight = 108;
constexpr int kBandColumnWidth = 64;
constexpr int kCompactBandColumnWidth = 48;
constexpr int kEqualizerMinimumWidth = 640;
constexpr int kCompactEqualizerMinimumWidth = 520;

class FrequencySpinBox : public QSpinBox {
public:
  using QSpinBox::QSpinBox;

protected:
  QString textFromValue(int value) const override {
    if (value >= 1000) {
      const float kilohertz = static_cast<float>(value) / 1000.0f;
      if (std::abs(kilohertz - std::round(kilohertz)) < 0.01f) {
        return QString("%1 kHz").arg(static_cast<int>(std::round(kilohertz)));
      }
      return QString("%1 kHz").arg(kilohertz, 0, 'f', 1);
    }
    return QString("%1 Hz").arg(value);
  }

  int valueFromText(const QString &text) const override {
    QString normalized = text.trimmed().toLower();
    const bool kilohertz = normalized.contains("k");
    normalized.remove("khz");
    normalized.remove("hz");
    normalized.remove("k");

    bool ok = false;
    const float value = normalized.trimmed().toFloat(&ok);
    if (!ok) {
      return QSpinBox::valueFromText(text);
    }
    return static_cast<int>(std::round(value * (kilohertz ? 1000.0f : 1.0f)));
  }
};

} // namespace

EqualizerPanel::EqualizerPanel(QWidget *parent)
    : CardContainer("Equalizer", "Subtle tuning for clarity and warmth.",
                    parent) {
  equalizerBody = new QWidget(this);
  equalizerBody->setObjectName("equalizerPanel");
  equalizerBody->setMinimumWidth(kEqualizerMinimumWidth);
  equalizerBody->setSizePolicy(QSizePolicy::MinimumExpanding,
                               QSizePolicy::Expanding);

  equalizerLayout = new QGridLayout(equalizerBody);
  equalizerLayout->setContentsMargins(16, 14, 16, 12);
  equalizerLayout->setHorizontalSpacing(10);
  equalizerLayout->setVerticalSpacing(8);

  const QStringList gainScale = {"+12", "+6", "0", "-6", "-12"};
  for (int i = 0; i < gainScale.size(); ++i) {
    auto *scaleLabel = new QLabel(gainScale.at(i), this);
    scaleLabel->setObjectName(i == 2 ? "zeroDbLabel" : "dbLabel");
    scaleLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    equalizerLayout->addWidget(scaleLabel, i, 0);
  }

  currentGains.reserve(DspConfig::eqBandCount);
  currentFrequencies.reserve(DspConfig::eqBandCount);

  for (std::size_t band = 0; band < DspConfig::eqFrequencies.size(); ++band) {
    const int i = static_cast<int>(band);
    const int bandValue = 0;
    const int frequency = static_cast<int>(DspConfig::eqFrequencies.at(band));
    currentGains.push_back(static_cast<float>(bandValue));
    currentFrequencies.push_back(static_cast<float>(frequency));

    auto *gainLabel = new QLabel(QString("%1").arg(bandValue), this);
    gainLabel->setObjectName("gainLabel");
    gainLabel->setAlignment(Qt::AlignCenter);
    gainLabel->setMinimumWidth(kBandColumnWidth);

    auto *slider = new QSlider(Qt::Vertical, this);
    slider->setObjectName("eqSlider");
    slider->setRange(kSliderMinimumDb, kSliderMaximumDb);
    slider->setValue(bandValue);
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(kSliderTickIntervalDb);
    slider->setFixedHeight(kSliderHeight);
    slider->setMinimumWidth(kBandColumnWidth);
    sliders.push_back(slider);

    auto *frequencyInput = new FrequencySpinBox(this);
    frequencyInput->setObjectName("frequencyInput");
    frequencyInput->setRange(
        static_cast<int>(DspConfig::minEditableEqFrequencyHz),
        static_cast<int>(DspConfig::maxEditableEqFrequencyHz));
    frequencyInput->setSingleStep(10);
    frequencyInput->setAccelerated(true);
    frequencyInput->setKeyboardTracking(false);
    frequencyInput->setAlignment(Qt::AlignCenter);
    frequencyInput->setValue(frequency);
    frequencyInput->setMinimumWidth(kBandColumnWidth);
    frequencyInputs.push_back(frequencyInput);

    QObject::connect(slider, &QSlider::valueChanged, gainLabel,
                     [this, gainLabel, index = i](int value) {
                       gainLabel->setText(QString("%1").arg(value));
                       currentGains.at(index) = static_cast<float>(value);
                       if (gainsChangedHandler &&
                           !suppressChangeNotifications) {
                         gainsChangedHandler(currentGains);
                       }
                     });
    QObject::connect(frequencyInput, QOverload<int>::of(&QSpinBox::valueChanged),
                     this, [this, index = i](int value) {
                       currentFrequencies.at(index) = static_cast<float>(value);
                       if (gainsChangedHandler &&
                           !suppressChangeNotifications) {
                         gainsChangedHandler(currentGains);
                       }
                     });

    equalizerLayout->addWidget(gainLabel, 0, i + 1);
    equalizerLayout->addWidget(slider, 1, i + 1, 3, 1, Qt::AlignHCenter);
    equalizerLayout->addWidget(frequencyInput, 4, i + 1);
    equalizerLayout->setColumnMinimumWidth(i + 1, kBandColumnWidth);
    equalizerLayout->setColumnStretch(i + 1, 1);
  }

  contentLayout()->addWidget(equalizerBody);
}

std::vector<float> EqualizerPanel::gains() const {
  return currentGains;
}

std::vector<float> EqualizerPanel::frequencies() const {
  return currentFrequencies;
}

void EqualizerPanel::setGains(const std::vector<float> &gains) {
  const std::size_t count = std::min(gains.size(), sliders.size());
  suppressChangeNotifications = true;
  for (std::size_t i = 0; i < count; ++i) {
    sliders.at(i)->setValue(static_cast<int>(gains.at(i)));
  }
  suppressChangeNotifications = false;
}

void EqualizerPanel::setFrequencies(const std::vector<float> &frequencies) {
  const std::size_t count = std::min(frequencies.size(), frequencyInputs.size());
  suppressChangeNotifications = true;
  for (std::size_t i = 0; i < count; ++i) {
    frequencyInputs.at(i)->setValue(static_cast<int>(frequencies.at(i)));
  }
  suppressChangeNotifications = false;
}

void EqualizerPanel::setGainsChangedHandler(
    std::function<void(const std::vector<float> &)> handler) {
  gainsChangedHandler = std::move(handler);
}

void EqualizerPanel::setCompactMode(bool compact) {
  if (compactMode == compact) {
    return;
  }

  compactMode = compact;
  const int columnWidth = compact ? kCompactBandColumnWidth : kBandColumnWidth;
  const int sliderHeight = compact ? kCompactSliderHeight : kSliderHeight;

  if (equalizerBody) {
    equalizerBody->setMinimumWidth(compact ? kCompactEqualizerMinimumWidth
                                           : kEqualizerMinimumWidth);
    equalizerBody->setProperty("compact", compact);
    equalizerBody->style()->unpolish(equalizerBody);
    equalizerBody->style()->polish(equalizerBody);
  }

  if (equalizerLayout) {
    equalizerLayout->setContentsMargins(compact ? 12 : 16, compact ? 10 : 14,
                                        compact ? 12 : 16, compact ? 10 : 12);
    equalizerLayout->setHorizontalSpacing(compact ? 6 : 10);
    equalizerLayout->setVerticalSpacing(compact ? 5 : 8);
    for (int column = 1; column <= static_cast<int>(sliders.size()); ++column) {
      equalizerLayout->setColumnMinimumWidth(column, columnWidth);
    }
  }

  for (auto *slider : sliders) {
    slider->setFixedHeight(sliderHeight);
    slider->setMinimumWidth(columnWidth);
  }

  for (auto *frequencyInput : frequencyInputs) {
    frequencyInput->setMinimumWidth(columnWidth);
  }
}
