#include "EnhancementToggle.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

EnhancementToggle::EnhancementToggle(QWidget *parent) : QWidget(parent) {
  setObjectName("powerDock");

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 8, 0, 2);
  root->setSpacing(10);

  titleLabel = new QLabel("Sound Enhancement", this);
  titleLabel->setObjectName("heroTitle");
  titleLabel->setAlignment(Qt::AlignCenter);

  statusLabel = new QLabel(
      "Bypassed. Enable processing to route audio through PulseForge.", this);
  statusLabel->setObjectName("heroSubtitle");
  statusLabel->setAlignment(Qt::AlignCenter);
  statusLabel->setWordWrap(true);

  toggleButton = new QPushButton("⏻", this);
  toggleButton->setObjectName("powerButton");
  toggleButton->setProperty("active", false);
  toggleButton->setToolTip("Enable or disable sound enhancement");

  root->addWidget(toggleButton, 0, Qt::AlignHCenter);
  root->addWidget(statusLabel, 0, Qt::AlignHCenter);
}

void EnhancementToggle::setEnabledState(bool enabled) {
  enabledState = enabled;

  if (enabled) {
    statusLabel->setText(
        "Active. Audio is routed through the enhancement chain.");
    toggleButton->setText("⏻");
    toggleButton->setProperty("active", true);
  } else {
    statusLabel->setText(
        "Bypassed. Enable processing to route audio through PulseForge.");
    toggleButton->setText("⏻");
    toggleButton->setProperty("active", false);
  }

  toggleButton->style()->unpolish(toggleButton);
  toggleButton->style()->polish(toggleButton);
}

bool EnhancementToggle::isEnabledState() const {
  return enabledState;
}

QPushButton *EnhancementToggle::button() const {
  return toggleButton;
}
