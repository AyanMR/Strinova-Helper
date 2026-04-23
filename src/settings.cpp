//
// Created by AyanMR on 26-3-3.
//

// You may need to build the project (run Qt uic code generator) to get "ui_settings.h" resolved

#include "settings.h"
#include "ui_settings.h"


settings::settings(QWidget *parent) : QWidget(parent), ui(new Ui::settings)
{
    ui->setupUi(this);
}

settings::~settings()
{
    delete ui;
}
