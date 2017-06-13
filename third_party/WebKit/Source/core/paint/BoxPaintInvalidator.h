// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPaintInvalidator_h
#define BoxPaintInvalidator_h

#include "core/CoreExport.h"
#include "platform/graphics/PaintInvalidationReason.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class LayoutBox;
class LayoutRect;
struct PaintInvalidatorContext;

class CORE_EXPORT BoxPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BoxPaintInvalidator(const LayoutBox& box,
                      const PaintInvalidatorContext& context)
      : box_(box), context_(context) {}

  static void BoxWillBeDestroyed(const LayoutBox&);

  PaintInvalidationReason InvalidatePaint();

 private:
  friend class BoxPaintInvalidatorTest;

  bool BackgroundGeometryDependsOnLayoutOverflowRect();
  bool BackgroundPaintsOntoScrollingContentsLayer();
  bool ShouldFullyInvalidateBackgroundOnLayoutOverflowChange(
      const LayoutRect& old_layout_overflow,
      const LayoutRect& new_layout_overflow);
  void InvalidateScrollingContentsBackgroundIfNeeded();

  PaintInvalidationReason ComputePaintInvalidationReason();

  void IncrementallyInvalidatePaint(PaintInvalidationReason,
                                    const LayoutRect& old_rect,
                                    const LayoutRect& new_rect);

  bool NeedsToSavePreviousContentBoxSizeOrLayoutOverflowRect();
  void SavePreviousBoxGeometriesIfNeeded();

  const LayoutBox& box_;
  const PaintInvalidatorContext& context_;
};

}  // namespace blink

#endif
