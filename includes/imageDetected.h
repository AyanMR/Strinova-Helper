//
// Created by AyanMR on 26-3-12.
//

#ifndef IMAGEDETECTED_H
#define IMAGEDETECTED_H
#include <QObject>

#include "ThreadPool.h"


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
        ThreadPool tp;
};


#endif //IMAGEDETECTED_H
