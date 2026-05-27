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
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "Crop"; }

    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;

        cv::Rect roi(cv::Point(m_x, m_y), cv::Size(m_w, m_h));
        roi &= cv::Rect(0, 0, packet.image.cols, packet.image.rows);

        ImagePacket out = packet;
        out.image = packet.image(roi).clone();
        // type 保持原类型
        m_outputs[0].data = QVariant::fromValue(out);
    }

    QMap<QString, QVariant> getParameters() const override {
        return { {"起始x", m_x}, {"起始y", m_y}, {"宽", m_w}, {"高", m_h} };
    }

    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "起始x") m_x = value.toInt();
        else if (name == "起始y") m_y = value.toInt();
        else if (name == "宽") m_w = value.toInt();
        else if (name == "高") m_h = value.toInt();
    }

    QMap<QString, ParameterMeta> getParameterMeta() const override {
        ParameterMeta meta;
        meta.typeId = QMetaType::Int;
        meta.minimum = 0;
        meta.maximum = 4096;
        meta.singleStep = 1;
        meta.defaultValue = 100;
        return { {"起始x", meta}, {"起始y", meta}, {"宽", meta}, {"高", meta} };
    }
};

// 2. 像素化节点（输出 PixelGrid）
class PixelateNode : public BaseNode {
    int m_blockSize = 8;
public:
    PixelateNode() {
        m_name = "像素化";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });  // 改为 Any
        m_outputs.push_back({ "PixelGrid", ImageType::PixelGrid, QVariant() });
    }

    QString typeName() const override { return "Pixelate"; }

    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        cv::Mat src = packet.image;
        if (src.empty()) return;

        // 如果是 RGBA，先转 BGR
        cv::Mat srcBGR;
        if (src.channels() == 4) {
            cv::cvtColor(src, srcBGR, cv::COLOR_BGRA2BGR);
        }
        else {
            srcBGR = src;
        }

        int w = srcBGR.cols, h = srcBGR.rows;
        int bw = std::max(1, w / m_blockSize);
        int bh = std::max(1, h / m_blockSize);

        // 缩小到网格尺寸
        cv::Mat small;
        cv::resize(srcBGR, small, cv::Size(bw, bh), 0, 0, cv::INTER_LINEAR);

        // 输出网格图像（每个像素代表一个格子）
        ImagePacket out(small, ImageType::PixelGrid);
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

    void loadPalette(const QString& name);


public:
    PaletteReduceNode() {
        m_name = "颜色限制";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
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
    void process() override;
};


// 5. AI 抠图节点（调用 remove.bg，输出 AlphaMasked）

