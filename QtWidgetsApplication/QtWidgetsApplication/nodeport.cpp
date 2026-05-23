#include "nodeport.h"
#include "NodeItem.h"
#include "nodeconnection.h"
#include <QGraphicsItem> 

static const double PORT_RADIUS = 5.0;

NodePort::NodePort(NodeItem* parent, const PortInfo& info, bool isInput, int index)
    : QGraphicsPathItem(parent)
    , m_parentNode(parent)
    , m_info(info)
    , m_isInput(isInput)
    , m_index(index)
{
    setFlag(QGraphicsItem::ItemSendsScenePositionChanges);

    // 绘制圆形
    QPainterPath p;
    p.addEllipse(-PORT_RADIUS, -PORT_RADIUS, 2 * PORT_RADIUS, 2 * PORT_RADIUS);
    setPath(p);
    setPen(QPen(m_info.color.darker(180), 1.5));
    setBrush(m_info.color);
    setZValue(1);

    // 添加文字标签（可选）
    m_label = new QGraphicsTextItem(this);
    m_label->setPlainText(m_info.type);
    QFont font("Monospace", 8);
    m_label->setFont(font);
    if (isInput) {
        m_label->setPos(PORT_RADIUS + 2, -m_label->boundingRect().height() / 2);
    }
    else {
        m_label->setPos(-PORT_RADIUS - m_label->boundingRect().width() - 2,
            -m_label->boundingRect().height() / 2);
    }
}

NodePort::~NodePort()
{
    // 断开所有连接
    while (!m_connections.isEmpty()) {
        delete m_connections.first();
    }
}

void NodePort::addConnection(NodeConnection* conn)
{
    if (!m_connections.contains(conn))
        m_connections.append(conn);
}

void NodePort::removeConnection(NodeConnection* conn)
{
    m_connections.removeAll(conn);
}

bool NodePort::canConnect(const NodePort* other) const
{
    // 不能连到自身所属节点
    if (m_parentNode == other->m_parentNode)
        return false;
    // 必须一个输入一个输出
    if (isInput() == other->isInput())
        return false;
    // 检查是否已经连接过
    for (auto conn : m_connections) {
        if (conn->hasPort(other))
            return false;
    }
    return true;
}

QVariant NodePort::itemChange(QGraphicsItem::GraphicsItemChange change, const QVariant& value)
{
    if (change == QGraphicsItem::ItemScenePositionHasChanged) {
        for (auto conn : m_connections)
            conn->updatePath();
    }
    return QGraphicsPathItem::itemChange(change, value);
}