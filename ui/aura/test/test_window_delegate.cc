// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_window_delegate.h"

#include "third_party/skia/include/core/SkCanvas.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"
#include "ui/gfx/canvas.h"

namespace aura {
namespace test {

////////////////////////////////////////////////////////////////////////////////
// TestWindowDelegate

TestWindowDelegate::TestWindowDelegate() {
}

TestWindowDelegate::~TestWindowDelegate() {
}

void TestWindowDelegate::OnBoundsChanging(gfx::Rect* new_bounds) {
}

void TestWindowDelegate::OnBoundsChanged(const gfx::Rect& old_bounds,
                                         const gfx::Rect& new_bounds) {
}

void TestWindowDelegate::OnFocus() {
}

void TestWindowDelegate::OnBlur() {
}

bool TestWindowDelegate::OnKeyEvent(KeyEvent* event) {
  return false;
}

gfx::NativeCursor TestWindowDelegate::GetCursor(const gfx::Point& point) {
  return gfx::kNullCursor;
}

int TestWindowDelegate::GetNonClientComponent(const gfx::Point& point) const {
  return HTCLIENT;
}

bool TestWindowDelegate::OnMouseEvent(MouseEvent* event) {
  return false;
}

ui::TouchStatus TestWindowDelegate::OnTouchEvent(TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

bool TestWindowDelegate::ShouldActivate(Event* event) {
  return true;
}

void TestWindowDelegate::OnActivated() {
}

void TestWindowDelegate::OnLostActive() {
}

void TestWindowDelegate::OnCaptureLost() {
}

void TestWindowDelegate::OnPaint(gfx::Canvas* canvas) {
}

void TestWindowDelegate::OnWindowDestroying() {
}

void TestWindowDelegate::OnWindowDestroyed() {
}

void TestWindowDelegate::OnWindowVisibilityChanged(bool visible) {
}


////////////////////////////////////////////////////////////////////////////////
// ColorTestWindowDelegate

ColorTestWindowDelegate::ColorTestWindowDelegate(SkColor color)
    : color_(color),
      last_key_code_(ui::VKEY_UNKNOWN) {
}
ColorTestWindowDelegate::~ColorTestWindowDelegate() {
}

bool ColorTestWindowDelegate::OnKeyEvent(KeyEvent* event) {
  last_key_code_ = event->key_code();
  return true;
}
void ColorTestWindowDelegate::OnWindowDestroyed() {
  delete this;
}
void ColorTestWindowDelegate::OnPaint(gfx::Canvas* canvas) {
  canvas->GetSkCanvas()->drawColor(color_, SkXfermode::kSrc_Mode);
}

////////////////////////////////////////////////////////////////////////////////
// ActivateWindowDelegate

ActivateWindowDelegate::ActivateWindowDelegate()
    : activate_(true),
      activated_count_(0),
      lost_active_count_(0),
      should_activate_count_(0) {
}

ActivateWindowDelegate::ActivateWindowDelegate(bool activate)
    : activate_(activate),
      activated_count_(0),
      lost_active_count_(0),
      should_activate_count_(0) {
}

bool ActivateWindowDelegate::ShouldActivate(Event* event) {
  should_activate_count_++;
  return activate_;
}
void ActivateWindowDelegate::OnActivated() {
  activated_count_++;
}
void ActivateWindowDelegate::OnLostActive() {
  lost_active_count_++;
}

}  // namespace test
}  // namespace aura
