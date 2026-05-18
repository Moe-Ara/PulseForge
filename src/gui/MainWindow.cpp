#include "MainWindow.hpp"

#include "components/CardContainer.hpp"
#include "components/DeviceSelector.hpp"
#include "components/EnhancementToggle.hpp"
#include "components/EffectControls.hpp"
#include "components/EqualizerPanel.hpp"
#include "components/PresetSelector.hpp"
#include "components/StatusIndicator.hpp"
#include "TrayManager.hpp"

#include "../presets/PresetFactory.hpp"
#include "../core/AppConfig.hpp"
#include "../dsp/DspConfig.hpp"

#include <QApplication>
#include <QBoxLayout>
#include <QCheckBox>
#include <QCloseEvent>
#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMetaObject>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <string>
#include <thread>
#include <vector>

namespace {

std::vector<float> opennessGains() {
  return {0.2f, 0.1f, -0.8f, -0.4f, 0.3f,
          1.0f, 1.4f, 1.2f, 0.7f};
}

constexpr int kMinimumWindowWidth = 840;
constexpr int kMinimumWindowHeight = 660;
constexpr int kMinimumContentWidth = 760;
constexpr int kCompactLayoutWidth = 1120;
constexpr int kCompactHeight = 780;
constexpr int kDraggableHeaderHeight = 76;

} // namespace

MainWindow::MainWindow(AudioService &audioService, QWidget *parent)
    : QMainWindow(parent), audioService(audioService) {
  setupUi();
  setupTray();
  loadDevices();
  setupConnections();
  restoreSettings();
  showFirstRunDialog();
}

MainWindow::~MainWindow() {
  saveSettings();
  if (audioService.isEnabled()) {
    audioService.disableEnhancement();
  }
}

