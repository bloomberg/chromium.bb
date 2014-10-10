// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/ViewDisplayList.h"

#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderView.h"
#include "platform/NotImplemented.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/GraphicsContext.h"

namespace blink {

PaintCommandRecorder::PaintCommandRecorder(GraphicsContext* context, RenderObject* renderer, PaintPhase phase, const FloatRect& clip)
    : m_context(context)
    , m_renderer(renderer)
    , m_phase(phase)
{
    if (RuntimeEnabledFeatures::slimmingPaintEnabled())
        m_context->beginRecording(clip);
}

PaintCommandRecorder::~PaintCommandRecorder()
{
    if (!RuntimeEnabledFeatures::slimmingPaintEnabled())
        return;

    OwnPtr<AtomicPaintChunk> paintChunk = adoptPtr(new AtomicPaintChunk(m_context->endRecording(), m_renderer, m_phase));
    ASSERT(m_renderer->view());
    m_renderer->view()->viewDisplayList().add(paintChunk.release());
}

const PaintCommandList& ViewDisplayList::paintCommandList()
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());

    updatePaintCommandList();
    return m_paintList;
}

void ViewDisplayList::add(WTF::PassOwnPtr<AtomicPaintChunk> atomicPaintChunk)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_newPaints.append(atomicPaintChunk);
}

void ViewDisplayList::invalidate(const RenderObject* renderer)
{
    ASSERT(RuntimeEnabledFeatures::slimmingPaintEnabled());
    m_invalidated.add(renderer);
}

bool ViewDisplayList::isRepaint(PaintCommandList::iterator begin, const AtomicPaintChunk& atomicPaintChunk)
{
    notImplemented();
    return false;
}

// Update the existing paintList by removing invalidated entries, updating repainted existing ones, and
// appending new items.
//
// The algorithm should be O(|existing paint list| + |newly painted list|). By using the ordering
// implied by the existing paint list, extra treewalks are avoided.
void ViewDisplayList::updatePaintCommandList()
{
    notImplemented();
}

} // namespace blink
