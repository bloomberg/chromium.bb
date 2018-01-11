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
  PrePaintTreeWalk() = default;
  void Walk(LocalFrameView& root_frame);

 private:
  void Walk(LocalFrameView&, const PrePaintTreeWalkContext&);

  // This is to minimize stack frame usage during recursion. Modern compilers
  // (MSVC in particular) can inline across compilation units, resulting in
  // very big stack frames. Splitting the heavy lifting to a separate function
  // makes sure the stack frame is freed prior to making a recursive call.
  // See https://crbug.com/781301 .
  NOINLINE void WalkInternal(const LayoutObject&, PrePaintTreeWalkContext&);
  void Walk(const LayoutObject&, const PrePaintTreeWalkContext&);

  // Invalidates paint-layer painting optimizations, such as subsequence caching
  // and empty paint phase optimizations if clips from the context have changed.
  void InvalidatePaintLayerOptimizationsIfNeeded(const LayoutObject&,
                                                 PrePaintTreeWalkContext&);

  bool NeedsTreeBuilderContextUpdate(const LocalFrameView&,
                                     const PrePaintTreeWalkContext&);
  bool NeedsTreeBuilderContextUpdate(const LayoutObject&,
                                     const PrePaintTreeWalkContext&);

  PaintInvalidator paint_invalidator_;

  FRIEND_TEST_ALL_PREFIXES(PrePaintTreeWalkTest, ClipRects);
};

}  // namespace blink

#endif  // PrePaintTreeWalk_h