void MainWindow::setupUi() {
  setWindowFlag(Qt::FramelessWindowHint, true);

  auto *central = new QWidget(this);
  central->setObjectName("appRoot");

  auto *pageLayout = new QVBoxLayout(central);
  pageLayout->setContentsMargins(0, 0, 0, 0);
  pageLayout->setSpacing(0);

  auto *headerLayout = new QHBoxLayout();
  headerLayout->setContentsMargins(28, 14, 28, 14);
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

  minimizeButton = new QPushButton("−", this);
  minimizeButton->setObjectName("windowMinimizeButton");
  minimizeButton->setToolTip("Minimize PulseForge to the system tray");
  minimizeButton->setFlat(true);

  closeButton = new QPushButton("×", this);
  closeButton->setObjectName("windowCloseButton");
  closeButton->setToolTip("Quit PulseForge and restore normal audio routing");
  closeButton->setFlat(true);

  headerLayout->addWidget(minimizeButton);
  headerLayout->addWidget(closeButton);

  auto *divider = new QFrame(this);
  divider->setObjectName("headerDivider");
  divider->setFrameShape(QFrame::HLine);

  auto *mainPanel = new QWidget(this);
  mainPanel->setObjectName("mainPanel");
  mainPanel->setMinimumWidth(kMinimumContentWidth);
  auto *mainPanelLayout = new QVBoxLayout(mainPanel);
  mainPanelLayout->setContentsMargins(26, 20, 26, 22);
  mainPanelLayout->setSpacing(14);

  topControlsLayout = new QHBoxLayout();
  topControlsLayout->setSpacing(14);

  presetSelector = new PresetSelector(this);
  loadPresets();
  deviceSelector = new DeviceSelector(this);

  presetSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  deviceSelector->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  topControlsLayout->addWidget(presetSelector, 1);
  topControlsLayout->addWidget(deviceSelector, 1);

  preferencesLayout = new QHBoxLayout();
  preferencesLayout->setSpacing(18);
  autoStartCheckBox = new QCheckBox("Start on login", this);
  autoStartCheckBox->setObjectName("preferenceCheckBox");
  autoStartCheckBox->setToolTip("Use a user-level systemd service.");
  startMinimizedCheckBox = new QCheckBox("Start minimized", this);
  startMinimizedCheckBox->setObjectName("preferenceCheckBox");
  startMinimizedCheckBox->setToolTip("Launch PulseForge hidden in the tray.");
  minimizeToTrayCheckBox = new QCheckBox("Minimize to tray on close", this);
  minimizeToTrayCheckBox->setObjectName("preferenceCheckBox");
  minimizeToTrayCheckBox->setToolTip(
      "Closing the window keeps PulseForge running in the tray.");
  preferencesLayout->addWidget(autoStartCheckBox);
  preferencesLayout->addWidget(startMinimizedCheckBox);
  preferencesLayout->addWidget(minimizeToTrayCheckBox);
  preferencesLayout->addStretch();

  statusIndicator = new StatusIndicator(this);
  statusIndicator->setMessage("Ready. Enhancement is currently bypassed.");

  auto *soundSurface = new QWidget(this);
  soundSurface->setObjectName("soundSurface");
  soundSurface->setSizePolicy(QSizePolicy::MinimumExpanding,
                              QSizePolicy::Expanding);
  soundSurfaceLayout = new QHBoxLayout(soundSurface);
  soundSurfaceLayout->setContentsMargins(18, 18, 18, 18);
  soundSurfaceLayout->setSpacing(18);

  effectControls = new EffectControls(this);
  effectControls->setValues(PresetFactory::defaultEffectValues());
  equalizerPanel = new EqualizerPanel(this);

  soundSurfaceLayout->addWidget(effectControls, 0);
  soundSurfaceLayout->addWidget(equalizerPanel, 1);

  enhancementToggle = new EnhancementToggle(this);

  mainPanelLayout->addLayout(topControlsLayout);
  mainPanelLayout->addLayout(preferencesLayout);
  mainPanelLayout->addWidget(soundSurface, 1);
  mainPanelLayout->addWidget(enhancementToggle);
  mainPanelLayout->addWidget(statusIndicator);

  auto *mainScrollArea = new QScrollArea(this);
  mainScrollArea->setObjectName("mainScrollArea");
  mainScrollArea->setFrameShape(QFrame::NoFrame);
  mainScrollArea->setWidgetResizable(true);
  mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mainScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  mainScrollArea->setWidget(mainPanel);

  pageLayout->addLayout(headerLayout);
  pageLayout->addWidget(divider);
  pageLayout->addWidget(mainScrollArea, 1);

  setCentralWidget(central);
  setWindowTitle(QString::fromUtf8(AppConfig::applicationName.data()));
  setMinimumSize(kMinimumWindowWidth, kMinimumWindowHeight);
  resize(1500, 840);
  updateResponsiveLayout(width());
}

void MainWindow::setupTray() {
  trayManager = new TrayManager(this);
  trayManager->setShowCallback([this]() { showFromTray(); });
  trayManager->setToggleEnhancementCallback(
      [this]() { requestToggleEnhancement(); });
  trayManager->setDisableEnhancementCallback([this]() {
    requestDisableEnhancement("Enhancement disabled. Audio is bypassed.");
  });
  trayManager->setAutoStartCallback(
      [this](bool enabled) { setAutoStartEnabledFromUi(enabled); });
  trayManager->setQuitCallback([this]() { requestQuit(); });
  trayManager->setEnabledState(audioService.isEnabled());
  trayManager->setAutoStartState(audioService.isAutoStartEnabled());
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
  connect(minimizeButton, &QPushButton::clicked, this,
          [this]() { minimizeToTray(); });
  connect(closeButton, &QPushButton::clicked, this,
          [this]() { requestQuit(); });

  connect(autoStartCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) { setAutoStartEnabledFromUi(checked); });
  connect(startMinimizedCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) {
            settingsStore.setStartMinimized(checked);
            settingsStore.sync();
            statusIndicator->setMessage(
                checked ? "PulseForge will start minimized."
                        : "PulseForge will open its window on launch.");
          });
  connect(minimizeToTrayCheckBox, &QCheckBox::toggled, this,
          [this](bool checked) {
            settingsStore.setMinimizeToTray(checked);
            settingsStore.sync();
            statusIndicator->setMessage(
                checked ? "Closing the window will minimize to tray."
                        : "Closing the window will quit PulseForge.");
          });

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

  connect(enhancementToggle->button(), &QPushButton::clicked, this,
          [this]() { requestToggleEnhancement(); });
}

