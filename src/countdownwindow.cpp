//
// Created by AyanMR on 26-3-3.
//

// You may need to build the project (run Qt uic code generator) to get "ui_CountdownWindow.h" resolved

#include "countdownwindow.h"
#include "ui_CountdownWindow.h"
#include <chrono>

#include "ThreadPool.h"

CountdownWindow::CountdownWindow(QWidget *parent, const QString &stylesheet) : QWidget(parent), ui_stylesheet(stylesheet),
                                                                               ui(new Ui::CountdownWindow), tp(1), isCountdown(false)
{
    ui->setupUi(this);
    ui->textEdit->setStyleSheet("background-color: transparent; border: none;");
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::WindowTransparentForInput | Qt::Tool);

    ui->textEdit->setStyleSheet(QString(
                                        "QTextEdit {"
                                        "   background-color: transparent;"
                                        "   border: none;"
                                        "   color: rgba(255, 255, 255, 0.7);"
                                        + ui_stylesheet +
                                        "}"
                                       ));
    const QString currentText = ui->textEdit->toPlainText();
    ui->textEdit->clear();

    QFont countdownFont = font();
    countdownFont.setPointSize(20);
    countdownFont.setBold(true);
    ui->textEdit->setFont(countdownFont);
    ui->textEdit->setText(currentText);
    ui->textEdit->setAlignment(Qt::AlignCenter);
}

void CountdownWindow::showEvent(QShowEvent *event)
{
    this->move(this->target_x, this->target_y);
    QWidget::showEvent(event);
    this->set_countdown_status(true);
    StartCountdown();
}

void CountdownWindow::closeEvent(QCloseEvent *event)
{
    this->set_countdown_status(false);
}


void CountdownWindow::StartCountdown()
{
    tp.enqueue([this] {
        const auto countdown_duration = std::chrono::milliseconds(this->countdown);
        const auto end_time           = std::chrono::steady_clock::now() + countdown_duration;
        long long  remaining_ms       = countdown_duration.count();
        while (remaining_ms > 0 && isCountdown)
        {
            remaining_ms = std::chrono::duration_cast < std::chrono::milliseconds >(end_time - std::chrono::steady_clock::now()).count();
            if (remaining_ms < 0)
            {
                remaining_ms = 0;
            }

            QMetaObject::invokeMethod(ui->textEdit, [this, remaining_ms] {
                int seconds = remaining_ms / 1000;
                int ms      = remaining_ms % 1000 / 10;
                ui->textEdit->setText(QString("%1:%2").arg(seconds, 2, 10, QChar('0')).arg(ms, 2, 10, QChar('0')));
            });

            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        this->close();
    });
}


CountdownWindow::~CountdownWindow()
{
    delete ui;
}
