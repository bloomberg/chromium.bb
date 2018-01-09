// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/AdjustPaintOffsetScope.h"

#include "core/layout/LayoutTableSection.h"

namespace blink {

bool AdjustPaintOffsetScope::AdjustPaintOffset(const LayoutBox& box) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());

  if (box.HasSelfPaintingLayer())
    return false;

  // TODO(wangxianzhu): Expose fragment so that the client doesn't need to
  // call FragmentToPaint() again when needed.
  const auto* fragment = old_paint_info_.FragmentToPaint(box);
  if (!fragment) {
    // TODO(wangxianzhu): The client should know the case and bail out of
    // painting of itself.
    return false;
  }

  const auto* paint_properties = fragment->PaintProperties();
  if (paint_properties && paint_properties->PaintOffsetTranslation()) {
    DCHECK(fragment->LocalBorderBoxProperties());
    contents_properties_.emplace(
        old_paint_info_.context.GetPaintController(),
        *fragment->LocalBorderBoxProperties(), box,
        DisplayItem::PaintPhaseToDrawingType(old_paint_info_.phase));

    new_paint_info_.emplace(old_paint_info_);
    new_paint_info_->UpdateCullRect(paint_properties->PaintOffsetTranslation()
                                        ->Matrix()
                                        .ToAffineTransform());

    adjusted_paint_offset_ = fragment->PaintOffset();
    return true;
  }

  if (box.IsTableSection() &&
      (!old_paint_info_.IsPrinting() || box.FirstFragment().NextFragment())) {
    const auto& section = ToLayoutTableSection(box);
    if (section.IsRepeatingHeaderGroup() || section.IsRepeatingFooterGroup()) {
      adjusted_paint_offset_ = fragment->PaintOffset();
      return true;
    }
  }

  // TODO(wangxianzhu): Use fragment->PaintOffset() for all cases and eliminate
  // the paint_offset parameter of paint methods.
  return false;
}

}  // namespace blink
