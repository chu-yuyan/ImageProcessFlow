#pragma once
#include "BaseNode.h"
#include "ImagePacket.h"
#include "NodeFactory.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QEventLoop>
#include <QBuffer>
#include "config.h" 
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

// 1. 裁剪节点

class CropNode : public BaseNode {
    int m_x = 0, m_y = 0, m_w = 100, m_h = 100;
public:
    CropNode() {
        m_name = "裁剪";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Color, QVariant() });
    }
    QString typeName() const override { return "Crop"; }
    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;
        cv::Rect roi(cv::Point(m_x, m_y), cv::Size(m_w, m_h));
        roi &= cv::Rect(0, 0, packet.image.cols, packet.image.rows);
        ImagePacket out = packet;
        out.image = packet.image(roi).clone();
        m_outputs[0].data = QVariant::fromValue(out);
    }
    QMap<QString, QVariant> getParameters() const override {
        return { {"x", m_x}, {"y", m_y}, {"width", m_w}, {"height", m_h} };
    }
    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "x") m_x = value.toInt();
        else if (name == "y") m_y = value.toInt();
        else if (name == "width") m_w = value.toInt();
        else if (name == "height") m_h = value.toInt();
    }
    QMap<QString, ParameterMeta> getParameterMeta() const override {
        ParameterMeta meta;
        meta.typeId = QMetaType::Int;
        meta.minimum = 0;
        meta.maximum = 4096;
        meta.singleStep = 1;
        meta.defaultValue = 100;
        return { {"x", meta}, {"y", meta}, {"width", meta}, {"height", meta} };
    }
};


// 2. 像素化节点（输出 PixelGrid）

class PixelateNode : public BaseNode {
    int m_blockSize = 8;
public:
    PixelateNode() {
        m_name = "像素化";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
        m_outputs.push_back({ "PixelGrid", ImageType::PixelGrid, QVariant() });
    }
    QString typeName() const override { return "Pixelate"; }
    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        cv::Mat src = packet.image;
        if (src.empty()) return;
        int w = src.cols, h = src.rows;
        int bw = std::max(1, w / m_blockSize);
        int bh = std::max(1, h / m_blockSize);
        cv::Mat small;
        cv::resize(src, small, cv::Size(bw, bh), 0, 0, cv::INTER_LINEAR);
        cv::Mat result;
        cv::resize(small, result, cv::Size(w, h), 0, 0, cv::INTER_NEAREST);
        ImagePacket out(result, ImageType::PixelGrid);
        out.pixelBlockSize = m_blockSize;
        m_outputs[0].data = QVariant::fromValue(out);
    }
    QMap<QString, QVariant> getParameters() const override { return { {"blockSize", m_blockSize} }; }
    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "blockSize") m_blockSize = value.toInt();
    }
    QMap<QString, ParameterMeta> getParameterMeta() const override {
        ParameterMeta meta;
        meta.typeId = QMetaType::Int;
        meta.minimum = 2;
        meta.maximum = 50;
        meta.singleStep = 1;
        meta.defaultValue = 8;
        return { {"blockSize", meta} };
    }
};


// 3. 调色板限制节点（输出 PaletteIndexed）

class PaletteReduceNode : public BaseNode {
    QString m_paletteName = "GameBoy";
    QVector<cv::Vec3b> m_palette;
    QVector<QString> m_colorCodes;   // 新增：色号列表

    void loadPalette(const QString& name) {
        m_palette.clear();
        m_colorCodes.clear();
        if (name == "GameBoy") {
            m_palette = { {0x0F,0x38,0x0F}, {0x30,0x62,0x30}, {0x8B,0xAC,0x0F}, {0x9B,0xBC,0x0F} };
            m_colorCodes = { "Dark", "Mid", "Light", "Lighter" };
        }
        else if (name == "BlackWhite") {
            m_palette = { {0,0,0}, {255,255,255} };
            m_colorCodes = { "Black", "White" };
        }
        else if (name == "Mard221") {
            QFile file("mard221.json");
            if (!file.open(QIODevice::ReadOnly)) {
                qDebug() << "Failed to open mard221.json, using GameBoy fallback";
                loadPalette("GameBoy");
                return;
            }
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();
            QJsonArray colors = root["colors"].toArray();
            for (const QJsonValue& val : colors) {
                QJsonObject obj = val.toObject();
                QString code = obj["code"].toString();
                QJsonArray rgbArr = obj["rgb"].toArray();
                cv::Vec3b rgb(rgbArr[0].toInt(), rgbArr[1].toInt(), rgbArr[2].toInt());
                m_palette.push_back(rgb);
                m_colorCodes.push_back(code);
            }
            qDebug() << "Loaded Mard221 palette with" << m_palette.size() << "colors";
        }
    }

public:
    PaletteReduceNode() {
        m_name = "颜色限制";
        m_inputs.push_back({ "PixelGrid", ImageType::PixelGrid, QVariant() });
        m_outputs.push_back({ "Indexed", ImageType::PaletteIndexed, QVariant() });
        loadPalette("GameBoy");
    }
    QString typeName() const override { return "PaletteReduce"; }

    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;
        cv::Mat src = packet.image;

