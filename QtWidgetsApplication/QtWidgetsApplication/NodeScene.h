#pragma once

#include <QGraphicsScene>
#include <QList>
#include "connection.h"

class NodeItem;
class NodeController;
class NodePort;
class NodeConnection;
class BaseNode;

class NodeScene : public QGraphicsScene
{
    Q_OBJECT

public:
    explicit NodeScene(QObject* parent = nullptr);
    ~NodeScene() = default;

    void setDefaultSceneRect();
    void addNode(NodeItem* node);

    void addConnection(Connection* conn);
    void removeConnection(Connection* conn);
    void clearConnections();
    const QList<Connection*>& connections() const { return m_connections; }

    void updateConnectionsForNode(NodeItem* node);
    bool canConnect(NodeItem* fromNode, int outPort, NodeItem* toNode, int inPort) const;

    bool executeWorkflow();
    void deleteSelected();
    void removeNode(NodeItem* node);

    QList<NodeItem*> nodes() const { return m_nodes; }
    void clear();

signals:
    void connectionCreated(Connection* conn);
    void connectionRemoved(Connection* conn);

    // 新增：删除节点前通知（防止外部缓存悬空指针）
    void nodeAboutToBeRemoved(BaseNode* logicNode);

public slots:
    void onConnectionRequest(NodePort* outputPort, NodePort* inputPort);

private:
    NodeController* m_controller{ nullptr };
    QList<NodeItem*> m_nodes;
    QList<Connection*> m_connections;

    QList<NodeItem*> topologicalSort();
    bool hasCycle();
};