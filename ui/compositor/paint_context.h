// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_COMPOSITOR_PAINT_CONTEXT_H_
#define UI_COMPOSITOR_PAINT_CONTEXT_H_

#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"

namespace gfx {
class Canvas;
}

namespace ui {
class PaintRecorder;

class PaintContext {
 public:
  // Construct a PaintContext that can only re-paint the area in the
  // |invalidation|.
  PaintContext(gfx::Canvas* canvas, const gfx::Rect& invalidation)
      : canvas_(canvas), invalidation_(invalidation) {
#if DCHECK_IS_ON()
    root_visited_ = nullptr;
#endif
  }

  // Construct a PaintContext that will re-paint everything (no consideration
  // for invalidation).
  explicit PaintContext(gfx::Canvas* canvas)
      : PaintContext(canvas, gfx::Rect()) {}

  ~PaintContext() {}

  // Clone a PaintContext with an additional |offset|.
  PaintContext CloneWithPaintOffset(const gfx::Vector2d& offset) const {
    return PaintContext(*this, offset);
  }

  // Clone a PaintContext that has no consideration for invalidation.
  PaintContext CloneWithoutInvalidation() const {
    return PaintContext(canvas_);
  }

  // TODO(danakj): Remove this once everything is painting to display lists.
  gfx::Canvas* canvas() const { return canvas_; }

  // When true, IsRectInvalidated() can be called, otherwise its result would be
  // invalid.
  bool CanCheckInvalidated() const { return !invalidation_.IsEmpty(); }

  // When true, the |bounds| touches an invalidated area, so should be
  // re-painted. When false, re-painting can be skipped. Bounds should be in
  // the local space with offsets up to the painting root in the PaintContext.
  bool IsRectInvalidated(const gfx::Rect& bounds) const {
    DCHECK(CanCheckInvalidated());
    return invalidation_.Intersects(bounds + offset_);
  }

#if DCHECK_IS_ON()
  void Visited(void* visited) const {
    if (!root_visited_)
      root_visited_ = visited;
  }
  void* RootVisited() const { return root_visited_; }
  const gfx::Vector2d& PaintOffset() const { return offset_; }
#endif

 private:
  // The PaintRecorder needs access to the internal canvas and friends, but we
  // don't want to expose them on this class so that people must go through the
  // recorder to access them.
  friend class PaintRecorder;

  // Clone a PaintContext with an additional |offset|.
  PaintContext(const PaintContext& other, const gfx::Vector2d& offset)
      : canvas_(other.canvas_),
        invalidation_(other.invalidation_),
        offset_(other.offset_ + offset) {
#if DCHECK_IS_ON()
    root_visited_ = other.root_visited_;
#endif
  }

  gfx::Canvas* canvas_;
  // Invalidation in the space of the paint root (ie the space of the layer
  // backing the paint taking place).
  gfx::Rect invalidation_;
  // Offset from the PaintContext to the space of the paint root and the
  // |invalidation_|.
  gfx::Vector2d offset_;

#if DCHECK_IS_ON()
  // Used to verify that the |invalidation_| is only used to compare against
  // rects in the same space.
  mutable void* root_visited_;
#endif
};

}  // namespace ui

#endif  // UI_COMPOSITOR_PAINT_CONTEXT_H_
