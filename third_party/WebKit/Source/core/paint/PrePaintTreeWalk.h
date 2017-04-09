// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrePaintTreeWalk_h
#define PrePaintTreeWalk_h

#include "core/paint/ClipRect.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

class FrameView;
class LayoutObject;
class PropertyTreeState;
struct PrePaintTreeWalkContext;

// This class walks the whole layout tree, beginning from the root FrameView,
// across frame boundaries. Helper classes are called for each tree node to
// perform actual actions.  It expects to be invoked in InPrePaint phase.
class PrePaintTreeWalk {
 public:
  PrePaintTreeWalk() {}
  void Walk(FrameView& root_frame);

 private:
  void Walk(FrameView&, const PrePaintTreeWalkContext&);
  void Walk(const LayoutObject&, const PrePaintTreeWalkContext&);

  // Invalidates paint-layer painting optimizations, such as subsequence caching
  // and empty paint phase optimizations if clips from the context have changed.
  ALWAYS_INLINE void InvalidatePaintLayerOptimizationsIfNeeded(
      const LayoutObject&,
      PrePaintTreeWalkContext&);

  // Returns in |clipRect| the clip applied to children for the given
  // contaiing block context + effect, in the space of ancestorState adjusted
  // by ancestorPaintOffset.
  ALWAYS_INLINE void ComputeClipRectForContext(
      const PaintPropertyTreeBuilderContext::ContainingBlockContext&,
      const EffectPaintPropertyNode*,
      const PropertyTreeState& ancestor_state,
      const LayoutPoint& ancestor_paint_offset,
      FloatClipRect&);

  bool ALWAYS_INLINE
  NeedsTreeBuilderContextUpdate(const FrameView&,
                                const PrePaintTreeWalkContext&);
  bool ALWAYS_INLINE
  NeedsTreeBuilderContextUpdate(const LayoutObject&,
                                const PrePaintTreeWalkContext&);

  PaintPropertyTreeBuilder property_tree_builder_;
  PaintInvalidator paint_invalidator_;
};

}  // namespace blink

#endif  // PrePaintTreeWalk_h
