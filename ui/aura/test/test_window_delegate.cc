// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/test/test_window_delegate.h"

#include "ui/base/hit_test.h"

namespace aura {
namespace test {

TestWindowDelegate::TestWindowDelegate() {
}

TestWindowDelegate::~TestWindowDelegate() {
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

}  // namespace test
}  // namespace aura
