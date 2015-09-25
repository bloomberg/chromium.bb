// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutObjectDrawingRecorder_h
#define LayoutObjectDrawingRecorder_h

#include "core/layout/LayoutObject.h"
#include "core/paint/PaintPhase.h"
#include "platform/geometry/LayoutPoint.h"
#include "platform/geometry/LayoutRect.h"
#include "platform/graphics/paint/DisplayItemCacheSkipper.h"
#include "platform/graphics/paint/DrawingRecorder.h"
#include "wtf/Allocator.h"
#include "wtf/Optional.h"

namespace blink {

class GraphicsContext;

// Convenience wrapper of DrawingRecorder for LayoutObject painters.
class LayoutObjectDrawingRecorder final {
    ALLOW_ONLY_INLINE_ALLOCATION();
public:
    static bool useCachedDrawingIfPossible(GraphicsContext& context, const LayoutObject& layoutObject, DisplayItem::Type displayItemType, const LayoutPoint& paintOffset)
    {
        if (RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled() && layoutObject.paintOffsetChanged(paintOffset))
            return false;
        if (layoutObject.fullPaintInvalidationReason() == PaintInvalidationDelayedFull)
            return false;
        return DrawingRecorder::useCachedDrawingIfPossible(context, layoutObject, displayItemType);
    }

    static bool useCachedDrawingIfPossible(GraphicsContext& context, const LayoutObject& layoutObject, PaintPhase phase, const LayoutPoint& paintOffset)
    {
        return useCachedDrawingIfPossible(context, layoutObject, DisplayItem::paintPhaseToDrawingType(phase), paintOffset);
    }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, DisplayItem::Type displayItemType, const FloatRect& clip, const LayoutPoint& paintOffset)
    {
        updatePaintOffsetIfNeeded(context.displayItemList(), layoutObject, paintOffset);
        // We may paint a delayed-invalidation object before it's actually invalidated.
        if (layoutObject.fullPaintInvalidationReason() == PaintInvalidationDelayedFull)
            m_cacheSkipper.emplace(context);
        m_drawingRecorder.emplace(context, layoutObject, displayItemType, clip);
    }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, DisplayItem::Type displayItemType, const LayoutRect& clip, const LayoutPoint& paintOffset)
        : LayoutObjectDrawingRecorder(context, layoutObject, displayItemType, FloatRect(clip), paintOffset) { }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, PaintPhase phase, const FloatRect& clip, const LayoutPoint& paintOffset)
        : LayoutObjectDrawingRecorder(context, layoutObject, DisplayItem::paintPhaseToDrawingType(phase), clip, paintOffset) { }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, PaintPhase phase, const LayoutRect& clip, const LayoutPoint& paintOffset)
        : LayoutObjectDrawingRecorder(context, layoutObject, DisplayItem::paintPhaseToDrawingType(phase), FloatRect(clip), paintOffset) { }

    LayoutObjectDrawingRecorder(GraphicsContext& context, const LayoutObject& layoutObject, PaintPhase phase, const IntRect& clip, const LayoutPoint& paintOffset)
        : LayoutObjectDrawingRecorder(context, layoutObject, DisplayItem::paintPhaseToDrawingType(phase), FloatRect(clip), paintOffset) { }

#if ENABLE(ASSERT)
    void setUnderInvalidationCheckingMode(DrawingDisplayItem::UnderInvalidationCheckingMode mode) { m_drawingRecorder->setUnderInvalidationCheckingMode(mode); }
#endif

private:
    static void updatePaintOffsetIfNeeded(DisplayItemList* displayItemList, const LayoutObject& layoutObject, const LayoutPoint& paintOffset)
    {
        if (!RuntimeEnabledFeatures::slimmingPaintOffsetCachingEnabled())
            return;

        if (layoutObject.paintOffsetChanged(paintOffset))
            displayItemList->invalidatePaintOffset(layoutObject);
        else
            ASSERT(!displayItemList->paintOffsetWasInvalidated(layoutObject.displayItemClient()) || !displayItemList->clientCacheIsValid(layoutObject.displayItemClient()));

        layoutObject.setPreviousPaintOffset(paintOffset);
    }

    Optional<DisplayItemCacheSkipper> m_cacheSkipper;
    Optional<DrawingRecorder> m_drawingRecorder;
};

} // namespace blink

#endif // LayoutObjectDrawingRecorder_h
