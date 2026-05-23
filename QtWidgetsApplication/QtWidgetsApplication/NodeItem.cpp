#include "NodeItem.h"

#include "NodeScene.h"

#include <QApplication>
#include <QFontMetricsF>
#include <QPen>

NodeItem::NodeItem(const QString& title)
    : m_title(title)
    , m_color(QColor(200, 200, 200))
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

QRectF NodeItem::boundingRect() const
{
    return QRectF(0.0, 0.0, kWidth, kHeight);
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal penWidth = 2.0;
    QRectF r = boundingRect().adjusted(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(isSelected() ? QColor(0, 120, 215) : QColor(60, 60, 60));
    painter->setPen(pen);
    painter->setBrush(m_color);
    painter->drawRoundedRect(r, kRadius, kRadius);

    // 标题
    painter->setPen(QColor(20, 20, 20));
    QFont f = QApplication::font();
    f.setBold(true);
    painter->setFont(f);

    const QRectF textRect = r.adjusted(10.0, 8.0, -10.0, -8.0);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop, m_title);

    // 绘制端口（小圆点）
    const qreal portRadius = 5.0;
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(30, 30, 30));

    const int inCount = inputPortCount();
    for (int i = 0; i < inCount; ++i)
    {
        const qreal y = (i + 0.5) * kHeight / inCount;
        painter->drawEllipse(QPointF(0.0, y), portRadius, portRadius);
    }

    const int outCount = outputPortCount();
    for (int i = 0; i < outCount; ++i)
    {
        const qreal y = (i + 0.5) * kHeight / outCount;
        painter->drawEllipse(QPointF(kWidth, y), portRadius, portRadius);
    }

    Q_UNUSED(option);
}

void NodeItem::setLogicNode(std::unique_ptr<BaseNode> node)
{
    m_logicNode = std::move(node);
    if (m_logicNode)
        m_title = m_logicNode->nodeName();
    createPorts();   // 新增
    update();
    updateConnections();
}

QPointF NodeItem::inputPortPos(int index) const
{
    const int count = inputPortCount();
    if (count <= 0 || index < 0 || index >= count)
        return mapToScene(QPointF(0.0, kHeight * 0.5));

    const qreal y = (index + 0.5) * kHeight / count;
    return mapToScene(QPointF(0.0, y));
}

QPointF NodeItem::outputPortPos(int index) const
{
    const int count = outputPortCount();
    if (count <= 0 || index < 0 || index >= count)
        return mapToScene(QPointF(kWidth, kHeight * 0.5));

    const qreal y = (index + 0.5) * kHeight / count;
    return mapToScene(QPointF(kWidth, y));
}

void NodeItem::updateConnections()
{
    if (!scene())
        return;

    NodeScene* sc = qobject_cast<NodeScene*>(scene());
    if (sc)
        sc->updateConnectionsForNode(this);
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value)
{
    if (change == ItemPositionHasChanged)
        updateConnections();

    return QGraphicsItem::itemChange(change, value);
}

void NodeItem::createPorts()
{
    // 清除旧的端口（如果存在）
    for (auto child : childItems()) {
        if (dynamic_cast<NodePort*>(child))
            delete child;
    }

    if (!m_logicNode)
        return;

    // 创建输入端口
    int idx = 0;
    for (const auto& port : m_logicNode->inputs()) {
        PortInfo info;
        info.type = (port.type == ImageType::Color) ? "Color" : "Gray";
        info.color = QColor(100, 150, 200); // 你可以根据类型设置不同颜色
        NodePort* p = new NodePort(this, info, true, idx);
        p->setPos(0, (idx + 0.5) * kHeight / m_logicNode->inputs().size());
        idx++;
    }
    // 创建输出端口
    idx = 0;
    for (const auto& port : m_logicNode->outputs()) {
        PortInfo info;
        switch (port.type) {
        case ImageType::Color: info.type = "Color"; break;
        case ImageType::Gray: info.type = "Gray"; break;
        case ImageType::PixelGrid: info.type = "PixelGrid"; break;
        case ImageType::PaletteIndexed: info.type = "Palette"; break;
        case ImageType::AlphaMasked: info.type = "RGBA"; break;
        case ImageType::BeadPattern: info.type = "Bead"; break;
        default: info.type = "Any";
        }
        info.color = QColor(200, 150, 100);
        NodePort* p = new NodePort(this, info, false, idx);
        p->setPos(kWidth, (idx + 0.5) * kHeight / m_logicNode->outputs().size());
        idx++;
    }
}

NodePort* NodeItem::getInputPort(int index) const
{
    for (auto* child : childItems()) {
        if (auto* port = dynamic_cast<NodePort*>(child)) {
            if (port->isInput() && port->index() == index)
                return port;
        }
    }
    return nullptr;
}

NodePort* NodeItem::getOutputPort(int index) const
{
    for (auto* child : childItems()) {
        if (auto* port = dynamic_cast<NodePort*>(child)) {
            if (!port->isInput() && port->index() == index)
                return port;
        }
    }
    return nullptr;
}