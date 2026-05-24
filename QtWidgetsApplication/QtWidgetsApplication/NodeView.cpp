#include "NodeView.h"
#include "NodeScene.h"
#include "NodePort.h"
#include <QMouseEvent>
#include <QScrollBar>
#include <QWheelEvent>
#include <QKeyEvent>

NodeView::NodeView(QWidget* parent) : QGraphicsView(parent)
{
    setRenderHint(QPainter::Antialiasing, true);

    m_defaultDragMode = QGraphicsView::RubberBandDrag;
    setDragMode(m_defaultDragMode);

    setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);
    setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
    setResizeAnchor(QGraphicsView::AnchorUnderMouse);
    setFocusPolicy(Qt::StrongFocus);
}

void NodeView::wheelEvent(QWheelEvent* event)
{
    constexpr qreal kZoomInFactor = 1.15;
    constexpr qreal kZoomOutFactor = 1.0 / kZoomInFactor;
    const auto delta = event->angleDelta().y();
    if (delta > 0)
        applyZoom(kZoomInFactor);
    else if (delta < 0)
        applyZoom(kZoomOutFactor);
    event->accept();
}

void NodeView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = true;
        m_lastPanPos = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    // 关键：如果点到端口，关闭 RubberBand，避免出现选框
    auto* item = itemAt(event->pos());
    if (dynamic_cast<NodePort*>(item) != nullptr) {
        setDragMode(QGraphicsView::NoDrag);
    } else {
        setDragMode(m_defaultDragMode);
    }

    QGraphicsView::mousePressEvent(event);
}

void NodeView::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton) {
        m_isPanning = false;
        unsetCursor();
        event->accept();
        return;
    }

    // 松开恢复默认拖拽模式
    setDragMode(m_defaultDragMode);

    QGraphicsView::mouseReleaseEvent(event);
}

void NodeView::mouseMoveEvent(QMouseEvent* event)
{
    if (m_isPanning) {
        QPoint delta = event->pos() - m_lastPanPos;
        m_lastPanPos = event->pos();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}

void NodeView::applyZoom(qreal factor)
{
    const QTransform t = transform();
    const qreal currentScale = t.m11();
    const qreal nextScale = currentScale * factor;
    constexpr qreal kMinScale = 0.2;
    constexpr qreal kMaxScale = 4.0;
    if (nextScale < kMinScale || nextScale > kMaxScale)
        return;
    scale(factor, factor);
}

void NodeView::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_Delete) {
        if (auto* sc = qobject_cast<NodeScene*>(scene())) {
            sc->deleteSelected();
            event->accept();
            return;
        }
    }
    QGraphicsView::keyPressEvent(event);
}
