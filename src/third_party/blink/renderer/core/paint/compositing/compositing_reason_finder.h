// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_

#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/wtf/allocator/allocator.h"

namespace blink {

class PaintLayer;
class LayoutObject;
class ComputedStyle;

class CORE_EXPORT CompositingReasonFinder {
  DISALLOW_NEW();

 public:
  CompositingReasonFinder(const CompositingReasonFinder&) = delete;
  CompositingReasonFinder& operator=(const CompositingReasonFinder&) = delete;

  // Composited scrolling reason is not included because
  // PaintLayerScrollableArea needs the result of this function to determine
  // composited scrolling status.
  static CompositingReasons DirectReasonsForPaintPropertiesExceptScrolling(
      const LayoutObject&);

  static bool ShouldForcePreferCompositingToLCDText(
      const LayoutObject&,
      CompositingReasons reasons_except_scrolling);

  // This must be called after
  // |DirectReasonsForPaintPropertiesExceptForScrolling()| and
  // |PaintLayerScrollableArea::UpdateNeedsCompositedScrolling()|.
  static CompositingReasons DirectReasonsForPaintProperties(
      const LayoutObject&,
      CompositingReasons reasons_except_scrolling);

  static CompositingReasons CompositingReasonsForAnimation(const LayoutObject&);
  // Some LayoutObject types do not support transforms (see:
  // |LayoutObject::HasTransformRelatedProperty|) so this can return reasons
  // that the LayoutObject does not end up using.
  static CompositingReasons PotentialCompositingReasonsFor3DTransform(
      const ComputedStyle&);
  static bool RequiresCompositingForRootScroller(const PaintLayer&);

  static CompositingReasons CompositingReasonsForScrollDependentPosition(
      const PaintLayer&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_
