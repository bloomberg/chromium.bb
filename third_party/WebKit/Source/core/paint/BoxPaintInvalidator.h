// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BoxPaintInvalidator_h
#define BoxPaintInvalidator_h

#include "platform/graphics/PaintInvalidationReason.h"
#include "wtf/Allocator.h"

namespace blink {

class LayoutBox;
class LayoutRect;
struct PaintInvalidatorContext;

class BoxPaintInvalidator {
  STACK_ALLOCATED();

 public:
  BoxPaintInvalidator(const LayoutBox& box,
                      const PaintInvalidatorContext& context)
      : m_box(box), m_context(context) {}

  static void boxWillBeDestroyed(const LayoutBox&);

  PaintInvalidationReason invalidatePaintIfNeeded();

 private:
  bool backgroundGeometryDependsOnLayoutOverflowRect();
  bool backgroundPaintsOntoScrollingContentsLayer();
  bool shouldFullyInvalidateBackgroundOnLayoutOverflowChange(
      const LayoutRect& oldLayoutOverflow,
      const LayoutRect& newLayoutOverflow);
  void invalidateScrollingContentsBackgroundIfNeeded();

  PaintInvalidationReason computePaintInvalidationReason();

  bool incrementallyInvalidatePaint(PaintInvalidationReason,
                                    const LayoutRect& oldRect,
                                    const LayoutRect& newRect);

  bool needsToSavePreviousContentBoxSizeOrLayoutOverflowRect();
  void savePreviousBoxGeometriesIfNeeded();

  const LayoutBox& m_box;
  const PaintInvalidatorContext& m_context;
};

}  // namespace blink

#endif
