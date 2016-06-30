// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/wm/test/testing_cursor_client_observer.h"

namespace wm {

TestingCursorClientObserver::TestingCursorClientObserver()
    : cursor_visibility_(false),
      did_visibility_change_(false),
      cursor_set_(ui::CURSOR_SET_NORMAL),
      did_cursor_set_change_(false) {}

void TestingCursorClientObserver::reset() {
  cursor_visibility_ = did_visibility_change_ = false;
  cursor_set_ = ui::CURSOR_SET_NORMAL;
  did_cursor_set_change_ = false;
}

void TestingCursorClientObserver::OnCursorVisibilityChanged(bool is_visible) {
  cursor_visibility_ = is_visible;
  did_visibility_change_ = true;
}

void TestingCursorClientObserver::OnCursorSetChanged(
    ui::CursorSetType cursor_set) {
  cursor_set_ = cursor_set;
  did_cursor_set_change_ = true;
}

}  // namespace wm
