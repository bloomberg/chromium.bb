// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PrePaintTreeWalk_h
#define PrePaintTreeWalk_h

#include "core/paint/ClipRect.h"
#include "core/paint/PaintInvalidator.h"
#include "core/paint/PaintPropertyTreeBuilder.h"

namespace blink {

class LayoutObject;
class LocalFrameView;
struct PrePaintTreeWalkContext;

// This class walks the whole layout tree, beginning from the root
// LocalFrameView, across frame boundaries. Helper classes are called for each
// tree node to perform actual actions.  It expects to be invoked in InPrePaint
// phase.
class CORE_EXPORT PrePaintTreeWalk {
 public:
  PrePaintTreeWalk() {}
  void Walk(LocalFrameView& root_frame);

 private:
  void Walk(LocalFrameView&, const PrePaintTreeWalkContext&);
  void Walk(const LayoutObject&, const PrePaintTreeWalkContext&);

  // Invalidates paint-layer painting optimizations, such as subsequence caching
  // and empty paint phase optimizations if clips from the context have changed.
  ALWAYS_INLINE void InvalidatePaintLayerOptimizationsIfNeeded(
      const LayoutObject&,
      PrePaintTreeWalkContext&);

  bool ALWAYS_INLINE
  NeedsTreeBuilderContextUpdate(const LocalFrameView&,
                                const PrePaintTreeWalkContext&);
  bool ALWAYS_INLINE
  NeedsTreeBuilderContextUpdate(const LayoutObject&,
                                const PrePaintTreeWalkContext&);

  PaintPropertyTreeBuilder property_tree_builder_;
  PaintInvalidator paint_invalidator_;

  FRIEND_TEST_ALL_PREFIXES(PrePaintTreeWalkTest, ClipRects);
};

}  // namespace blink

#endif  // PrePaintTreeWalk_h
