// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasonFinder_h
#define CompositingReasonFinder_h

#include "core/CoreExport.h"
#include "core/paint/compositing/CompositingTriggers.h"
#include "platform/graphics/CompositingReasons.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Noncopyable.h"

namespace blink {

class PaintLayer;
class LayoutObject;
class ComputedStyle;
class LayoutView;

class CORE_EXPORT CompositingReasonFinder {
  DISALLOW_NEW();
  WTF_MAKE_NONCOPYABLE(CompositingReasonFinder);

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
  static bool RequiresCompositingForAnimation(const ComputedStyle&);
  static bool RequiresCompositingForOpacityAnimation(const ComputedStyle&);
  static bool RequiresCompositingForFilterAnimation(const ComputedStyle&);
  static bool RequiresCompositingForBackdropFilterAnimation(
      const ComputedStyle&);
  static bool RequiresCompositingForEffectAnimation(const ComputedStyle&);
  static bool RequiresCompositingForTransformAnimation(const ComputedStyle&);
  static bool RequiresCompositingForTransform(const LayoutObject&);

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
};

}  // namespace blink

#endif  // CompositingReasonFinder_h
