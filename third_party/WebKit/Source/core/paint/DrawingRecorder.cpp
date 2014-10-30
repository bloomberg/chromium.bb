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

DrawingRecorder::DrawingRecorder(GraphicsContext* context, RenderObject* renderer, PaintPhase phase, const FloatRect& clip)
    : m_context(context)
    , m_renderer(renderer)
    , m_phase(phase)
    , m_bounds(clip)
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    m_context->beginRecording(clip);
}

DrawingRecorder::~DrawingRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    RefPtr<DisplayList> displayList = m_context->endRecording();
    if (!displayList->picture()->approximateOpCount())
        return;
    ASSERT(displayList->bounds() == m_bounds);
    OwnPtr<DrawingDisplayItem> drawingItem = adoptPtr(
        new DrawingDisplayItem(displayList->picture(), m_bounds.location(), m_phase, m_renderer));
    ASSERT(m_renderer->view());
    m_renderer->view()->viewDisplayList().add(drawingItem.release());
}

} // namespace blink
