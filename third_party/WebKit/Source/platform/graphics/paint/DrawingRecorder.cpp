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
#include "platform/graphics/paint/DrawingDisplayItem.h"
#include "third_party/skia/include/core/SkPicture.h"

namespace blink {

DrawingRecorder::DrawingRecorder(GraphicsContext* context, DisplayItemClient displayItemClient, DisplayItem::Type displayItemType, const FloatRect& bounds)
    : m_context(context)
    , m_displayItemClient(displayItemClient)
    , m_displayItemType(displayItemType)
    , m_canUseCachedDrawing(false)
#if ENABLE(ASSERT)
    , m_checkedCachedDrawing(false)
    , m_displayItemPosition(RuntimeEnabledFeatures::slimmingPaintEnabled() ? m_context->displayItemList()->newPaintsSize() : 0)
#endif
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    ASSERT(DisplayItem::isDrawingType(displayItemType));
#if ENABLE(ASSERT)
    context->setInDrawingRecorder(true);
#endif
    ASSERT(context->displayItemList());
    m_canUseCachedDrawing = context->displayItemList()->clientCacheIsValid(displayItemClient)
        && !RuntimeEnabledFeatures::slimmingPaintUnderInvalidationCheckingEnabled();

#ifndef NDEBUG
    // Enable recording to check if any painter is still doing unnecessary painting when we can use cache.
    m_context->beginRecording(bounds);
#else
    if (!m_canUseCachedDrawing)
        m_context->beginRecording(bounds);
#endif
}

DrawingRecorder::~DrawingRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

#if ENABLE(ASSERT)
    ASSERT(m_checkedCachedDrawing);
    m_context->setInDrawingRecorder(false);
#endif

    OwnPtr<DisplayItem> displayItem;

    if (m_canUseCachedDrawing) {
#ifndef NDEBUG
        RefPtr<const SkPicture> picture = m_context->endRecording();
        if (picture && picture->approximateOpCount()) {
            WTF_LOG_ERROR("Unnecessary painting for %s\n. Should check recorder.canUseCachedDrawing() before painting",
                m_clientDebugString.utf8().data());
        }
#endif
        displayItem = CachedDisplayItem::create(m_displayItemClient, DisplayItem::drawingTypeToCachedType(m_displayItemType));
    } else {
        RefPtr<const SkPicture> picture = m_context->endRecording();
        if (!picture || !picture->approximateOpCount())
            return;
        displayItem = DrawingDisplayItem::create(m_displayItemClient, m_displayItemType, picture);
    }

#ifndef NDEBUG
    displayItem->setClientDebugString(m_clientDebugString);
#endif

#if ENABLE(ASSERT)
    ASSERT(m_displayItemPosition == m_context->displayItemList()->newPaintsSize());
#endif
    m_context->displayItemList()->add(displayItem.release());
}

#ifndef NDEBUG
void DrawingRecorder::setClientDebugString(const WTF::String& clientDebugString)
{
    m_clientDebugString = clientDebugString;
}
#endif

} // namespace blink
