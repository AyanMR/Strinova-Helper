#include <QApplication>
#include <QSystemTrayIcon>
#include <memory>
#include <opencv2/opencv.hpp>
#include <Windows.h>
#include <QFile>
#include <QTimer>
#include <QMenu>
#include <QPointer>
#include <QFontDatabase>

#include "countdownwindow.h"
#include "imageDetected.h"
#include "mainwindow.h"
#include "settings.h"


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    QFont font = a.font();
    const int fontId = QFontDatabase::addApplicationFont(":/resources/fonts/SourceHanSansSC-Normal.otf");
    const QStringList fontFamilies = fontId == -1 ? QStringList() : QFontDatabase::applicationFontFamilies(fontId);
    if (!fontFamilies.isEmpty()) font.setFamily(fontFamilies.first());
    font.setStyleStrategy(QFont::PreferAntialias);
    a.setFont(font);

    QApplication::setQuitOnLastWindowClosed(false);
    auto *mainWindow = new MainWindow();
    if (QSystemTrayIcon::isSystemTrayAvailable())
    {
        auto *  tray_icon = new QSystemTrayIcon(&a);
        QString iconPath  = ":/resources/icons/tray_icon128.png";
        QIcon   icon(iconPath);
        tray_icon->setIcon(icon);
        tray_icon->show();
        QTimer::singleShot(200, tray_icon, [tray_icon] {
            tray_icon->setToolTip("卡丘助手");
        });
        QMenu *menu = new QMenu(mainWindow);

        menu->setWindowFlags(Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint);

        menu->setAttribute(Qt::WA_TranslucentBackground);

        QString styleSheet = QString(
                                     "QMenu {"
                                     "    background-color: black;"
                                     "    border: 1px solid #333333;"
                                     "    border-radius: 12px;"
                                     "    padding: 3px;"
                                     "    font-family: '%1';"
                                     "    font-size: 12px;"
                                     "}"
                                     "QMenu::item {"
                                     "    color: white;"
                                     "    background-color: transparent;"
                                     "    padding: 3px 12px;"
                                     "    margin: 0px 3px;"
                                     "    border-radius: 4px;"
                                     "}"
                                     "QMenu::item:selected {"
                                     "    background-color: #333333;"
                                     "}"
                                     "QMenu::separator {"
                                     "    height: 1px;"
                                     "    background-color: #333333;"
                                     "    margin: 5px 10px;"
                                     "}"
                                     ).arg(font.family());
        menu->setStyleSheet(styleSheet);
        // QPointer < settings > st = nullptr;
        // menu->addAction("设置", [&] {
        //     if (st.isNull())
        //     {
        //         st = new settings();
        //         st->setAttribute(Qt::WA_DeleteOnClose);
        //     }
        //     st->show();
        //     st->activateWindow();
        //     st->raise();
        // });
        menu->addAction("打开查询面板", mainWindow, [mainWindow] {
            if (mainWindow->isMinimized()) mainWindow->showNormal();
            else mainWindow->show();
            mainWindow->raise();
            mainWindow->activateWindow();
        });
        menu->addSeparator();
        menu->addAction("退出程序", &a, [] { std::exit(0); });
        tray_icon->setContextMenu(menu);
    }

    auto id             = std::make_shared < imageDetected >(&a);
    auto bomb_cw        = std::make_shared < CountdownWindow >();
    auto helping_cw     = std::make_shared < CountdownWindow >(nullptr, QString("color: rgba(0, 0, 0, 0.7);"));
    auto plant_cw       = std::make_shared < CountdownWindow >(nullptr, QString("color: rgba(0, 0, 0, 0.7);"));
    auto defuse_bomb_cw = std::make_shared < CountdownWindow >(nullptr, QString("color: rgba(0, 0, 0, 0.7);"));
    QObject::connect(id.get(), &imageDetected::StartBombCountdown, bomb_cw.get(), [&bomb_cw] {
        // qDebug() << "received";
        bomb_cw->set_countdown(50000);
        bomb_cw->set_position(1240, 220);
        bomb_cw->show();
    }, Qt::QueuedConnection);

    QObject::connect(id.get(), &imageDetected::StopBombCountdown, bomb_cw.get(), [&bomb_cw] {
        bomb_cw->close();
    }, Qt::QueuedConnection);

    QObject::connect(id.get(), &imageDetected::StartPlantBombCountdown, plant_cw.get(), [&plant_cw] {
        plant_cw->set_countdown(4000);
        plant_cw->set_position(1790, 1047);
        plant_cw->show();
    }, Qt::QueuedConnection);


    QObject::connect(id.get(), &imageDetected::StopPlantBombCountdown, plant_cw.get(), [&plant_cw] {
        plant_cw->close();
    }, Qt::QueuedConnection);


    QObject::connect(id.get(), &imageDetected::StartDefuseBomb, defuse_bomb_cw.get(), [&defuse_bomb_cw] {
        defuse_bomb_cw->set_countdown(9000);
        defuse_bomb_cw->set_position(1790, 1047);
        defuse_bomb_cw->show();
    }, Qt::QueuedConnection);

    QObject::connect(id.get(), &imageDetected::StartDefuseHalfBomb, defuse_bomb_cw.get(), [&defuse_bomb_cw] {
        defuse_bomb_cw->set_countdown(4500);
        defuse_bomb_cw->set_position(1790, 1047);
        defuse_bomb_cw->show();
    }, Qt::QueuedConnection);

    QObject::connect(id.get(), &imageDetected::StopDefuseBomb, defuse_bomb_cw.get(), [&defuse_bomb_cw] {
        defuse_bomb_cw->close();
    }, Qt::QueuedConnection);

    QObject::connect(id.get(), &imageDetected::StartHelping, helping_cw.get(), [&helping_cw] {
        helping_cw->set_countdown(4000);
        helping_cw->set_position(1790, 1047);
        helping_cw->show();
    });
    QObject::connect(id.get(), &imageDetected::StopHelping, helping_cw.get(), [&helping_cw] {
        helping_cw->close();
    });

    id->work();

    mainWindow->show();

    return QApplication::exec();
}
