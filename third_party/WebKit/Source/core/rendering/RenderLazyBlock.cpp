/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/RenderLazyBlock.h"

#include "core/dom/ClientRect.h"
#include "core/rendering/LayoutRepainter.h"
#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderView.h"

namespace WebCore {

RenderLazyBlock::RenderLazyBlock(Element* element)
    : RenderBlock(element)
    , m_next(0)
    , m_previous(0)
    , m_firstVisibleChildBox(0)
    , m_lastVisibleChildBox(0)
    , m_attached(false)
    , m_isNestedLayout(false)
{
    setChildrenInline(false); // All of our children must be block-level.
}

RenderLazyBlock::~RenderLazyBlock()
{
    ASSERT(!m_attached);
}

void RenderLazyBlock::willBeDestroyed()
{
    detachLazyBlock();
    RenderBlock::willBeDestroyed();
}

void RenderLazyBlock::willBeRemovedFromTree()
{
    RenderBlock::willBeRemovedFromTree();
    detachLazyBlock();
}

bool RenderLazyBlock::isNested() const
{
    for (RenderObject* ancestor = parent(); ancestor; ancestor = ancestor->parent()) {
        if (ancestor->isRenderLazyBlock())
            return true;
    }
    return false;
}

// FIXME: This method and detachLazyBlock are essentially identical to
// RenderQuote::attachQuote and detachQuote. We should just have a
// RenderTreeOrderedList that does this stuff internally.
void RenderLazyBlock::attachLazyBlock()
{
    ASSERT(view());
    ASSERT(!m_attached);
    ASSERT(!m_next && !m_previous);
    ASSERT(isRooted());

    if (!view()->firstLazyBlock()) {
        view()->setFirstLazyBlock(this);
        m_attached = true;
        return;
    }

    for (RenderObject* predecessor = previousInPreOrder(); predecessor; predecessor = predecessor->previousInPreOrder()) {
        if (!predecessor->isRenderLazyBlock() || !toRenderLazyBlock(predecessor)->isAttached())
            continue;
        m_previous = toRenderLazyBlock(predecessor);
        m_next = m_previous->m_next;
        m_previous->m_next = this;
        if (m_next)
            m_next->m_previous = this;
        break;
    }

    if (!m_previous) {
        m_next = view()->firstLazyBlock();
        view()->setFirstLazyBlock(this);
        if (m_next)
            m_next->m_previous = this;
    }
    m_attached = true;

    ASSERT(!m_next || m_next->m_attached);
    ASSERT(!m_next || m_next->m_previous == this);
    ASSERT(!m_previous || m_previous->m_attached);
    ASSERT(!m_previous || m_previous->m_next == this);
}

void RenderLazyBlock::detachLazyBlock()
{
    ASSERT(!m_next || m_next->m_attached);
    ASSERT(!m_previous || m_previous->m_attached);
    if (!m_attached)
        return;
    if (m_previous)
        m_previous->m_next = m_next;
    else if (view())
        view()->setFirstLazyBlock(m_next);
    if (m_next)
        m_next->m_previous = m_previous;
    m_attached = false;
    m_next = 0;
    m_previous = 0;
}

void RenderLazyBlock::paintChildren(PaintInfo& paintInfo, const LayoutPoint& paintOffset, PaintInfo& paintInfoForChild, bool usePrintRect)
{
    for (RenderBox* child = m_firstVisibleChildBox; child && child != m_lastVisibleChildBox; child = child->nextSiblingBox()) {
        if (!paintChild(child, paintInfo, paintOffset, paintInfoForChild, usePrintRect))
            return;
    }
}

void RenderLazyBlock::layoutChildren(bool relayoutChildren)
{
    LayoutUnit afterEdge = borderAfter() + paddingAfter() + scrollbarLogicalHeight();
    LayoutUnit height = borderBefore() + paddingBefore();
    LayoutRect intersectRect = m_intersectRect;

    // FIXME: If we already have m_firstVisibleChildBox we should start there
    // and stop when we have enough to fill the viewport. This catches the most
    // common cases of scrolling upward or downward.

    m_firstVisibleChildBox = 0;
    m_lastVisibleChildBox = 0;

    // FIXME: This should approximate the height so we don't actually need to walk
    // every child and can optimistically layout children until we fill the
    // the expandedViewportRect.

    setLogicalHeight(height);
    int childCount = 0;
    LayoutUnit heightOfChildren = 0;
    for (RenderBox* child = firstChildBox(); child; child = child->nextSiblingBox()) {
        ++childCount;
        updateBlockChildDirtyBitsBeforeLayout(relayoutChildren, child);

        if (relayoutChildren)
            child->setNeedsLayout(true, MarkOnlyThis);

        if (child->style()->logicalHeight().isSpecified()) {
            LogicalExtentComputedValues computedValues;
            child->computeLogicalHeight(-1, height, computedValues);
            child->setLogicalHeight(computedValues.m_extent);
            heightOfChildren += computedValues.m_extent;
        } else {
            // FIXME: Enable guessing about height so we don't need to do layout
            // on every non fixed height child.
            setLogicalHeight(height);
            setLogicalTopForChild(child, height);
            if (heightOfChildren && child->needsLayout())
                child->setLogicalHeight(heightOfChildren / childCount);
            else
                child->layoutIfNeeded();
            heightOfChildren += child->logicalHeight();
        }

        intersectRect.setHeight(child->logicalHeight());

        if (m_expandedViewportRect.intersects(enclosingIntRect(intersectRect))) {
            if (!m_firstVisibleChildBox)
                m_firstVisibleChildBox = child;
            m_lastVisibleChildBox = child->nextSiblingBox();
            setLogicalHeight(height);
            setLogicalTopForChild(child, height);
            child->layoutIfNeeded();
            // FIXME: Track how far off our estimated height is from the actual height,
            // and adjust the scrollbars accordingly.
        }

        intersectRect.setY(intersectRect.y() + child->logicalHeight());
        height += child->logicalHeight();
    }

    setLogicalHeight(height + afterEdge);

    updateLogicalHeight();
}

void RenderLazyBlock::layoutBlock(bool relayoutChildren, LayoutUnit pageLogicalHeight)
{
    ASSERT(needsLayout());

    if (!m_attached)
        attachLazyBlock();

    // FIXME: We should adjust the style to disallow columns too.
    ASSERT(!hasColumns());

    LayoutRepainter repainter(*this, checkForRepaintDuringLayout());
    LayoutStateMaintainer statePusher(view(), this, locationOffset(), hasTransform() || hasReflection() || style()->isFlippedBlocksWritingMode());

    // FIXME: The compositor can instead give us a list of rects it thinks
    // are important.
    IntRect expandedViewportRect = view()->frameView()->visibleContentRect(ScrollableArea::IncludeScrollbars);
    expandedViewportRect.move(-4000, -4000);
    expandedViewportRect.expand(8000, 8000);

    // FIXME: We probably want a RenderGeometryMap instead since we need to handle
    // rotation of the RenderLazyBlock and other transforms.
    Vector<FloatQuad> quads;
    enclosingBoxModelObject()->absoluteQuads(quads);
    LayoutRect intersectRect = quads[0].enclosingBoundingBox();
    if (hasOverflowClip())
        intersectRect.move(-scrolledContentOffset());

    // Avoid doing work inside the nested layout if we know everything is correct already.
    if (!m_isNestedLayout || m_intersectRect != intersectRect || m_expandedViewportRect != expandedViewportRect) {
        m_intersectRect = intersectRect;
        m_expandedViewportRect = expandedViewportRect;
        layoutChildren(relayoutChildren || updateLogicalWidthAndColumnWidth());
    }

    statePusher.pop();
    updateLayerTransform();
    updateScrollInfoAfterLayout();
    repainter.repaintAfterLayout();

    m_isNestedLayout = false;
    setNeedsLayout(false);
}

} // namespace WebCore
