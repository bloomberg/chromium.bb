// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/DrawingRecorder.h"

#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

void DrawingDisplayItem::replay(GraphicsContext* context)
{
    context->drawPicture(m_picture.get(), m_location);
}

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
        new DrawingDisplayItem(displayList->picture(), m_bounds.location(), m_phase, m_renderer));
    ASSERT(m_renderer->view());
    m_renderer->view()->viewDisplayList().add(drawingItem.release());
}

#ifndef NDEBUG
WTF::String DrawingDisplayItem::asDebugString() const
{
    return String::format("{%s, type: \"%s\", location: [%f,%f]}",
        rendererDebugString(renderer()).utf8().data(), typeAsDebugString(type()).utf8().data(),
        m_location.x(), m_location.y());
}
#endif

} // namespace blink
