#include "EnhancementToggle.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QLayout>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

EnhancementToggle::EnhancementToggle(QWidget *parent) : QWidget(parent) {
  setObjectName("powerDock");

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 4, 0, 0);
  root->setSpacing(6);

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

void EnhancementToggle::setCompactMode(bool compact) {
  if (compactMode == compact) {
    return;
  }

  compactMode = compact;
  setProperty("compact", compact);
  toggleButton->setProperty("compact", compact);
  statusLabel->setProperty("compact", compact);

  if (auto *boxLayout = qobject_cast<QVBoxLayout *>(layout())) {
    boxLayout->setContentsMargins(0, compact ? 0 : 4, 0, 0);
    boxLayout->setSpacing(compact ? 4 : 6);
  }

  style()->unpolish(this);
  style()->polish(this);
  toggleButton->style()->unpolish(toggleButton);
  toggleButton->style()->polish(toggleButton);
  statusLabel->style()->unpolish(statusLabel);
  statusLabel->style()->polish(statusLabel);
}
