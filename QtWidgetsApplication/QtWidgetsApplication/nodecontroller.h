#pragma once

#include <QObject>

class QGraphicsScene;
class QGraphicsSceneMouseEvent;
class NodeConnection;
class NodePort;

class NodeController : public QObject {
    Q_OBJECT
public:
    explicit NodeController(QGraphicsScene* scene);

signals:
    void connectionRequest(NodePort* outputPort, NodePort* inputPort);

protected:
    bool eventFilter(QObject* obj, QEvent* event) override;

private:
    bool processMousePress(QGraphicsSceneMouseEvent* event);
    bool processMouseMove(QGraphicsSceneMouseEvent* event);
    bool processMouseRelease(QGraphicsSceneMouseEvent* event);
    void resetConnection();

    QGraphicsScene* m_scene;
    NodeConnection* m_tempConnection{ nullptr };
    NodePort* m_startPort{ nullptr };
};