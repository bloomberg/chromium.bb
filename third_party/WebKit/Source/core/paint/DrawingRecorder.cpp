// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/DrawingRecorder.h"

#include "core/rendering/RenderLayer.h"
#include "core/rendering/RenderObject.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/DisplayList.h"
#include "platform/graphics/GraphicsContext.h"
#include "platform/graphics/GraphicsLayer.h"
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

    RefPtr<DisplayList> displayList = m_context->endRecording();
    if (!displayList->picture()->approximateOpCount())
        return;
    ASSERT(displayList->bounds() == m_bounds);
    OwnPtr<DrawingDisplayItem> drawingItem = adoptPtr(
        new DrawingDisplayItem(m_renderer->displayItemClient(), (DisplayItem::Type)m_phase, displayList->picture(), m_bounds.location()));
#ifndef NDEBUG
    if (!m_renderer)
        drawingItem->setClientDebugString("nullptr");
    else if (m_renderer->node())
        drawingItem->setClientDebugString(String::format("nodeName: \"%s\", renderer: \"%p\"", m_renderer->node()->nodeName().utf8().data(), m_renderer));
    else
        drawingItem->setClientDebugString(String::format("renderer: \"%p\"", m_renderer));
#endif

    if (RenderLayer* container = m_renderer->enclosingLayer()->enclosingLayerForPaintInvalidationCrossingFrameBoundaries())
        container->graphicsLayerBacking()->displayItemList().add(drawingItem.release());
}

} // namespace blink
