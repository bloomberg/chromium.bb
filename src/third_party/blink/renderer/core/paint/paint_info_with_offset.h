// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INFO_WITH_OFFSET_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INFO_WITH_OFFSET_H_

#include "third_party/blink/renderer/core/layout/layout_box_model_object.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

// Adjusts cull rect of the input PaintInfo and finds the paint offset for a
// LayoutObject or an NGPaintFragment before painting. Normally a
// Paint(const PaintInfo&) method creates an PaintInfoWithOffset and holds it
// in the stack, and passes it to other PaintXXX() methods that paint different
// parts of the object.
class PaintInfoWithOffset {
  STACK_ALLOCATED();

 public:
  PaintInfoWithOffset(const LayoutObject& object, const PaintInfo& paint_info)
      : fragment_to_paint_(paint_info.FragmentToPaint(object)),
        input_paint_info_(paint_info) {
    if (!fragment_to_paint_) {
      // The object has nothing to paint in the current fragment.
      return;
    }
    if (&object == paint_info.PaintContainer()) {
      // PaintLayerPainter already adjusted for PaintOffsetTranslation for
      // PaintContainer. TODO(wangxianzhu): Can we combine the code?
      return;
    }
    const auto* properties = fragment_to_paint_->PaintProperties();
    if (properties && properties->PaintOffsetTranslation()) {
      AdjustForPaintOffsetTranslation(object,
                                      properties->PaintOffsetTranslation());
    }
  }

  PaintInfoWithOffset(const NGPaintFragment& fragment,
                      const PaintInfo& paint_info)
      : PaintInfoWithOffset(*fragment.GetLayoutObject(), paint_info) {}

  ~PaintInfoWithOffset() {
    if (paint_offset_translation_as_drawing_)
      FinishPaintOffsetTranslationAsDrawing();
  }

  const PaintInfo& GetPaintInfo() const {
    return adjusted_paint_info_ ? *adjusted_paint_info_ : input_paint_info_;
  }

  PaintInfo& MutablePaintInfo() {
    if (!adjusted_paint_info_)
      adjusted_paint_info_.emplace(input_paint_info_);
    return *adjusted_paint_info_;
  }

  LayoutPoint PaintOffset() const {
    // TODO(wangxianzhu): Use DCHECK(fragment_to_paint_) when all painters
    // check FragmentToPaint() before painting.
    return fragment_to_paint_
               ? fragment_to_paint_->PaintOffset()
               : LayoutPoint(LayoutUnit::NearlyMax(), LayoutUnit::NearlyMax());
  }

  const FragmentData* FragmentToPaint() const { return fragment_to_paint_; }

  bool LocalRectIntersectsCullRect(const LayoutRect& local_rect) const {
    LayoutRect rect_in_paint_info_space = local_rect;
    rect_in_paint_info_space.MoveBy(PaintOffset());
    return GetPaintInfo().GetCullRect().IntersectsCullRect(
        rect_in_paint_info_space);
  }

 private:
  void AdjustForPaintOffsetTranslation(
      const LayoutObject&,
      const TransformPaintPropertyNode* paint_offset_translation);

  void FinishPaintOffsetTranslationAsDrawing();

  const FragmentData* fragment_to_paint_;
  const PaintInfo& input_paint_info_;
  base::Optional<PaintInfo> adjusted_paint_info_;
  base::Optional<ScopedPaintChunkProperties> chunk_properties_;
  bool paint_offset_translation_as_drawing_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_PAINT_INFO_WITH_OFFSET_H_