void MainWindow::restoreSettings() {
  restoringSettings = true;

  const QString savedSink = settingsStore.selectedOutputSinkName();
  const bool previousDeviceSignalsBlocked =
      deviceSelector->comboBox()->blockSignals(true);
  if (deviceSelector->selectDeviceBySinkName(savedSink)) {
    audioService.selectOutputDevice(deviceSelector->currentDeviceId());
  }
  deviceSelector->comboBox()->blockSignals(previousDeviceSignalsBlocked);

  const QString presetId = settingsStore.lastPresetId();
  const std::vector<float> savedGains = settingsStore.customEqGains();
  const std::vector<float> savedFrequencies =
      settingsStore.customEqFrequencies();
  const std::vector<int> savedEffectValues =
      settingsStore.customEffectValues();
  const bool previousPresetSignalsBlocked =
      presetSelector->comboBox()->blockSignals(true);

  if (presetId == "custom-eq" &&
      savedGains.size() == static_cast<int>(DspConfig::eqBandCount)) {
    customCurveActive = true;
    selectPresetById("custom-eq");
    if (savedFrequencies.size() == static_cast<int>(DspConfig::eqBandCount)) {
      equalizerPanel->setFrequencies(savedFrequencies);
    }
    if (savedEffectValues.size() >= 5) {
      effectControls->setValues(savedEffectValues);
    } else {
      effectControls->setValues(PresetFactory::defaultEffectValues());
    }
    equalizerPanel->setGains(savedGains);
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

  const bool startMinimizedSignalsBlocked =
      startMinimizedCheckBox->blockSignals(true);
  const bool minimizeToTraySignalsBlocked =
      minimizeToTrayCheckBox->blockSignals(true);
  startMinimizedCheckBox->setChecked(settingsStore.startMinimized());
  minimizeToTrayCheckBox->setChecked(settingsStore.minimizeToTray());
  startMinimizedCheckBox->blockSignals(startMinimizedSignalsBlocked);
  minimizeToTrayCheckBox->blockSignals(minimizeToTraySignalsBlocked);
  setAutoStartChecked(audioService.isAutoStartEnabled());

  restoringSettings = false;

  if (settingsStore.enhancementEnabled()) {
    setEnhancementOperationBusy(true);
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
        self->setEnhancementOperationBusy(false);
        self->saveEnhancementEnabled(enabled);
        self->setEnhancementActive(
            enabled, enabled ? "Enhancement restored from last session."
                             : "PulseForge could not restore enhancement.");
        if (self->quitRequested) {
          self->beginQuitCleanup();
        }
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

  settingsStore.setSelectedOutputSinkName(deviceSelector->currentSinkName());
  settingsStore.setLastPresetId(
      customCurveActive ? QString("custom-eq")
                        : presetSelector->comboBox()->currentData().toString());
  settingsStore.setCustomEqGains(equalizerPanel->gains());
  settingsStore.setCustomEqFrequencies(equalizerPanel->frequencies());
  settingsStore.setCustomEffectValues(effectControls->values());
  settingsStore.setStartMinimized(startMinimizedCheckBox->isChecked());
  settingsStore.setMinimizeToTray(minimizeToTrayCheckBox->isChecked());
  settingsStore.setStartOnLoginPreference(autoStartCheckBox->isChecked());
  settingsStore.sync();
}

void MainWindow::saveEnhancementEnabled(bool enabled) const {
  settingsStore.setEnhancementEnabled(enabled);
  settingsStore.sync();
}

void MainWindow::showFirstRunDialog() {
  if (settingsStore.firstRunComplete()) {
    return;
  }

  const QMessageBox::StandardButton answer = QMessageBox::question(
      this, "Welcome to PulseForge",
      "PulseForge creates a virtual audio sink, routes app audio through its "
      "DSP engine, and plays the enhanced result through your selected output "
      "device.\n\n"
      "Closing the window can keep PulseForge running in the tray. You can "
      "change the output device and startup behavior from the main window.\n\n"
      "Enable Start on login now?",
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

  settingsStore.setFirstRunComplete(true);
  settingsStore.sync();

  if (answer == QMessageBox::Yes) {
    setAutoStartEnabledFromUi(true);
  }
}

void MainWindow::setEnhancementActive(bool active, const QString &message) {
  enhancementToggle->setEnabledState(active);
  statusIndicator->setActive(active);
  statusIndicator->setMessage(message);
  if (trayManager) {
    trayManager->setEnabledState(active);
  }
}

void MainWindow::setEnhancementOperationBusy(bool busy) {
  enhancementOperationInProgress = busy;
  enhancementToggle->button()->setEnabled(!busy);
  if (trayManager) {
    trayManager->setBusy(busy);
  }
}

void MainWindow::requestToggleEnhancement() {
  if (enhancementOperationInProgress) {
    return;
  }

  if (audioService.isEnabled()) {
    requestDisableEnhancement("Enhancement disabled. Audio is bypassed.");
    return;
  }

  requestEnableEnhancement();
}

void MainWindow::requestEnableEnhancement() {
  if (enhancementOperationInProgress) {
    return;
  }

  setEnhancementOperationBusy(true);
  const std::vector<float> gains = combinedEqualizerGains();
  const std::vector<float> frequencies = equalizerPanel->frequencies();
  const std::vector<int> effectValues = effectControls->values();
  const float preampDb = effectControls->preampDb();
  const float limiterCeilingDb = effectControls->limiterCeilingDb();
  statusIndicator->setMessage("Enabling enhancement...");

  QPointer<MainWindow> self(this);
  AudioService *service = &audioService;
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
      self->setEnhancementOperationBusy(false);
      if (enabled) {
        self->saveEnhancementEnabled(true);
        self->setEnhancementActive(
            true, "Enhancement enabled. PulseForge is processing audio.");
      } else {
        self->saveEnhancementEnabled(false);
        self->setEnhancementActive(false,
                                   "PulseForge could not enable enhancement.");
      }

      if (self->quitRequested) {
        self->beginQuitCleanup();
      }
    });
  }).detach();
}

