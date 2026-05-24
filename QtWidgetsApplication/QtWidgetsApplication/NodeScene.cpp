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

    // 新增：反向指针
    line->setConnection(conn);

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

    const auto fromType = fromNode->logicNode()->outputs()[outPort].type;
    const auto toType = toNode->logicNode()->inputs()[inPort].type;

    // Any 输入端口允许任何连接
    if (toType == ImageType::Any)
        return true;

    // 完全相同类型允许
    if (fromType == toType)
        return true;

    return false;
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

void NodeScene::removeNode(NodeItem* node)
{
    if (!node) return;

    // 新增：删除前通知外部清理引用
    if (node->logicNode())
        emit nodeAboutToBeRemoved(node->logicNode());

    // 1) 先删掉所有关联连接（复制列表，避免遍历中修改）
    QList<Connection*> toRemove;
    for (auto* c : m_connections) {
        if (!c) continue;
        if (c->fromNode == node || c->toNode == node)
            toRemove.append(c);
    }
    for (auto* c : toRemove)
        removeConnection(c);

    // 2) 从 m_nodes 移除并从场景移除/释放
    m_nodes.removeOne(node);
    removeItem(node);
    delete node;
}

void NodeScene::deleteSelected()
{
    const auto selected = selectedItems();
    if (selected.isEmpty()) return;

    // 先删线，再删点（更稳）
    QList<Connection*> connectionsToDelete;
    QList<NodeItem*> nodesToDelete;

    for (auto* item : selected) {
        if (auto* line = dynamic_cast<NodeConnection*>(item)) {
            if (auto* c = line->connection()) // 反向指针（见第③部分）
                connectionsToDelete.append(c);
        } else if (auto* node = dynamic_cast<NodeItem*>(item)) {
            nodesToDelete.append(node);
        }
    }

    // 去重（同一条线可能被重复加入）
    connectionsToDelete = QList<Connection*>(QSet<Connection*>(connectionsToDelete.begin(), connectionsToDelete.end()).values());
    nodesToDelete = QList<NodeItem*>(QSet<NodeItem*>(nodesToDelete.begin(), nodesToDelete.end()).values());

    for (auto* c : connectionsToDelete)
        removeConnection(c);

    for (auto* n : nodesToDelete)
        removeNode(n);

    clearSelection();
}

void NodeScene::clear()
{
    // 先删除所有连接（deleteSelected 中已实现批量删除，但这里直接清空）
    clearConnections();   // 已有的函数，会删除所有 Connection 对象并清空列表
    // 删除所有节点
    for (NodeItem* node : m_nodes) {
        removeItem(node);
        delete node;
    }
    m_nodes.clear();
}