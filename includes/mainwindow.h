//
// Created by AyanMR on 26-3-1.
//

#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QWidget>
#include <windows.h>
#include <mutex>

extern int refresh_rate;

QT_BEGIN_NAMESPACE

namespace Ui
{
    class MainWindow;
}

QT_END_NAMESPACE

class MainWindow : public QWidget
{
        Q_OBJECT

    public:
        explicit MainWindow(QWidget *parent = nullptr);

        ~MainWindow() override;

    private:
        Ui::MainWindow *ui;
        RECT            rect;
        std::mutex      rectMutex;
};


#endif //MAINWINDOW_H