class RemoveBackgroundNode : public BaseNode {
    QString m_apiKey;
    QNetworkAccessManager m_nam;
public:
    RemoveBackgroundNode() {
        m_name = "AI抠图";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
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


// 6. 拼豆图纸节点（接受 PixelGrid，内部使用 Mard221 调色板显示色号）
class BeadPatternNode : public BaseNode {
    int m_cellSize = 20;
    int m_fontSize = 10;
    bool m_showColor = true;

    // Mard221 调色板（硬编码，不依赖外部 JSON）
    QVector<cv::Vec3b> m_palette;
    QVector<QString> m_colorCodes;

    void initMard221Palette() {
        m_palette.clear();
        m_colorCodes.clear();

        // 直接从 JSON 数据中提取的部分常用色（完整版可以从文件加载）
        // 或者保留从 mard221.json 加载的逻辑
        QFile file("mard221.json");
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject root = doc.object();
            QJsonArray colors = root["colors"].toArray();
            for (const QJsonValue& val : colors) {
                QJsonObject obj = val.toObject();
                QString code = obj["code"].toString();
                QJsonArray rgbArr = obj["rgb"].toArray();
                const int r = rgbArr.size() > 0 ? rgbArr[0].toInt() : 0;
                const int g = rgbArr.size() > 1 ? rgbArr[1].toInt() : 0;
                const int b = rgbArr.size() > 2 ? rgbArr[2].toInt() : 0;
                m_palette.push_back(cv::Vec3b(b, g, r));  // BGR
                m_colorCodes.push_back(code);
            }
            qDebug() << "Loaded Mard221 palette with" << m_palette.size() << "colors";
        }
        else {
            // 如果找不到文件，使用 GameBoy 作为后备
            qDebug() << "Failed to open mard221.json, using GameBoy fallback";
            m_palette = { {0x0F,0x38,0x0F}, {0x30,0x62,0x30}, {0x8B,0xAC,0x0F}, {0x9B,0xBC,0x0F} };
            m_colorCodes = { "Dark", "Mid", "Light", "Lighter" };
        }
    }

public:
    BeadPatternNode() {
        m_name = "拼豆图纸";
        m_inputs.push_back({ "Grid", ImageType::PixelGrid, QVariant() });
        m_outputs.push_back({ "Pattern", ImageType::BeadPattern, QVariant() });
        m_cellSize = 40;  // 从20改为40
        m_fontSize = 14;  // 从10改为14
        initMard221Palette();
    }

    QString typeName() const override { return "BeadPattern"; }

    void process() override;

    QMap<QString, QVariant> getParameters() const override {
        return { {"cellSize", m_cellSize}, {"fontSize", m_fontSize}, {"showColor", m_showColor} };
    }

    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "cellSize") m_cellSize = value.toInt();
        else if (name == "fontSize") m_fontSize = value.toInt();
        else if (name == "showColor") m_showColor = value.toBool();
    }

    QMap<QString, ParameterMeta> getParameterMeta() const override {
        ParameterMeta cellMeta;
        cellMeta.typeId = QMetaType::Int;
        cellMeta.minimum = 10;
        cellMeta.maximum = 100;
        cellMeta.singleStep = 1;
        cellMeta.defaultValue = 20;

        ParameterMeta fontMeta;
        fontMeta.typeId = QMetaType::Int;
        fontMeta.minimum = 6;
        fontMeta.maximum = 30;
        fontMeta.singleStep = 1;
        fontMeta.defaultValue = 10;

        return { {"cellSize", cellMeta}, {"fontSize", fontMeta} };
    }
};
// 7. AlphaMasked 转 Any 节点（RGBA -> BGR）
class AlphaToAnyNode : public BaseNode {
public:
    AlphaToAnyNode() {
        m_name = "Alpha转RGB";
        m_inputs.push_back({ "RGBA", ImageType::AlphaMasked, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "AlphaToAny"; }

    void process() override {
        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;

        cv::Mat result;

        if (packet.type == ImageType::AlphaMasked && packet.image.channels() == 4) {
            // RGBA 转 BGR
            cv::cvtColor(packet.image, result, cv::COLOR_BGRA2BGR);
        }
        else if (packet.image.channels() == 3) {
            // 已经是 BGR，直接复制
            result = packet.image.clone();
        }
        else {
            qDebug() << "AlphaToAnyNode: unsupported channels" << packet.image.channels();
            return;
        }

        ImagePacket out(result, ImageType::Any);
        m_outputs[0].data = QVariant::fromValue(out);
    }
};

//汇合
class MergeImageNode : public BaseNode {
    double m_blendRatio = 0.5;
    QString m_mergeMode = "Blend";

    // 辅助函数：统一图像格式
    cv::Mat normalizeImage(const cv::Mat& src) {
        cv::Mat dst;
        if (src.channels() == 1) {
            cv::cvtColor(src, dst, cv::COLOR_GRAY2BGR);
        }
        else if (src.channels() == 4) {
            cv::cvtColor(src, dst, cv::COLOR_BGRA2BGR);
        }
        else {
            dst = src.clone();
        }
        return dst;
    }

public:
    MergeImageNode() {
        m_name = "图像比对";
        m_inputs.push_back({ "Image A", ImageType::Any, QVariant() });
        m_inputs.push_back({ "Image B", ImageType::Any, QVariant() });
        m_outputs.push_back({ "Merged", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "MergeImage"; }

    void process() override {
        if (m_inputs.size() < 2) return;

        auto packetA = m_inputs[0].data.value<ImagePacket>();
        auto packetB = m_inputs[1].data.value<ImagePacket>();

        qDebug() << "=== MergeImageNode Debug ===";
        qDebug() << "Input A empty:" << packetA.image.empty()
            << "size:" << packetA.image.cols << "x" << packetA.image.rows;
        qDebug() << "Input B empty:" << packetB.image.empty()
            << "size:" << packetB.image.cols << "x" << packetB.image.rows;

        if (packetA.image.empty() || packetB.image.empty()) {
            qDebug() << "One input is empty!";
            return;
        }

        // 1. 统一通道数（转3通道BGR）
        cv::Mat imgA = normalizeImage(packetA.image);
        cv::Mat imgB = normalizeImage(packetB.image);

        // 2. 统一尺寸（取较小尺寸）
        if (imgA.size() != imgB.size()) {
            cv::Size targetSize(
                std::min(imgA.cols, imgB.cols),
                std::min(imgA.rows, imgB.rows)
            );
            if (imgA.size() != targetSize) {
                cv::resize(imgA, imgA, targetSize);
            }
            if (imgB.size() != targetSize) {
                cv::resize(imgB, imgB, targetSize);
            }
        }

        cv::Mat result;

        if (m_mergeMode == "Blend") {
            cv::addWeighted(imgA, m_blendRatio, imgB, 1 - m_blendRatio, 0, result);
        }
        else if (m_mergeMode == "Max") {
            cv::max(imgA, imgB, result);
        }
        else if (m_mergeMode == "Min") {
            cv::min(imgA, imgB, result);
        }
        else if (m_mergeMode == "Average") {
            cv::add(imgA, imgB, result);
            result = result / 2;
        }

        ImagePacket out(result, ImageType::Color);  // 输出统一为Color类型
        m_outputs[0].data = QVariant::fromValue(out);
    }

    QMap<QString, QVariant> getParameters() const override {
        return { {"blendRatio", m_blendRatio}, {"mergeMode", m_mergeMode} };
    }

    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "blendRatio") m_blendRatio = value.toDouble();
        else if (name == "mergeMode") m_mergeMode = value.toString();
    }
};