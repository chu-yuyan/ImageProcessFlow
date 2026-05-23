#include "WorkflowExecutor.h"
#include "NodeFactory.h"
#include "BaseNode.h"
#include <QMap>
#include <QQueue>
#include <QDebug>
#include <QUuid>
#include <map>          // 关键：使用 std::map 存 unique_ptr

bool WorkflowExecutor::execute(const WorkflowData& data)
{
    // 1. 创建所有节点实例，用 id 映射
    std::map<QUuid, std::unique_ptr<BaseNode>> nodeMap;   // 所有权
    QMap<QUuid, BaseNode*> rawNodeMap;                    // 裸指针，供快速访问

    for (const NodeData& nd : data.nodes) {
        auto node = NodeFactory::instance().create(nd.type);
        if (!node) {
            qDebug() << "Unknown node type:" << nd.type;
            return false;
        }
        // 恢复参数
        for (auto it = nd.parameters.begin(); it != nd.parameters.end(); ++it) {
            node->setParameter(it.key(), it.value());
        }
        BaseNode* ptr = node.get();
        rawNodeMap[nd.id] = ptr;
        nodeMap[nd.id] = std::move(node);
    }

    // 2. 构建连接关系：记录每个输入端口对应的输出节点和端口
    struct InputSource {
        BaseNode* fromNode;
        int outPort;
    };
    QMap<BaseNode*, QMap<int, InputSource>> inputSources;

    for (const ConnectionData& cd : data.connections) {
        auto fromIt = rawNodeMap.find(cd.fromNodeId);
        auto toIt = rawNodeMap.find(cd.toNodeId);
        if (fromIt == rawNodeMap.end() || toIt == rawNodeMap.end()) {
            qDebug() << "Invalid connection: node not found";
            return false;
        }
        BaseNode* from = fromIt.value();
        BaseNode* to = toIt.value();
        inputSources[to][cd.inputPortIndex] = { from, cd.outputPortIndex };
    }

    // 3. 拓扑排序
    QMap<BaseNode*, int> inDegree;
    QMap<BaseNode*, QList<BaseNode*>> graph;
    for (auto it = rawNodeMap.begin(); it != rawNodeMap.end(); ++it) {
        inDegree[it.value()] = 0;
        graph[it.value()] = {};
    }
    for (const ConnectionData& cd : data.connections) {
        auto fromIt = rawNodeMap.find(cd.fromNodeId);
        auto toIt = rawNodeMap.find(cd.toNodeId);
        if (fromIt != rawNodeMap.end() && toIt != rawNodeMap.end()) {
            BaseNode* from = fromIt.value();
            BaseNode* to = toIt.value();
            graph[from].append(to);
            inDegree[to]++;
        }
    }

    QQueue<BaseNode*> q;
    for (auto it = inDegree.begin(); it != inDegree.end(); ++it) {
        if (it.value() == 0) q.enqueue(it.key());
    }
    QList<BaseNode*> order;
    while (!q.isEmpty()) {
        BaseNode* node = q.dequeue();
        order.append(node);
        for (BaseNode* neighbor : graph[node]) {
            if (--inDegree[neighbor] == 0) q.enqueue(neighbor);
        }
    }
    if (order.size() != rawNodeMap.size()) {
        qDebug() << "Cycle detected in workflow";
        return false;
    }

    // 4. 按拓扑顺序执行
    for (BaseNode* node : order) {
        auto srcMap = inputSources.value(node);
        for (int i = 0; i < node->inputs().size(); ++i) {
            if (srcMap.contains(i)) {
                InputSource& src = srcMap[i];
                if (src.outPort < src.fromNode->outputs().size()) {
                    node->inputs()[i].data = src.fromNode->outputs()[src.outPort].data;
                }
            }
            else {
                node->inputs()[i].data = QVariant();
            }
        }
        node->process();
    }

    qDebug() << "Workflow executed successfully.";
    return true;
}