// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/BoxClipper.h"

#include "core/paint/ClipRecorder.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLayer.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

BoxClipper::BoxClipper(RenderBox& box, const PaintInfo& paintInfo, const LayoutPoint& accumulatedOffset, ContentsClipBehavior contentsClipBehavior)
    : m_pushedClip(false)
    , m_accumulatedOffset(accumulatedOffset)
    , m_paintInfo(paintInfo)
    , m_box(box)
{
    if (m_paintInfo.phase == PaintPhaseBlockBackground || m_paintInfo.phase == PaintPhaseSelfOutline || m_paintInfo.phase == PaintPhaseMask)
        return;

    bool isControlClip = m_box.hasControlClip();
    bool isOverflowClip = m_box.hasOverflowClip() && !m_box.layer()->isSelfPaintingLayer();

    if (!isControlClip && !isOverflowClip)
        return;

    LayoutRect clipRect = isControlClip ? m_box.controlClipRect(m_accumulatedOffset) : m_box.overflowClipRect(m_accumulatedOffset);
    RoundedRect clipRoundedRect(0, 0, 0, 0);
    bool hasBorderRadius = m_box.style()->hasBorderRadius();
    if (hasBorderRadius)
        clipRoundedRect = m_box.style()->getRoundedInnerBorderFor(LayoutRect(m_accumulatedOffset, m_box.size()));

    if (contentsClipBehavior == SkipContentsClipIfPossible) {
        LayoutRect contentsVisualOverflow = m_box.contentsVisualOverflowRect();
        if (contentsVisualOverflow.isEmpty())
            return;

        LayoutRect conservativeClipRect = clipRect;
        if (hasBorderRadius)
            conservativeClipRect.intersect(clipRoundedRect.radiusCenterRect());
        conservativeClipRect.moveBy(-m_accumulatedOffset);
        if (m_box.hasLayer())
            conservativeClipRect.move(m_box.scrolledContentOffset());
        if (conservativeClipRect.contains(contentsVisualOverflow))
            return;
    }

    DisplayItem::Type clipType = DisplayItem::ClipBoxForeground;
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        clipType = ClipRecorder::paintPhaseToClipType(m_paintInfo.phase);

    OwnPtr<ClipDisplayItem> clipDisplayItem = adoptPtr(new ClipDisplayItem(&m_box, clipType, pixelSnappedIntRect(clipRect)));
    if (hasBorderRadius)
        clipDisplayItem->roundedRectClips().append(clipRoundedRect);

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        if (RenderLayer* container = m_box.enclosingLayer()->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
            container->graphicsLayerBacking()->displayItemList().add(clipDisplayItem.release());
    } else
        clipDisplayItem->replay(paintInfo.context);

    m_pushedClip = true;
}

BoxClipper::~BoxClipper()
{
    if (!m_pushedClip)
        return;

    ASSERT(m_box.hasControlClip() || (m_box.hasOverflowClip() && !m_box.layer()->isSelfPaintingLayer()));

    OwnPtr<EndClipDisplayItem> endClipDisplayItem = adoptPtr(new EndClipDisplayItem(&m_box));

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        if (RenderLayer* container = m_box.enclosingLayer()->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
            container->graphicsLayerBacking()->displayItemList().add(endClipDisplayItem.release());
    } else {
        endClipDisplayItem->replay(m_paintInfo.context);
    }
}

} // namespace blink
