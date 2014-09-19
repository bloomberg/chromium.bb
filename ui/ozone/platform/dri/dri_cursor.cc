// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/dri/dri_cursor.h"

#include "ui/base/cursor/ozone/bitmap_cursor_factory_ozone.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/ozone/platform/dri/dri_surface_factory.h"
#include "ui/ozone/platform/dri/dri_window.h"
#include "ui/ozone/platform/dri/dri_window_manager.h"
#include "ui/ozone/platform/dri/hardware_cursor_delegate.h"

namespace ui {

DriCursor::DriCursor(HardwareCursorDelegate* hardware,
                     DriWindowManager* window_manager)
    : hardware_(hardware),
      window_manager_(window_manager),
      cursor_window_(gfx::kNullAcceleratedWidget) {
}

DriCursor::~DriCursor() {
}

void DriCursor::SetCursor(gfx::AcceleratedWidget widget,
                          PlatformCursor platform_cursor) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  scoped_refptr<BitmapCursorOzone> cursor =
      BitmapCursorFactoryOzone::GetBitmapCursor(platform_cursor);
  if (cursor_ == cursor || cursor_window_ != widget)
    return;

  cursor_ = cursor;
  ShowCursor();
}

void DriCursor::ShowCursor() {
  DCHECK_NE(cursor_window_, gfx::kNullAcceleratedWidget);
  if (cursor_.get())
    hardware_->SetHardwareCursor(cursor_window_,
                                 cursor_->bitmaps(),
                                 bitmap_location(),
                                 cursor_->frame_delay_ms());
  else
    HideCursor();
}

void DriCursor::HideCursor() {
  DCHECK_NE(cursor_window_, gfx::kNullAcceleratedWidget);
  hardware_->SetHardwareCursor(
      cursor_window_, std::vector<SkBitmap>(), gfx::Point(), 0);
}

void DriCursor::MoveCursorTo(gfx::AcceleratedWidget widget,
                             const gfx::PointF& location) {
  if (widget != cursor_window_ && cursor_window_ != gfx::kNullAcceleratedWidget)
    HideCursor();

  cursor_window_ = widget;
  cursor_location_ = location;

  if (cursor_window_ == gfx::kNullAcceleratedWidget)
    return;

  DriWindow* window = window_manager_->GetWindow(cursor_window_);
  const gfx::Size& size = window->GetBounds().size();
  cursor_location_.SetToMax(gfx::PointF(0, 0));
  // Right and bottom edges are exclusive.
  cursor_location_.SetToMin(gfx::PointF(size.width() - 1, size.height() - 1));

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
