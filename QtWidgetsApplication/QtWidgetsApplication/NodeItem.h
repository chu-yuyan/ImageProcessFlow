#pragma once

#include <QColor>
#include <QGraphicsItem>
#include <QPainter>
#include <QString>
#include <QPointF>
#include <memory>
#include "NodePort.h"


#include "BaseNode.h"

class NodeItem : public QGraphicsItem
{
public:
    enum { Type = QGraphicsItem::UserType + 1 };

    explicit NodeItem(const QString& title);

    int type() const override { return Type; }

    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

    QString title() const { return m_title; }
    QColor color() const { return m_color; }

    void setLogicNode(std::unique_ptr<BaseNode> node);
    BaseNode* logicNode() const { return m_logicNode.get(); }

    // ¶Ë¿ÚÐÅÏ¢
    QPointF inputPortPos(int index) const;
    QPointF outputPortPos(int index) const;
    int inputPortCount() const { return m_logicNode ? m_logicNode->inputs().size() : 0; }
    int outputPortCount() const { return m_logicNode ? m_logicNode->outputs().size() : 0; }

    void updateConnections();
    void createPorts();

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;

private:
    QString m_title;
    QColor m_color;

    static constexpr qreal kWidth = 160.0;
    static constexpr qreal kHeight = 80.0;
    static constexpr qreal kRadius = 8.0;

    std::unique_ptr<BaseNode> m_logicNode;
};
