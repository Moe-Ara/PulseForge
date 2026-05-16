#include "EnhancementToggle.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

EnhancementToggle::EnhancementToggle(QWidget *parent) : QWidget(parent)
{
auto* root=new QHBoxLayout(this);
auto* textLayout=new QVBoxLayout();

titleLabel=new QLabel("PulseForge Enhanced",this);
statusLabel=new QLabel("Disabled",this);
toggleButton=new QPushButton("Enable",this);
toggleButton->setObjectName("primaryButton");

textLayout->addWidget(titleLabel);
textLayout->addWidget(statusLabel);

root->addLayout(textLayout);
root->addStretch();
root->addWidget(toggleButton);

setLayout(root);
}

void EnhancementToggle::setEnabledState(bool enabled)
{
    if(enabled)
    {
        statusLabel->setText("Enabled");
        toggleButton->setText("Disable");
    }
    else
    {
        statusLabel->setText("Disabled");
        toggleButton->setText("Enable");
    }
}
QPushButton* EnhancementToggle::button() const
{
    return toggleButton;
}