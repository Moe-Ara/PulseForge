#include "MainWindow.hpp"

#include "components/CardContainer.hpp"
#include "components/DeviceSelector.hpp"
#include "components/EnhancementToggle.hpp"
#include "components/EffectControls.hpp"
#include "components/EqualizerPanel.hpp"
#include "components/PresetSelector.hpp"
#include "components/StatusIndicator.hpp"

#include "../presets/PresetFactory.hpp"
#include "../core/AppConfig.hpp"
#include "../dsp/DspConfig.hpp"

#include <QComboBox>
#include <QFrame>
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
#include <algorithm>
#include <string>
#include <thread>
#include <vector>

namespace {

QSettings appSettings() {
  return QSettings(QString::fromUtf8(AppConfig::organizationName.data()),
                   QString::fromUtf8(AppConfig::applicationName.data()));
}

QVariantList floatsToVariantList(const std::vector<float> &floats) {
  QVariantList values;
  for (float value : floats) {
    values.push_back(value);
  }
  return values;
}

std::vector<float> floatsFromVariantList(const QVariantList &values) {
  std::vector<float> floats;
  floats.reserve(values.size());
  for (const auto &value : values) {
    floats.push_back(value.toFloat());
  }
  return floats;
}

QVariantList intsToVariantList(const std::vector<int> &ints) {
  QVariantList values;
  for (int value : ints) {
    values.push_back(value);
  }
  return values;
}

std::vector<int> intsFromVariantList(const QVariantList &values) {
  std::vector<int> ints;
  ints.reserve(values.size());
  for (const auto &value : values) {
    ints.push_back(value.toInt());
  }
  return ints;
}

std::vector<float> opennessGains() {
  return {0.4f, 0.2f, -1.0f, -0.5f, 0.5f,
          1.8f, 2.6f, 2.4f, 1.4f};
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
  pageLayout->setContentsMargins(0, 0, 0, 0);
  pageLayout->setSpacing(0);

  auto *headerLayout = new QHBoxLayout();
  headerLayout->setContentsMargins(28, 22, 28, 22);
  headerLayout->setSpacing(10);

  auto *brandMark = new QLabel("▮▮▮", this);
  brandMark->setObjectName("brandMark");
  brandMark->setAlignment(Qt::AlignCenter);

  auto *titleLabel =
      new QLabel(QString::fromUtf8(AppConfig::applicationName.data()), this);
  titleLabel->setObjectName("appTitle");

  headerLayout->addWidget(brandMark);
  headerLayout->addWidget(titleLabel);
  headerLayout->addStretch();

  auto *closeLabel = new QLabel("×", this);
  closeLabel->setObjectName("windowCloseGlyph");
  closeLabel->setAlignment(Qt::AlignCenter);
  headerLayout->addWidget(closeLabel);

  auto *divider = new QFrame(this);
  divider->setObjectName("headerDivider");
  divider->setFrameShape(QFrame::HLine);

  auto *mainPanel = new QWidget(this);
  mainPanel->setObjectName("mainPanel");
  auto *mainPanelLayout = new QVBoxLayout(mainPanel);
  mainPanelLayout->setContentsMargins(30, 28, 30, 34);
  mainPanelLayout->setSpacing(22);

  auto *topControls = new QHBoxLayout();
  topControls->setSpacing(14);

  presetSelector = new PresetSelector(this);
  loadPresets();
  deviceSelector = new DeviceSelector(this);

  topControls->addWidget(presetSelector, 1);
  topControls->addWidget(deviceSelector, 1);

  statusIndicator = new StatusIndicator(this);
  statusIndicator->setMessage("Ready. Enhancement is currently bypassed.");

  auto *soundSurface = new QWidget(this);
  soundSurface->setObjectName("soundSurface");
  auto *soundSurfaceLayout = new QHBoxLayout(soundSurface);
  soundSurfaceLayout->setContentsMargins(22, 22, 22, 22);
  soundSurfaceLayout->setSpacing(22);

  effectControls = new EffectControls(this);
  effectControls->setValues(PresetFactory::defaultEffectValues());
  equalizerPanel = new EqualizerPanel(this);

  soundSurfaceLayout->addWidget(effectControls, 1);
  soundSurfaceLayout->addWidget(equalizerPanel, 4);

  enhancementToggle = new EnhancementToggle(this);

  pageLayout->addLayout(headerLayout);
  pageLayout->addWidget(divider);
  mainPanelLayout->addLayout(topControls);
  mainPanelLayout->addWidget(soundSurface, 1);
  mainPanelLayout->addWidget(enhancementToggle);
  mainPanelLayout->addWidget(statusIndicator);
  pageLayout->addWidget(mainPanel, 1);

  setCentralWidget(central);
  setWindowTitle(QString::fromUtf8(AppConfig::applicationName.data()));
  setMinimumSize(1360, 760);
  resize(1500, 840);
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
              equalizerPanel->setFrequencies(preset.frequencies);
              equalizerPanel->setGains(preset.gains);
              effectControls->setValues(preset.effectValues);
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

  effectControls->setValuesChangedHandler([this]() {
    customCurveActive = true;
    const bool previousSignalsBlocked =
        presetSelector->comboBox()->blockSignals(true);
    selectPresetById("custom-eq");
    presetSelector->comboBox()->blockSignals(previousSignalsBlocked);
    applyCurrentEqualizerCurveLive();
    saveSettings();
    statusIndicator->setMessage(audioService.isEnabled()
                                    ? "Enhancement controls updated."
                                    : "Enhancement controls ready.");
  });

  connect(presetSelector->saveButton(), &QPushButton::clicked, this, [this]() {
    bool accepted = false;
    const QString presetName = QInputDialog::getText(
        this, "Save Preset", "Preset name", QLineEdit::Normal, {}, &accepted);

    if (!accepted || presetName.trimmed().isEmpty()) {
      return;
    }

    if (!presetStore.savePreset(presetName, equalizerPanel->gains(),
                                equalizerPanel->frequencies(),
                                effectControls->values())) {
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

    const std::vector<float> gains = combinedEqualizerGains();
    const std::vector<float> frequencies = equalizerPanel->frequencies();
    const std::vector<int> effectValues = effectControls->values();
    const float preampDb = effectControls->preampDb();
    const float limiterCeilingDb = effectControls->limiterCeilingDb();
    statusIndicator->setMessage("Enabling enhancement...");

    std::thread([self, service, gains, frequencies, effectValues, preampDb,
                 limiterCeilingDb]() {
      service->applyPreset(PresetFactory::equalizer(
          gains, frequencies, effectValues, preampDb, limiterCeilingDb));
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
  const QVariantList savedFrequencies =
      settings.value("presets/customEqFrequencies").toList();
  const QVariantList savedEffectValues =
      settings.value("presets/customEffectValues").toList();
  const bool previousPresetSignalsBlocked =
      presetSelector->comboBox()->blockSignals(true);

  if (presetId == "custom-eq" &&
      savedGains.size() == static_cast<int>(DspConfig::eqBandCount)) {
    customCurveActive = true;
    selectPresetById("custom-eq");
    if (savedFrequencies.size() == static_cast<int>(DspConfig::eqBandCount)) {
      equalizerPanel->setFrequencies(floatsFromVariantList(savedFrequencies));
    }
    if (savedEffectValues.size() >= 5) {
      effectControls->setValues(intsFromVariantList(savedEffectValues));
    } else {
      effectControls->setValues(PresetFactory::defaultEffectValues());
    }
    equalizerPanel->setGains(floatsFromVariantList(savedGains));
    applyCurrentEqualizerCurveLive();
  } else if (selectPresetById(presetId)) {
    if (const auto preset = PresetFactory::presetById(presetId.toStdString())) {
      customCurveActive = false;
      applyPresetLive(*preset, true);
    } else {
      SavedPreset savedPreset;
      if (presetStore.loadPreset(presetId, savedPreset)) {
        customCurveActive = true;
        equalizerPanel->setFrequencies(savedPreset.frequencies);
        equalizerPanel->setGains(savedPreset.gains);
        effectControls->setValues(savedPreset.effectValues);
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
                    floatsToVariantList(equalizerPanel->gains()));
  settings.setValue("presets/customEqFrequencies",
                    floatsToVariantList(equalizerPanel->frequencies()));
  settings.setValue("presets/customEffectValues",
                    intsToVariantList(effectControls->values()));
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
    equalizerPanel->setFrequencies(PresetFactory::defaultFrequencies());
    equalizerPanel->setGains(PresetFactory::gainsForPreset(preset));
    effectControls->setValues(PresetFactory::effectValuesForPreset(preset));
  }
  audioService.applyPreset(PresetFactory::equalizer(
      combinedEqualizerGains(), equalizerPanel->frequencies(),
      effectControls->values(), effectControls->preampDb(),
      effectControls->limiterCeilingDb()));
}

void MainWindow::applyCurrentEqualizerCurveLive() {
  audioService.applyPreset(PresetFactory::equalizer(
      combinedEqualizerGains(), equalizerPanel->frequencies(),
      effectControls->values(), effectControls->preampDb(),
      effectControls->limiterCeilingDb()));
}

std::vector<float> MainWindow::combinedEqualizerGains() const {
  std::vector<float> gains = equalizerPanel->gains();

  if (effectControls) {
    const std::vector<float> tonalGains = effectControls->tonalGains();
    const std::size_t count = std::min(gains.size(), tonalGains.size());
    for (std::size_t i = 0; i < count; ++i) {
      gains.at(i) += tonalGains.at(i);
    }
  }

  const std::vector<float> openGains = opennessGains();
  const std::size_t openCount = std::min(gains.size(), openGains.size());
  for (std::size_t i = 0; i < openCount; ++i) {
    gains.at(i) += openGains.at(i);
  }
  return gains;
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
