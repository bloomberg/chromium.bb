// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/layout/LayoutTableBoxComponent.h"

#include "core/layout/LayoutTable.h"
#include "core/paint/ObjectPaintInvalidator.h"
#include "core/style/ComputedStyle.h"

namespace blink {

bool LayoutTableBoxComponent::doCellsHaveDirtyWidth(
    const LayoutObject& tablePart,
    const LayoutTable& table,
    const StyleDifference& diff,
    const ComputedStyle& oldStyle) {
  // ComputedStyle::diffNeedsFullLayoutAndPaintInvalidation sets needsFullLayout
  // when border sizes change: checking diff.needsFullLayout() is an
  // optimization, not required for correctness.
  // TODO(dgrogan): Remove tablePart.needsLayout()? Perhaps it was an old
  // optimization but now it seems that diff.needsFullLayout() implies
  // tablePart.needsLayout().
  return diff.needsFullLayout() && tablePart.needsLayout() &&
         table.collapseBorders() &&
         !oldStyle.border().sizeEquals(tablePart.style()->border());
}

void LayoutTableBoxComponent::MutableForPainting::updatePaintResult(
    PaintResult paintResult,
    const CullRect& paintRect) {
  DCHECK_EQ(m_layoutObject.document().lifecycle().state(),
            DocumentLifecycle::LifecycleState::InPaint);

  // A table row or section may paint large background display item which
  // contains paint operations of the background in each contained cell.
  // The display item can be clipped by the paint rect to avoid painting
  // on areas not interested. If we didn't fully paint and paint rect changes,
  // we need to invalidate the display item (using setDisplayItemUncached()
  // because we are already in painting.)
  auto& box = static_cast<LayoutTableBoxComponent&>(m_layoutObject);
  if (box.m_lastPaintResult != FullyPainted && box.m_lastPaintRect != paintRect)
    m_layoutObject.setDisplayItemsUncached();

  box.m_lastPaintResult = paintResult;
  box.m_lastPaintRect = paintRect;
}

}  // namespace blink
