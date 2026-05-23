#include "NodeScene.h"
#include "NodeItem.h"
#include "NodeController.h"
#include "NodePort.h"
#include "NodeConnection.h"
#include "connection.h"
#include <QQueue>
#include <QSet>
#include <QMap>

NodeScene::NodeScene(QObject* parent)
    : QGraphicsScene(parent)
{
    setDefaultSceneRect();
    m_controller = new NodeController(this);
    connect(m_controller, &NodeController::connectionRequest, this, &NodeScene::onConnectionRequest);
}

void NodeScene::setDefaultSceneRect()
{
    setSceneRect(-1000.0, -1000.0, 1000.0, 1000.0);
}

void NodeScene::addNode(NodeItem* node)
{
    if (!node) return;
    addItem(node);
    m_nodes.push_back(node);
}

void NodeScene::onConnectionRequest(NodePort* outputPort, NodePort* inputPort)
{
    NodeItem* fromNode = outputPort->parentNode();
    NodeItem* toNode = inputPort->parentNode();
    int fromIdx = outputPort->index();
    int toIdx = inputPort->index();

    if (!canConnect(fromNode, fromIdx, toNode, toIdx))
        return;

    NodeConnection* line = new NodeConnection(this);
    line->setPort1(outputPort);
    line->setPort2(inputPort);
    line->updatePath();

    Connection* conn = new Connection(fromNode, fromIdx, toNode, toIdx, line);
    addConnection(conn);

}

void NodeScene::addConnection(Connection* conn)
{
    if (!conn) return;
    m_connections.append(conn);
    if (conn->lineItem && !conn->lineItem->scene())
        addItem(conn->lineItem);
    emit connectionCreated(conn);
}

void NodeScene::removeConnection(Connection* conn)
{
    if (!conn) return;
    m_connections.removeOne(conn);
    if (conn->lineItem) {
        removeItem(conn->lineItem);
        delete conn->lineItem;
        conn->lineItem = nullptr;
    }
    emit connectionRemoved(conn);
    delete conn;
}

void NodeScene::clearConnections()
{
    for (auto* conn : m_connections) {
        if (conn->lineItem) {
            removeItem(conn->lineItem);
            delete conn->lineItem;
        }
        delete conn;
    }
    m_connections.clear();
}

void NodeScene::updateConnectionsForNode(NodeItem* node)
{
    if (!node) return;
    for (auto* conn : m_connections) {
        if (!conn || !conn->lineItem) continue;
        if (conn->fromNode == node || conn->toNode == node) {
            conn->lineItem->updatePath();   // 注意：lineItem 是 NodeConnection*，调用 updatePath
        }
    }
}

bool NodeScene::canConnect(NodeItem* fromNode, int outPort, NodeItem* toNode, int inPort) const
{
    if (!fromNode || !toNode) return false;
    if (fromNode == toNode) return false;
    if (!fromNode->logicNode() || !toNode->logicNode()) return false;
    if (outPort < 0 || outPort >= fromNode->outputPortCount()) return false;
    if (inPort < 0 || inPort >= toNode->inputPortCount()) return false;

    const auto fromPortType = fromNode->logicNode()->outputs()[outPort].type;
    const auto toPortType = toNode->logicNode()->inputs()[inPort].type;
    if (fromPortType != toPortType) return false;

    for (auto* conn : m_connections) {
        if (conn->fromNode == fromNode && conn->outputPortIndex == outPort &&
            conn->toNode == toNode && conn->inputPortIndex == inPort)
            return false;
    }
    return true;
}

QList<NodeItem*> NodeScene::topologicalSort()
{
    QMap<NodeItem*, int> inDegree;
    QMap<NodeItem*, QList<NodeItem*>> graph;

    // 初始化所有节点的入度为0
    for (auto node : m_nodes) {
        inDegree[node] = 0;
        graph[node] = {};
    }

    // 构建邻接表：fromNode -> toNode (根据连接的方向)
    for (auto conn : m_connections) {
        NodeItem* from = conn->fromNode;
        NodeItem* to = conn->toNode;
        if (from && to) {
            graph[from].append(to);
            inDegree[to]++;
        }
    }

    // 队列存储入度为0的节点
    QQueue<NodeItem*> q;
    for (auto it = inDegree.begin(); it != inDegree.end(); ++it) {
        if (it.value() == 0)
            q.enqueue(it.key());
    }

    QList<NodeItem*> result;
    while (!q.isEmpty()) {
        NodeItem* node = q.dequeue();
        result.append(node);
        for (NodeItem* neighbor : graph[node]) {
            if (--inDegree[neighbor] == 0)
                q.enqueue(neighbor);
        }
    }

    // 如果结果数量不等于节点总数，说明存在环
    if (result.size() != m_nodes.size())
        return {};  // 返回空列表表示有环
    return result;
}

bool NodeScene::hasCycle()
{
    return topologicalSort().isEmpty();
}

bool NodeScene::executeWorkflow()
{
    // 先检测环
    if (hasCycle()) {
        qDebug() << "Workflow contains cycles, cannot execute!";
        return false;
    }

    QList<NodeItem*> order = topologicalSort();
    if (order.isEmpty()) return false;

    // 清空所有节点的输入端口数据（可选，避免残留）
    for (auto node : m_nodes) {
        for (auto& port : node->logicNode()->inputs()) {
            port.data = QVariant();
        }
    }

    // 按照拓扑顺序执行
    for (NodeItem* node : order) {
        // 1. 从连接中收集输入数据
        BaseNode* logic = node->logicNode();
        if (!logic) continue;

        // 将各个输入端口的数据从上游节点拷贝过来
        for (int i = 0; i < logic->inputs().size(); ++i) {
            // 查找连接到这个输入端口的连接
            for (auto conn : m_connections) {
                if (conn->toNode == node && conn->inputPortIndex == i) {
                    NodeItem* fromNode = conn->fromNode;
                    int outIdx = conn->outputPortIndex;
                    QVariant data = fromNode->logicNode()->outputs()[outIdx].data;
                    logic->inputs()[i].data = data;
                    break;
                }
            }
        }

        // 2. 执行节点
        logic->process();
    }

    return true;
}