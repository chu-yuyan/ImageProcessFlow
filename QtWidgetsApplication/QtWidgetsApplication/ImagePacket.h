#pragma once 
#include <opencv2/opencv.hpp>
#include <QMetaType>
#include "BaseNode.h"

struct ImagePacket {
    cv::Mat image;          // 始终可显示的 BGR/BGRA 图像
    cv::Mat indexMap;       // 可选：索引图，CV_8UC1，存储调色板索引（仅 PaletteIndexed 有效）
    ImageType type = ImageType::Color;
    int pixelBlockSize = 1;           // 仅对 PixelGrid 有意义
    QString paletteName;              // 仅对 PaletteIndexed 有意义
    QVector<cv::Vec3b> palette;       // 调色板 RGB 列表
    bool hasAlpha = false;            // 是否带透明通道
    cv::Mat alphaMask;    
    QVector<QString> colorCodes;

    ImagePacket() = default;
    ImagePacket(const cv::Mat& img, ImageType t = ImageType::Color)
        : image(img), type(t) {
    }
};
Q_DECLARE_METATYPE(ImagePacket)