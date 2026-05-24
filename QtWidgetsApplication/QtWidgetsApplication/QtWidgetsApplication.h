#pragma once

#include <opencv2/opencv.hpp>
#include <QtWidgets/QMainWindow>
#include <memory>
#include "ui_QtWidgetsApplication.h"

class QListWidgetItem;
class NodeScene;
class QLabel;
class BaseNode;
class ShowImageNode;

class ZoomableImageView : public QWidget {
    Q_OBJECT
public:
    explicit ZoomableImageView(QWidget* parent = nullptr);
    void setImage(const cv::Mat& image);
    void setZoom(qreal zoom);
    qreal zoom() const { return m_zoom; }
    void zoomIn();
    void zoomOut();
    void fitToView();

protected:
    void paintEvent(QPaintEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void updatePixmap();

    cv::Mat m_image;
    QPixmap m_pixmap;
    qreal m_zoom = 1.0;
    bool m_fitToView = true;
};

class QtWidgetsApplication : public QMainWindow
{
    Q_OBJECT
public:
    QtWidgetsApplication(QWidget* parent = nullptr);
    ~QtWidgetsApplication();

    void displayImage(const cv::Mat& image);

private slots:
    void onNodeListItemDoubleClicked(QListWidgetItem* item);
    void executeWorkflow();

public slots:
    void saveWorkflow();
    void loadWorkflow();

private:
    void initNodeList();
    void initSceneAndView();
    void showParametersForNode(BaseNode* node);
    void clearPropertyPanel();

    Ui::QtWidgetsApplicationClass ui;
    std::unique_ptr<NodeScene> m_scene;

    QLabel* m_previewLabel = nullptr;
    ZoomableImageView* m_imageView = nullptr;  // 替换原来的 QLabel
    QWidget* m_propertyPanel = nullptr;
    QWidget* m_propertyContent = nullptr;

    BaseNode* m_currentNodeForProperties = nullptr;

    // 只允许一个 ShowImage 输出到预览
    ShowImageNode* m_activeShowNode = nullptr;
};