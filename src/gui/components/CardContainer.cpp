#include "CardContainer.hpp"

#include <QLabel>
#include <QVBoxLayout>

CardContainer::CardContainer(const QString &title, const QString &subtitle,
                             QWidget *parent)
    : QWidget(parent) {
  setObjectName("cardContainer");

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(20, 18, 20, 20);
  root->setSpacing(14);

  auto *headerLayout = new QVBoxLayout();
  headerLayout->setSpacing(4);

  auto *titleLabel = new QLabel(title, this);
  titleLabel->setObjectName("cardTitle");

  subtitleLabel = new QLabel(subtitle, this);
  subtitleLabel->setObjectName("cardSubtitle");
  subtitleLabel->setWordWrap(true);
  subtitleLabel->setVisible(!subtitle.isEmpty());

  headerLayout->addWidget(titleLabel);
  headerLayout->addWidget(subtitleLabel);

  bodyLayout = new QVBoxLayout();
  bodyLayout->setContentsMargins(0, 0, 0, 0);
  bodyLayout->setSpacing(12);

  root->addLayout(headerLayout);
  root->addLayout(bodyLayout);
}

QVBoxLayout *CardContainer::contentLayout() const {
  return bodyLayout;
}

void CardContainer::setSubtitle(const QString &subtitle) {
  subtitleLabel->setText(subtitle);
  subtitleLabel->setVisible(!subtitle.isEmpty());
}
