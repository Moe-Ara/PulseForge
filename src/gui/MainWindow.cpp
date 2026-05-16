#include "MainWindow.hpp"

#include <QComboBox>
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
  auto *layout = new QVBoxLayout(central);

  statusLabel = new QLabel("Bazzite Sound ready", this);

  deviceComboBox = new QComboBox(this);
  presetComboBox = new QComboBox(this);

  enableButton = new QPushButton("Enable Enhancement", this);
  disableButton = new QPushButton("Disable Enhancement", this);
  applyPresetButton = new QPushButton("Apply Preset", this);

  presetComboBox->addItem("Gaming", "gaming");

  layout->addWidget(statusLabel);
  layout->addWidget(deviceComboBox);
  layout->addWidget(presetComboBox);
  layout->addWidget(enableButton);
  layout->addWidget(disableButton);
  layout->addWidget(applyPresetButton);

  setCentralWidget(central);
  setWindowTitle("Bazzite Sound");
  resize(420, 260);
}

void MainWindow::loadDevices() {
  auto devices = audioService.getOutputDevices();

  for (const auto &device : devices) {
    deviceComboBox->addItem(QString::fromStdString(device.name),
                            QString::fromStdString(device.id));
  }
}

void MainWindow::setupConnections() {
  connect(enableButton, &QPushButton::clicked, this, [this]() {
    audioService.enableEnhancement();
    statusLabel->setText("Enhancement enabled");
  });

  connect(disableButton, &QPushButton::clicked, this, [this]() {
    audioService.disableEnhancement();
    statusLabel->setText("Enhancement disabled");
  });

  connect(deviceComboBox, &QComboBox::currentIndexChanged, this, [this]() {
    auto deviceId = deviceComboBox->currentData().toString().toStdString();
    audioService.selectOutputDevice(deviceId);
    statusLabel->setText("Selected output device");
  });

  connect(applyPresetButton, &QPushButton::clicked, this, [this]() {
    auto preset = createGamingPreset();
    audioService.applyPreset(preset);
    statusLabel->setText("Gaming preset applied");
  });
}

Preset MainWindow::createGamingPreset() const {
  return Preset{"gaming",
                "Gaming",
                "Boost clarity and footsteps.",
                {{{"eq", {{"bass", 1.1f}, {"treble", 1.25f}}},
                  {"compressor", {{"threshold", -18.0f}, {"ratio", 2.0f}}},
                  {"limiter", {{"ceiling", -1.0f}}}}}};
}
