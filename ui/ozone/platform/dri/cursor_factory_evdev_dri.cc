// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/cursor_factory_evdev_dri.h"

#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/ozone/dri/dri_surface_factory.h"

namespace ui {

CursorFactoryEvdevDri::CursorFactoryEvdevDri(gfx::DriSurfaceFactory* dri)
    : dri_(dri) {
  cursor_window_ = dri_->GetAcceleratedWidget();
  cursor_bounds_ = gfx::RectF(0, 0, 2560, 1700);  // TODO(spang): Argh!
  cursor_location_ =
      gfx::PointF(cursor_bounds_.width() / 2, cursor_bounds_.height() / 2);
}

CursorFactoryEvdevDri::~CursorFactoryEvdevDri() {}

void CursorFactoryEvdevDri::SetBitmapCursor(
    gfx::AcceleratedWidget widget,
    scoped_refptr<BitmapCursorOzone> cursor) {
  if (cursor_ == cursor)
    return;
  cursor_ = cursor;
  if (cursor_)
    dri_->SetHardwareCursor(
        cursor_window_, cursor_->bitmap(), bitmap_location());
  else
    dri_->UnsetHardwareCursor(cursor_window_);
}

void CursorFactoryEvdevDri::MoveCursorTo(gfx::AcceleratedWidget widget,
                                         const gfx::PointF& location) {
  cursor_window_ = widget;
  cursor_location_ = location;
  cursor_location_.SetToMax(
      gfx::PointF(cursor_bounds_.x(), cursor_bounds_.y()));
  cursor_location_.SetToMin(
      gfx::PointF(cursor_bounds_.right(), cursor_bounds_.bottom()));
  if (cursor_)
    dri_->MoveHardwareCursor(cursor_window_, bitmap_location());
}

void CursorFactoryEvdevDri::MoveCursor(const gfx::Vector2dF& delta) {
  MoveCursorTo(cursor_window_, cursor_location_ + delta);
}

gfx::AcceleratedWidget CursorFactoryEvdevDri::window() {
  return cursor_window_;
}

gfx::PointF CursorFactoryEvdevDri::location() { return cursor_location_; }

gfx::Point CursorFactoryEvdevDri::bitmap_location() {
  return gfx::ToFlooredPoint(cursor_location_) -
         cursor_->hotspot().OffsetFromOrigin();
}

}  // namespace ui
