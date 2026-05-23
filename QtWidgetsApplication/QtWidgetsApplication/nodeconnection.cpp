#include "nodeconnection.h"
#include "NodePort.h"
#include <QPainter>
#include <QGraphicsScene>

NodeConnection::NodeConnection(QGraphicsScene* scene)
{
    setFlag(QGraphicsItem::ItemIsSelectable);
    setPen(QPen(Qt::darkGray, 2));
    setBrush(Qt::NoBrush);
    setZValue(-1);
    if (scene)
        scene->addItem(this);
}

NodeConnection::~NodeConnection()
{
    if (m_port1)
        m_port1->removeConnection(this);
    if (m_port2)
        m_port2->removeConnection(this);
}

void NodeConnection::setPort1(NodePort* port)
{
    m_port1 = port;
    if (m_port1)
        m_port1->addConnection(this);
}

void NodeConnection::setPort2(NodePort* port)
{
    m_port2 = port;
    if (m_port2)
        m_port2->addConnection(this);
}

void NodeConnection::updatePath()
{
    if (!m_port1 || !m_port2)
        return;

    QPointF pos1 = m_port1->scenePos();
    QPointF pos2 = m_port2->scenePos();
    QPainterPath path;
    path.moveTo(pos1);
    qreal dx = pos2.x() - pos1.x();
    dx = qMax(dx, 200.0);
    QPointF ctrl1(pos1.x() + dx * 0.25, pos1.y());
    QPointF ctrl2(pos2.x() - dx * 0.25, pos2.y());
    path.cubicTo(ctrl1, ctrl2, pos2);
    setPath(path);
}

bool NodeConnection::hasPort(const NodePort* port) const
{
    return m_port1 == port || m_port2 == port;
}

void NodeConnection::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
{
    painter->setPen(isSelected() ? Qt::DashLine : QPen(Qt::darkGray, 2));
    painter->setBrush(Qt::NoBrush);
    painter->drawPath(path());
}