// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_cursor.h"

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/hardware_cursor_delegate.h"

namespace ui {

DriCursor::DriCursor(HardwareCursorDelegate* hardware) : hardware_(hardware) {
  // TODO(dnicoara) Assume the first widget since at this point there are no
  // widgets initialized.
  cursor_window_ = DriSurfaceFactory::kDefaultWidgetHandle;
  cursor_location_ = gfx::PointF(2560 / 2, 1700 / 2);  // TODO(spang): Argh!
}

DriCursor::~DriCursor() {
}

void DriCursor::SetCursor(gfx::AcceleratedWidget widget,
                          PlatformCursor platform_cursor) {
  scoped_refptr<BitmapCursorOzone> cursor =
      BitmapCursorFactoryOzone::GetBitmapCursor(platform_cursor);
  if (cursor_ == cursor || cursor_window_ != widget)
    return;

  cursor_ = cursor;
  ShowCursor();
}

void DriCursor::ShowCursor() {
   if (cursor_.get())
    hardware_->SetHardwareCursor(cursor_window_,
                                 cursor_->bitmaps(),
                                 bitmap_location(),
                                 cursor_->frame_delay_ms());
  else
    HideCursor();
}

void DriCursor::HideCursor() {
  hardware_->SetHardwareCursor(
      cursor_window_, std::vector<SkBitmap>(), gfx::Point(), 0);
}

void DriCursor::MoveCursorTo(gfx::AcceleratedWidget widget,
                             const gfx::PointF& location) {
  if (widget != cursor_window_)
    HideCursor();

  cursor_window_ = widget;
  cursor_location_ = location;

  gfx::Size size = gfx::Size(2560, 1700);  // TODO(spang): Fix.
  cursor_location_.SetToMax(gfx::PointF(0, 0));
  cursor_location_.SetToMin(gfx::PointF(size.width(), size.height()));

  if (cursor_.get())
    hardware_->MoveHardwareCursor(cursor_window_, bitmap_location());
}

void DriCursor::MoveCursor(const gfx::Vector2dF& delta) {
  MoveCursorTo(cursor_window_, cursor_location_ + delta);
}

gfx::AcceleratedWidget DriCursor::GetCursorWindow() {
  return cursor_window_;
}

bool DriCursor::IsCursorVisible() {
  return cursor_.get();
}

gfx::PointF DriCursor::location() {
  return cursor_location_;
}

gfx::Point DriCursor::bitmap_location() {
  return gfx::ToFlooredPoint(cursor_location_) -
         cursor_->hotspot().OffsetFromOrigin();
}

}  // namespace ui
