//
// Created by AyanMR on 26-2-24.
//

#ifndef GETWINDOWPOSITION_H
#define GETWINDOWPOSITION_H
#include <Windows.h>
#include <cwchar>
#include <string>
#include <optional>

std::optional < RECT > GetWindowRectByProcessName(const wchar_t *procName);

inline int RectWidth(const RECT &rect) { return rect.right - rect.left; }
inline int RectHeight(const RECT &rect) { return rect.bottom - rect.top; }

#endif //GETWINDOWPOSITION_H
