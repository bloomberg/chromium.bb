// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/DrawingRecorder.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
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

DrawingRecorder::DrawingRecorder(GraphicsContext* context, const RenderObject* renderer, PaintPhase phase, const FloatRect& clip)
    : m_context(context)
    , m_renderer(renderer)
    , m_phase(phase)
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
    if (!picture || !picture->approximateOpCount())
        return;
    OwnPtr<DrawingDisplayItem> drawingItem = adoptPtr(
        new DrawingDisplayItem(m_renderer->displayItemClient(), (DisplayItem::Type)m_phase, picture));
#ifndef NDEBUG
    if (!m_renderer)
        drawingItem->setClientDebugString("nullptr");
    else
        drawingItem->setClientDebugString(String::format("renderer: \"%p %s\"", m_renderer, m_renderer->debugName().utf8().data()));
#endif

    ASSERT(m_context->displayItemList());
    m_context->displayItemList()->add(drawingItem.release());
}

} // namespace blink