void MainWindow::requestDisableEnhancement(const QString &successMessage) {
  if (enhancementOperationInProgress) {
    return;
  }

  if (!audioService.isEnabled()) {
    saveEnhancementEnabled(false);
    setEnhancementActive(false, "Enhancement is already bypassed.");
    return;
  }

  setEnhancementOperationBusy(true);
  statusIndicator->setMessage("Disabling enhancement...");
  QPointer<MainWindow> self(this);
  AudioService *service = &audioService;

  std::thread([self, service, successMessage]() {
    const bool disabled = service->disableEnhancement();
    if (!self) {
      return;
    }
    QMetaObject::invokeMethod(self, [self, disabled, successMessage]() {
      if (!self) {
        return;
      }
      self->setEnhancementOperationBusy(false);
      if (disabled) {
        self->saveEnhancementEnabled(false);
        self->setEnhancementActive(false, successMessage);
      } else {
        self->statusIndicator->setMessage(
            "PulseForge could not fully disable enhancement.");
      }

      if (self->quitRequested) {
        QApplication::quit();
      }
    });
  }).detach();
}

void MainWindow::requestQuit() {
  quitRequested = true;

  if (enhancementOperationInProgress) {
    statusIndicator->setMessage("Waiting for audio operation to finish...");
    return;
  }

  beginQuitCleanup();
}

void MainWindow::beginQuitCleanup() {
  if (quitCleanupInProgress) {
    return;
  }

  quitCleanupInProgress = true;
  saveSettings();

  if (!audioService.isEnabled()) {
    saveEnhancementEnabled(false);
    QApplication::quit();
    return;
  }

  setEnhancementOperationBusy(true);
  statusIndicator->setMessage("Restoring audio routing before quit...");
  QPointer<MainWindow> self(this);
  AudioService *service = &audioService;

  std::thread([self, service]() {
    service->disableEnhancement();
    if (!self) {
      return;
    }
    QMetaObject::invokeMethod(self, [self]() {
      if (!self) {
        return;
      }
      self->saveEnhancementEnabled(false);
      self->setEnhancementOperationBusy(false);
      self->setEnhancementActive(false, "Enhancement disabled.");
      QApplication::quit();
    });
  }).detach();
}

