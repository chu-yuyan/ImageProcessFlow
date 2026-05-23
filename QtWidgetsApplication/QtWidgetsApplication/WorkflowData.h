#pragma once
#include <QString>
#include <QMap>
#include <QList>
#include <QUuid>
#include <QPointF>

struct NodeData {
    QUuid id;
    QString type;           // 节点工厂键，如 "LoadImage"
    QString displayName;    // 可选，仅用于保存显示名
    QPointF position;       // 画布坐标
    QMap<QString, QVariant> parameters;
};

struct ConnectionData {
    QUuid fromNodeId;
    int outputPortIndex;
    QUuid toNodeId;
    int inputPortIndex;
};

struct WorkflowData {
    QList<NodeData> nodes;
    QList<ConnectionData> connections;
};