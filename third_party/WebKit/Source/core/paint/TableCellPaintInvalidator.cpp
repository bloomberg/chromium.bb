// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TableCellPaintInvalidator.h"

#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTableSection.h"
#include "core/paint/BlockPaintInvalidator.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintLayer.h"

namespace blink {

PaintInvalidationReason TableCellPaintInvalidator::InvalidatePaint() {
  // The cell's containing row and section paint backgrounds behind the cell.
  // If the cell's geometry changed, invalidate the background display items.
  if (context_.old_location != context_.new_location ||
      cell_.Size() != cell_.PreviousSize()) {
    const auto& row = *cell_.Row();
    if (row.GetPaintInvalidationReason() == PaintInvalidationReason::kNone &&
        row.StyleRef().HasBackground()) {
      if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled())
        context_.parent_context->painting_layer->SetNeedsRepaint();
      else
        ObjectPaintInvalidator(row).SlowSetPaintingLayerNeedsRepaint();
      row.InvalidateDisplayItemClients(PaintInvalidationReason::kGeometry);
    }

    const auto& section = *row.Section();
    if (section.GetPaintInvalidationReason() ==
        PaintInvalidationReason::kNone) {
      bool section_paints_background = section.StyleRef().HasBackground();
      if (!section_paints_background) {
        auto col_and_colgroup = section.Table()->ColElementAtAbsoluteColumn(
            cell_.AbsoluteColumnIndex());
        if ((col_and_colgroup.col &&
             col_and_colgroup.col->StyleRef().HasBackground()) ||
            (col_and_colgroup.colgroup &&
             col_and_colgroup.colgroup->StyleRef().HasBackground()))
          section_paints_background = true;
      }
      if (section_paints_background) {
        if (RuntimeEnabledFeatures::slimmingPaintInvalidationEnabled()) {
          context_.parent_context->parent_context->painting_layer
              ->SetNeedsRepaint();
        } else {
          ObjectPaintInvalidator(section).SlowSetPaintingLayerNeedsRepaint();
        }
        section.InvalidateDisplayItemClients(
            PaintInvalidationReason::kGeometry);
      }
    }
  }

  return BlockPaintInvalidator(cell_).InvalidatePaint(context_);
}

}  // namespace blink
