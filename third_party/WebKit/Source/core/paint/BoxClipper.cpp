// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxClipper.h"

#include "core/layout/Layer.h"
#include "core/layout/LayoutBox.h"
#include "core/layout/PaintInfo.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

BoxClipper::BoxClipper(LayoutBox& box, const PaintInfo& paintInfo, const LayoutPoint& accumulatedOffset, ContentsClipBehavior contentsClipBehavior)
    : m_pushedClip(false)
    , m_accumulatedOffset(accumulatedOffset)
    , m_paintInfo(paintInfo)
    , m_box(box)
    , m_clipType(DisplayItem::ClipBoxPaintPhaseFirst)
{
    if (m_paintInfo.phase == PaintPhaseBlockBackground || m_paintInfo.phase == PaintPhaseSelfOutline || m_paintInfo.phase == PaintPhaseMask)
        return;

    bool isControlClip = m_box.hasControlClip();
    bool isOverflowClip = m_box.hasOverflowClip() && !m_box.layer()->isSelfPaintingLayer();

    if (!isControlClip && !isOverflowClip)
        return;

    LayoutRect clipRect = isControlClip ? m_box.controlClipRect(m_accumulatedOffset) : m_box.overflowClipRect(m_accumulatedOffset);
    FloatRoundedRect clipRoundedRect(0, 0, 0, 0);
    bool hasBorderRadius = m_box.style()->hasBorderRadius();
    if (hasBorderRadius)
        clipRoundedRect = m_box.style()->getRoundedInnerBorderFor(LayoutRect(m_accumulatedOffset, m_box.size()));

    if (contentsClipBehavior == SkipContentsClipIfPossible) {
        LayoutRect contentsVisualOverflow = m_box.contentsVisualOverflowRect();
        if (contentsVisualOverflow.isEmpty())
            return;

        LayoutRect conservativeClipRect = clipRect;
        if (hasBorderRadius)
            conservativeClipRect.intersect(LayoutRect(clipRoundedRect.radiusCenterRect()));
        conservativeClipRect.moveBy(-m_accumulatedOffset);
        if (m_box.hasLayer())
            conservativeClipRect.move(m_box.scrolledContentOffset());
        if (conservativeClipRect.contains(contentsVisualOverflow))
            return;
    }

    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        m_clipType = m_paintInfo.displayItemTypeForClipping();

    OwnPtr<ClipDisplayItem> clipDisplayItem = ClipDisplayItem::create(m_box.displayItemClient(), m_clipType, pixelSnappedIntRect(clipRect));
    if (hasBorderRadius)
        clipDisplayItem->roundedRectClips().append(clipRoundedRect);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_paintInfo.context->displayItemList());
        m_paintInfo.context->displayItemList()->add(clipDisplayItem.release());
    } else
        clipDisplayItem->replay(paintInfo.context);

    m_pushedClip = true;
}

BoxClipper::~BoxClipper()
{
    if (!m_pushedClip)
        return;

    ASSERT(m_box.hasControlClip() || (m_box.hasOverflowClip() && !m_box.layer()->isSelfPaintingLayer()));

    DisplayItem::Type endType = DisplayItem::clipTypeToEndClipType(m_clipType);
    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        OwnPtr<EndClipDisplayItem> endClipDisplayItem = EndClipDisplayItem::create(m_box.displayItemClient(), endType);
        ASSERT(m_paintInfo.context->displayItemList());
        m_paintInfo.context->displayItemList()->add(endClipDisplayItem.release());
    } else {
        EndClipDisplayItem endClipDisplayItem(m_box.displayItemClient(), endType);
        endClipDisplayItem.replay(m_paintInfo.context);
    }
}

} // namespace blink
