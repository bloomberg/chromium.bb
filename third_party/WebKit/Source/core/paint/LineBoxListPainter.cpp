// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LineBoxListPainter.h"

#include "core/paint/InlinePainter.h"
#include "core/paint/ViewDisplayList.h"
#include "core/rendering/InlineFlowBox.h"
#include "core/rendering/PaintInfo.h"
#include "core/rendering/RenderBoxModelObject.h"
#include "core/rendering/RenderLineBoxList.h"
#include "core/rendering/RootInlineBox.h"

namespace blink {

void LineBoxListPainter::paint(RenderBoxModelObject* renderer, PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    // Only paint during the foreground/selection phases.
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseOutline
        && paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines && paintInfo.phase != PaintPhaseTextClip
        && paintInfo.phase != PaintPhaseMask)
        return;

    ASSERT(renderer->isRenderBlock() || (renderer->isRenderInline() && renderer->hasLayer())); // The only way an inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!m_renderLineBoxList.firstLineBox())
        return;

    if (!m_renderLineBoxList.anyLineIntersectsRect(renderer, paintInfo.rect, paintOffset))
        return;

    PaintInfo info(paintInfo);
    PaintCommandRecorder recorder(paintInfo.context, renderer, paintInfo.phase, pixelSnappedIntRect(paintOffset, renderer->borderBoundingBox().size()));
    ListHashSet<RenderInline*> outlineObjects;
    info.setOutlineObjects(&outlineObjects);

    // See if our root lines intersect with the dirty rect. If so, then we paint
    // them. Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (InlineFlowBox* curr = m_renderLineBoxList.firstLineBox(); curr; curr = curr->nextLineBox()) {
        if (m_renderLineBoxList.lineIntersectsDirtyRect(renderer, curr, info, paintOffset)) {
            RootInlineBox& root = curr->root();
            curr->paint(info, paintOffset, root.lineTop(), root.lineBottom());
        }
    }

    if (info.phase == PaintPhaseOutline || info.phase == PaintPhaseSelfOutline || info.phase == PaintPhaseChildOutlines) {
        ListHashSet<RenderInline*>::iterator end = info.outlineObjects()->end();
        for (ListHashSet<RenderInline*>::iterator it = info.outlineObjects()->begin(); it != end; ++it) {
            RenderInline* flow = *it;
            InlinePainter(*flow).paintOutline(info, paintOffset);
        }
        info.outlineObjects()->clear();
    }
}

} // namespace blink
