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
    QWidget* m_propertyPanel = nullptr;
    QWidget* m_propertyContent = nullptr;

    BaseNode* m_currentNodeForProperties = nullptr;

    // 只允许一个 ShowImage 输出到预览：用指针，别用不存在的 id()
    ShowImageNode* m_activeShowNode = nullptr;
};
