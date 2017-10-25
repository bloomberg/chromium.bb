// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/TablePaintInvalidator.h"

#include "core/layout/LayoutTable.h"
#include "core/layout/LayoutTableCell.h"
#include "core/layout/LayoutTableCol.h"
#include "core/layout/LayoutTableRow.h"
#include "core/layout/LayoutTableSection.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

PaintInvalidationReason TablePaintInvalidator::InvalidatePaint() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(table_, context_).InvalidatePaint();

  // If any col changed background, we need to invalidate all sections because
  // col background paints into section's background display item.
  bool has_col_changed_background = false;
  if (table_.HasColElements()) {
    bool visual_rect_changed =
        context_.old_visual_rect != context_.fragment_data->VisualRect();
    for (LayoutTableCol* col = table_.FirstColumn(); col;
         col = col->NextColumn()) {
      // LayoutTableCol uses the table's localVisualRect(). Should check column
      // for paint invalidation when table's visual rect changed.
      if (visual_rect_changed)
        col->SetMayNeedPaintInvalidation();
      // This ensures that the backgroundChangedSinceLastPaintInvalidation flag
      // is up-to-date.
      col->EnsureIsReadyForPaintInvalidation();
      if (col->BackgroundChangedSinceLastPaintInvalidation()) {
        has_col_changed_background = true;
        break;
      }
    }
  }

  if (has_col_changed_background) {
    for (LayoutObject* child = table_.FirstChild(); child;
         child = child->NextSibling()) {
      if (!child->IsTableSection())
        continue;
      LayoutTableSection* section = ToLayoutTableSection(child);
      section->EnsureIsReadyForPaintInvalidation();
      ObjectPaintInvalidator(*section)
          .SlowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
              *section, PaintInvalidationReason::kStyle);
    }
  }

  return reason;
}

}  // namespace blink
