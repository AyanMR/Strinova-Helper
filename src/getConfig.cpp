//
// Created by AyanMR on 26-2-25.
//
#include "getConfig.h"
#include <QFile>
#include <QMessageBox>
#include <QDebug>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

bool createDefaultConfig(QString &configPath)
{
    QFile file(configPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QJsonObject defaultConfig;
        defaultConfig["refresh_rate"]          = 240;
        defaultConfig["helping_match_area"]    = QJsonArray({1649, 1057, 1729, 1088});
        defaultConfig["plant_bomb_match_area"] = QJsonArray({1648, 1059, 1781, 1090});
        defaultConfig["bomb_match_area"]       = QJsonArray({1258, 146, 1302, 188});
        defaultConfig["bomb_time"]             = QJsonArray({26, 38, 45, 50});
        defaultConfig["plant_bomb_time"]       = 4;
        defaultConfig["defuse_bomb_time"]      = QJsonArray({4.5, 9});
        defaultConfig["helping_time"]          = 4;
        defaultConfig["switch_detect_hotkey"]  = VK_F6;

        QJsonDocument doc(defaultConfig);
        file.write(doc.toJson(QJsonDocument::Indented));
        file.close();
        return true;
    }
    return false;
}

void getRefreshRate(QJsonObject config) { refresh_rate = config["refresh_rate"].toInt(); }

void LoadConfig()
{
    QString configPath = "config.json";
    QString message    = "";
    if (!QFile::exists(configPath))
    {
        message.append("未找到配置文件,");
        qDebug() << "Config file not found.";
        if (createDefaultConfig(configPath))
        {
            qDebug() << "Default config file created successfully.";
            message.append("默认配置文件已生成");
        }
        else
        {
            qDebug() << "Failed to create default config file.";
            message.append("默认配置文件生成失败");
        }
        QMessageBox::warning(nullptr, "配置文件提示", message);
    }
    else
    {
        QFile file(configPath);
        if (file.open(QIODevice::ReadOnly | QIODevice::Text))
        {
            auto doc = QJsonDocument::fromJson(file.readAll());
            file.close();
            QJsonObject config = doc.object();

            getRefreshRate(config);

            QJsonArray helpingMatchArea = config["helping_match_area"].toArray();
            helping_match_area          = RECT{
                helpingMatchArea[0].toInt(), helpingMatchArea[1].toInt(), helpingMatchArea[2].toInt(),
                helpingMatchArea[3].toInt()
            };

            QJsonArray plantBombMatchArea = config["plant_bomb_match_area"].toArray();
            plant_bomb_match_area         = RECT{
                plantBombMatchArea[0].toInt(), plantBombMatchArea[1].toInt(), plantBombMatchArea[2].toInt(),
                plantBombMatchArea[3].toInt()
            };

            QJsonArray bombMatchArea = config["bomb_match_area"].toArray();
            bomb_match_area          = RECT{
                bombMatchArea[0].toInt(), bombMatchArea[1].toInt(), bombMatchArea[2].toInt(),
                bombMatchArea[3].toInt()
            };

            QJsonArray bombTime = config["bomb_time"].toArray();
            bomb_time.clear();
            for (const auto &val : bombTime)
                bomb_time.push_back(val.toInt());


            plant_bomb_time = config["plant_bomb_time"].toInt();

            QJsonArray defuseBombTime = config["defuse_bomb_time"].toArray();
            defuse_bomb_time.clear();
            for (const auto &val : defuseBombTime)
                defuse_bomb_time.push_back(val.toDouble());

            helping_time = config["helping_time"].toInt();

            switch_detect_hotkey = config["switch_detect_hotkey"].toInt();
        }
    }
}
