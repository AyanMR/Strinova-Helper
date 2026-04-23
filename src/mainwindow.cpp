//
// Created by AyanMR on 26-3-1.
//

// You may need to build the project (run Qt uic code generator) to get "ui_MainWindow.h" resolved

#include "mainwindow.h"
#include "ui_MainWindow.h"
#include <thread>
#include "getWindowPosition.h"
#include <future>
#include <windows.h>


MainWindow::MainWindow(QWidget *parent) : QWidget(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    auto positionThread = std::thread([this] {
        while (true)
        {
            RECT tempRect;
            if (auto tag_rect = GetWindowRectByProcessName(L"Calabiyau-Win64-Shipping.exe") ; tag_rect.has_value())
            {
                tempRect = tag_rect.value();
                std::unique_lock lock(rectMutex);
                if (memcmp(&rect, &tempRect, sizeof(RECT)) != 0)
                {
                    rect = tempRect;
                }
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    });
    positionThread.detach();


}

MainWindow::~MainWindow()
{
    delete ui;
}
