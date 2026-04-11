//
// Created by AyanMR on 26-3-3.
//

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QWidget>


QT_BEGIN_NAMESPACE

namespace Ui
{
    class settings;
}

QT_END_NAMESPACE

class settings : public QWidget
{
        Q_OBJECT

    public:
        explicit settings(QWidget *parent = nullptr);

        ~settings() override;

    private:
        Ui::settings *ui;
};


#endif //SETTINGS_H
