/*
 * Copyright (c) 2013, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/rendering/LayoutRectRecorder.h"

#include "core/rendering/RenderObject.h"

namespace WebCore {

bool LayoutRectRecorder::shouldRecordLayoutRects()
{
    bool isTracing;
    TRACE_EVENT_CATEGORY_GROUP_ENABLED(TRACE_DISABLED_BY_DEFAULT("blink.debug.layout"), &isTracing);
    return RuntimeEnabledFeatures::repaintAfterLayoutEnabled() || isTracing;
}

LayoutRectRecorder::LayoutRectRecorder(RenderObject& object, bool record)
    : m_object(object)
    , m_record(record)
{
    if (!shouldRecordLayoutRects())
        return;
    if (!m_record)
        return;

    if (!m_object.layoutDidGetCalled()) {
        RenderLayerModelObject* containerForRepaint = m_object.containerForRepaint();
        m_object.setOldRepaintRect(m_object.clippedOverflowRectForRepaint(containerForRepaint));

        if (m_object.hasOutline())
            m_object.setOldOutlineRect(m_object.outlineBoundsForRepaint(containerForRepaint));
    }

    // If should do repaint was set previously make sure we don't accidentally unset it.
    if (!m_object.shouldDoFullRepaintAfterLayout())
        m_object.setShouldDoFullRepaintAfterLayout(m_object.selfNeedsLayout());
    if (m_object.needsPositionedMovementLayoutOnly())
        m_object.setOnlyNeededPositionedMovementLayout(true);

    m_object.setLayoutDidGetCalled(true);
}

LayoutRectRecorder::~LayoutRectRecorder()
{
    if (!shouldRecordLayoutRects())
        return;
    if (!m_record)
        return;

    // Note, we don't store the repaint container because it can change during layout.
    RenderLayerModelObject* containerForRepaint = m_object.containerForRepaint();
    m_object.setNewRepaintRect(m_object.clippedOverflowRectForRepaint(containerForRepaint));

    if (m_object.hasOutline())
        m_object.setNewOutlineRect(m_object.outlineBoundsForRepaint(containerForRepaint));
}

} // namespace WebCore

