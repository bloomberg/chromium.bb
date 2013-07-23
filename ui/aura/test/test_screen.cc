// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_screen.h"

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/root_window_host.h"
#include "ui/aura/window.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"

namespace aura {

// static
TestScreen* TestScreen::Create() {
  // Use (0,0) because the desktop aura tests are executed in
  // native environment where the display's origin is (0,0).
  return new TestScreen(gfx::Rect(0, 0, 800, 600));
}

// static
TestScreen* TestScreen::CreateFullscreen() {
  return new TestScreen(gfx::Rect(RootWindowHost::GetNativeScreenSize()));
}

TestScreen::~TestScreen() {
}

RootWindow* TestScreen::CreateRootWindowForPrimaryDisplay() {
  DCHECK(!root_window_);
  root_window_ = new RootWindow(RootWindow::CreateParams(display_.bounds()));
  root_window_->AddObserver(this);
  root_window_->Init();
  return root_window_;
}

void TestScreen::SetDeviceScaleFactor(float device_scale_factor) {
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  display_.SetScaleAndBounds(device_scale_factor, bounds_in_pixel);
  root_window_->OnHostResized(bounds_in_pixel.size());
}

void TestScreen::SetDisplayRotation(gfx::Display::Rotation rotation) {
  display_.set_rotation(rotation);
  // TODO(oshima|mukai): Update the display_ as well.
  root_window_->SetTransform(GetRotationTransform() * GetUIScaleTransform());
}

void TestScreen::SetUIScale(float ui_scale) {
  ui_scale_ = ui_scale;
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  gfx::Rect new_bounds = gfx::ToNearestRect(
      gfx::ScaleRect(bounds_in_pixel, 1.0f / ui_scale));
  display_.SetScaleAndBounds(display_.device_scale_factor(), new_bounds);
  root_window_->SetTransform(GetRotationTransform() * GetUIScaleTransform());
}

gfx::Transform TestScreen::GetRotationTransform() const {
  gfx::Transform rotate;
  float one_pixel = 1.0f / display_.device_scale_factor();
  switch (display_.rotation()) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      rotate.Translate(display_.bounds().height() - one_pixel, 0);
      rotate.Rotate(90);
      break;
    case gfx::Display::ROTATE_270:
      rotate.Translate(0, display_.bounds().width() - one_pixel);
      rotate.Rotate(270);
      break;
    case gfx::Display::ROTATE_180:
      rotate.Translate(display_.bounds().width() - one_pixel,
                       display_.bounds().height() - one_pixel);
      rotate.Rotate(180);
      break;
  }

  return rotate;
}

gfx::Transform TestScreen::GetUIScaleTransform() const {
  gfx::Transform ui_scale;
  ui_scale.Scale(1.0f / ui_scale_, 1.0f / ui_scale_);
  return ui_scale;
}

bool TestScreen::IsDIPEnabled() {
  return true;
}

void TestScreen::OnWindowBoundsChanged(
    Window* window, const gfx::Rect& old_bounds, const gfx::Rect& new_bounds) {
  DCHECK_EQ(root_window_, window);
  display_.SetSize(new_bounds.size());
}

void TestScreen::OnWindowDestroying(Window* window) {
  if (root_window_ == window)
    root_window_ = NULL;
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return Env::GetInstance()->last_mouse_location();
}

gfx::NativeWindow TestScreen::GetWindowAtCursorScreenPoint() {
  const gfx::Point point = GetCursorScreenPoint();
  return root_window_->GetTopWindowContainingPoint(point);
}

int TestScreen::GetNumDisplays() {
  return 1;
}

gfx::Display TestScreen::GetDisplayNearestWindow(
    gfx::NativeWindow window) const {
  return display_;
}

gfx::Display TestScreen::GetDisplayNearestPoint(const gfx::Point& point) const {
  return display_;
}

gfx::Display TestScreen::GetDisplayMatching(const gfx::Rect& match_rect) const {
  return display_;
}

gfx::Display TestScreen::GetPrimaryDisplay() const {
  return display_;
}

void TestScreen::AddObserver(gfx::DisplayObserver* observer) {
}

void TestScreen::RemoveObserver(gfx::DisplayObserver* observer) {
}

TestScreen::TestScreen(const gfx::Rect& screen_bounds)
    : root_window_(NULL),
      ui_scale_(1.0f) {
  static int64 synthesized_display_id = 2000;
  display_.set_id(synthesized_display_id++);
  display_.SetScaleAndBounds(1.0f, screen_bounds);
}

}  // namespace aura
