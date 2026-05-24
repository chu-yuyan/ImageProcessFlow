#include "NodeItem.h"
#include "NodeScene.h"
#include <QApplication>
#include <QFontMetricsF>
#include <QPen>

NodeItem::NodeItem(const QString& title)
    : m_title(title)
    , m_color(QColor(200, 200, 200))
    , m_boundingRect(0, 0, kWidth, kHeight)
{
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable);
    setAcceptHoverEvents(true);
}

// 新增：更新节点大小
void NodeItem::updateBoundingRect()
{
    prepareGeometryChange();
    int maxPorts = qMax(inputPortCount(), outputPortCount());
    qreal dynamicHeight = qMax(kHeight, maxPorts * kPortSpacing);
    m_boundingRect = QRectF(0, 0, kWidth, dynamicHeight);
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* /*widget*/)
{
    painter->setRenderHint(QPainter::Antialiasing, true);

    const qreal penWidth = 2.0;

    // 使用成员变量 m_boundingRect
    QRectF r = m_boundingRect;
    QRectF drawRect = r.adjusted(penWidth / 2.0, penWidth / 2.0, -penWidth / 2.0, -penWidth / 2.0);

    QPen pen;
    pen.setWidthF(penWidth);
    pen.setColor(isSelected() ? QColor(0, 120, 215) : QColor(60, 60, 60));
    painter->setPen(pen);
    painter->setBrush(m_color);
    painter->drawRoundedRect(drawRect, kRadius, kRadius);

    // 标题
    painter->setPen(QColor(20, 20, 20));
    QFont f = QApplication::font();
    f.setBold(true);
    painter->setFont(f);

    const QRectF textRect = drawRect.adjusted(10.0, 8.0, -10.0, -8.0);
    painter->drawText(textRect, Qt::AlignLeft | Qt::AlignTop, m_title);

    // 绘制端口小圆点
    const qreal portRadius = 5.0;
    painter->setPen(Qt::NoPen);
    painter->setBrush(QColor(30, 30, 30));

    const int inCount = inputPortCount();
    qreal nodeHeight = m_boundingRect.height();
    for (int i = 0; i < inCount; ++i)
    {
        const qreal y = (i + 0.5) * nodeHeight / inCount;
        painter->drawEllipse(QPointF(0.0, y), portRadius, portRadius);
    }

    const int outCount = outputPortCount();
    for (int i = 0; i < outCount; ++i)
    {
        const qreal y = (i + 0.5) * nodeHeight / outCount;
        painter->drawEllipse(QPointF(kWidth, y), portRadius, portRadius);
    }

    Q_UNUSED(option);
}

void NodeItem::setLogicNode(std::unique_ptr<BaseNode> node)
{
    m_logicNode = std::move(node);
    if (m_logicNode)
        m_title = m_logicNode->nodeName();
    updateBoundingRect();  // 先更新大小
    createPorts();
    update();
    updateConnections();
}

QPointF NodeItem::inputPortPos(int index) const
{
    const int count = inputPortCount();
    if (count <= 0 || index < 0 || index >= count)
        return mapToScene(QPointF(0.0, m_boundingRect.height() * 0.5));

    const qreal y = (index + 0.5) * m_boundingRect.height() / count;
    return mapToScene(QPointF(0.0, y));
}

QPointF NodeItem::outputPortPos(int index) const
{
    const int count = outputPortCount();
    if (count <= 0 || index < 0 || index >= count)
        return mapToScene(QPointF(kWidth, m_boundingRect.height() * 0.5));

    const qreal y = (index + 0.5) * m_boundingRect.height() / count;
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
    // 清除旧的端口
    for (auto child : childItems()) {
        if (dynamic_cast<NodePort*>(child))
            delete child;
    }

    if (!m_logicNode)
        return;

    // 辅助函数：获取端口显示名称和颜色
    auto getPortInfo = [](ImageType type) -> PortInfo {
        PortInfo info;
        switch (type) {
        case ImageType::Any:
            info.type = "Any";
            info.color = QColor(150, 150, 150);
            break;
        case ImageType::Color:
            info.type = "RGB";
            info.color = QColor(100, 150, 200);
            break;
        case ImageType::Gray:
            info.type = "Gray";
            info.color = QColor(120, 120, 120);
            break;
        case ImageType::PixelGrid:
            info.type = "Grid";
            info.color = QColor(150, 200, 100);
            break;
        case ImageType::PaletteIndexed:
            info.type = "Idx";
            info.color = QColor(200, 150, 100);
            break;
        case ImageType::AlphaMasked:
            info.type = "RGBA";
            info.color = QColor(200, 100, 150);
            break;
        case ImageType::BeadPattern:
            info.type = "Bead";
            info.color = QColor(100, 200, 200);
            break;
        default:
            info.type = "?";
            info.color = QColor(150, 150, 150);
        }
        return info;
        };

    qreal nodeHeight = m_boundingRect.height();

    // 创建输入端口
    int idx = 0;
    int inCount = m_logicNode->inputs().size();
    for (const auto& port : m_logicNode->inputs()) {
        PortInfo info = getPortInfo(port.type);
        NodePort* p = new NodePort(this, info, true, idx);
        p->setPos(0, (idx + 0.5) * nodeHeight / std::max(1, inCount));
        idx++;
    }

    // 创建输出端口
    idx = 0;
    int outCount = m_logicNode->outputs().size();
    for (const auto& port : m_logicNode->outputs()) {
        PortInfo info = getPortInfo(port.type);
        NodePort* p = new NodePort(this, info, false, idx);
        p->setPos(kWidth, (idx + 0.5) * nodeHeight / std::max(1, outCount));
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