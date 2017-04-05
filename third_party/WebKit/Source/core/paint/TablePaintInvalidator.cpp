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

PaintInvalidationReason TablePaintInvalidator::invalidatePaintIfNeeded() {
  PaintInvalidationReason reason =
      BoxPaintInvalidator(m_table, m_context).invalidatePaintIfNeeded();

  // If any col changed background, we need to invalidate all sections because
  // col background paints into section's background display item.
  bool hasColChangedBackground = false;
  if (m_table.hasColElements()) {
    bool visualRectChanged = m_context.oldVisualRect != m_table.visualRect();
    for (LayoutTableCol* col = m_table.firstColumn(); col;
         col = col->nextColumn()) {
      // LayoutTableCol uses the table's localVisualRect(). Should check column
      // for paint invalidation when table's visual rect changed.
      if (visualRectChanged)
        col->setMayNeedPaintInvalidation();
      // This ensures that the backgroundChangedSinceLastPaintInvalidation flag
      // is up-to-date.
      col->ensureIsReadyForPaintInvalidation();
      if (col->backgroundChangedSinceLastPaintInvalidation()) {
        hasColChangedBackground = true;
        break;
      }
    }
  }

  if (hasColChangedBackground) {
    for (LayoutObject* child = m_table.firstChild(); child;
         child = child->nextSibling()) {
      if (!child->isTableSection())
        continue;
      LayoutTableSection* section = toLayoutTableSection(child);
      section->ensureIsReadyForPaintInvalidation();
      ObjectPaintInvalidator(*section)
          .slowSetPaintingLayerNeedsRepaintAndInvalidateDisplayItemClient(
              *section, PaintInvalidationStyleChange);
    }
  }

  return reason;
}

}  // namespace blink
