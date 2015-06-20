// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/RoundedInnerRectClipper.h"

#include "core/layout/LayoutBox.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

RoundedInnerRectClipper::RoundedInnerRectClipper(LayoutObject& layoutObject, const PaintInfo& paintInfo, const LayoutRect& rect, const FloatRoundedRect& clipRect, RoundedInnerRectClipperBehavior behavior)
    : m_layoutObject(layoutObject)
    , m_paintInfo(paintInfo)
    , m_useDisplayItemList(RuntimeEnabledFeatures::slimmingPaintEnabled() && behavior == ApplyToDisplayListIfEnabled)
    , m_clipType(m_useDisplayItemList ? m_paintInfo.displayItemTypeForClipping() : DisplayItem::ClipBoxPaintPhaseFirst)
{
    OwnPtr<Vector<FloatRoundedRect>> roundedRectClips = adoptPtr(new Vector<FloatRoundedRect>());
    if (clipRect.isRenderable()) {
        roundedRectClips->append(clipRect);
    } else {
        // We create a rounded rect for each of the corners and clip it, while making sure we clip opposing corners together.
        if (!clipRect.radii().topLeft().isEmpty() || !clipRect.radii().bottomRight().isEmpty()) {
            FloatRect topCorner(clipRect.rect().x(), clipRect.rect().y(), rect.maxX() - clipRect.rect().x(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopLeft(clipRect.radii().topLeft());
            roundedRectClips->append(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(rect.x().toFloat(), rect.y().toFloat(), clipRect.rect().maxX() - rect.x().toFloat(), clipRect.rect().maxY() - rect.y().toFloat());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomRight(clipRect.radii().bottomRight());
            roundedRectClips->append(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        }

        if (!clipRect.radii().topRight().isEmpty() || !clipRect.radii().bottomLeft().isEmpty()) {
            FloatRect topCorner(rect.x().toFloat(), clipRect.rect().y(), clipRect.rect().maxX() - rect.x().toFloat(), rect.maxY() - clipRect.rect().y());
            FloatRoundedRect::Radii topCornerRadii;
            topCornerRadii.setTopRight(clipRect.radii().topRight());
            roundedRectClips->append(FloatRoundedRect(topCorner, topCornerRadii));

            FloatRect bottomCorner(clipRect.rect().x(), rect.y().toFloat(), rect.maxX() - clipRect.rect().x(), clipRect.rect().maxY() - rect.y().toFloat());
            FloatRoundedRect::Radii bottomCornerRadii;
            bottomCornerRadii.setBottomLeft(clipRect.radii().bottomLeft());
            roundedRectClips->append(FloatRoundedRect(bottomCorner, bottomCornerRadii));
        }
    }

    OwnPtr<ClipDisplayItem> clipDisplayItem = ClipDisplayItem::create(layoutObject, m_clipType, LayoutRect::infiniteIntRect(), roundedRectClips.release());
    if (m_useDisplayItemList) {
        ASSERT(m_paintInfo.context->displayItemList());
        if (m_paintInfo.context->displayItemList()->displayItemConstructionIsDisabled())
            return;
        m_paintInfo.context->displayItemList()->add(clipDisplayItem.release());
    } else {
        clipDisplayItem->replay(*paintInfo.context);
    }
}

RoundedInnerRectClipper::~RoundedInnerRectClipper()
{
    DisplayItem::Type endType = DisplayItem::clipTypeToEndClipType(m_clipType);
    if (m_useDisplayItemList) {
        ASSERT(m_paintInfo.context->displayItemList());
        if (m_paintInfo.context->displayItemList()->displayItemConstructionIsDisabled())
            return;
        OwnPtr<EndClipDisplayItem> endClipDisplayItem = EndClipDisplayItem::create(m_layoutObject, endType);
        m_paintInfo.context->displayItemList()->add(endClipDisplayItem.release());
    } else {
        EndClipDisplayItem endClipDisplayItem(m_layoutObject, endType);
        endClipDisplayItem.replay(*m_paintInfo.context);
    }
}

} // namespace blink
