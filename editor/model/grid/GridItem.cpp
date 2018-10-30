#include "GridItem.h"

#include <QtCore/QDataStream>

#include "GridSurface.h"

using namespace AxiomModel;

GridItem::GridItem(GridSurface *parent, QPoint pos, QSize size, QSize minSize, bool selected)
    : parentSurface(parent), m_pos(parent->grid().findNearestAvailable(pos, size)), m_size(size), m_minSize(minSize),
      m_selected(selected) {
    parentSurface->grid().setRect(m_pos, m_size, this);
    parentSurface->setDirty();
}

GridItem::~GridItem() {
    parentSurface->grid().setRect(m_pos, m_size, nullptr);
    parentSurface->setDirty();
}

bool GridItem::isDragAvailable(QPoint delta) {
    return parentSurface->grid().isRectAvailable(_dragStartPos + delta, m_size, this);
}

void GridItem::setSize(QSize size) {
    if (!isResizable()) return;

    if (size != m_size) {
        if (size.width() < m_minSize.width()) size.setWidth(m_minSize.width());
        if (size.height() < m_minSize.height()) size.setHeight(m_minSize.height());

        if (!parentSurface->grid().isRectAvailable(m_pos, size, this)) return;

        beforeSizeChanged(size);
        parentSurface->grid().moveRect(m_pos, m_size, m_pos, size, this);
        parentSurface->setDirty();
        m_size = size;
        sizeChanged(size);
    }
}

void GridItem::setRect(QRect rect) {
    setCorners(rect.topLeft(), rect.topLeft() + QPoint(rect.width(), rect.height()));
}

void GridItem::setCorners(QPoint topLeft, QPoint bottomRight) {
    if (!isResizable()) return;

    auto newSize = QSize(bottomRight.x() - topLeft.x(), bottomRight.y() - topLeft.y());
    if (newSize.width() < m_minSize.width()) newSize.setWidth(m_minSize.width());
    if (newSize.height() < m_minSize.height()) newSize.setHeight(m_minSize.height());
    if (topLeft == m_pos && newSize == m_size) return;

    if (!parentSurface->grid().isRectAvailable(topLeft, newSize, this)) {
        // try resizing just horizontally or just vertically
        auto hTopLeft = QPoint(topLeft.x(), m_pos.y());
        auto hSize = QSize(newSize.width(), m_size.height());
        auto vTopLeft = QPoint(m_pos.x(), topLeft.y());
        auto vSize = QSize(m_size.width(), newSize.height());
        if (parentSurface->grid().isRectAvailable(hTopLeft, hSize, this)) {
            topLeft = hTopLeft;
            newSize = hSize;
        } else if (parentSurface->grid().isRectAvailable(vTopLeft, vSize, this)) {
            topLeft = vTopLeft;
            newSize = vSize;
        } else
            return;
    }

    if (topLeft == m_pos && newSize == m_size) return;
    parentSurface->grid().moveRect(m_pos, m_size, topLeft, newSize, this);
    parentSurface->setDirty();
    beforePosChanged(topLeft);
    m_pos = topLeft;
    posChanged(m_pos);
    beforeSizeChanged(newSize);
    m_size = newSize;
    sizeChanged(m_size);
}

void GridItem::select(bool exclusive) {
    if (exclusive || !m_selected) {
        m_selected = true;
        selectedChanged(m_selected);
        selected(exclusive);
    }
}

void GridItem::deselect() {
    if (!m_selected) return;
    m_selected = false;
    selectedChanged(m_selected);
    deselected();
}

void GridItem::startDragging() {
    _dragStartPos = m_pos;
}

void GridItem::dragTo(QPoint delta) {
    setPos(_dragStartPos + delta, false, false);
}

void GridItem::finishDragging() {}

void GridItem::setPos(QPoint pos, bool updateGrid, bool checkPositions) {
    if (pos != m_pos) {
        if (checkPositions && !parentSurface->grid().isRectAvailable(pos, m_size, this)) return;

        if (pos == m_pos) return;

        beforePosChanged(pos);
        if (updateGrid) {
            parentSurface->grid().moveRect(m_pos, m_size, pos, m_size, this);
            parentSurface->setDirty();
        }
        m_pos = pos;
        posChanged(pos);
    }
}
