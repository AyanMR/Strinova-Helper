//
// Created by AyanMR on 26-3-3.
//

#ifndef COUNTDOWNWINDOW_H
#define COUNTDOWNWINDOW_H

#include <QWidget>

#include "getConfig.h"
#include "ThreadPool.h"


QT_BEGIN_NAMESPACE

namespace Ui
{
    class CountdownWindow;
}

QT_END_NAMESPACE

class CountdownWindow : public QWidget
{
        Q_OBJECT

    public:
        explicit CountdownWindow(QWidget *parent = nullptr, const QString &stylesheet = "color: rgba(255, 255, 255, 0.7);");

        void set_countdown(int ms) { this->countdown = ms; }

        void set_position(POINT point)
        {
            this->target_x = point.x;
            this->target_y = point.y;
        }

        void set_position(int x, int y)
        {
            this->target_x = x;
            this->target_y = y;
        }

        void set_countdown_status(bool status) { isCountdown = status; }

        ~CountdownWindow() override;

    protected:
        void showEvent(QShowEvent *event) override;

        void closeEvent(QCloseEvent *event) override;

    private:
        int                  target_x  = 1280, target_y = 720;
        int                  countdown = 50000;
        QString              ui_stylesheet;
        Ui::CountdownWindow *ui;

        ThreadPool           tp;
        std::atomic < bool > isCountdown;

        void StartCountdown();
};


#endif //COUNTDOWNWINDOW_H
