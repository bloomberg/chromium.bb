// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/rendering/RenderBoxClipper.h"

#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBox.h"
#include "core/rendering/RenderLayer.h"

namespace blink {

RenderBoxClipper::RenderBoxClipper(RenderBox& box, PaintInfo& paintInfo, const LayoutPoint& accumulatedOffset, ContentsClipBehavior contentsClipBehavior)
    : m_pushedClip(false)
    , m_accumulatedOffset(accumulatedOffset)
    , m_paintInfo(paintInfo)
    , m_originalPhase(paintInfo.phase)
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

    if (m_paintInfo.phase == PaintPhaseOutline) {
        m_paintInfo.phase = PaintPhaseChildOutlines;
    } else if (m_paintInfo.phase == PaintPhaseChildBlockBackground) {
        m_paintInfo.phase = PaintPhaseBlockBackground;
        m_box.paintObject(m_paintInfo, m_accumulatedOffset);
        m_paintInfo.phase = PaintPhaseChildBlockBackgrounds;
    }
    m_paintInfo.context->save();
    if (hasBorderRadius)
        m_paintInfo.context->clipRoundedRect(clipRoundedRect);
    m_paintInfo.context->clip(pixelSnappedIntRect(clipRect));
    m_pushedClip = true;
}

RenderBoxClipper::~RenderBoxClipper()
{
    if (!m_pushedClip)
        return;

    ASSERT(m_box.hasControlClip() || (m_box.hasOverflowClip() && !m_box.layer()->isSelfPaintingLayer()));

    m_paintInfo.context->restore();
    if (m_originalPhase == PaintPhaseOutline) {
        m_paintInfo.phase = PaintPhaseSelfOutline;
        m_box.paintObject(m_paintInfo, m_accumulatedOffset);
        m_paintInfo.phase = m_originalPhase;
    } else if (m_originalPhase == PaintPhaseChildBlockBackground) {
        m_paintInfo.phase = m_originalPhase;
    }
}

} // namespace blink
