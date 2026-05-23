#include "WorkflowSerializer.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonValue>
#include <QDebug>

bool WorkflowSerializer::saveToFile(const WorkflowData& data, const QString& filePath)
{
    QJsonObject root;
    QJsonArray nodesArray;
    for (const NodeData& nd : data.nodes) {
        QJsonObject nodeObj;
        nodeObj["id"] = nd.id.toString();
        nodeObj["type"] = nd.type;
        nodeObj["displayName"] = nd.displayName;
        nodeObj["posX"] = nd.position.x();
        nodeObj["posY"] = nd.position.y();

        QJsonObject paramsObj;
        for (auto it = nd.parameters.begin(); it != nd.parameters.end(); ++it) {
            paramsObj[it.key()] = QJsonValue::fromVariant(it.value());
        }
        nodeObj["parameters"] = paramsObj;
        nodesArray.append(nodeObj);
    }
    root["nodes"] = nodesArray;

    QJsonArray connsArray;
    for (const ConnectionData& cd : data.connections) {
        QJsonObject connObj;
        connObj["fromNodeId"] = cd.fromNodeId.toString();
        connObj["fromPort"] = cd.outputPortIndex;
        connObj["toNodeId"] = cd.toNodeId.toString();
        connObj["toPort"] = cd.inputPortIndex;
        connsArray.append(connObj);
    }
    root["connections"] = connsArray;

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.write(QJsonDocument(root).toJson());
    return true;
}

WorkflowData WorkflowSerializer::loadFromFile(const QString& filePath)
{
    WorkflowData data;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) return data;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    QJsonObject root = doc.object();

    QJsonArray nodesArray = root["nodes"].toArray();
    for (const QJsonValue& val : nodesArray) {
        QJsonObject obj = val.toObject();
        NodeData nd;
        nd.id = QUuid(obj["id"].toString());
        nd.type = obj["type"].toString();
        nd.displayName = obj["displayName"].toString();
        nd.position = QPointF(obj["posX"].toDouble(), obj["posY"].toDouble());

        QJsonObject paramsObj = obj["parameters"].toObject();
        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
            nd.parameters[it.key()] = it.value().toVariant();
        }
        data.nodes.append(nd);
    }

    QJsonArray connsArray = root["connections"].toArray();
    for (const QJsonValue& val : connsArray) {
        QJsonObject obj = val.toObject();
        ConnectionData cd;
        cd.fromNodeId = QUuid(obj["fromNodeId"].toString());
        cd.outputPortIndex = obj["fromPort"].toInt();
        cd.toNodeId = QUuid(obj["toNodeId"].toString());
        cd.inputPortIndex = obj["toPort"].toInt();
        data.connections.append(cd);
    }
    return data;
}