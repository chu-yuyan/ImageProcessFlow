#pragma once

#include <QGraphicsPathItem>

class NodePort;
struct Connection;

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

    // 新增：反向指针（用于删除时从 line 找到业务 Connection）
    void setConnection(Connection* conn) { m_connection = conn; }
    Connection* connection() const { return m_connection; }

private:
    NodePort* m_port1{nullptr};
    NodePort* m_port2{nullptr};
    Connection* m_connection{ nullptr };
};