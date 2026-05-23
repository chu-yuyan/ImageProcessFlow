#pragma once

#include <QGraphicsView>

class NodeView : public QGraphicsView
{
    Q_OBJECT
public:
    explicit NodeView(QWidget* parent = nullptr);

protected:
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void applyZoom(qreal factor);

    bool m_isPanning = false;
    QPoint m_lastPanPos;

    DragMode m_defaultDragMode = QGraphicsView::RubberBandDrag;
};