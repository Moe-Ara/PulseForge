#include "EnhancementToggle.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

EnhancementToggle::EnhancementToggle(QWidget *parent) : QWidget(parent) {
  setObjectName("heroCard");

  auto *root = new QHBoxLayout(this);
  root->setContentsMargins(28, 26, 28, 26);
  root->setSpacing(24);

  auto *textLayout = new QVBoxLayout();
  textLayout->setSpacing(8);

  titleLabel = new QLabel("Sound Enhancement", this);
  titleLabel->setObjectName("heroTitle");

  statusLabel = new QLabel(
      "Bypassed. Enable processing to route audio through PulseForge.", this);
  statusLabel->setObjectName("heroSubtitle");
  statusLabel->setWordWrap(true);

  toggleButton = new QPushButton("Enable Sound Enhancement", this);
  toggleButton->setObjectName("powerButton");
  toggleButton->setProperty("active", false);

  textLayout->addWidget(titleLabel);
  textLayout->addWidget(statusLabel);

  root->addLayout(textLayout);
  root->addStretch();
  root->addWidget(toggleButton);
}

void EnhancementToggle::setEnabledState(bool enabled) {
  enabledState = enabled;

  if (enabled) {
    statusLabel->setText(
        "Active. Audio is routed through the enhancement chain.");
    toggleButton->setText("Disable Sound Enhancement");
    toggleButton->setProperty("active", true);
  } else {
    statusLabel->setText(
        "Bypassed. Enable processing to route audio through PulseForge.");
    toggleButton->setText("Enable Sound Enhancement");
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
