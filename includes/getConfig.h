//
// Created by AyanMR on 26-2-25.
//

#ifndef GETCONFIG_H
#define GETCONFIG_H
#include <QJsonDocument>
#include <windows.h>

inline int                    refresh_rate;
inline RECT                   helping_match_area;
inline RECT                   plant_bomb_match_area;
inline RECT                   bomb_match_area;
inline std::vector < int >    bomb_time;
inline int                    plant_bomb_time;
inline std::vector < double > defuse_bomb_time;
inline int                    helping_time;
inline UINT                   switch_detect_hotkey;

bool createDefaultConfig(QString &configPath);

void LoadConfig();

void SaveConfig();

void getRefreshRate(QJsonObject config);


#endif //GETCONFIG_H
