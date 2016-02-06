// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "core/frame/FrameView.h"
#include "core/layout/LayoutView.h"
#include "core/paint/PaintLayerScrollableArea.h"
#include "wtf/Assertions.h"

namespace blink {

ScrollAnchor::ScrollAnchor(ScrollableArea* scroller)
    : m_scroller(scroller)
    , m_anchorObject(nullptr)
{
    ASSERT(m_scroller);
    ASSERT(m_scroller->isFrameView() || m_scroller->isPaintLayerScrollableArea());
}

ScrollAnchor::~ScrollAnchor()
{
}

static LayoutBox* scrollerLayoutBox(const ScrollableArea* scroller)
{
    LayoutBox* box = scroller->isFrameView()
        ? toFrameView(scroller)->layoutView()
        : &toPaintLayerScrollableArea(scroller)->box();
    ASSERT(box);
    return box;
}

static LayoutObject* findAnchor(const ScrollableArea* scroller)
{
    LayoutBox* scrollerBox = scrollerLayoutBox(scroller);

    FloatRect absoluteVisibleRect = scroller->isFrameView()
        ? scroller->visibleContentRect()
        : scrollerBox->localToAbsoluteQuad(
            FloatQuad(FloatRect(scrollerBox->overflowClipRect(LayoutPoint())))).boundingBox();

    LayoutObject* child = scrollerBox->nextInPreOrder(scrollerBox);
    LayoutObject* candidate = nullptr;
    while (child) {
        // TODO(skobes): Compute scroller-relative bounds instead of absolute bounds.
        FloatRect childRect = child->absoluteBoundingBoxFloatRect();
        if (absoluteVisibleRect.intersects(childRect))
            candidate = child;
        if (absoluteVisibleRect.contains(childRect))
            break;
        child = child->nextInPreOrder(scrollerBox);
    }
    return candidate;
}

static DoublePoint computeRelativeOffset(const ScrollableArea* scroller, const LayoutObject* layoutObject)
{
    return DoublePoint(layoutObject->localToAbsolute() - scrollerLayoutBox(scroller)->localToAbsolute());
}

void ScrollAnchor::save()
{
    if (m_anchorObject)
        return;
    m_anchorObject = findAnchor(m_scroller);
    if (!m_anchorObject)
        return;
    m_anchorObject->setIsScrollAnchorObject();
    m_savedRelativeOffset = computeRelativeOffset(m_scroller, m_anchorObject);
}

void ScrollAnchor::restore()
{
    if (!m_anchorObject)
        return;

    DoubleSize adjustment = computeRelativeOffset(m_scroller, m_anchorObject) - m_savedRelativeOffset;
    if (!adjustment.isZero())
        m_scroller->setScrollPosition(m_scroller->scrollPositionDouble() + adjustment, AnchoringScroll);
}

void ScrollAnchor::clear()
{
    LayoutObject* anchorObject = m_anchorObject;
    m_anchorObject = nullptr;

    if (anchorObject)
        anchorObject->maybeClearIsScrollAnchorObject();
}

} // namespace blink
