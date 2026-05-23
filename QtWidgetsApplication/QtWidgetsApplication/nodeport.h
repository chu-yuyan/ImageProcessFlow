#pragma once

#include <QGraphicsPathItem>
#include <QPointer>

class NodeItem;
class NodeConnection;

// 端口信息（类型和颜色）
struct PortInfo {
    QString type;
    QColor color;
};

// 端口基类：表示节点上的输入/输出端口
class NodePort : public QGraphicsPathItem {
public:
    NodePort(NodeItem* parent, const PortInfo& info, bool isInput, int index);
    ~NodePort();

    // 端口类型字符串（例如 "Image", "GrayImage"）
    QString portType() const { return m_info.type; }
    bool isInput() const { return m_isInput; }
    int index() const { return m_index; }

    // 所属节点
    NodeItem* parentNode() const { return m_parentNode; }

    // 连接管理
    void addConnection(NodeConnection* conn);
    void removeConnection(NodeConnection* conn);
    const QList<NodeConnection*>& connections() const { return m_connections; }

    // 判断是否可与另一个端口连接
    bool canConnect(const NodePort* other) const;

protected:
    QVariant itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value) override;

private:
    NodeItem* m_parentNode;
    PortInfo m_info;
    bool m_isInput;
    int m_index;
    QList<NodeConnection*> m_connections;
    QGraphicsTextItem* m_label;
};