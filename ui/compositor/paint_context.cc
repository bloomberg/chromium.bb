// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/compositor/paint_context.h"

#include "ui/gfx/canvas.h"

namespace ui {

PaintContext::PaintContext(cc::DisplayItemList* list,
                           float device_scale_factor,
                           const gfx::Rect& invalidation)
    : list_(list),
      device_scale_factor_(device_scale_factor),
      invalidation_(invalidation) {
#if DCHECK_IS_ON()
  root_visited_ = nullptr;
  inside_paint_recorder_ = false;
#endif
}

PaintContext::PaintContext(const PaintContext& other,
                           const gfx::Vector2d& offset)
    : list_(other.list_),
      device_scale_factor_(other.device_scale_factor_),
      invalidation_(other.invalidation_),
      offset_(other.offset_ + offset) {
#if DCHECK_IS_ON()
  root_visited_ = other.root_visited_;
  inside_paint_recorder_ = other.inside_paint_recorder_;
#endif
}

PaintContext::PaintContext(const PaintContext& other,
                           CloneWithoutInvalidation c)
    : list_(other.list_),
      device_scale_factor_(other.device_scale_factor_),
      invalidation_(),
      offset_(other.offset_) {
#if DCHECK_IS_ON()
  root_visited_ = other.root_visited_;
  inside_paint_recorder_ = other.inside_paint_recorder_;
#endif
}

PaintContext::~PaintContext() {
}

gfx::Rect PaintContext::ToLayerSpaceBounds(
    const gfx::Size& size_in_context) const {
  return gfx::Rect(size_in_context) + offset_;
}

gfx::Rect PaintContext::ToLayerSpaceRect(const gfx::Rect& rect) const {
  return rect + offset_;
}

}  // namespace ui
