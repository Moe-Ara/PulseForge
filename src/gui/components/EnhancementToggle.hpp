#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class EnhancementToggle : public QWidget {
public:
  explicit EnhancementToggle(QWidget *parent = nullptr);

  void setEnabledState(bool enabled);
  bool isEnabledState() const;
  void setCompactMode(bool compact);

  QPushButton *button() const;

private:
  QLabel *titleLabel = nullptr;
  QLabel *statusLabel = nullptr;
  QPushButton *toggleButton = nullptr;
  bool enabledState = false;
  bool compactMode = false;
};
