#pragma once

#include <QString>
#include <QWidget>

class QLabel;
class QVBoxLayout;

class CardContainer : public QWidget {
public:
  explicit CardContainer(const QString &title, const QString &subtitle = {},
                         QWidget *parent = nullptr);

  QVBoxLayout *contentLayout() const;
  void setSubtitle(const QString &subtitle);

private:
  QLabel *subtitleLabel = nullptr;
  QVBoxLayout *bodyLayout = nullptr;
};
