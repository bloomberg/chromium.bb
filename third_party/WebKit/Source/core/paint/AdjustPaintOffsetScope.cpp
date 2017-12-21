// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/paint/AdjustPaintOffsetScope.h"

namespace blink {

bool AdjustPaintOffsetScope::AdjustForPaintOffsetTranslation(
    const LayoutBox& box) {
  DCHECK(RuntimeEnabledFeatures::SlimmingPaintV175Enabled());

  if (box.HasSelfPaintingLayer())
    return false;

  const auto* fragment = old_paint_info_.FragmentToPaint(box);
  if (!fragment)
    return false;
  const auto* paint_properties = fragment->PaintProperties();
  if (!paint_properties || !paint_properties->PaintOffsetTranslation())
    return false;

  DCHECK(fragment->LocalBorderBoxProperties());
  contents_properties_.emplace(old_paint_info_.context.GetPaintController(),
                               *fragment->LocalBorderBoxProperties(), box);

  new_paint_info_.emplace(old_paint_info_);
  new_paint_info_->UpdateCullRect(
      paint_properties->PaintOffsetTranslation()->Matrix().ToAffineTransform());

  adjusted_paint_offset_ = box.FirstFragment().PaintOffset();
  return true;
}

}  // namespace blink
