// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ClipRecorder.h"

#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderLayerModelObject.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/paint/ClipDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"

namespace blink {

ClipRecorder::ClipRecorder(RenderLayerModelObject& renderer, const PaintInfo& paintInfo, const LayoutRect& clipRect)
    : m_paintInfo(paintInfo)
    , m_renderer(renderer)
{
    DisplayItem::Type type = paintPhaseToClipType(paintInfo.phase);
    OwnPtr<ClipDisplayItem> clipDisplayItem = ClipDisplayItem::create(m_renderer.displayItemClient(), type, pixelSnappedIntRect(clipRect));

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_paintInfo.context->displayItemList());
        m_paintInfo.context->displayItemList()->add(clipDisplayItem.release());
    } else
        clipDisplayItem->replay(paintInfo.context);
}

ClipRecorder::~ClipRecorder()
{
    OwnPtr<EndClipDisplayItem> endClipDisplayItem = EndClipDisplayItem::create(m_renderer.displayItemClient());

    if (RuntimeEnabledFeatures::slimmingPaintEnabled()) {
        ASSERT(m_paintInfo.context->displayItemList());
        m_paintInfo.context->displayItemList()->add(endClipDisplayItem.release());
    } else {
        endClipDisplayItem->replay(m_paintInfo.context);
    }
}

DisplayItem::Type ClipRecorder::paintPhaseToClipType(PaintPhase paintPhase)
{
    switch (paintPhase) {
    case PaintPhaseChildBlockBackgrounds:
        return DisplayItem::ClipBoxChildBlockBackgrounds;
        break;
    case PaintPhaseFloat:
        return DisplayItem::ClipBoxFloat;
        break;
    case PaintPhaseForeground:
        return DisplayItem::ClipBoxChildBlockBackgrounds;
        break;
    case PaintPhaseChildOutlines:
        return DisplayItem::ClipBoxChildOutlines;
        break;
    case PaintPhaseSelection:
        return DisplayItem::ClipBoxSelection;
        break;
    case PaintPhaseCollapsedTableBorders:
        return DisplayItem::ClipBoxCollapsedTableBorders;
        break;
    case PaintPhaseTextClip:
        return DisplayItem::ClipBoxTextClip;
        break;
    case PaintPhaseClippingMask:
        return DisplayItem::ClipBoxClippingMask;
        break;
    case PaintPhaseChildBlockBackground:
    case PaintPhaseOutline:
    case PaintPhaseBlockBackground:
    case PaintPhaseSelfOutline:
    case PaintPhaseMask:
        ASSERT_NOT_REACHED();
    }
    // This should never happen.
    return DisplayItem::ClipBoxForeground;
}

} // namespace blink