void MainWindow::minimizeToTray() {
  if (settingsStore.minimizeToTray() && trayManager &&
      trayManager->isAvailable()) {
    statusIndicator->setMessage("PulseForge is still running in the tray.");
    hide();
    trayManager->showStillRunningMessageOnce();
    return;
  }

  showMinimized();
}

void MainWindow::showFromTray() {
  showNormal();
  raise();
  activateWindow();
}

void MainWindow::startHiddenInTray() {
  if (trayManager && trayManager->isAvailable()) {
    hide();
    return;
  }

  showMinimized();
}

void MainWindow::closeEvent(QCloseEvent *event) {
  if (quitRequested) {
    event->accept();
    return;
  }

  if (settingsStore.minimizeToTray() && trayManager &&
      trayManager->isAvailable()) {
    event->ignore();
    minimizeToTray();
    return;
  }

  event->ignore();
  requestQuit();
}

void MainWindow::resizeEvent(QResizeEvent *event) {
  QMainWindow::resizeEvent(event);
  updateResponsiveLayout(width());
}

void MainWindow::updateResponsiveLayout(int width) {
  const bool compact = width < kCompactLayoutWidth || height() < kCompactHeight;
  const QBoxLayout::Direction direction =
      compact ? QBoxLayout::TopToBottom : QBoxLayout::LeftToRight;

  if (topControlsLayout && topControlsLayout->direction() != direction) {
    topControlsLayout->setDirection(direction);
  }
  if (preferencesLayout && preferencesLayout->direction() != direction) {
    preferencesLayout->setDirection(direction);
  }
  if (soundSurfaceLayout && soundSurfaceLayout->direction() != direction) {
    soundSurfaceLayout->setDirection(direction);
    soundSurfaceLayout->setContentsMargins(compact ? 12 : 18, compact ? 12 : 18,
                                           compact ? 12 : 18, compact ? 12 : 18);
    soundSurfaceLayout->setSpacing(compact ? 12 : 18);
  }

  if (effectControls) {
    effectControls->setCompactMode(compact);
  }
  if (equalizerPanel) {
    equalizerPanel->setCompactMode(compact);
  }
  if (enhancementToggle) {
    enhancementToggle->setCompactMode(compact);
  }
}

void MainWindow::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton &&
      event->position().y() <= kDraggableHeaderHeight) {
    draggingWindow = true;
    windowDragOffset =
        event->globalPosition().toPoint() - frameGeometry().topLeft();
    event->accept();
    return;
  }

  QMainWindow::mousePressEvent(event);
}

void MainWindow::mouseMoveEvent(QMouseEvent *event) {
  if (draggingWindow && (event->buttons() & Qt::LeftButton)) {
    move(event->globalPosition().toPoint() - windowDragOffset);
    event->accept();
    return;
  }

  QMainWindow::mouseMoveEvent(event);
}

void MainWindow::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    draggingWindow = false;
  }

  QMainWindow::mouseReleaseEvent(event);
}

void MainWindow::setAutoStartEnabledFromUi(bool enabled) {
  setAutoStartChecked(enabled);
  settingsStore.setStartOnLoginPreference(enabled);
  settingsStore.sync();

  QPointer<MainWindow> self(this);
  AudioService *service = &audioService;
  std::thread([self, service, enabled]() {
    const bool ok = enabled ? service->enableAutoStart()
                            : service->disableAutoStart();
    if (!self) {
      return;
    }
    QMetaObject::invokeMethod(self, [self, enabled, ok]() {
      if (!self) {
        return;
      }

      if (!ok) {
        self->setAutoStartChecked(!enabled);
        self->settingsStore.setStartOnLoginPreference(!enabled);
        self->settingsStore.sync();
        self->statusIndicator->setMessage(
            "PulseForge could not update start-on-login.");
        return;
      }

      self->statusIndicator->setMessage(
          enabled ? "PulseForge will start on login."
                  : "Start on login disabled.");
    });
  }).detach();
}

void MainWindow::setAutoStartChecked(bool enabled) {
  const bool blocked = autoStartCheckBox->blockSignals(true);
  autoStartCheckBox->setChecked(enabled);
  autoStartCheckBox->blockSignals(blocked);
  if (trayManager) {
    trayManager->setAutoStartState(enabled);
  }
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
