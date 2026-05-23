#include "QtWidgetsApplication.h"
#include <opencv2/opencv.hpp>
#include <QGraphicsPixmapItem>
#include <QPixmap>

#include "NodeItem.h"
#include "NodeScene.h"
#include "NodeFactory.h"
#include "nodes.h"
#include "NodeView.h"

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

static bool s_matRegistered = []() {
    qRegisterMetaType<cv::Mat>("cv::Mat");
    return true;
    }();

QtWidgetsApplication::QtWidgetsApplication(QWidget* parent)
    : QMainWindow(parent)
{
    ui.setupUi(this);

    // 从 UI 中获取控件
    m_previewLabel = findChild<QLabel*>("previewLabel");
    if (!m_previewLabel) {
        qDebug() << "Error: previewLabel not found!";
    }
    else {
        m_previewLabel->setAlignment(Qt::AlignCenter);
        m_previewLabel->setScaledContents(true);
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

        // 调试用：确认区域存在（稳定后可删）
        m_propertyPanel->setStyleSheet("background:#f5f5f5; border:1px solid #cccccc;");
    }

    // ========== 创建自己的工具栏（不依赖 UI） ==========
    ui.mainToolBar->setWindowTitle("Execution");
    ui.mainToolBar->setVisible(true);
    ui.mainToolBar->show();

    auto* executeAction = new QAction("Execute Workflow", this);
    ui.mainToolBar->addAction(executeAction);
    connect(executeAction, &QAction::triggered, this, &QtWidgetsApplication::executeWorkflow);
    // 确保工具栏显示
    //myToolBar->show();
    // ===============================================

    initSceneAndView();
    initNodeList();

    if (auto* list = findChild<QListWidget*>("nodeListWidget")) {
        connect(list, &QListWidget::itemDoubleClicked, this, &QtWidgetsApplication::onNodeListItemDoubleClicked);
    }
}

QtWidgetsApplication::~QtWidgetsApplication() = default;

void QtWidgetsApplication::displayImage(const cv::Mat& image)
{
    if (!m_previewLabel) return;

    // 关键：把 UI 更新投递到 GUI 线程
    cv::Mat imgCopy = image.clone();
    QMetaObject::invokeMethod(this, [this, imgCopy]() mutable {
        if (!m_previewLabel) return;

        if (imgCopy.empty()) {
            m_previewLabel->clear();
            m_previewLabel->setText("No Image");
            return;
        }

        cv::Mat rgb;
        if (imgCopy.channels() == 3) {
            cv::cvtColor(imgCopy, rgb, cv::COLOR_BGR2RGB);
            QImage qimg(rgb.data, rgb.cols, rgb.rows, static_cast<int>(rgb.step), QImage::Format_RGB888);
            m_previewLabel->setPixmap(QPixmap::fromImage(qimg.copy()));
        } else if (imgCopy.channels() == 1) {
            QImage qimg(imgCopy.data, imgCopy.cols, imgCopy.rows, static_cast<int>(imgCopy.step), QImage::Format_Grayscale8);
            m_previewLabel->setPixmap(QPixmap::fromImage(qimg.copy()));
        } else {
            m_previewLabel->setText("Unsupported image format");
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
    QStringList types = { "LoadImage","SaveImage","ShowImage",
                         "Brightness","Blur","Gray","Resize","Rotate" };

    for (const QString& type : types) {
        auto* item = new QListWidgetItem(type);
        item->setData(Qt::UserRole, type);
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
        // 只允许一个 ShowImage 输出到右侧预览：新的替换旧的
        if (m_activeShowNode && m_activeShowNode != show) {
            m_activeShowNode->clearDisplayCallback();
        }

        m_activeShowNode = show;
        show->setDisplayCallback([this](const cv::Mat& img) { displayImage(img); });
    }

    auto* nodeItem = new NodeItem(typeName);
    nodeItem->setLogicNode(std::move(logicNode));

    // 创建完立刻在右侧显示该节点参数
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

    if (auto* oldLayout = m_propertyContent->layout()) {
        QLayoutItem* child = nullptr;
        while ((child = oldLayout->takeAt(0)) != nullptr) {
            if (child->widget()) child->widget()->deleteLater();
            delete child;
        }
        delete oldLayout;
    }
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

    for (auto it = params.begin(); it != params.end(); ++it) {
        const QString key = it.key();
        const QVariant val = it.value();

        if (val.typeId() == QMetaType::Int) {
            auto* spin = new QSpinBox(m_propertyContent);
            spin->setRange(-99999, 99999);
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
            spin->setRange(-99999, 99999);
            spin->setDecimals(2);
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
        else {
            auto* edit = new QLineEdit(val.toString(), m_propertyContent);
            form->addRow(key, edit);

            connect(edit, &QLineEdit::editingFinished,
                    this, [this, key, edit]() {
                        if (m_currentNodeForProperties)
                            m_currentNodeForProperties->setParameter(key, edit->text());
                    });
        }
    }

    m_propertyContent->update();
    m_propertyContent->updateGeometry();
    m_propertyContent->update();
}