// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TablePainter.h"

#include "core/layout/CollapsedBorderValue.h"
#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableSection.h"
#include "core/paint/BoxClipper.h"
#include "core/paint/BoxPainter.h"
#include "core/paint/LayoutObjectDrawingRecorder.h"
#include "core/paint/ObjectPainter.h"
#include "core/paint/PaintInfo.h"
#include "core/paint/TableSectionPainter.h"

namespace blink {

void TablePainter::PaintObject(const PaintInfo& paint_info,
                               const LayoutPoint& paint_offset) {
  PaintPhase paint_phase = paint_info.phase;

  if (ShouldPaintSelfBlockBackground(paint_phase)) {
    PaintBoxDecorationBackground(paint_info, paint_offset);
    if (paint_phase == kPaintPhaseSelfBlockBackgroundOnly)
      return;
  }

  if (paint_phase == kPaintPhaseMask) {
    PaintMask(paint_info, paint_offset);
    return;
  }

  if (paint_phase != kPaintPhaseSelfOutlineOnly) {
    PaintInfo paint_info_for_descendants = paint_info.ForDescendants();

    for (LayoutObject* child = layout_table_.FirstChild(); child;
         child = child->NextSibling()) {
      if (child->IsBox() && !ToLayoutBox(child)->HasSelfPaintingLayer() &&
          (child->IsTableSection() || child->IsTableCaption())) {
        LayoutPoint child_point = layout_table_.FlipForWritingModeForChild(
            ToLayoutBox(child), paint_offset);
        child->Paint(paint_info_for_descendants, child_point);
      }
    }

    if (layout_table_.HasCollapsedBorders() &&
        ShouldPaintDescendantBlockBackgrounds(paint_phase) &&
        layout_table_.Style()->Visibility() == EVisibility::kVisible) {
      for (LayoutTableSection* section = layout_table_.BottomSection(); section;
           section = layout_table_.SectionAbove(section)) {
        LayoutPoint child_point =
            layout_table_.FlipForWritingModeForChild(section, paint_offset);
        TableSectionPainter(*section).PaintCollapsedBorders(
            paint_info_for_descendants, child_point);
      }
    }
  }

  if (ShouldPaintSelfOutline(paint_phase))
    ObjectPainter(layout_table_).PaintOutline(paint_info, paint_offset);
}

void TablePainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const LayoutPoint& paint_offset) {
  if (!layout_table_.HasBoxDecorationBackground() ||
      layout_table_.Style()->Visibility() != EVisibility::kVisible)
    return;

  LayoutRect rect(paint_offset, layout_table_.Size());
  layout_table_.SubtractCaptionRect(rect);
  BoxPainter(layout_table_)
      .PaintBoxDecorationBackgroundWithRect(paint_info, paint_offset, rect);
}

void TablePainter::PaintMask(const PaintInfo& paint_info,
                             const LayoutPoint& paint_offset) {
  if (layout_table_.Style()->Visibility() != EVisibility::kVisible ||
      paint_info.phase != kPaintPhaseMask)
    return;

  if (LayoutObjectDrawingRecorder::UseCachedDrawingIfPossible(
          paint_info.context, layout_table_, paint_info.phase))
    return;

  LayoutRect rect(paint_offset, layout_table_.Size());
  layout_table_.SubtractCaptionRect(rect);

  LayoutObjectDrawingRecorder recorder(paint_info.context, layout_table_,
                                       paint_info.phase, rect);
  BoxPainter(layout_table_).PaintMaskImages(paint_info, rect);
}

}  // namespace blink
