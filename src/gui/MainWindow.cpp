#include "MainWindow.hpp"

#include "components/CardContainer.hpp"
#include "components/DeviceSelector.hpp"
#include "components/EnhancementToggle.hpp"
#include "components/EqualizerPanel.hpp"
#include "components/PresetSelector.hpp"
#include "components/StatusIndicator.hpp"

#include "../presets/PresetFactory.hpp"
#include "../core/AppConfig.hpp"
#include "../dsp/DspConfig.hpp"

#include <QComboBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QMessageBox>
#include <QPointer>
#include <QPushButton>
#include <QSettings>
#include <QVariantList>
#include <QVBoxLayout>
#include <QWidget>
#include <string>
#include <thread>
#include <vector>

namespace {

QSettings appSettings() {
  return QSettings(QString::fromUtf8(AppConfig::organizationName.data()),
                   QString::fromUtf8(AppConfig::applicationName.data()));
}

QVariantList gainsToVariantList(const std::vector<float> &gains) {
  QVariantList values;
  for (float gain : gains) {
    values.push_back(gain);
  }
  return values;
}

std::vector<float> gainsFromVariantList(const QVariantList &values) {
  std::vector<float> gains;
  gains.reserve(values.size());
  for (const auto &value : values) {
    gains.push_back(value.toFloat());
  }
  return gains;
}

} // namespace

MainWindow::MainWindow(AudioService &audioService, QWidget *parent)
    : QMainWindow(parent), audioService(audioService) {
  setupUi();
  loadDevices();
  setupConnections();
  restoreSettings();
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

  auto *titleLabel =
      new QLabel(QString::fromUtf8(AppConfig::applicationName.data()), this);
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
  setWindowTitle(QString::fromUtf8(AppConfig::applicationName.data()));
  setMinimumSize(1040, 620);
  resize(1180, 680);
}

void MainWindow::loadDevices() {
  deviceSelector->setDevices(audioService.getOutputDevices());
}

void MainWindow::loadPresets() {
  const bool previousSignalsBlocked =
      presetSelector->comboBox()->blockSignals(true);
  presetSelector->clearPresets();
  for (const auto &preset : PresetFactory::builtInPresets()) {
    presetSelector->addPreset(QString::fromStdString(preset.name),
                              QString::fromStdString(preset.id));
  }
  presetSelector->addPreset("Custom EQ", "custom-eq");

  for (const auto &preset : presetStore.loadPresets()) {
    presetSelector->addPreset(preset.name, preset.id);
  }
  presetSelector->comboBox()->blockSignals(previousSignalsBlocked);
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
            saveSettings();
            statusIndicator->setMessage("Output device updated.");
          });

  connect(presetSelector->comboBox(),
          QOverload<int>::of(&QComboBox::currentIndexChanged), this,
          [this]() {
            const QString presetId =
                presetSelector->comboBox()->currentData().toString();
            const std::string builtInId = presetId.toStdString();

            if (const auto preset = PresetFactory::presetById(builtInId)) {
              customCurveActive = false;
              applyPresetLive(*preset, true);
              saveSettings();
              statusIndicator->setMessage(
                  QString::fromStdString(preset->name) + " preset selected.");
              return;
            }

            if (presetId == "custom-eq") {
              customCurveActive = true;
              applyCurrentEqualizerCurveLive();
              saveSettings();
              statusIndicator->setMessage("Custom EQ selected.");
              return;
            }

            SavedPreset preset;
            if (presetStore.loadPreset(presetId, preset)) {
              customCurveActive = true;
              equalizerPanel->setGains(preset.gains);
              applyCurrentEqualizerCurveLive();
              saveSettings();
              statusIndicator->setMessage("Preset loaded: " + preset.name);
            }
          });

  equalizerPanel->setGainsChangedHandler(
      [this](const std::vector<float> &gains) {
        customCurveActive = true;
        const bool previousSignalsBlocked =
            presetSelector->comboBox()->blockSignals(true);
        selectPresetById("custom-eq");
        presetSelector->comboBox()->blockSignals(previousSignalsBlocked);
        applyCurrentEqualizerCurveLive();
        saveSettings();
        statusIndicator->setMessage(audioService.isEnabled()
                                        ? "Live EQ updated."
                                        : "Equalizer curve updated.");
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
    enhancementToggle->button()->setEnabled(false);
    QPointer<MainWindow> self(this);
    AudioService *service = &audioService;

    if (audioService.isEnabled()) {
      std::thread([self, service]() {
        const bool disabled = service->disableEnhancement();
        if (!self) {
          return;
        }
        QMetaObject::invokeMethod(self, [self, disabled]() {
          if (!self) {
            return;
          }
          self->enhancementToggle->button()->setEnabled(true);
          if (disabled) {
            self->saveEnhancementEnabled(false);
            self->setEnhancementActive(
                false, "Enhancement disabled. Audio is bypassed.");
          } else {
            self->statusIndicator->setMessage(
                "PulseForge could not disable enhancement.");
          }
        });
      }).detach();
      return;
    }

    const std::vector<float> gains = equalizerPanel->gains();
    const std::string presetId =
        presetSelector->comboBox()->currentData().toString().toStdString();
    const bool useCustomCurve = customCurveActive;
    statusIndicator->setMessage("Enabling enhancement...");

    std::thread([self, service, gains, presetId, useCustomCurve]() {
      const auto preset = PresetFactory::presetById(presetId);
      if (!useCustomCurve && preset) {
        service->applyPreset(*preset);
      } else {
        service->applyPreset(PresetFactory::equalizer(gains));
      }
      const bool enabled = service->enableEnhancement();
      if (!self) {
        return;
      }
      QMetaObject::invokeMethod(self, [self, enabled]() {
        if (!self) {
          return;
        }
        self->enhancementToggle->button()->setEnabled(true);
        if (enabled) {
          self->saveEnhancementEnabled(true);
          self->setEnhancementActive(
              true, "Enhancement enabled. PulseForge is processing audio.");
        } else {
          self->saveEnhancementEnabled(false);
          self->setEnhancementActive(false,
                                     "PulseForge could not enable enhancement.");
        }
      });
    }).detach();
  });
}

