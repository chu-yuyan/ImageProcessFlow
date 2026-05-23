#pragma once

#include <QGraphicsPathItem>

class NodePort;

// 连接线（贝塞尔曲线）
class NodeConnection : public QGraphicsPathItem {
public:
    NodeConnection(QGraphicsScene* scene = nullptr);
    ~NodeConnection();

    void setPort1(NodePort* port);
    void setPort2(NodePort* port);
    void updatePath();

    bool hasPort(const NodePort* port) const;

    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

private:
    NodePort* m_port1{nullptr};
    NodePort* m_port2{nullptr};
};