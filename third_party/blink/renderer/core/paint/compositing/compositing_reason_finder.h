// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_

#include "base/macros.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/graphics/compositing_reasons.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"

namespace blink {

class PaintLayer;
class LayoutObject;
class ComputedStyle;
class LayoutView;

class CORE_EXPORT CompositingReasonFinder {
  DISALLOW_NEW();

 public:
  static CompositingReasons PotentialCompositingReasonsFromStyle(
      const LayoutObject&);

  static CompositingReasons NonStyleDeterminedDirectReasons(const PaintLayer&);

  DISALLOW_COPY_AND_ASSIGN(CompositingReasonFinder);

  // Returns the direct reasons for compositing the given layer.
  static CompositingReasons DirectReasons(const PaintLayer&);

  static bool RequiresCompositingForScrollableFrame(const LayoutView&);
  static CompositingReasons CompositingReasonsForAnimation(
      const ComputedStyle&);
  static bool RequiresCompositingForOpacityAnimation(const ComputedStyle&);
  static bool RequiresCompositingForFilterAnimation(const ComputedStyle&);
  static bool RequiresCompositingForBackdropFilterAnimation(
      const ComputedStyle&);
  static bool RequiresCompositingForTransformAnimation(const ComputedStyle&);
  static bool RequiresCompositingForTransform(const LayoutObject&);
  static bool RequiresCompositingForRootScroller(const PaintLayer&);

  static bool RequiresCompositingForScrollDependentPosition(const PaintLayer&);
};

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_PAINT_COMPOSITING_COMPOSITING_REASON_FINDER_H_
