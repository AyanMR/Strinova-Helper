//
// Created by AyanMR on 26-3-12.
//

#ifndef IMAGEDETECTED_H
#define IMAGEDETECTED_H
#include <QObject>

#include "ThreadPool.h"
#include "matchScreenRegion.h"



class imageDetected : public QObject
{
        Q_OBJECT

    public:
        explicit imageDetected(QObject *parent = nullptr);

        ~imageDetected() override;

    public slots:
        void work();

    signals:
        void StartBombCountdown();

        void StopBombCountdown();

        void StartPlantBombCountdown();

        void StopPlantBombCountdown();

        void StartHelping();

        void StopHelping();

        void StartDefuseBomb();

        void StartDefuseHalfBomb();

        void StopDefuseBomb();

    private:
        ThreadPool           tp;
        RECT                 bomb_rect             = {1258, 146, 1302, 188};
        RECT                 plant_bomb_rect       = {1648, 1059, 1781, 1090};
        RECT                 helping_rect          = {1649, 1057, 1729, 1088};
        RECT                 defuse_bomb_rect      = {1650, 1058, 1781, 1086};
        RECT                 defuse_bomb_half_rect = {1649, 1089, 1780, 1109};
        std::atomic < bool > bomb_planted          = false, bomb_planting = false, helping = false, defuse_bomb = false;
};


#endif //IMAGEDETECTED_H
