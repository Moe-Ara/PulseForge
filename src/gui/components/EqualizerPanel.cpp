#include "EqualizerPanel.hpp"

#include <QGridLayout>
#include <QLabel>
#include <QList>
#include <QObject>
#include <QSlider>
#include <QStringList>

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
  const QList<int> bandValues = {2, 1, -1, 2, 5, 6, 4, 3, 2};

  for (int i = 0; i < bandNames.size(); ++i) {
    auto *gainLabel = new QLabel(QString("%1").arg(bandValues.at(i)), this);
    gainLabel->setObjectName("gainLabel");
    gainLabel->setAlignment(Qt::AlignCenter);

    auto *slider = new QSlider(Qt::Vertical, this);
    slider->setObjectName("eqSlider");
    slider->setRange(-12, 12);
    slider->setValue(bandValues.at(i));
    slider->setTickPosition(QSlider::TicksBothSides);
    slider->setTickInterval(6);
    slider->setFixedHeight(150);

    auto *bandLabel = new QLabel(bandNames.at(i), this);
    bandLabel->setObjectName("bandLabel");
    bandLabel->setAlignment(Qt::AlignCenter);

    QObject::connect(slider, &QSlider::valueChanged, gainLabel,
                     [gainLabel](int value) {
                       gainLabel->setText(QString("%1").arg(value));
                     });

    layout->addWidget(gainLabel, 0, i + 1);
    layout->addWidget(slider, 1, i + 1, 3, 1, Qt::AlignHCenter);
    layout->addWidget(bandLabel, 4, i + 1);
  }

  contentLayout()->addWidget(equalizer);
}
