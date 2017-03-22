// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/BlockFlowPaintInvalidator.h"

#include "core/layout/LayoutBlockFlow.h"
#include "core/layout/LayoutInline.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "core/paint/PaintInvalidator.h"

namespace blink {

void BlockFlowPaintInvalidator::invalidatePaintForOverhangingFloatsInternal(
    InvalidateDescendantMode invalidateDescendants) {
  // Invalidate paint of any overhanging floats (if we know we're the one to
  // paint them).  Otherwise, bail out.
  if (!m_blockFlow.hasOverhangingFloats())
    return;

  for (const auto& floatingObject : m_blockFlow.floatingObjects()->set()) {
    // Only issue paint invalidations for the object if it is overhanging, is
    // not in its own layer, and is our responsibility to paint (m_shouldPaint
    // is set). When paintAllDescendants is true, the latter condition is
    // replaced with being a descendant of us.
    if (m_blockFlow.isOverhangingFloat(*floatingObject) &&
        !floatingObject->layoutObject()->hasSelfPaintingLayer() &&
        (floatingObject->shouldPaint() ||
         (invalidateDescendants == InvalidateDescendants &&
          floatingObject->layoutObject()->isDescendantOf(&m_blockFlow)))) {
      LayoutBox* floatingBox = floatingObject->layoutObject();
      floatingBox->setShouldDoFullPaintInvalidation();
      if (floatingBox->isLayoutBlockFlow())
        BlockFlowPaintInvalidator(*toLayoutBlockFlow(floatingBox))
            .invalidatePaintForOverhangingFloatsInternal(
                DontInvalidateDescendants);
    }
  }
}

void BlockFlowPaintInvalidator::invalidateDisplayItemClients(
    PaintInvalidationReason reason) {
  ObjectPaintInvalidator objectPaintInvalidator(m_blockFlow);
  objectPaintInvalidator.invalidateDisplayItemClient(m_blockFlow, reason);

  // PaintInvalidationRectangle happens when we invalidate the caret.
  // The later conditions don't apply when we invalidate the caret or the
  // selection.
  if (reason == PaintInvalidationRectangle ||
      reason == PaintInvalidationSelection)
    return;

  RootInlineBox* line = m_blockFlow.firstRootBox();
  if (line && line->isFirstLineStyle()) {
    // It's the RootInlineBox that paints the ::first-line background. Note that
    // since it may be expensive to figure out if the first line is affected by
    // any ::first-line selectors at all, we just invalidate it unconditionally
    // which is typically cheaper.
    objectPaintInvalidator.invalidateDisplayItemClient(*line, reason);
  }

  if (m_blockFlow.multiColumnFlowThread()) {
    // Invalidate child LayoutMultiColumnSets which may need to repaint column
    // rules after m_blockFlow's column rule style and/or layout changed.
    for (LayoutObject* child = m_blockFlow.firstChild(); child;
         child = child->nextSibling()) {
      if (child->isLayoutMultiColumnSet() &&
          !child->shouldDoFullPaintInvalidation())
        objectPaintInvalidator.invalidateDisplayItemClient(*child, reason);
    }
  }
}

}  // namespace blink
