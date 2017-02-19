// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ViewPaintInvalidator.h"

#include "core/layout/LayoutView.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "platform/geometry/LayoutSize.h"

namespace blink {

PaintInvalidationReason ViewPaintInvalidator::invalidatePaintIfNeeded() {
  invalidateBackgroundIfNeeded();
  return BoxPaintInvalidator(m_view, m_context).invalidatePaintIfNeeded();
}

void ViewPaintInvalidator::invalidateBackgroundIfNeeded() {
  // Fixed attachment background is handled in LayoutView::layout().
  // TODO(wangxianzhu): Combine code for fixed-attachment background when we
  // enable rootLayerScrolling permanently.
  if (m_view.styleRef().hasEntirelyFixedBackground())
    return;

  // LayoutView's non-fixed-attachment background is positioned in the
  // document element.
  // See https://drafts.csswg.org/css-backgrounds-3/#root-background.
  Element* documentElement = m_view.document().documentElement();
  if (!documentElement)
    return;
  const LayoutObject* backgroundObject = documentElement->layoutObject();
  if (!backgroundObject || !backgroundObject->isBox())
    return;

  const LayoutBox& backgroundBox = toLayoutBox(*backgroundObject);
  LayoutSize oldSize = backgroundBox.previousSize();
  LayoutSize newSize = backgroundBox.size();
  const auto& backgroundLayers = m_view.styleRef().backgroundLayers();
  if ((oldSize.width() != newSize.width() &&
       LayoutBox::mustInvalidateFillLayersPaintOnWidthChange(
           backgroundLayers)) ||
      (oldSize.height() != newSize.height() &&
       LayoutBox::mustInvalidateFillLayersPaintOnHeightChange(
           backgroundLayers))) {
    m_view.getMutableForPainting().setShouldDoFullPaintInvalidation(
        PaintInvalidationViewBackground);
  }
}

}  // namespace blink
