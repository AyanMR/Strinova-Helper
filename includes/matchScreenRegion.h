//
// Created by AyanMR on 26-3-2.
//

#ifndef MATCHSCREENREGION_H
#define MATCHSCREENREGION_H

#include <windows.h>
#include <opencv2/opencv.hpp>
#include <QDebug>

cv::Mat HBitmapToMat(HBITMAP hBitmap);

bool compareScreenRegionWithImage(bool debug, const RECT &targetRect, const std::string &imagePath, double threshold = 0.83);

#endif //MATCHSCREENREGION_H
