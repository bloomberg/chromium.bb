// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/LineBoxListPainter.h"

#include "core/layout/LayoutBoxModelObject.h"
#include "core/layout/api/LineLayoutBoxModel.h"
#include "core/layout/line/InlineFlowBox.h"
#include "core/layout/line/LineBoxList.h"
#include "core/layout/line/RootInlineBox.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "platform/graphics/paint/PaintController.h"

namespace blink {

static void AddPDFURLRectsForInlineChildrenRecursively(
    const LayoutObject& layout_object,
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  for (LayoutObject* child = layout_object.SlowFirstChild(); child;
       child = child->NextSibling()) {
    if (!child->IsLayoutInline() ||
        ToLayoutBoxModelObject(child)->HasSelfPaintingLayer())
      continue;
    ObjectPainter(*child).AddPDFURLRectIfNeeded(paint_info, paint_offset);
    AddPDFURLRectsForInlineChildrenRecursively(*child, paint_info,
                                               paint_offset);
  }
}

void LineBoxListPainter::Paint(const LayoutBoxModelObject& layout_object,
                               const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) const {
  DCHECK(!ShouldPaintSelfOutline(paint_info.phase) &&
         !ShouldPaintDescendantOutlines(paint_info.phase));

  // Only paint during the foreground/selection phases.
  if (paint_info.phase != kPaintPhaseForeground &&
      paint_info.phase != kPaintPhaseSelection &&
      paint_info.phase != kPaintPhaseTextClip &&
      paint_info.phase != kPaintPhaseMask)
    return;

  // The only way an inline could paint like this is if it has a layer.
  DCHECK(layout_object.IsLayoutBlock() ||
         (layout_object.IsLayoutInline() && layout_object.HasLayer()));

  if (paint_info.phase == kPaintPhaseForeground && paint_info.IsPrinting())
    AddPDFURLRectsForInlineChildrenRecursively(layout_object, paint_info,
                                               paint_offset);

  // If we have no lines then we have no work to do.
  if (!line_box_list_.FirstLineBox())
    return;

  if (!line_box_list_.AnyLineIntersectsRect(
          LineLayoutBoxModel(const_cast<LayoutBoxModelObject*>(&layout_object)),
          paint_info.GetCullRect(), paint_offset))
    return;

  PaintInfo info(paint_info);

  // See if our root lines intersect with the dirty rect. If so, then we paint
  // them. Note that boxes can easily overlap, so we can't make any assumptions
  // based off positions of our first line box or our last line box.
  for (InlineFlowBox* curr = line_box_list_.FirstLineBox(); curr;
       curr = curr->NextLineBox()) {
    if (line_box_list_.LineIntersectsDirtyRect(
            LineLayoutBoxModel(
                const_cast<LayoutBoxModelObject*>(&layout_object)),
            curr, info.GetCullRect(), paint_offset)) {
      RootInlineBox& root = curr->Root();
      curr->Paint(info, paint_offset, root.LineTop(), root.LineBottom());
    }
  }
}

}  // namespace blink
