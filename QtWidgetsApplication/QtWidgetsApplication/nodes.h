#pragma once
#include "BaseNode.h"
#include "ImagePacket.h"
#include <QFileDialog>
#include "NodeFactory.h"
#include "nodes2.h"

//加载图像
class LoadImageNode : public BaseNode
{
    QString m_filePath;
public:
    LoadImageNode()
    {
        m_name = "加载图像";
        m_outputs.push_back({ "Image", ImageType::Color, QVariant() });
    }

    QString typeName() const override { return "LoadImage"; }

    void process() override
    {
        if (m_filePath.isEmpty())
        {
            qDebug() << "No file selected";
            return;
        }
        cv::Mat img = cv::imread(m_filePath.toStdString());
        if (img.empty()) return;

        ImagePacket packet(img, ImageType::Color);
        m_outputs[0].data = QVariant::fromValue(packet);
    }

    void setFilePath(const QString& path) { m_filePath = path; }

    QMap<QString, QVariant> getParameters() const override { return { {"filePath", m_filePath} }; }

    void setParameter(const QString& name, const QVariant& value) override
    {
        if (name == "filePath") m_filePath = value.toString();
    }
};

//保存图像
class SaveImageNode : public BaseNode
{
    QString m_filePath;
public:
    SaveImageNode()
    {
        m_name = "保存图像";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "SaveImage"; }

    void process() override
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;

        auto packet = m_inputs[0].data.value<ImagePacket>();
        const cv::Mat& img = packet.image;

        if (img.empty() || m_filePath.isEmpty()) return;
        cv::imwrite(m_filePath.toStdString(), img);
    }

    void setFilePath(const QString& path) { m_filePath = path; }
    QMap<QString, QVariant> getParameters() const override { return { {"filePath", m_filePath} }; }

    void setParameter(const QString& name, const QVariant& value) override
    {
        if (name == "filePath") m_filePath = value.toString();
    }
};

//显示图片
class ShowImageNode : public BaseNode
{
    std::function<void(const cv::Mat&)> m_displayCallback;

public:
    ShowImageNode()
    {
        m_name = "显示图片";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "ShowImage"; }

    void setDisplayCallback(std::function<void(const cv::Mat&)> callback)
    {
        m_displayCallback = callback;
    }

    // 新增：清除显示回调
    void clearDisplayCallback()
    {
        m_displayCallback = nullptr;
    }

    void process() override
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;

        auto packet = m_inputs[0].data.value<ImagePacket>();
        const cv::Mat& img = packet.image;

        if (img.empty()) return;
        if (m_displayCallback)
        {
            m_displayCallback(img);
        }
    }
};

//亮度
class BrightnessNode : public BaseNode
{
    int m_brightness = 0;
public:
    BrightnessNode()
    {
        m_name = "亮度";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "Brightness"; }

    QMap<QString, QVariant> getParameters() const override { return { {"brightness", m_brightness} }; }

    QMap<QString, ParameterMeta> getParameterMeta() const override
    {
        ParameterMeta meta;
        meta.typeId = QMetaType::Int;
        meta.minimum = -255;
        meta.maximum = 255;
        meta.singleStep = 5;
        meta.defaultValue = 0;
        return { {"brightness", meta} };
    }

    void setParameter(const QString& name, const QVariant& value) override
    {
        if (name == "brightness") m_brightness = value.toInt();
    }

    void process() override
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;

        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;

        cv::Mat dst;
        packet.image.convertTo(dst, -1, 1, m_brightness);

        ImagePacket out(dst, packet.type);
        m_outputs[0].data = QVariant::fromValue(out);
    }
};

//高斯模糊
class BlurNode : public BaseNode
{
    int m_radius = 3;
public:

    BlurNode()
    {
        m_name = "高斯模糊";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Any, QVariant() });
    }
    QString typeName() const override { return "Blur"; }

    void process() override
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;

        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;

        int ksize = m_radius * 2 + 1;
        cv::Mat dst;
        cv::GaussianBlur(packet.image, dst, cv::Size(ksize, ksize), 0);

        ImagePacket out(dst, packet.type);
        m_outputs[0].data = QVariant::fromValue(out);
    }

    QMap<QString, QVariant> getParameters() const override
    {
        return { {"radius", m_radius} };
    }
    void setParameter(const QString& name, const QVariant& value) override
    {
        if (name == "radius") m_radius = value.toInt();
    }

    QMap<QString, ParameterMeta> getParameterMeta() const override
    {
        ParameterMeta meta;
        meta.typeId = QMetaType::Int;
        meta.minimum = 1;
        meta.maximum = 20;
        meta.singleStep = 1;
        meta.defaultValue = 3;
        return { {"radius", meta} };
    }
};

