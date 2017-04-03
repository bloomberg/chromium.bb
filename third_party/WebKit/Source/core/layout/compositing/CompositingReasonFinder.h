// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CompositingReasonFinder_h
#define CompositingReasonFinder_h

#include "core/CoreExport.h"
#include "core/layout/compositing/CompositingTriggers.h"
#include "platform/graphics/CompositingReasons.h"
#include "wtf/Allocator.h"
#include "wtf/Noncopyable.h"

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

  CompositingReasons potentialCompositingReasonsFromStyle(LayoutObject&) const;

  // Returns the direct reasons for compositing the given layer. If
  // |ignoreLCDText| is true promotion will not try to preserve subpixel text
  // rendering (i.e. partially transparent layers will be promoted).
  CompositingReasons directReasons(const PaintLayer*, bool ignoreLCDText) const;

  void updateTriggers();

  bool requiresCompositingForScrollableFrame() const;
  static bool requiresCompositingForAnimation(const ComputedStyle&);
  static bool requiresCompositingForOpacityAnimation(const ComputedStyle&);
  static bool requiresCompositingForFilterAnimation(const ComputedStyle&);
  static bool requiresCompositingForBackdropFilterAnimation(
      const ComputedStyle&);
  static bool requiresCompositingForEffectAnimation(const ComputedStyle&);
  static bool requiresCompositingForTransformAnimation(const ComputedStyle&);
  static bool requiresCompositingForTransform(const LayoutObject&);

 private:
  bool isMainFrame() const;

  CompositingReasons nonStyleDeterminedDirectReasons(const PaintLayer*,
                                                     bool ignoreLCDText) const;
  bool requiresCompositingForScrollDependentPosition(const PaintLayer*,
                                                     bool ignoreLCDText) const;

  LayoutView& m_layoutView;
  CompositingTriggerFlags m_compositingTriggers;
};

}  // namespace blink

#endif  // CompositingReasonFinder_h
