// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "platform/graphics/paint/DrawingRecorder.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
#include "platform/graphics/Picture.h"
#include "platform/graphics/paint/DisplayItemList.h"
#include "platform/graphics/paint/DrawingDisplayItem.h"

namespace blink {

#if ENABLE(ASSERT)
static bool s_inDrawingRecorder = false;
#endif

DrawingRecorder::DrawingRecorder(GraphicsContext* context, const DisplayItemClient displayItemClient, DisplayItem::Type displayItemType, const FloatRect& clip)
    : m_context(context)
    , m_displayItemClient(displayItemClient)
    , m_displayItemType(displayItemType)
    , m_bounds(clip)
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

#if ENABLE(ASSERT)
    ASSERT(!s_inDrawingRecorder);
    s_inDrawingRecorder = true;
#endif

    m_context->beginRecording(clip);
}

DrawingRecorder::~DrawingRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

#if ENABLE(ASSERT)
    s_inDrawingRecorder = false;
#endif

    RefPtr<Picture> picture = m_context->endRecording();
    if (!picture->skPicture() || !picture->skPicture()->approximateOpCount())
        return;
    ASSERT(picture->bounds() == m_bounds);
    OwnPtr<DrawingDisplayItem> drawingItem = adoptPtr(
        new DrawingDisplayItem(m_displayItemClient, m_displayItemType, picture->skPicture(), m_bounds.location()));

#ifndef NDEBUG
    drawingItem->setClientDebugString(m_clientDebugString);
#endif

    ASSERT(m_context->displayItemList());
    m_context->displayItemList()->add(drawingItem.release());
}

#ifndef NDEBUG
void DrawingRecorder::setClientDebugString(const WTF::String& clientDebugString)
{
    m_clientDebugString = clientDebugString;
}
#endif

} // namespace blink
