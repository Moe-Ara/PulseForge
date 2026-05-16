#include "MainWindow.hpp"

#include "components/CardContainer.hpp"
#include "components/DeviceSelector.hpp"
#include "components/EnhancementToggle.hpp"
#include "components/EqualizerPanel.hpp"
#include "components/PresetSelector.hpp"
#include "components/StatusIndicator.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(AudioService &audioService, QWidget *parent)
    : QMainWindow(parent), audioService(audioService) {
  setupUi();
  loadDevices();
  setupConnections();
}

void MainWindow::setupUi() {
  auto *central = new QWidget(this);
  central->setObjectName("appRoot");

  auto *pageLayout = new QVBoxLayout(central);
  pageLayout->setContentsMargins(32, 28, 32, 24);
  pageLayout->setSpacing(20);

  auto *headerLayout = new QHBoxLayout();
  headerLayout->setSpacing(16);

  auto *brandMark = new QLabel("PF", this);
  brandMark->setObjectName("brandMark");
  brandMark->setAlignment(Qt::AlignCenter);

  auto *titleStack = new QVBoxLayout();
  titleStack->setSpacing(4);

  auto *titleLabel = new QLabel("PulseForge", this);
  titleLabel->setObjectName("appTitle");

  auto *subtitleLabel =
      new QLabel("Lightweight PipeWire sound enhancement for Linux", this);
  subtitleLabel->setObjectName("appSubtitle");

  titleStack->addWidget(titleLabel);
  titleStack->addWidget(subtitleLabel);

  headerLayout->addWidget(brandMark);
  headerLayout->addLayout(titleStack);
  headerLayout->addStretch();

  auto *contentLayout = new QHBoxLayout();
  contentLayout->setSpacing(20);

  auto *sideColumn = new QVBoxLayout();
  sideColumn->setSpacing(16);

  auto *deviceCard =
      new CardContainer("Output", "Choose where enhanced audio should play.",
                        this);
  deviceSelector = new DeviceSelector(this);
  deviceCard->contentLayout()->addWidget(deviceSelector);

  auto *presetCard =
      new CardContainer("Preset", "Simple profiles without extra clutter.",
                        this);
  presetSelector = new PresetSelector(this);
  presetSelector->addPreset("Gaming clarity", "gaming");
  presetCard->contentLayout()->addWidget(presetSelector);

  statusIndicator = new StatusIndicator(this);
  statusIndicator->setMessage("Ready. Enhancement is currently bypassed.");

  sideColumn->addWidget(deviceCard);
  sideColumn->addWidget(presetCard);
  sideColumn->addStretch();

  auto *mainColumn = new QVBoxLayout();
  mainColumn->setSpacing(16);

  enhancementToggle = new EnhancementToggle(this);
  auto *equalizerPanel = new EqualizerPanel(this);

  mainColumn->addWidget(enhancementToggle);
  mainColumn->addWidget(equalizerPanel);
  mainColumn->addStretch();

  contentLayout->addLayout(sideColumn, 2);
  contentLayout->addLayout(mainColumn, 5);

  pageLayout->addLayout(headerLayout);
  pageLayout->addLayout(contentLayout, 1);
  pageLayout->addWidget(statusIndicator);

  setCentralWidget(central);
  setWindowTitle("PulseForge");
  setMinimumSize(920, 600);
  resize(1040, 640);
}

void MainWindow::loadDevices() {
  deviceSelector->setDevices(audioService.getOutputDevices());
}

void MainWindow::setupConnections() {
  connect(deviceSelector->comboBox(),
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() {
            const auto deviceId = deviceSelector->currentDeviceId();
            if (deviceId.empty()) {
              return;
            }

            audioService.selectOutputDevice(deviceId);
            statusIndicator->setMessage("Output device updated.");
          });

  connect(presetSelector->comboBox(),
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() {
            audioService.applyPreset(createGamingPreset());
            statusIndicator->setMessage("Gaming clarity preset selected.");
          });

  connect(enhancementToggle->button(), &QPushButton::clicked, this, [this]() {
    if (audioService.isEnabled()) {
      if (audioService.disableEnhancement()) {
        setEnhancementActive(false, "Enhancement disabled. Audio is bypassed.");
      }
      return;
    }

    audioService.applyPreset(createGamingPreset());

    if (audioService.enableEnhancement()) {
      setEnhancementActive(
          true, "Enhancement enabled. PulseForge is processing audio.");
    }
  });
}

void MainWindow::setEnhancementActive(bool active, const QString &message) {
  enhancementToggle->setEnabledState(active);
  statusIndicator->setActive(active);
  statusIndicator->setMessage(message);
}

Preset MainWindow::createGamingPreset() const {
  return Preset{"gaming",
                "Gaming",
                "Boost clarity and footsteps.",
                {{{"eq", {{"bass", 1.1f}, {"treble", 1.25f}}},
                  {"compressor", {{"threshold", -18.0f}, {"ratio", 2.0f}}},
                  {"limiter", {{"ceiling", -1.0f}}}}}};
}
