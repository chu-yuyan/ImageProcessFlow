#pragma once
#include "BaseNode.h"
#include <QFileDialog>
#include "NodeFactory.h"

//加载图像
class LoadImageNode : public BaseNode 
{
    QString m_filePath;
public:
    LoadImageNode() 
    {
        m_name = "Load Image";
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
        m_outputs[0].data = QVariant::fromValue(img);
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
        m_name = "Save Image";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
    }

    QString typeName() const override { return "SaveImage"; }
    
    void process() override 
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;
        cv::Mat img = m_inputs[0].data.value<cv::Mat>();
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
        m_name = "Show Image";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
    }

    QString typeName() const override { return "ShowImage"; }

    void setDisplayCallback(std::function<void(const cv::Mat&)> callback) 
    {
        m_displayCallback = std::move(callback);
    }

    void clearDisplayCallback()
    {
        m_displayCallback = nullptr;
    }

    void process() override 
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;
        cv::Mat img = m_inputs[0].data.value<cv::Mat>();
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
        m_name = "Brightness";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Color, QVariant() });
    }

    QString typeName() const override { return "Brightness"; }

    void process() override 
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;
        cv::Mat src = m_inputs[0].data.value<cv::Mat>();
        if (src.empty()) return;
        cv::Mat dst;
        src.convertTo(dst, -1, 1, m_brightness);
        m_outputs[0].data = QVariant::fromValue(dst);
    }

    QMap<QString, QVariant> getParameters() const override { return { {"brightness", m_brightness} }; }

    void setParameter(const QString& name, const QVariant& value) override 
    {
        if (name == "brightness") m_brightness = value.toInt();
    }
};



//高斯模糊
class BlurNode : public BaseNode 
{
    int m_radius = 3;
public:

    BlurNode() 
    {
        m_name = "Blur";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Color, QVariant() });
    }
    QString typeName() const override { return "Blur"; }

    void process() override 
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;
        cv::Mat src = m_inputs[0].data.value<cv::Mat>();
        if (src.empty()) return;
        int ksize = m_radius * 2 + 1;
        cv::Mat dst;
        cv::GaussianBlur(src, dst, cv::Size(ksize, ksize), 0);
        m_outputs[0].data = QVariant::fromValue(dst);
    }

    QMap<QString, QVariant> getParameters() const override
    {
        return { {"radius", m_radius} };
    }
    void setParameter(const QString & name, const QVariant & value) override 
    {
        if (name == "radius") m_radius = value.toInt();
    }
};


//变成灰度
class GrayNode : public BaseNode 
{
    public:
        GrayNode() 
        {
            m_name = "To Gray";
            m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
            m_outputs.push_back({ "Image", ImageType::Gray, QVariant() });
        }

        QString typeName() const override { return "Gray"; }
        void process() override 
        {
            if (m_inputs.empty() || m_inputs[0].data.isNull()) return;
            cv::Mat src = m_inputs[0].data.value<cv::Mat>();
            if (src.empty()) return;
            cv::Mat gray;
            cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
            m_outputs[0].data = QVariant::fromValue(gray);
        }
};

//调整大小
class ResizeNode : public BaseNode 
{
    int m_width = 640;
    int m_height = 480;
public:
    ResizeNode() 
    {
        m_name = "Resize";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Color, QVariant() });
    }
    QString typeName() const override { return "Resize"; }
    void process() override {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;
        cv::Mat src = m_inputs[0].data.value<cv::Mat>();
        if (src.empty()) return;
        cv::Mat dst;
        cv::resize(src, dst, cv::Size(m_width, m_height));
        m_outputs[0].data = QVariant::fromValue(dst);
    }
    QMap<QString, QVariant> getParameters() const override { return { {"width", m_width}, {"height", m_height} }; }
    void setParameter(const QString& name, const QVariant& value) override {
        if (name == "width") m_width = value.toInt();
        else if (name == "height") m_height = value.toInt();
    }
};

//旋转
class RotateNode : public BaseNode 
{
    double m_angle = 0.0;
public:
    RotateNode() 
    {
        m_name = "Rotate";
        m_inputs.push_back({ "Image", ImageType::Color, QVariant() });
        m_outputs.push_back({ "Image", ImageType::Color, QVariant() });
    }
    QString typeName() const override { return "Rotate"; }

    void process() override 
    {
        if (m_inputs.empty() || m_inputs[0].data.isNull()) return;
        cv::Mat src = m_inputs[0].data.value<cv::Mat>();
        if (src.empty()) return;
        cv::Point2f center(src.cols / 2.0, src.rows / 2.0);
        cv::Mat rot = cv::getRotationMatrix2D(center, m_angle, 1.0);
        cv::Rect2f bbox = cv::RotatedRect(center, src.size(), m_angle).boundingRect2f();
        rot.at<double>(0, 2) += bbox.width / 2.0 - center.x;
        rot.at<double>(1, 2) += bbox.height / 2.0 - center.y;
        cv::Mat dst;
        cv::warpAffine(src, dst, rot, bbox.size());
        m_outputs[0].data = QVariant::fromValue(dst);
    }
    QMap<QString, QVariant> getParameters() const override { return { {"angle", m_angle} }; }
    void setParameter(const QString& name, const QVariant& value) override 
    {
        if (name == "angle") m_angle = value.toDouble();
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
    return true;
}
static bool _registered = registerAllNodes();