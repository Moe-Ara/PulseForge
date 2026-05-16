#pragma once

#include <QWidget>

class QLabel;
class QPushButton;

class EnhancementToggle: public QWidget{
public:
  explicit EnhancementToggle(QWidget *parent = nullptr);

  void setEnabledState(bool enabled);

  QPushButton* button() const;

  private:
    QLabel* titleLabel=nullptr;
    QLabel* statusLabel=nullptr;
    QPushButton* toggleButton=nullptr;
};