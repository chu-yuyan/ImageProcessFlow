#include "nodecontroller.h"
#include "nodeport.h"
#include "nodeconnection.h"
#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>

NodeController::NodeController(QGraphicsScene* scene)
    : QObject(scene)
    , m_scene(scene)
{
    scene->installEventFilter(this);
}

bool NodeController::eventFilter(QObject* obj, QEvent* event)
{
    if (auto mouseEvent = dynamic_cast<QGraphicsSceneMouseEvent*>(event)) {
        switch (event->type()) {
        case QEvent::GraphicsSceneMousePress:
            return processMousePress(mouseEvent);
        case QEvent::GraphicsSceneMouseMove:
            return processMouseMove(mouseEvent);
        case QEvent::GraphicsSceneMouseRelease:
            return processMouseRelease(mouseEvent);
        default:
            break;
        }
    }
    return QObject::eventFilter(obj, event);
}

bool NodeController::processMousePress(QGraphicsSceneMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_tempConnection) {
        QGraphicsItem* item = m_scene->itemAt(event->scenePos(), QTransform());
        NodePort* port = dynamic_cast<NodePort*>(item);
        if (port && !port->isInput()) {   // 只允许从输出端口开始拖拽
            m_startPort = port;
            m_tempConnection = new NodeConnection(m_scene);
            m_tempConnection->setPort1(port);
            m_tempConnection->updatePath();
            return true;
        }
    }
    return false;
}

bool NodeController::processMouseMove(QGraphicsSceneMouseEvent* event)
{
    if (m_tempConnection) {
        m_tempConnection->setPort2(nullptr);
        QPainterPath path;
        path.moveTo(m_startPort->scenePos());
        path.lineTo(event->scenePos());
        m_tempConnection->setPath(path);
        return true;
    }
    return false;
}

bool NodeController::processMouseRelease(QGraphicsSceneMouseEvent* event)
{
    if (m_tempConnection && event->button() == Qt::LeftButton) {
        QGraphicsItem* item = m_scene->itemAt(event->scenePos(), QTransform());
        NodePort* endPort = dynamic_cast<NodePort*>(item);
        if (endPort && endPort->isInput() && m_startPort->canConnect(endPort)) {
            emit connectionRequest(m_startPort, endPort);
        }
        resetConnection();
        return true;
    }
    return false;
}

void NodeController::resetConnection()
{
    if (m_tempConnection) {
        delete m_tempConnection;
        m_tempConnection = nullptr;
    }
    m_startPort = nullptr;
}