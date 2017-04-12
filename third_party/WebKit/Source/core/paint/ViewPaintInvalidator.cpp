// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/ViewPaintInvalidator.h"

#include "core/layout/LayoutView.h"
#include "core/paint/BoxPaintInvalidator.h"
#include "platform/geometry/LayoutSize.h"

namespace blink {

PaintInvalidationReason ViewPaintInvalidator::InvalidatePaintIfNeeded() {
  InvalidateBackgroundIfNeeded();
  return BoxPaintInvalidator(view_, context_).InvalidatePaintIfNeeded();
}

void ViewPaintInvalidator::InvalidateBackgroundIfNeeded() {
  // Fixed attachment background is handled in LayoutView::layout().
  // TODO(wangxianzhu): Combine code for fixed-attachment background when we
  // enable rootLayerScrolling permanently.
  if (view_.StyleRef().HasEntirelyFixedBackground())
    return;

  // LayoutView's non-fixed-attachment background is positioned in the
  // document element.
  // See https://drafts.csswg.org/css-backgrounds-3/#root-background.
  Element* document_element = view_.GetDocument().documentElement();
  if (!document_element)
    return;
  const LayoutObject* background_object = document_element->GetLayoutObject();
  if (!background_object || !background_object->IsBox())
    return;

  const LayoutBox& background_box = ToLayoutBox(*background_object);
  LayoutSize old_size = background_box.PreviousSize();
  LayoutSize new_size = background_box.Size();
  const auto& background_layers = view_.StyleRef().BackgroundLayers();
  if ((old_size.Width() != new_size.Width() &&
       LayoutBox::MustInvalidateFillLayersPaintOnWidthChange(
           background_layers)) ||
      (old_size.Height() != new_size.Height() &&
       LayoutBox::MustInvalidateFillLayersPaintOnHeightChange(
           background_layers))) {
    view_.GetMutableForPainting().SetShouldDoFullPaintInvalidation(
        kPaintInvalidationViewBackground);
  }
}

}  // namespace blink
