// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/core/paint/adjust_paint_offset_scope.h"

#include "third_party/blink/renderer/core/layout/layout_block_flow.h"
#include "third_party/blink/renderer/core/layout/layout_table_section.h"

namespace blink {

#if DCHECK_IS_ON()
static void CheckPaintOffset(const LayoutPoint& new_path,
                             const LayoutPoint& old_path,
                             const LayoutBox& box) {
  if (new_path != old_path) {
    LOG(ERROR) << "Paint offset mismatch: new=" << new_path
               << " old=" << old_path << " for " << box.DebugName();
  }
}
#endif

void AdjustPaintOffsetScope::AdjustPaintOffset(
    const LayoutBox& box,
    const LayoutPoint& old_path_paint_offset) {
  fragment_to_paint_ = old_paint_info_.FragmentToPaint(box);
  if (!fragment_to_paint_) {
    // The object doesn't overlap with the current painting fragment, and
    // should not paint itself.
    return;
  }

  if (&box == old_paint_info_.PaintContainer()) {
#if DCHECK_IS_ON()
    CheckPaintOffset(AdjustedPaintOffset(), old_path_paint_offset, box);
#endif
    // PaintLayerPainter already adjusted for PaintOffsetTranslation for
    // PaintContainer. TODO(wangxianzhu): Can we combine the code?
    return;
  }

  const auto* paint_properties = fragment_to_paint_->PaintProperties();
  if (paint_properties && paint_properties->PaintOffsetTranslation()) {
    if (old_paint_info_.context.InDrawingRecorder()) {
      // If we are recording drawings, we should issue the translation as a raw
      // paint operation instead of paint chunk properties. One case is that we
      // are painting table row background behind a cell having paint offset
      // translation.
      old_paint_info_.context.Save();
      FloatSize translation = paint_properties->PaintOffsetTranslation()
                                  ->Matrix()
                                  .To2DTranslation();
      old_paint_info_.context.Translate(translation.Width(),
                                        translation.Height());
      paint_offset_translation_as_drawing_ = true;
    } else {
      contents_properties_.emplace(
          old_paint_info_.context.GetPaintController(),
          paint_properties->PaintOffsetTranslation(), box,
          DisplayItem::PaintPhaseToDrawingType(old_paint_info_.phase));
    }

    new_paint_info_.emplace(old_paint_info_);
    new_paint_info_->UpdateCullRect(paint_properties->PaintOffsetTranslation()
                                        ->Matrix()
                                        .ToAffineTransform());
    return;
  }

  if (box.IsFixedPositionObjectInPagedMedia())
    return;

  if (box.IsTableSection()) {
    const auto& section = ToLayoutTableSection(box);
    if (section.IsRepeatingHeaderGroup() || section.IsRepeatingFooterGroup())
      return;
  }

#if DCHECK_IS_ON()
  CheckPaintOffset(AdjustedPaintOffset(), old_path_paint_offset, box);
#endif
}

void AdjustPaintOffsetScope::FinishPaintOffsetTranslationAsDrawing() {
  // This scope should not interlace with scopes of DrawingRecorders.
  DCHECK(paint_offset_translation_as_drawing_);
  DCHECK(old_paint_info_.context.InDrawingRecorder());
  old_paint_info_.context.Restore();
}

bool AdjustPaintOffsetScope::WillUseLegacyLocation(const LayoutBox* child) {
  if (child->HasSelfPaintingLayer())
    return true;
  if (child->IsLayoutNGMixin()) {
    NGPaintFragment* paint_fragment = ToLayoutBlockFlow(child)->PaintFragment();
    if (!paint_fragment)
      return true;
    if (!paint_fragment->PhysicalFragment().IsPlacedByLayoutNG())
      return true;
    return false;
  }
  return true;
}

}  // namespace blink
