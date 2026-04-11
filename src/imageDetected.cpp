//
// Created by AyanMR on 26-3-12.
//

#include "imageDetected.h"
#include "ThreadPool.h"
#include "matchScreenRegion.h"

imageDetected::imageDetected(QObject *parent) : QObject(parent), tp(12)
{
}

void imageDetected::work()
{
    RECT bomb_rect             = {1258, 146, 1302, 188};
    RECT plant_bomb_rect       = {1648, 1059, 1781, 1090};
    RECT helping_rect          = {1649, 1057, 1729, 1088};
    RECT defuse_bomb_rect      = {1650, 1058, 1781, 1086};
    RECT defuse_bomb_half_rect = {1649, 1089, 1780, 1109};
    bool bomb_planted          = false, bomb_planting = false, helping = false, defuse_bomb = false;

    tp.enqueue([bomb_rect, &bomb_planted, this] {
        while (!tp.isStop.load(std::memory_order_relaxed))
        {
            if (compareScreenRegionWithImage(false, bomb_rect, ":/resources/templates/bomb_1.png") ||
                compareScreenRegionWithImage(false, bomb_rect, ":/resources/templates/bomb_2.png") ||
                compareScreenRegionWithImage(false, bomb_rect, ":/resources/templates/bomb_3.png"))
            {
                if (bomb_planted == false)
                {
                    bomb_planted = true;
                    emit this->StartBombCountdown();
                    // qDebug() << "sent";
                }
            }
            else
            {
                emit this->StopBombCountdown();
                bomb_planted = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    tp.enqueue([=,&bomb_planting] {
        while (!tp.isStop.load(std::memory_order_relaxed))
        {
            if (compareScreenRegionWithImage(false, plant_bomb_rect, ":/resources/templates/plant_bomb.png"))
            {
                if (bomb_planting == false)
                {
                    bomb_planting = true;
                    emit this->StartPlantBombCountdown();
                }
            }
            else
            {
                if (bomb_planting)
                    emit this->StopPlantBombCountdown();
                bomb_planting = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
    tp.enqueue([=,&helping] {
        while (!tp.isStop.load(std::memory_order_relaxed))
        {
            if (compareScreenRegionWithImage(false, helping_rect, ":/resources/templates/helping.png"))
            {
                if (helping == false)
                {
                    helping = true;
                    emit this->StartHelping();
                }
            }
            else
            {
                if (helping)
                    emit this->StopHelping();
                helping = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    tp.enqueue([=,&defuse_bomb] {
        while (!tp.isStop.load(std::memory_order_relaxed))
        {
            if (compareScreenRegionWithImage(false, defuse_bomb_rect, ":/resources/templates/defuse_bomb.png"))
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));//cnm
                if (defuse_bomb == true)
                    continue;
                if (compareScreenRegionWithImage(false, defuse_bomb_half_rect, ":/resources/templates/defuse_bomb_half2.png"))
                {
                    emit this->StartDefuseBomb();
                    defuse_bomb = true;
                }
                else if (compareScreenRegionWithImage(false, defuse_bomb_half_rect, ":/resources/templates/defuse_bomb_half.png"))
                {
                    emit this->StartDefuseHalfBomb();
                    defuse_bomb = true;
                }
            }
            else
            {
                if (defuse_bomb)
                    emit this->StopDefuseBomb();
                defuse_bomb = false;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });
}

imageDetected::~imageDetected()
{
}
