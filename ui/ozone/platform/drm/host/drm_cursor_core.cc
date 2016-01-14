// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/host/drm_cursor_core.h"

#include "ui/events/ozone/chromeos/cursor_controller.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/ozone/platform/drm/host/drm_window_host.h"
#include "ui/ozone/platform/drm/host/drm_window_host_manager.h"

namespace ui {

DrmCursorCore::DrmCursorCore(DrmCursorProxy* proxy,
                             DrmWindowHostManager* window_manager)
    : window_(gfx::kNullAcceleratedWidget),
      window_manager_(window_manager),
      proxy_(proxy) {}

DrmCursorCore::~DrmCursorCore() {}

gfx::Point DrmCursorCore::GetBitmapLocationLocked() {
  return gfx::ToFlooredPoint(location_) - bitmap_->hotspot().OffsetFromOrigin();
}

void DrmCursorCore::SetCursor(gfx::AcceleratedWidget window,
                              scoped_refptr<BitmapCursorOzone> bitmap) {
  if (window_ != window || bitmap_ == bitmap)
    return;

  bitmap_ = bitmap;

  SendCursorShow();
}

void DrmCursorCore::OnWindowAdded(gfx::AcceleratedWidget window,
                                  const gfx::Rect& bounds_in_screen,
                                  const gfx::Rect& cursor_confined_bounds) {
  if (window_ == gfx::kNullAcceleratedWidget) {
    // First window added & cursor is not placed. Place it.
    window_ = window;
    display_bounds_in_screen_ = bounds_in_screen;
    confined_bounds_ = cursor_confined_bounds;
    SetCursorLocation(gfx::PointF(cursor_confined_bounds.CenterPoint()));
  }
}

void DrmCursorCore::OnWindowRemoved(gfx::AcceleratedWidget window) {
  if (window_ == window) {
    // Try to find a new location for the cursor.
    DrmWindowHost* dest_window = window_manager_->GetPrimaryWindow();

    if (dest_window) {
      window_ = dest_window->GetAcceleratedWidget();
      display_bounds_in_screen_ = dest_window->GetBounds();
      confined_bounds_ = dest_window->GetCursorConfinedBounds();
      SetCursorLocation(gfx::PointF(confined_bounds_.CenterPoint()));
      SendCursorShow();
    } else {
      window_ = gfx::kNullAcceleratedWidget;
      display_bounds_in_screen_ = gfx::Rect();
      confined_bounds_ = gfx::Rect();
      location_ = gfx::PointF();
    }
  }
}

void DrmCursorCore::CommitBoundsChange(
    gfx::AcceleratedWidget window,
    const gfx::Rect& new_display_bounds_in_screen,
    const gfx::Rect& new_confined_bounds) {
  if (window_ == window) {
    display_bounds_in_screen_ = new_display_bounds_in_screen;
    confined_bounds_ = new_confined_bounds;
    SetCursorLocation(location_);
    SendCursorShow();
  }
}

void DrmCursorCore::MoveCursorTo(gfx::AcceleratedWidget window,
                                 const gfx::PointF& location) {
  gfx::AcceleratedWidget old_window = window_;

  if (window != old_window) {
    // When moving between displays, hide the cursor on the old display
    // prior to showing it on the new display.
    if (old_window != gfx::kNullAcceleratedWidget)
      SendCursorHide();

    // TODO(rjk): pass this in?
    DrmWindowHost* drm_window_host = window_manager_->GetWindow(window);
    display_bounds_in_screen_ = drm_window_host->GetBounds();
    confined_bounds_ = drm_window_host->GetCursorConfinedBounds();
    window_ = window;
  }

  SetCursorLocation(location);
  if (window != old_window)
    SendCursorShow();
  else
    SendCursorMove();
}

void DrmCursorCore::MoveCursorTo(const gfx::PointF& screen_location) {
  // TODO(spang): Moving between windows doesn't work here, but
  // is not needed for current uses.
  SetCursorLocation(screen_location -
                    display_bounds_in_screen_.OffsetFromOrigin());

  SendCursorMove();
}

void DrmCursorCore::MoveCursor(const gfx::Vector2dF& delta) {
  if (window_ == gfx::kNullAcceleratedWidget)
    return;

  gfx::Point location;
#if defined(OS_CHROMEOS)
  gfx::Vector2dF transformed_delta = delta;
  ui::CursorController::GetInstance()->ApplyCursorConfigForWindow(
      window_, &transformed_delta);
  SetCursorLocation(location_ + transformed_delta);
#else
  SetCursorLocation(location_ + delta);
#endif
  SendCursorMove();
}

bool DrmCursorCore::IsCursorVisible() {
  return bitmap_;
}

gfx::PointF DrmCursorCore::GetLocation() {
  return location_ + display_bounds_in_screen_.OffsetFromOrigin();
}

gfx::Rect DrmCursorCore::GetCursorConfinedBounds() {
  return confined_bounds_ + display_bounds_in_screen_.OffsetFromOrigin();
}

void DrmCursorCore::SetCursorLocation(const gfx::PointF& location) {
  gfx::PointF clamped_location = location;
  clamped_location.SetToMax(gfx::PointF(confined_bounds_.origin()));
  // Right and bottom edges are exclusive.
  clamped_location.SetToMin(
      gfx::PointF(confined_bounds_.right() - 1, confined_bounds_.bottom() - 1));

  location_ = clamped_location;
}

void DrmCursorCore::SendCursorShow() {
  if (!bitmap_) {
    SendCursorHide();
    return;
  }
  proxy_->CursorSet(window_, bitmap_->bitmaps(), GetBitmapLocationLocked(),
                    bitmap_->frame_delay_ms());
}

void DrmCursorCore::SendCursorHide() {
  proxy_->CursorSet(window_, std::vector<SkBitmap>(), gfx::Point(), 0);
}

void DrmCursorCore::SendCursorMove() {
  if (!bitmap_)
    return;

  proxy_->Move(window_, GetBitmapLocationLocked());
}

}  // namespace ui
