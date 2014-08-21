// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_screen.h"

#include "base/logging.h"
#include "ui/aura/env.h"
#include "ui/aura/window.h"
#include "ui/aura/window_event_dispatcher.h"
#include "ui/aura/window_tree_host.h"
#include "ui/gfx/geometry/size_conversions.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/rect_conversions.h"
#include "ui/gfx/screen.h"

namespace aura {

namespace {

bool IsRotationPortrait(gfx::Display::Rotation rotation) {
  return rotation == gfx::Display::ROTATE_90 ||
         rotation == gfx::Display::ROTATE_270;
}

}  // namespace

// static
TestScreen* TestScreen::Create(const gfx::Size& size) {
  const gfx::Size kDefaultSize(800, 600);
  // Use (0,0) because the desktop aura tests are executed in
  // native environment where the display's origin is (0,0).
  return new TestScreen(gfx::Rect(size.IsEmpty() ? kDefaultSize : size));
}

// static
TestScreen* TestScreen::CreateFullscreen() {
  return new TestScreen(gfx::Rect(WindowTreeHost::GetNativeScreenSize()));
}

TestScreen::~TestScreen() {
}

WindowTreeHost* TestScreen::CreateHostForPrimaryDisplay() {
  DCHECK(!host_);
  host_ = WindowTreeHost::Create(gfx::Rect(display_.GetSizeInPixel()));
  host_->window()->AddObserver(this);
  host_->InitHost();
  return host_;
}

void TestScreen::SetDeviceScaleFactor(float device_scale_factor) {
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  display_.SetScaleAndBounds(device_scale_factor, bounds_in_pixel);
  host_->OnHostResized(bounds_in_pixel.size());
}

void TestScreen::SetDisplayRotation(gfx::Display::Rotation rotation) {
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  gfx::Rect new_bounds(bounds_in_pixel);
  if (IsRotationPortrait(rotation) != IsRotationPortrait(display_.rotation())) {
    new_bounds.set_width(bounds_in_pixel.height());
    new_bounds.set_height(bounds_in_pixel.width());
  }
  display_.set_rotation(rotation);
  display_.SetScaleAndBounds(display_.device_scale_factor(), new_bounds);
  host_->SetRootTransform(GetRotationTransform() * GetUIScaleTransform());
}

void TestScreen::SetUIScale(float ui_scale) {
  ui_scale_ = ui_scale;
  gfx::Rect bounds_in_pixel(display_.GetSizeInPixel());
  gfx::Rect new_bounds = gfx::ToNearestRect(
      gfx::ScaleRect(bounds_in_pixel, 1.0f / ui_scale));
  display_.SetScaleAndBounds(display_.device_scale_factor(), new_bounds);
  host_->SetRootTransform(GetRotationTransform() * GetUIScaleTransform());
}

void TestScreen::SetWorkAreaInsets(const gfx::Insets& insets) {
  display_.UpdateWorkAreaFromInsets(insets);
}

gfx::Transform TestScreen::GetRotationTransform() const {
  gfx::Transform rotate;
  switch (display_.rotation()) {
    case gfx::Display::ROTATE_0:
      break;
    case gfx::Display::ROTATE_90:
      rotate.Translate(display_.bounds().height(), 0);
      rotate.Rotate(90);
      break;
    case gfx::Display::ROTATE_270:
      rotate.Translate(0, display_.bounds().width());
      rotate.Rotate(270);
      break;
    case gfx::Display::ROTATE_180:
      rotate.Translate(display_.bounds().width(),
                       display_.bounds().height());
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
  DCHECK_EQ(host_->window(), window);
  display_.SetSize(gfx::ToFlooredSize(
      gfx::ScaleSize(new_bounds.size(), display_.device_scale_factor())));
}

void TestScreen::OnWindowDestroying(Window* window) {
  if (host_->window() == window)
    host_ = NULL;
}

gfx::Point TestScreen::GetCursorScreenPoint() {
  return Env::GetInstance()->last_mouse_location();
}

gfx::NativeWindow TestScreen::GetWindowUnderCursor() {
  return GetWindowAtScreenPoint(GetCursorScreenPoint());
}

gfx::NativeWindow TestScreen::GetWindowAtScreenPoint(const gfx::Point& point) {
  return host_->window()->GetTopWindowContainingPoint(point);
}

int TestScreen::GetNumDisplays() const {
  return 1;
}

std::vector<gfx::Display> TestScreen::GetAllDisplays() const {
  return std::vector<gfx::Display>(1, display_);
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
    : host_(NULL),
      ui_scale_(1.0f) {
  static int64 synthesized_display_id = 2000;
  display_.set_id(synthesized_display_id++);
  display_.SetScaleAndBounds(1.0f, screen_bounds);
}

}  // namespace aura
