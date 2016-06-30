// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/client/cursor_client_observer.h"
#include "ui/wm/core/cursor_manager.h"

namespace wm {

// CursorClientObserver for testing.
class TestingCursorClientObserver : public aura::client::CursorClientObserver {
 public:
  TestingCursorClientObserver();
  void reset();

  bool is_cursor_visible() const { return cursor_visibility_; }
  bool did_visibility_change() const { return did_visibility_change_; }
  ui::CursorSetType cursor_set() const { return cursor_set_; }
  bool did_cursor_set_change() const { return did_cursor_set_change_; }

  // Overridden from aura::client::CursorClientObserver:
  void OnCursorVisibilityChanged(bool is_visible) override;
  void OnCursorSetChanged(ui::CursorSetType cursor_set) override;

 private:
  bool cursor_visibility_;
  bool did_visibility_change_;
  ui::CursorSetType cursor_set_;
  bool did_cursor_set_change_;

  DISALLOW_COPY_AND_ASSIGN(TestingCursorClientObserver);
};

}  // namespace wm
