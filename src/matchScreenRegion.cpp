//
// Created by AyanMR on 26-3-2.
//

#include "matchScreenRegion.h"

#include <QFile>

cv::Mat HBitmapToMat(HBITMAP hBitmap)
{
    BITMAP bmp;
    GetObject(hBitmap, sizeof(BITMAP), &bmp);

    int width  = bmp.bmWidth;
    int height = bmp.bmHeight;

    cv::Mat mat(height, width, CV_8UC4);

    HDC     hdc       = CreateCompatibleDC(nullptr);
    HBITMAP oldBitmap = (HBITMAP) SelectObject(hdc, hBitmap);

    BITMAPINFO bmi              = {0};
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = width;
    bmi.bmiHeader.biHeight      = -height;
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 32;
    bmi.bmiHeader.biCompression = BI_RGB;

    if (GetDIBits(hdc, hBitmap, 0, height, mat.data, &bmi, DIB_RGB_COLORS) == 0)
    {
        qDebug() << "GetDIBits 失败!";
        SelectObject(hdc, oldBitmap);
        DeleteDC(hdc);
        return cv::Mat();
    }

    SelectObject(hdc, oldBitmap);
    DeleteDC(hdc);

    cv::cvtColor(mat, mat, cv::COLOR_BGRA2BGR);
    return mat;
}

bool compareScreenRegionWithImage(const bool debug, const RECT &targetRect, const std::string &imagePath, double threshold)
{
    QFile file(QString::fromStdString(imagePath));
    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "无法打开资源文件:" << QString::fromStdString(imagePath);
        return false;
    }
    QByteArray imageData = file.readAll();
    file.close();

    std::vector data(imageData.begin(), imageData.end());
    cv::Mat     templateImage = cv::imdecode(data, cv::IMREAD_COLOR);

    if (templateImage.empty())
    {
        qDebug() << "无法解码图像数据:" << QString::fromStdString(imagePath);
        return false;
    }

    int width  = targetRect.right - targetRect.left;
    int height = targetRect.bottom - targetRect.top;

    HDC     hScreenDC  = GetDC(NULL);
    HDC     hMemoryDC  = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap    = CreateCompatibleBitmap(hScreenDC, width, height);
    auto    hOldBitmap = static_cast < HBITMAP >(SelectObject(hMemoryDC, hBitmap));

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, targetRect.left, targetRect.top, SRCCOPY);
    hBitmap = static_cast < HBITMAP >(SelectObject(hMemoryDC, hOldBitmap));

    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    cv::Mat screenMat = HBitmapToMat(hBitmap);
    DeleteObject(hBitmap);

    if (screenMat.empty())
    {
        qDebug() << "截屏失败或转换失败。";
        return false;
    }

    if (screenMat.channels() == 4)
    {
        cv::cvtColor(screenMat, screenMat, cv::COLOR_BGRA2BGR);
    }
    if (templateImage.channels() == 4)
    {
        cv::cvtColor(templateImage, templateImage, cv::COLOR_BGRA2BGR);
    }

    if (screenMat.channels() != 3 || templateImage.channels() != 3)
    {
        qDebug() << "图像通道数不为3，无法比较。";
        return false;
    }


    if (templateImage.rows > screenMat.rows || templateImage.cols > screenMat.cols)
    {
        qDebug() << "模板图像尺寸大于截图区域尺寸，无法匹配。";
        return false;
    }

    cv::Mat result;
    cv::matchTemplate(screenMat, templateImage, result, cv::TM_CCOEFF_NORMED);

    double    minVal, maxVal;
    cv::Point minLoc, maxLoc;
    cv::minMaxLoc(result, &minVal, &maxVal, &minLoc, &maxLoc);
    if (debug)
        qDebug() << "图像相似度:" << maxVal;

    return maxVal >= threshold;
}

