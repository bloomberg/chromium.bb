// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DrawingRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/paint/CachedDisplayItem.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

DrawingRecorder::DrawingRecorder(GraphicsContext& context, const DisplayItemClientWrapper& displayItemClient, DisplayItem::Type displayItemType, const FloatRect& cullRect)
    : m_context(context)
    , m_displayItemClient(displayItemClient)
    , m_displayItemType(displayItemType)
    , m_canUseCachedDrawing(false)
#if ENABLE(ASSERT)
    , m_checkedCachedDrawing(false)
    , m_displayItemPosition(RuntimeEnabledFeatures::slimmingPaintEnabled() ? m_context.displayItemList()->newDisplayItemsSize() : 0)
    , m_underInvalidationCheckingMode(DrawingDisplayItem::CheckPicture)
#endif
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    ASSERT(context.displayItemList());
    if (context.displayItemList()->displayItemConstructionIsDisabled())
        return;

    ASSERT(DisplayItem::isDrawingType(displayItemType));
    m_canUseCachedDrawing = context.displayItemList()->clientCacheIsValid(displayItemClient.displayItemClient());

#if ENABLE(ASSERT)
    context.setInDrawingRecorder(true);
    m_canUseCachedDrawing &= !RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled();
#endif

#ifndef NDEBUG
    // Enable recording to check if any painter is still doing unnecessary painting when we can use cache.
    context.beginRecording(cullRect);
#else
    if (!m_canUseCachedDrawing)
        context.beginRecording(cullRect);
#endif

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintStrictCullRectClippingEnabled() && !m_canUseCachedDrawing) {
        // Skia depends on the cull rect containing all of the display item commands. When strict
        // cull rect clipping is enabled, make this explicit. This allows us to identify potential
        // incorrect cull rects that might otherwise be masked due to Skia internal optimizations.
        context.save();
        IntRect verificationClip = enclosingIntRect(cullRect);
        // Expand the verification clip by one pixel to account for Skia's SkCanvas::getClipBounds()
        // expansion, used in testing cull rects.
        // TODO(schenney) This is not the best place to do this. Ideally, we would expand by one pixel
        // in device (pixel) space, but to do that we would need to add the verification mode to Skia.
        verificationClip.inflate(1);
        context.clipRect(verificationClip, NotAntiAliased, SkRegion::kIntersect_Op);
    }
#endif
}

DrawingRecorder::~DrawingRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    ASSERT(m_context.displayItemList());
    if (m_context.displayItemList()->displayItemConstructionIsDisabled())
        return;

#if ENABLE(ASSERT)
    if (RuntimeEnabledFeatures::slimmingPaintStrictCullRectClippingEnabled() && !m_canUseCachedDrawing) {
        m_context.restore();
    }
    ASSERT(m_checkedCachedDrawing);
    m_context.setInDrawingRecorder(false);
    ASSERT(m_displayItemPosition == m_context.displayItemList()->newDisplayItemsSize());
#endif

    if (m_canUseCachedDrawing) {
#ifndef NDEBUG
        RefPtr<const SkPicture> picture = m_context.endRecording();
        if (picture && picture->approximateOpCount()) {
            WTF_LOG_ERROR("Unnecessary painting for %s\n. Should check recorder.canUseCachedDrawing() before painting",
                m_displayItemClient.debugName().utf8().data());
        }
#endif
        m_context.displayItemList()->add(CachedDisplayItem::create(m_displayItemClient, DisplayItem::drawingTypeToCachedType(m_displayItemType)));
    } else {
        OwnPtr<DrawingDisplayItem> drawingDisplayItem = DrawingDisplayItem::create(m_displayItemClient
            , m_displayItemType
            , m_context.endRecording()
#if ENABLE(ASSERT)
            , m_underInvalidationCheckingMode
#endif
            );
        m_context.displayItemList()->add(drawingDisplayItem.release());
    }
}

} // namespace blink
