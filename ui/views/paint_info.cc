// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/paint_info.h"

namespace views {

// static
PaintInfo PaintInfo::CreateRootPaintInfo(const ui::PaintContext& root_context,
                                         const gfx::Size& size) {
  return PaintInfo(root_context, size);
}

//  static
PaintInfo PaintInfo::CreateChildPaintInfo(const PaintInfo& parent_paint_info,
                                          const gfx::Rect& bounds,
                                          const gfx::Size& parent_size,
                                          ScaleType scale_type) {
  return PaintInfo(parent_paint_info, bounds, parent_size, scale_type);
}

PaintInfo::~PaintInfo() {}

bool PaintInfo::IsPixelCanvas() const {
  return context().is_pixel_canvas();
}

PaintInfo::PaintInfo(const PaintInfo& other)
    : paint_recording_scale_x_(other.paint_recording_scale_x_),
      paint_recording_scale_y_(other.paint_recording_scale_y_),
      paint_recording_bounds_(other.paint_recording_bounds_),
      offset_from_parent_(other.offset_from_parent_),
      context_(other.context(), gfx::Vector2d()),
      root_context_(nullptr) {}

PaintInfo::PaintInfo(const ui::PaintContext& root_context,
                     const gfx::Size& size)
    : paint_recording_scale_x_(root_context.is_pixel_canvas()
                                   ? root_context.device_scale_factor()
                                   : 1.f),
      paint_recording_scale_y_(paint_recording_scale_x_),
      paint_recording_bounds_(
          gfx::ScaleToRoundedRect(gfx::Rect(size), paint_recording_scale_x_)),
      context_(root_context, gfx::Vector2d()),
      root_context_(&root_context) {}

PaintInfo::PaintInfo(const PaintInfo& parent_paint_info,
                     const gfx::Rect& bounds,
                     const gfx::Size& parent_size,
                     ScaleType scale_type)
    : paint_recording_scale_x_(1.f),
      paint_recording_scale_y_(1.f),
      paint_recording_bounds_(
          parent_paint_info.GetSnappedRecordingBounds(parent_size, bounds)),
      offset_from_parent_(
          paint_recording_bounds_.OffsetFromOrigin() -
          parent_paint_info.paint_recording_bounds_.OffsetFromOrigin()),
      context_(parent_paint_info.context(), offset_from_parent_),
      root_context_(nullptr) {
  if (IsPixelCanvas()) {
    if (scale_type == ScaleType::kUniformScaling) {
      paint_recording_scale_x_ = paint_recording_scale_y_ =
          context().device_scale_factor();
    } else if (scale_type == ScaleType::kScaleWithEdgeSnapping) {
      if (bounds.size().width() > 0) {
        paint_recording_scale_x_ =
            static_cast<float>(paint_recording_bounds_.width()) /
            static_cast<float>(bounds.size().width());
      }
      if (bounds.size().height() > 0) {
        paint_recording_scale_y_ =
            static_cast<float>(paint_recording_bounds_.height()) /
            static_cast<float>(bounds.size().height());
      }
    }
  }
}

gfx::Rect PaintInfo::GetSnappedRecordingBounds(
    const gfx::Size& parent_size,
    const gfx::Rect& child_bounds) const {
  if (!IsPixelCanvas())
    return (child_bounds + paint_recording_bounds_.OffsetFromOrigin());

  const gfx::Vector2d& child_origin = child_bounds.OffsetFromOrigin();

  int right = child_origin.x() + child_bounds.width();
  int bottom = child_origin.y() + child_bounds.height();

  int new_x = std::round(child_origin.x() * context().device_scale_factor());
  int new_y = std::round(child_origin.y() * context().device_scale_factor());

  int new_right;
  int new_bottom;

  if (right == parent_size.width())
    new_right = paint_recording_bounds_.width();
  else
    new_right = std::round(right * context().device_scale_factor());

  if (bottom == parent_size.height())
    new_bottom = paint_recording_bounds_.height();
  else
    new_bottom = std::round(bottom * context().device_scale_factor());

  return gfx::Rect(new_x + paint_recording_bounds_.x(),
                   new_y + paint_recording_bounds_.y(), new_right - new_x,
                   new_bottom - new_y);
}

}  // namespace views
