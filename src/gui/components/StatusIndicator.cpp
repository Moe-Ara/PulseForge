#include "StatusIndicator.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QStyle>

StatusIndicator::StatusIndicator(QWidget *parent) : QWidget(parent) {
  setObjectName("statusIndicator");

  auto *layout = new QHBoxLayout(this);
  layout->setContentsMargins(12, 10, 12, 10);
  layout->setSpacing(10);

  dotLabel = new QLabel(this);
  dotLabel->setObjectName("statusDot");
  dotLabel->setFixedSize(9, 9);

  messageLabel = new QLabel("Ready", this);
  messageLabel->setObjectName("statusText");
  messageLabel->setWordWrap(true);

  layout->addWidget(dotLabel, 0, Qt::AlignTop);
  layout->addWidget(messageLabel, 1);

  setActive(false);
}

void StatusIndicator::setMessage(const QString &message) {
  messageLabel->setText(message);
}

void StatusIndicator::setActive(bool active) {
  dotLabel->setProperty("active", active);
  dotLabel->style()->unpolish(dotLabel);
  dotLabel->style()->polish(dotLabel);
}
