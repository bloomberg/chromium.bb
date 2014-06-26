// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/cursor_factory_evdev_dri.h"

#include "ui/gfx/geometry/point_conversions.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/hardware_cursor_delegate.h"

namespace ui {

CursorFactoryEvdevDri::CursorFactoryEvdevDri(HardwareCursorDelegate* hardware)
    : hardware_(hardware) {
  // TODO(dnicoara) Assume the first widget since at this point there are no
  // widgets initialized.
  cursor_window_ = DriSurfaceFactory::kDefaultWidgetHandle;
  cursor_location_ = gfx::PointF(2560 / 2, 1700 / 2);  // TODO(spang): Argh!
}

CursorFactoryEvdevDri::~CursorFactoryEvdevDri() {}

void CursorFactoryEvdevDri::SetBitmapCursor(
    gfx::AcceleratedWidget widget,
    scoped_refptr<BitmapCursorOzone> cursor) {
  if (cursor_ == cursor)
    return;

  cursor_ = cursor;
  if (cursor_)
    hardware_->SetHardwareCursor(
        cursor_window_, cursor_->bitmap(), bitmap_location());
  else
    hardware_->SetHardwareCursor(cursor_window_, SkBitmap(), gfx::Point());
}

void CursorFactoryEvdevDri::MoveCursorTo(gfx::AcceleratedWidget widget,
                                         const gfx::PointF& location) {
  if (widget != cursor_window_)
    hardware_->SetHardwareCursor(cursor_window_, SkBitmap(), gfx::Point());

  cursor_window_ = widget;
  cursor_location_ = location;

  gfx::Size size = gfx::Size(2560, 1700);  // TODO(spang): Fix.
  cursor_location_.SetToMax(gfx::PointF(0, 0));
  cursor_location_.SetToMin(gfx::PointF(size.width(), size.height()));

  if (cursor_)
    hardware_->MoveHardwareCursor(cursor_window_, bitmap_location());
}

void CursorFactoryEvdevDri::MoveCursor(const gfx::Vector2dF& delta) {
  MoveCursorTo(cursor_window_, cursor_location_ + delta);
}

gfx::AcceleratedWidget CursorFactoryEvdevDri::GetCursorWindow() {
  return cursor_window_;
}

gfx::PointF CursorFactoryEvdevDri::location() { return cursor_location_; }

gfx::Point CursorFactoryEvdevDri::bitmap_location() {
  return gfx::ToFlooredPoint(cursor_location_) -
         cursor_->hotspot().OffsetFromOrigin();
}

}  // namespace ui
