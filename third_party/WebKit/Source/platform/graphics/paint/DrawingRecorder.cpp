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

#if ENABLE(ASSERT)
static bool s_inDrawingRecorder = false;
#endif

DrawingRecorder::DrawingRecorder(GraphicsContext* context, const DisplayItemClient displayItemClient, DisplayItem::Type displayItemType, const FloatRect& bounds)
    : m_context(context)
    , m_displayItemClient(displayItemClient)
    , m_displayItemType(displayItemType)
    , m_bounds(bounds)
    , m_canUseCachedDrawing(false)
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

#if ENABLE(ASSERT)
    ASSERT(!s_inDrawingRecorder);
    s_inDrawingRecorder = true;
#endif

    m_canUseCachedDrawing = context->displayItemList()->clientCacheIsValid(displayItemClient);
    // FIXME: beginRecording only if !m_canUseCachedDrawing or ENABLE(ASSERT)
    // after all painters call canUseCachedDrawing().
    m_context->beginRecording(bounds);
}

DrawingRecorder::~DrawingRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

#if ENABLE(ASSERT)
    s_inDrawingRecorder = false;
#endif

    OwnPtr<DisplayItem> displayItem;

    if (m_canUseCachedDrawing) {
        // FIXME: endRecording only if ENABLE(ASSERT), and enable the following ASSERT
        // after all painters call canUseCachedDrawing().
        RefPtr<const SkPicture> picture = m_context->endRecording();
        // ASSERT(!picture || !picture->approximateOpCount());
        displayItem = CachedDisplayItem::create(m_displayItemClient, m_displayItemType);
    } else {
        RefPtr<const SkPicture> picture = m_context->endRecording();
        if (!picture || !picture->approximateOpCount())
            return;
        displayItem = DrawingDisplayItem::create(m_displayItemClient, m_displayItemType, picture);
    }

#ifndef NDEBUG
    displayItem->setClientDebugString(m_clientDebugString);
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
