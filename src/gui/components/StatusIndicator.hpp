#pragma once

#include <QString>
#include <QWidget>

class QLabel;

class StatusIndicator : public QWidget {
public:
  explicit StatusIndicator(QWidget *parent = nullptr);

  void setMessage(const QString &message);
  void setActive(bool active);

private:
  QLabel *dotLabel = nullptr;
  QLabel *messageLabel = nullptr;
};
