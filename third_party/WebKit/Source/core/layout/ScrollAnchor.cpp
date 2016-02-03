// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/ScrollAnchor.h"

#include "platform/scroll/ScrollableArea.h"
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

static LayoutObject* findAnchor(const ScrollableArea* scroller)
{
    // TODO(skobes): implement.
    return nullptr;
}

static DoublePoint computeRelativeOffset(const ScrollableArea* scroller, const LayoutObject* layoutObject)
{
    // TODO(skobes): implement.
    return DoublePoint();
}

void ScrollAnchor::save()
{
    if (!m_anchorObject)
        m_anchorObject = findAnchor(m_scroller);

    if (m_anchorObject)
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

} // namespace blink