void MainWindow::restoreSettings() {
  restoringSettings = true;
  QSettings settings = appSettings();

  const QString savedSink = settings.value("audio/outputSinkName").toString();
  const bool previousDeviceSignalsBlocked =
      deviceSelector->comboBox()->blockSignals(true);
  if (deviceSelector->selectDeviceBySinkName(savedSink)) {
    audioService.selectOutputDevice(deviceSelector->currentDeviceId());
  }
  deviceSelector->comboBox()->blockSignals(previousDeviceSignalsBlocked);

  const QString presetId =
      settings.value("presets/lastPresetId", "flat").toString();
  const QVariantList savedGains =
      settings.value("presets/customEqGains").toList();
  const bool previousPresetSignalsBlocked =
      presetSelector->comboBox()->blockSignals(true);

  if (presetId == "custom-eq" &&
      savedGains.size() == static_cast<int>(DspConfig::eqBandCount)) {
    customCurveActive = true;
    selectPresetById("custom-eq");
    equalizerPanel->setGains(gainsFromVariantList(savedGains));
    applyCurrentEqualizerCurveLive();
  } else if (selectPresetById(presetId)) {
    if (const auto preset = PresetFactory::presetById(presetId.toStdString())) {
      customCurveActive = false;
      applyPresetLive(*preset, true);
    } else {
      SavedPreset savedPreset;
      if (presetStore.loadPreset(presetId, savedPreset)) {
        customCurveActive = true;
        equalizerPanel->setGains(savedPreset.gains);
        applyCurrentEqualizerCurveLive();
      }
    }
  } else if (const auto preset = PresetFactory::presetById("flat")) {
    selectPresetById("flat");
    customCurveActive = false;
    applyPresetLive(*preset, true);
  }
  presetSelector->comboBox()->blockSignals(previousPresetSignalsBlocked);

  restoringSettings = false;

  if (settings.value("audio/enhancementEnabled", false).toBool()) {
    enhancementToggle->button()->setEnabled(false);
    QPointer<MainWindow> self(this);
    AudioService *service = &audioService;
    std::thread([self, service]() {
      const bool enabled = service->enableEnhancement();
      if (!self) {
        return;
      }
      QMetaObject::invokeMethod(self, [self, enabled]() {
        if (!self) {
          return;
        }
        self->enhancementToggle->button()->setEnabled(true);
        self->saveEnhancementEnabled(enabled);
        self->setEnhancementActive(
            enabled, enabled ? "Enhancement restored from last session."
                             : "PulseForge could not restore enhancement.");
      });
    }).detach();
  } else {
    statusIndicator->setMessage("Ready. Enhancement is currently bypassed.");
  }
}

void MainWindow::saveSettings() const {
  if (restoringSettings) {
    return;
  }

  QSettings settings = appSettings();
  settings.setValue("audio/outputSinkName", deviceSelector->currentSinkName());
  settings.setValue("presets/lastPresetId",
                    customCurveActive
                        ? QString("custom-eq")
                        : presetSelector->comboBox()->currentData().toString());
  settings.setValue("presets/customEqGains",
                    gainsToVariantList(equalizerPanel->gains()));
  settings.sync();
}

void MainWindow::saveEnhancementEnabled(bool enabled) const {
  QSettings settings = appSettings();
  settings.setValue("audio/enhancementEnabled", enabled);
  settings.sync();
}

void MainWindow::setEnhancementActive(bool active, const QString &message) {
  enhancementToggle->setEnabledState(active);
  statusIndicator->setActive(active);
  statusIndicator->setMessage(message);
}

void MainWindow::applyPresetLive(const Preset &preset,
                                 bool updateEqualizerSliders) {
  if (updateEqualizerSliders) {
    equalizerPanel->setGains(PresetFactory::gainsForPreset(preset));
  }
  audioService.applyPreset(preset);
}

void MainWindow::applyCurrentEqualizerCurveLive() {
  audioService.applyPreset(PresetFactory::equalizer(equalizerPanel->gains()));
}

bool MainWindow::selectPresetById(const QString &presetId) {
  for (int i = 0; i < presetSelector->comboBox()->count(); ++i) {
    if (presetSelector->comboBox()->itemData(i).toString() == presetId) {
      presetSelector->comboBox()->setCurrentIndex(i);
      return true;
    }
  }
  return false;
}
