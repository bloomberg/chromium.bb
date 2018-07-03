// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ADJUST_PAINT_OFFSET_SCOPE_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ADJUST_PAINT_OFFSET_SCOPE_H_

#include "third_party/blink/renderer/core/layout/layout_box.h"
#include "third_party/blink/renderer/core/paint/ng/ng_paint_fragment.h"
#include "third_party/blink/renderer/core/paint/paint_info.h"
#include "third_party/blink/renderer/platform/graphics/paint/scoped_paint_chunk_properties.h"

namespace blink {

class AdjustPaintOffsetScope {
  STACK_ALLOCATED();

 public:
  AdjustPaintOffsetScope(const LayoutBox& box,
                         const PaintInfo& paint_info,
                         const LayoutPoint& paint_offset)
      : old_paint_info_(paint_info) {
    AdjustPaintOffset(box, paint_offset + box.Location());
  }

  AdjustPaintOffsetScope(const NGPaintFragment& fragment,
                         const PaintInfo& paint_info,
                         const LayoutPoint& paint_offset)
      : old_paint_info_(paint_info) {
    DCHECK(RuntimeEnabledFeatures::LayoutNGEnabled());
    DCHECK(fragment.GetLayoutObject());
    const LayoutBox& box = ToLayoutBox(*fragment.GetLayoutObject());
    LayoutPoint old_path_paint_offset = paint_offset;
    if (box.HasSelfPaintingLayer()) {
      // There is no containing block here, we are painting from origin.
      // paint_offset is 0,0
      // box.Location is offset from Layer()
      old_path_paint_offset += box.Location();
    } else {
      old_path_paint_offset += fragment.Offset().ToLayoutPoint();
    }
    AdjustPaintOffset(box, old_path_paint_offset);
  }

  ~AdjustPaintOffsetScope() {
    if (paint_offset_translation_as_drawing_)
      FinishPaintOffsetTranslationAsDrawing();
  }

  const PaintInfo& GetPaintInfo() const {
    return new_paint_info_ ? *new_paint_info_ : old_paint_info_;
  }

  PaintInfo& MutablePaintInfo() {
    if (!new_paint_info_)
      new_paint_info_.emplace(old_paint_info_);
    return *new_paint_info_;
  }

  LayoutPoint AdjustedPaintOffset() const {
    // TODO(wangxianzhu): Use DCHECK(fragment_to_paint_) when removing paint
    // offset parameter from paint methods.
    return fragment_to_paint_
               ? fragment_to_paint_->PaintOffset()
               : LayoutPoint(LayoutUnit::NearlyMax(), LayoutUnit::NearlyMax());
  }

  const FragmentData* FragmentToPaint() const { return fragment_to_paint_; }

  // True if child will use LayoutObject::Location to compute adjusted_offset.
  static bool WillUseLegacyLocation(const LayoutBox* child);

 private:
  void AdjustPaintOffset(const LayoutBox&,
                         const LayoutPoint& old_path_paint_offset);

  void FinishPaintOffsetTranslationAsDrawing();

  const FragmentData* fragment_to_paint_;
  const PaintInfo& old_paint_info_;
  base::Optional<PaintInfo> new_paint_info_;
  base::Optional<ScopedPaintChunkProperties> contents_properties_;
  bool paint_offset_translation_as_drawing_ = false;
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_ADJUST_PAINT_OFFSET_SCOPE_H_
