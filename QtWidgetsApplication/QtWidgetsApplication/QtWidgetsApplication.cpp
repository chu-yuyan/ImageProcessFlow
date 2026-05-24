#include "QtWidgetsApplication.h"
#include <opencv2/opencv.hpp>
#include <QGraphicsPixmapItem>
#include <QPixmap>

#include "NodeItem.h"
#include "NodeScene.h"
#include "NodeFactory.h"
#include "nodes.h"
#include "NodeView.h"
#include "WorkflowSerializer.h"
#include "WorkflowExecutor.h"

#include <QGraphicsView>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPointF>
#include <QLabel>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QFormLayout>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QLayoutItem>
#include <limits>
#include <QComboBox>
#include<qevent.h>

// ========== ZoomableImageView 实现 ==========
ZoomableImageView::ZoomableImageView(QWidget* parent) : QWidget(parent) {
    setMinimumSize(100, 100);
    setStyleSheet("background-color: #2a2a2a;");
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void ZoomableImageView::setImage(const cv::Mat& image) {
    m_image = image.clone();
    if (m_fitToView) {
        fitToView();
    }
    updatePixmap();
    update();
}

void ZoomableImageView::updatePixmap() {
    if (m_image.empty()) {
        m_pixmap = QPixmap();
        return;
    }

    // 确保是连续内存
    cv::Mat src = m_image;
    if (!src.isContinuous()) {
        src = src.clone();
    }

    if (src.channels() == 3) {  // BGR
        cv::Mat rgb;
        cv::cvtColor(src, rgb, cv::COLOR_BGR2RGB);
        QImage qimg(rgb.data, rgb.cols, rgb.rows,
            static_cast<int>(rgb.step),
            QImage::Format_RGB888);
        m_pixmap = QPixmap::fromImage(qimg);
    }
    else if (src.channels() == 1) {  // Gray
        QImage qimg(src.data, src.cols, src.rows,
            static_cast<int>(src.step),
            QImage::Format_Grayscale8);
        m_pixmap = QPixmap::fromImage(qimg);
    }
    else if (src.channels() == 4) {  // BGRA
        cv::Mat rgba;
        cv::cvtColor(src, rgba, cv::COLOR_BGRA2RGBA);
        QImage qimg(rgba.data, rgba.cols, rgba.rows,
            static_cast<int>(rgba.step),
            QImage::Format_RGBA8888);
        m_pixmap = QPixmap::fromImage(qimg);
    }
    else {
        m_pixmap = QPixmap();
        return;
    }

    // 应用缩放
    if (!m_pixmap.isNull() && m_zoom != 1.0) {
        QSize newSize = m_pixmap.size() * m_zoom;
        m_pixmap = m_pixmap.scaled(newSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
}

void ZoomableImageView::paintEvent(QPaintEvent* event) {
    QPainter painter(this);
    painter.fillRect(rect(), QColor(42, 42, 42));

    if (m_pixmap.isNull()) {
        painter.setPen(QColor(150, 150, 150));
        painter.drawText(rect(), Qt::AlignCenter, "No Image");
        return;
    }

    // 居中显示
    QPoint pos((width() - m_pixmap.width()) / 2, (height() - m_pixmap.height()) / 2);
    painter.drawPixmap(pos, m_pixmap);

    // 绘制边框
    painter.setPen(QPen(QColor(80, 80, 80), 1));
    painter.drawRect(QRect(pos, m_pixmap.size()));
}

void ZoomableImageView::wheelEvent(QWheelEvent* event) {
    m_fitToView = false;
    if (event->angleDelta().y() > 0) {
        zoomIn();
    }
    else {
        zoomOut();
    }
    updatePixmap();
    update();
    event->accept();
}

void ZoomableImageView::resizeEvent(QResizeEvent* event) {
    if (m_fitToView) {
        fitToView();
    }
    QWidget::resizeEvent(event);
}

void ZoomableImageView::zoomIn() {
    m_zoom *= 1.15;
    m_zoom = qMin(m_zoom, 5.0);
}

void ZoomableImageView::zoomOut() {
    m_zoom /= 1.15;
    m_zoom = qMax(m_zoom, 0.1);
}

void ZoomableImageView::fitToView() {
    if (m_image.empty()) return;

    qreal scaleX = (qreal)width() / m_image.cols;
    qreal scaleY = (qreal)height() / m_image.rows;
    m_zoom = qMin(scaleX, scaleY);
    m_zoom = qMax(m_zoom, 0.1);
    m_fitToView = true;
    updatePixmap();
}

QtWidgetsApplication::QtWidgetsApplication(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    // 替换 previewLabel 为 ZoomableImageView
    QWidget* rightWidget = findChild<QWidget*>("rightWidget");
    if (rightWidget && rightWidget->layout()) {
        QVBoxLayout* layout = qobject_cast<QVBoxLayout*>(rightWidget->layout());
        if (layout) {
            QLabel* originalLabel = findChild<QLabel*>("previewLabel");
            if (originalLabel) {
                layout->removeWidget(originalLabel);
                originalLabel->hide();
                originalLabel->deleteLater();
            }

            m_imageView = new ZoomableImageView(rightWidget);
            layout->insertWidget(0, m_imageView);
            layout->setStretch(0, 1);
        }
    }

    if (!m_imageView) {
        m_imageView = new ZoomableImageView(this);
    }


    m_propertyPanel = findChild<QWidget*>("propertyPanel");
    if (!m_propertyPanel) {
        qDebug() << "Error: propertyPanel not found!";
    } else {
        m_propertyPanel->setMinimumHeight(160);
        m_propertyPanel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

        // 外层布局只建一次
        if (!m_propertyPanel->layout()) {
            auto* layout = new QVBoxLayout(m_propertyPanel);
            layout->setContentsMargins(4, 4, 4, 4);
            layout->setSpacing(4);
            m_propertyPanel->setLayout(layout);
        }

        // 内容容器（真正承载动态表单）
        if (!m_propertyContent) {
            m_propertyContent = new QWidget(m_propertyPanel);
            m_propertyContent->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

            auto* layout = qobject_cast<QVBoxLayout*>(m_propertyPanel->layout());
            layout->addWidget(m_propertyContent);
            layout->addStretch(1);
        }

    }

    // ========== 创建自己的工具栏（不依赖 UI） ==========
    ui.mainToolBar->setWindowTitle("Execution");
    ui.mainToolBar->setVisible(true);
    ui.mainToolBar->show();

    //仨按钮
    auto* executeAction = new QAction("Execute Workflow", this);
    ui.mainToolBar->addAction(executeAction);
    connect(executeAction, &QAction::triggered, this, &QtWidgetsApplication::executeWorkflow);
 
    auto* saveAction = new QAction("Save Workflow", this);
    ui.mainToolBar->addAction(saveAction);
    connect(saveAction, &QAction::triggered, this, &QtWidgetsApplication::saveWorkflow);

    auto* loadAction = new QAction("Load Workflow", this);
    ui.mainToolBar->addAction(loadAction);
    connect(loadAction, &QAction::triggered, this, &QtWidgetsApplication::loadWorkflow);
    initSceneAndView();
    initNodeList();

    if (m_scene) {
        connect(m_scene.get(), &NodeScene::nodeAboutToBeRemoved, this, [this](BaseNode* logic) {
            if (!logic) return;

            // 1) 如果参数面板正显示该节点，清空，避免悬空指针
            if (m_currentNodeForProperties == logic) {
                showParametersForNode(nullptr);
            }

            // 2) 如果删的是 active show，清回调并置空指针，同时清空预览
            if (m_activeShowNode && m_activeShowNode == logic) {
                m_activeShowNode->clearDisplayCallback();
                m_activeShowNode = nullptr;

                displayImage(cv::Mat()); // 这里：清空右侧预览
            }
        });
    }

    if (auto* list = findChild<QListWidget*>("nodeListWidget")) {
        connect(list, &QListWidget::itemDoubleClicked, this, &QtWidgetsApplication::onNodeListItemDoubleClicked);
    }
}

QtWidgetsApplication::~QtWidgetsApplication() = default;

void QtWidgetsApplication::displayImage(const cv::Mat& image)
{
    if (!m_imageView) return;

    cv::Mat imgCopy = image.clone();
    QMetaObject::invokeMethod(this, [this, imgCopy]() {
        if (m_imageView) {
            m_imageView->setImage(imgCopy);
        }
        }, Qt::QueuedConnection);
}

void QtWidgetsApplication::executeWorkflow()
{
    ui.logTextEd->append("Execute clicked");

    if (!m_scene) return;
    bool success = m_scene->executeWorkflow();
    ui.logTextEd->append(success ? "Workflow executed successfully."
                                : "Execution failed: maybe cyclic dependency or missing data.");
}

void QtWidgetsApplication::initSceneAndView()
{
    auto* view = findChild<QGraphicsView*>("graphicsView");
    if (!view) {
        qDebug() << "Error: graphicsView not found!";
        return;
    }

    m_scene = std::make_unique<NodeScene>(this);
    view->setScene(m_scene.get());
    view->setSceneRect(m_scene->sceneRect());
    view->centerOn(0.0, 0.0);

    // 点击/框选导致 selection 变化时，刷新右侧参数面板
    connect(m_scene.get(), &QGraphicsScene::selectionChanged, this, [this]() {
        if (!m_scene) return;

        const auto selected = m_scene->selectedItems();
        if (selected.isEmpty()) {
            showParametersForNode(nullptr);
            return;
        }

        // 只取第一个选中节点
        auto* nodeItem = dynamic_cast<NodeItem*>(selected.first());
        if (!nodeItem) {
            showParametersForNode(nullptr);
            return;
        }

        showParametersForNode(nodeItem->logicNode());
    });
}

void QtWidgetsApplication::initNodeList()
{
    auto* list = findChild<QListWidget*>("nodeListWidget");
    if (!list) return;
    list->clear();

    struct NodeEntry {
        QString key;   // 工厂 key / typeName
        QString text;  // UI 显示
    };

    const QList<NodeEntry> entries = 
    {
        { "LoadImage",   "加载图像" },
        { "SaveImage",   "保存图像" },
        { "ShowImage",   "显示图像" },
        { "Brightness",  "亮度调整" },
        { "Blur",        "高斯模糊" },
        { "Gray",        "灰度化" },
        { "Resize",      "调整大小" },
        { "Rotate",      "旋转" },
        { "Crop",           "裁剪" },
        { "Pixelate",       "像素化" },
        { "PaletteReduce",  "颜色限制" },
        { "Dither",         "抖动" },
        { "RemoveBackground","AI抠图" },
        { "BeadPattern",    "拼豆图纸" },
        { "AlphaToAny",  "Alpha转RGB" }
    };

    for (const auto& e : entries) {
        auto* item = new QListWidgetItem(e.text);
        item->setData(Qt::UserRole, e.key);
        list->addItem(item);
    }
}

void QtWidgetsApplication::onNodeListItemDoubleClicked(QListWidgetItem* item)
{
    if (!item || !m_scene) return;
    QString typeName = item->data(Qt::UserRole).toString();
    auto logicNode = NodeFactory::instance().create(typeName);
    if (!logicNode) return;

    LoadImageNode* load = dynamic_cast<LoadImageNode*>(logicNode.get());
    SaveImageNode* save = dynamic_cast<SaveImageNode*>(logicNode.get());
    ShowImageNode* show = dynamic_cast<ShowImageNode*>(logicNode.get());

    if (load) {
        QString path = QFileDialog::getOpenFileName(nullptr, "Select Image", "", "Images (*.png *.jpg *.bmp)");
        if (path.isEmpty()) return;
        load->setFilePath(path);
    }
    else if (save) {
        QString path = QFileDialog::getSaveFileName(nullptr, "Save Image", "", "Images (*.png *.jpg)");
        if (path.isEmpty()) return;
        save->setFilePath(path);
    }
    else if (show) {
        if (m_activeShowNode && m_activeShowNode != show) {
            m_activeShowNode->clearDisplayCallback();
        }
        m_activeShowNode = show;
        show->setDisplayCallback([this](const cv::Mat& img) { displayImage(img); });
    }

    auto* nodeItem = new NodeItem(typeName);
    nodeItem->setLogicNode(std::move(logicNode));

    BaseNode* rawNode = nodeItem->logicNode();
    showParametersForNode(rawNode);

    QGraphicsView* view = findChild<QGraphicsView*>("graphicsView");
    QPointF center = view ? view->mapToScene(view->viewport()->rect().center()) : QPointF(0, 0);
    nodeItem->setPos(center);
    m_scene->addNode(nodeItem);

    m_scene->clearSelection();
    nodeItem->setSelected(true);
}

void QtWidgetsApplication::clearPropertyPanel()
{
    if (!m_propertyContent) return;

    QLayout* layout = m_propertyContent->layout();
    if (!layout) return;

    // 循环删除所有子项，直到没有为止
    QLayoutItem* child;
    while ((child = layout->takeAt(0)) != nullptr) {
        if (child->widget()) {
            child->widget()->deleteLater();
        }
        delete child;
    }
    delete layout;   // 删除旧的布局
    m_propertyContent->setLayout(nullptr);
}

void QtWidgetsApplication::showParametersForNode(BaseNode* node)
{
    m_currentNodeForProperties = node;
    if (!m_propertyContent) return;

    // 清空旧 layout
    if (auto* old = m_propertyContent->layout()) {
        QLayoutItem* item = nullptr;
        while ((item = old->takeAt(0)) != nullptr) {
            if (item->widget()) item->widget()->deleteLater();
            delete item;
        }
        delete old;
    }

    auto* form = new QFormLayout();
    form->setContentsMargins(8, 8, 8, 8);
    form->setSpacing(6);
    m_propertyContent->setLayout(form);

    if (!node) {
        form->addRow(new QLabel("No node selected", m_propertyContent));
        m_propertyContent->update();
        return;
    }

    auto params = node->getParameters();
    if (params.isEmpty()) {
        form->addRow(new QLabel("No parameters", m_propertyContent));
        m_propertyContent->update();
        return;
    }

    const auto metas = node->getParameterMeta();

    for (auto it = params.begin(); it != params.end(); ++it) {
        const QString key = it.key();
        const QVariant val = it.value();
        const auto metaIt = metas.find(key);
        const ParameterMeta* meta = (metaIt != metas.end()) ? &metaIt.value() : nullptr;

        if (val.typeId() == QMetaType::Int) {
            auto* spin = new QSpinBox(m_propertyContent);

            if (meta) {
                spin->setRange(meta->minimum.toInt(), meta->maximum.toInt());
                spin->setSingleStep(meta->singleStep.toInt());
            } else {
                spin->setRange(std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
                spin->setSingleStep(1);
            }

            spin->setAccelerated(true);
            spin->setValue(val.toInt());
            form->addRow(key, spin);

            connect(spin, QOverload<int>::of(&QSpinBox::valueChanged),
                    this, [this, key](int v) {
                        if (m_currentNodeForProperties)
                            m_currentNodeForProperties->setParameter(key, v);
                    });
        }
        else if (val.typeId() == QMetaType::Double) {
            auto* spin = new QDoubleSpinBox(m_propertyContent);

            if (meta) {
                spin->setRange(meta->minimum.toDouble(), meta->maximum.toDouble());
                spin->setSingleStep(meta->singleStep.toDouble());
            } else {
                spin->setRange(-1e9, 1e9);
                spin->setSingleStep(0.1);
            }

            spin->setDecimals(2);
            spin->setAccelerated(true);
            spin->setValue(val.toDouble());
            form->addRow(key, spin);

            connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
                    this, [this, key](double v) {
                        if (m_currentNodeForProperties)
                            m_currentNodeForProperties->setParameter(key, v);
                    });
        }
        else if (val.typeId() == QMetaType::Bool) {
            auto* cb = new QCheckBox(m_propertyContent);
            cb->setChecked(val.toBool());
            form->addRow(key, cb);

            connect(cb, &QCheckBox::toggled,
                    this, [this, key](bool v) {
                        if (m_currentNodeForProperties)
                            m_currentNodeForProperties->setParameter(key, v);
                    });
        }
        else if (key == "paletteName") {
            struct PaletteInfo {
                QString id;
                QString displayName;
            };

            const auto paletteInfos = []() -> QVector<PaletteInfo> {
                return {
                    { "GameBoy",      "Game Boy" },
                    { "BlackWhite",   "黑白双色" },
                    { "CGA",          "CGA 经典四色" },
                    { "EGA",          "EGA 16色" },
                    { "PICO8",        "PICO-8 像素风" },
                    { "Commodore64",  "Commodore 64" },
                    { "Arne16",       "Arne 16色" },
                    { "Endesga32",    "Endesga 32色" },
                    { "AAP-64",       "AAP-64 调色板" },
                    { "Mard221",      "马尔德 221色拼豆" }
                };
                };

            auto* combo = new QComboBox(m_propertyContent);

            for (const auto& p : paletteInfos()) {
                combo->addItem(p.displayName, p.id);  // 存储内部 ID 在 UserData
            }

            const QString currentId = val.toString();
            int idx = combo->findData(currentId);
            if (idx < 0) idx = combo->findData(QStringLiteral("GameBoy"));
            if (idx < 0) idx = 0;
            combo->setCurrentIndex(idx);

            form->addRow(key, combo);

            // 修改这里：使用 currentData 而不是 currentText
            connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this, combo, key](int index) {
                    if (m_currentNodeForProperties && index >= 0) {
                        QString id = combo->itemData(index).toString();
                        m_currentNodeForProperties->setParameter(key, id);
                    }
                });
        }
    }

    m_propertyContent->update();
    m_propertyContent->updateGeometry();
    m_propertyContent->update();
}
void QtWidgetsApplication::saveWorkflow()
{
    WorkflowData data;
    // 遍历 m_scene 中的节点和连接，填充 data
    for (NodeItem* item : m_scene->nodes()) {  // 您需要在 NodeScene 中暴露 nodes()
        NodeData nd;
        nd.id = item->logicNode()->id();
        nd.type = item->logicNode()->typeName();
        nd.displayName = item->title();
        nd.position = item->pos();
        nd.parameters = item->logicNode()->getParameters();
        data.nodes.append(nd);
    }
    for (Connection* conn : m_scene->connections()) {
        ConnectionData cd;
        cd.fromNodeId = conn->fromNode->logicNode()->id();
        cd.outputPortIndex = conn->outputPortIndex;
        cd.toNodeId = conn->toNode->logicNode()->id();
        cd.inputPortIndex = conn->inputPortIndex;
        data.connections.append(cd);
    }
    QString path = QFileDialog::getSaveFileName(this, "Save Workflow", "", "JSON (*.json)");
    if (!path.isEmpty())
        WorkflowSerializer::saveToFile(data, path);
}

void QtWidgetsApplication::loadWorkflow()
{
    QString path = QFileDialog::getOpenFileName(this, "Load Workflow", "", "JSON (*.json)");
    if (path.isEmpty()) return;
    WorkflowData data = WorkflowSerializer::loadFromFile(path);
    if (data.nodes.isEmpty()) {
        qDebug() << "Failed to load workflow or empty workflow.";
        return;
    }

    // 清空当前场景
    m_scene->clear();
    // 清空旧的活动显示节点指针
    if (m_activeShowNode) {
        m_activeShowNode = nullptr;
    }

    // 1. 创建所有节点，并保存 id -> NodeItem 映射
    QMap<QUuid, NodeItem*> nodeMap;
    for (const NodeData& nd : data.nodes) {
        auto logicNode = NodeFactory::instance().create(nd.type);
        if (!logicNode) {
            qDebug() << "Unknown node type:" << nd.type;
            continue;
        }
        // 恢复参数
        for (auto it = nd.parameters.begin(); it != nd.parameters.end(); ++it) {
            logicNode->setParameter(it.key(), it.value());
        }
        // 特殊处理 ShowImageNode
        if (auto* show = dynamic_cast<ShowImageNode*>(logicNode.get())) {
            // 如果已有活动显示节点，清除其回调（理论上已经被清空，但安全起见）
            if (m_activeShowNode) {
                m_activeShowNode->clearDisplayCallback();
            }
            m_activeShowNode = show;
            show->setDisplayCallback([this](const cv::Mat& img) { displayImage(img); });
        }

        auto* nodeItem = new NodeItem(nd.displayName);
        nodeItem->setLogicNode(std::move(logicNode));
        nodeItem->setPos(nd.position);
        m_scene->addNode(nodeItem);
        nodeMap[nd.id] = nodeItem;
    }

    // 2. 建立连接
    for (const ConnectionData& cd : data.connections) {
        NodeItem* fromNode = nodeMap.value(cd.fromNodeId);
        NodeItem* toNode = nodeMap.value(cd.toNodeId);
        if (!fromNode || !toNode) {
            qDebug() << "Invalid connection: node not found";
            continue;
        }
        NodePort* outPort = fromNode->getOutputPort(cd.outputPortIndex);
        NodePort* inPort = toNode->getInputPort(cd.inputPortIndex);
        if (outPort && inPort) {
            m_scene->onConnectionRequest(outPort, inPort);
        }
        else {
            qDebug() << "Failed to find ports for connection:"
                << " from port" << cd.outputPortIndex
                << " to port" << cd.inputPortIndex;
        }
    }

    // 刷新视图
    if (auto* view = findChild<QGraphicsView*>("graphicsView"))
        view->update();
}