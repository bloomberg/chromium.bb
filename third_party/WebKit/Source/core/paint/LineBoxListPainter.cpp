// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/paint/LineBoxListPainter.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/PaintInfo.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/line/LineBoxList.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/InlinePainter.h"

namespace blink {

void LineBoxListPainter::paint(LayoutBoxModelObject* renderer, const PaintInfo& paintInfo, const LayoutPoint& paintOffset) const
{
    // Only paint during the foreground/selection phases.
    if (paintInfo.phase != PaintPhaseForeground && paintInfo.phase != PaintPhaseSelection && paintInfo.phase != PaintPhaseOutline
        && paintInfo.phase != PaintPhaseSelfOutline && paintInfo.phase != PaintPhaseChildOutlines && paintInfo.phase != PaintPhaseTextClip
        && paintInfo.phase != PaintPhaseMask)
        return;

    ASSERT(renderer->isLayoutBlock() || (renderer->isLayoutInline() && renderer->hasLayer())); // The only way an inline could paint like this is if it has a layer.

    // If we have no lines then we have no work to do.
    if (!m_lineBoxList.firstLineBox())
        return;

    if (!m_lineBoxList.anyLineIntersectsRect(renderer, LayoutRect(paintInfo.rect), paintOffset))
        return;

    PaintInfo info(paintInfo);
    ListHashSet<LayoutInline*> outlineObjects;
    info.setOutlineObjects(&outlineObjects);

    // See if our root lines intersect with the dirty rect. If so, then we paint
    // them. Note that boxes can easily overlap, so we can't make any assumptions
    // based off positions of our first line box or our last line box.
    for (InlineFlowBox* curr = m_lineBoxList.firstLineBox(); curr; curr = curr->nextLineBox()) {
        if (m_lineBoxList.lineIntersectsDirtyRect(renderer, curr, info, paintOffset)) {
            RootInlineBox& root = curr->root();
            curr->paint(info, paintOffset, root.lineTop(), root.lineBottom());
        }
    }

    if (info.phase == PaintPhaseOutline || info.phase == PaintPhaseSelfOutline || info.phase == PaintPhaseChildOutlines) {
        for (LayoutInline* flow : *info.outlineObjects())
            InlinePainter(*flow).paintOutline(info, paintOffset);
        info.outlineObjects()->clear();
    }
}

} // namespace blink