//变成灰度
class GrayNode : public BaseNode
{
public:
    GrayNode()
    {
        m_name = "变成灰度";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Gray, QVariant() });
    }

    QString typeName() const override { return "Gray"; }
    void process() override
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;

        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;

        cv::Mat gray;
        cv::cvtColor(packet.image, gray, cv::COLOR_BGR2GRAY);

        ImagePacket out(gray, ImageType::Gray);
        m_outputs[0].data = QVariant::fromValue(out);
    }
};

//调整大小
class ResizeNode : public BaseNode
{
    int m_width = 400;
    int m_height = 400;
public:
    ResizeNode()
    {
        m_name = "调整大小";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "Resize"; }

    QMap<QString, QVariant> getParameters() const override { return { {"width", m_width}, {"height", m_height} }; }

    QMap<QString, ParameterMeta> getParameterMeta() const override
    {
        ParameterMeta meta;
        meta.typeId = QMetaType::Int;
        meta.minimum = 1;
        meta.maximum = 4096;
        meta.singleStep = 10;
        meta.defaultValue = 400;
        return { {"width", meta}, {"height", meta} };
    }

    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "width") m_width = value.toInt();
        else if (name == "height") m_height = value.toInt();
    }

    void process() override {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;

        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;

        cv::Mat dst;
        cv::resize(packet.image, dst, cv::Size(m_width, m_height));

        ImagePacket out(dst, packet.type);  // 保持原类型
        m_outputs[0].data = QVariant::fromValue(out);
    }
};
//旋转
class RotateNode : public BaseNode
{
    double m_angle = 0.0;
public:
    RotateNode()
    {
        m_name = "旋转";
        m_inputs.push_back({ "Image", ImageType::Any, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Any, QVariant() });
    }

    QString typeName() const override { return "Rotate"; }

    void process() override
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;

        auto packet = m_inputs[0].data.value<ImagePacket>();
        if (packet.image.empty()) return;

        const cv::Mat& src = packet.image;

        cv::Point2f center(src.cols / 2.0F, src.rows / 2.0F);
        cv::Mat rot = cv::getRotationMatrix2D(center, m_angle, 1.0);
        cv::Rect2f bbox = cv::RotatedRect(center, src.size(), static_cast<float>(m_angle)).boundingRect2f();
        rot.at<double>(0, 2) += bbox.width / 2.0 - center.x;
        rot.at<double>(1, 2) += bbox.height / 2.0 - center.y;

        cv::Mat dst;
        cv::warpAffine(src, dst, rot, bbox.size());

        ImagePacket out(dst, packet.type);  // 保持原类型
        m_outputs[0].data = QVariant::fromValue(out);
    }

    QMap<QString, QVariant> getParameters() const override { return { {"angle", m_angle} }; }

    void setParameter(const QString& name, const QVariant& value) override
    {
        if (name == "angle") m_angle = value.toDouble();
    }

    QMap<QString, ParameterMeta> getParameterMeta() const override
    {
        ParameterMeta meta;
        meta.typeId = QMetaType::Double;
        meta.minimum = -360.0;
        meta.maximum = 360.0;
        meta.singleStep = 1.0;
        meta.defaultValue = 0.0;
        return { {"angle", meta} };
    }
};

static bool registerAllNodes()
{
    NodeFactory& f = NodeFactory::instance();
    f.registerNode("LoadImage", []() { return std::make_unique<LoadImageNode>(); });
    f.registerNode("SaveImage", []() { return std::make_unique<SaveImageNode>(); });
    f.registerNode("ShowImage", []() { return std::make_unique<ShowImageNode>(); });
    f.registerNode("Brightness", []() { return std::make_unique<BrightnessNode>(); });
    f.registerNode("Blur", []() { return std::make_unique<BlurNode>(); });
    f.registerNode("Gray", []() { return std::make_unique<GrayNode>(); });
    f.registerNode("Resize", []() { return std::make_unique<ResizeNode>(); });
    f.registerNode("Rotate", []() { return std::make_unique<RotateNode>(); });
    f.registerNode("Crop", []() { return std::make_unique<CropNode>(); });
    f.registerNode("Pixelate", []() { return std::make_unique<PixelateNode>(); });
    f.registerNode("PaletteReduce", []() { return std::make_unique<PaletteReduceNode>(); });
    f.registerNode("Dither", []() { return std::make_unique<DitherNode>(); });
    f.registerNode("RemoveBackground", []() { return std::make_unique<RemoveBackgroundNode>(); });
    f.registerNode("BeadPattern", []() { return std::make_unique<BeadPatternNode>(); });
    f.registerNode("AlphaToAny", []() { return std::make_unique<AlphaToAnyNode>(); });
    f.registerNode("MergeImage", []() { return std::make_unique<MergeImageNode>(); });
    return true;
}
static bool _registered = registerAllNodes();