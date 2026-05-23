#pragma once

#include <opencv2/opencv.hpp>
#include <QtWidgets/QMainWindow>
#include <memory>
#include "ui_QtWidgetsApplication.h"

class QListWidgetItem;
class NodeScene;
class QLabel;
class BaseNode;
class QFormLayout;

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

private:
    void initNodeList();
    void initSceneAndView();

    void showParametersForNode(BaseNode* node);
    void clearPropertyPanel();

    Ui::QtWidgetsApplicationClass ui;
    std::unique_ptr<NodeScene> m_scene;

    QLabel* m_previewLabel = nullptr;
    QWidget* m_propertyPanel = nullptr;  // 右侧参数容器（ui里叫 widget）
    QWidget* m_propertyContent = nullptr; // 参数面板真正承载控件的子 QWidget

    BaseNode* m_currentNodeForProperties = nullptr;

    // 解决多个 ShowImage：只允许一个 active
    class ShowImageNode* m_activeShowNode = nullptr;
};
