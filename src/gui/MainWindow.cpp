#include "MainWindow.hpp"

#include "components/CardContainer.hpp"
#include "components/DeviceSelector.hpp"
#include "components/EnhancementToggle.hpp"
#include "components/EqualizerPanel.hpp"
#include "components/PresetSelector.hpp"
#include "components/StatusIndicator.hpp"

#include "../presets/PresetFactory.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>
#include <vector>

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
  loadPresets();
  presetCard->contentLayout()->addWidget(presetSelector);

  statusIndicator = new StatusIndicator(this);
  statusIndicator->setMessage("Ready. Enhancement is currently bypassed.");

  sideColumn->addWidget(deviceCard);
  sideColumn->addWidget(presetCard);
  sideColumn->addStretch();

  auto *mainColumn = new QVBoxLayout();
  mainColumn->setSpacing(16);

  enhancementToggle = new EnhancementToggle(this);
  equalizerPanel = new EqualizerPanel(this);

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
  setMinimumSize(1040, 620);
  resize(1180, 680);
}

void MainWindow::loadDevices() {
  deviceSelector->setDevices(audioService.getOutputDevices());
}

void MainWindow::loadPresets() {
  presetSelector->clearPresets();
  presetSelector->addPreset("Gaming clarity", "gaming");

  for (const auto &preset : presetStore.loadPresets()) {
    presetSelector->addPreset(preset.name, preset.id);
  }
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
            const QString presetId =
                presetSelector->comboBox()->currentData().toString();

            if (presetId == "gaming") {
              audioService.applyPreset(PresetFactory::gaming());
              statusIndicator->setMessage("Gaming clarity preset selected.");
              return;
            }

            SavedPreset preset;
            if (presetStore.loadPreset(presetId, preset)) {
              equalizerPanel->setGains(preset.gains);
              audioService.applyPreset(PresetFactory::equalizer(preset.gains));
              statusIndicator->setMessage("Preset loaded: " + preset.name);
            }
          });

  equalizerPanel->setGainsChangedHandler(
      [this](const std::vector<float> &gains) {
        audioService.applyPreset(PresetFactory::equalizer(gains));
        statusIndicator->setMessage("Equalizer curve updated.");
      });

  connect(presetSelector->saveButton(), &QPushButton::clicked, this, [this]() {
    bool accepted = false;
    const QString presetName = QInputDialog::getText(
        this, "Save Preset", "Preset name", QLineEdit::Normal, {}, &accepted);

    if (!accepted || presetName.trimmed().isEmpty()) {
      return;
    }

    if (!presetStore.savePreset(presetName, equalizerPanel->gains())) {
      QMessageBox::warning(this, "Preset Not Saved",
                           "PulseForge could not save this preset.");
      return;
    }

    loadPresets();
    statusIndicator->setMessage("Preset saved: " + presetName.trimmed());
  });

  connect(enhancementToggle->button(), &QPushButton::clicked, this, [this]() {
    if (audioService.isEnabled()) {
      if (audioService.disableEnhancement()) {
        setEnhancementActive(false, "Enhancement disabled. Audio is bypassed.");
      }
      return;
    }

    audioService.applyPreset(PresetFactory::equalizer(equalizerPanel->gains()));

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
