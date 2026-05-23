#pragma once

#include <QGraphicsScene>
#include <QList>
#include "connection.h"  

class NodeItem;
class NodeController;
class NodePort;
class NodeConnection;

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
signals:
    void connectionCreated(Connection* conn);
    void connectionRemoved(Connection* conn);

private slots:
    void onConnectionRequest(NodePort* outputPort, NodePort* inputPort);

private:
    NodeController* m_controller{ nullptr };
    QList<NodeItem*> m_nodes;
    QList<Connection*> m_connections;

    QList<NodeItem*> topologicalSort(); 
    bool hasCycle();
};