        cv::Mat indexed(src.size(), CV_8UC1);
        cv::Mat colorResult(src.size(), CV_8UC3);

        for (int y = 0; y < src.rows; ++y) {
            for (int x = 0; x < src.cols; ++x) {
                cv::Vec3b c = src.at<cv::Vec3b>(y, x);
                int bestIdx = 0, bestDist = INT_MAX;
                for (int i = 0; i < m_palette.size(); ++i) {
                    int dr = c[0] - m_palette[i][0];
                    int dg = c[1] - m_palette[i][1];
                    int db = c[2] - m_palette[i][2];
                    int d = dr * dr + dg * dg + db * db;
                    if (d < bestDist) { bestDist = d; bestIdx = i; }
                }
                indexed.at<uchar>(y, x) = bestIdx;
                colorResult.at<cv::Vec3b>(y, x) = m_palette[bestIdx];
            }
        }

        ImagePacket out;
        out.image = colorResult;
        out.indexMap = indexed;
        out.type = ImageType::PaletteIndexed;
        out.paletteName = m_paletteName;
        out.palette = m_palette;
        out.colorCodes = m_colorCodes;   // 传递色号列表
        m_outputs[0].data = QVariant::fromValue(out);
    }

    QMap<QString, QVariant> getParameters() const override {
        return { {"paletteName", m_paletteName} };
    }
    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "paletteName") {
            m_paletteName = value.toString();
            loadPalette(m_paletteName);
        }
    }
};

// 4. Floyd-Steinberg 抖动节点（输入 PaletteIndexed）

class DitherNode : public BaseNode {
public:
    DitherNode() {
        m_name = "抖动";
        m_inputs.push_back({ "Indexed", ImageType::PaletteIndexed, QVariant() });
        m_outputs.push_back({ "Indexed", ImageType::PaletteIndexed, QVariant() });
    }
    QString typeName() const override { return "Dither"; }
    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.type != ImageType::PaletteIndexed || packet.image.empty()) return;
        cv::Mat src = packet.image; // CV_8UC1 索引图
        cv::Mat dst = src.clone();
        // Floyd-Steinberg 简化实现（只做误差扩散，颜色不变）
        // 真实抖动需知道调色板 RGB，此处为演示仅拷贝
        // 完整实现可参考网上代码
        m_outputs[0].data = QVariant::fromValue(packet);
    }
};


// 5. AI 抠图节点（调用 remove.bg，输出 AlphaMasked）

class RemoveBackgroundNode : public BaseNode {
    QString m_apiKey;
    QNetworkAccessManager m_nam;
public:
    RemoveBackgroundNode() {
        m_name = "AI抠图";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
        m_outputs.push_back({ "RGBA", ImageType::AlphaMasked, QVariant() });
        m_apiKey = AppConfig::removeBgApiKey();   // <-- 从 config.h 读取 Key
    }
    QString typeName() const override { return "RemoveBackground"; }
    void process() override;
    QMap<QString, QVariant> getParameters() const override { return { {"apiKey", m_apiKey} }; }
    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "apiKey") m_apiKey = value.toString();
    }
};


// 6. 拼豆图纸节点（接受 PixelGrid 或 PaletteIndexed，输出 BeadPattern）

class BeadPatternNode : public BaseNode {
    int m_cellSize = 20;
    int m_fontSize = 10;
    bool m_showColor = true;
    cv::Mat drawGridFromRGB(const cv::Mat& rgb, int cellSize, int fontSize, bool showColor);
    cv::Mat drawGridFromIndexed(const ImagePacket& packet, int cellSize, int fontSize, bool showColor);
public:
    BeadPatternNode() {
        m_name = "拼豆图纸";
        m_inputs.push_back({ "Grid", ImageType::PixelGrid, QVariant() });
        m_outputs.push_back({ "Pattern", ImageType::BeadPattern, QVariant() });
    }
    QString typeName() const override { return "BeadPattern"; }
    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;
        cv::Mat canvas;
        if (packet.type == ImageType::PixelGrid) {
            canvas = drawGridFromRGB(packet.image, m_cellSize, m_fontSize, m_showColor);
        }
        else if (packet.type == ImageType::PaletteIndexed) {
            canvas = drawGridFromIndexed(packet, m_cellSize, m_fontSize, m_showColor);
        }
        else {
            qDebug() << "BeadPatternNode: unsupported input type";
            return;
        }
        ImagePacket out(canvas, ImageType::BeadPattern);
        m_outputs[0].data = QVariant::fromValue(out);
    }
    QMap<QString, QVariant> getParameters() const override {
        return { {"cellSize", m_cellSize}, {"fontSize", m_fontSize}, {"showColor", m_showColor} };
    }
    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "cellSize") m_cellSize = value.toInt();
        else if (name == "fontSize") m_fontSize = value.toInt();
        else if (name == "showColor") m_showColor = value.toBool();
    }
};

