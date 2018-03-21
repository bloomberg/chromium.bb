// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasonFinder_h
#define CompositingReasonFinder_h

#include "base/macros.h"
#include "core/CoreExport.h"
#include "core/paint/compositing/CompositingTriggers.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/wtf/Allocator.h"

namespace blink {

class PaintLayer;
class LayoutObject;
class ComputedStyle;
class LayoutView;

class CORE_EXPORT CompositingReasonFinder {
  DISALLOW_NEW();

 public:
  explicit CompositingReasonFinder(LayoutView&);

  CompositingReasons PotentialCompositingReasonsFromStyle(LayoutObject&) const;

  // Returns the direct reasons for compositing the given layer. If
  // |ignoreLCDText| is true promotion will not try to preserve subpixel text
  // rendering (i.e. partially transparent layers will be promoted).
  CompositingReasons DirectReasons(const PaintLayer*,
                                   bool ignore_lcd_text) const;

  void UpdateTriggers();

  bool RequiresCompositingForScrollableFrame() const;
  static CompositingReasons CompositingReasonsForAnimation(
      const ComputedStyle&);
  static bool RequiresCompositingForOpacityAnimation(const ComputedStyle&);
  static bool RequiresCompositingForFilterAnimation(const ComputedStyle&);
  static bool RequiresCompositingForBackdropFilterAnimation(
      const ComputedStyle&);
  static bool RequiresCompositingForTransformAnimation(const ComputedStyle&);
  static bool RequiresCompositingForTransform(const LayoutObject&);
  static bool RequiresCompositingForRootScroller(const PaintLayer&);

  bool RequiresCompositingForScrollDependentPosition(
      const PaintLayer*,
      bool ignore_lcd_text) const;

 private:
  bool IsMainFrame() const;

  CompositingReasons NonStyleDeterminedDirectReasons(
      const PaintLayer*,
      bool ignore_lcd_text) const;
  LayoutView& layout_view_;
  CompositingTriggerFlags compositing_triggers_;
  DISALLOW_COPY_AND_ASSIGN(CompositingReasonFinder);
};

}  // namespace blink

#endif  // CompositingReasonFinder_h
