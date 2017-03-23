// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_test_util.h"

#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_controller.h"

namespace {

class WindowVisibilityChangeWaiter : public aura::WindowObserver {
 public:
  explicit WindowVisibilityChangeWaiter(aura::Window* window, bool wait_until)
      : window_(window), wait_until_(wait_until) {
    window_->AddObserver(this);
  }
  ~WindowVisibilityChangeWaiter() override { window_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

 private:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    if (window_ == window && visible == wait_until_) {
      run_loop_.QuitWhenIdle();
    }
  }

  aura::Window* window_;
  base::RunLoop run_loop_;
  bool const wait_until_;

  DISALLOW_COPY_AND_ASSIGN(WindowVisibilityChangeWaiter);
};

bool WaitVisibilityChangesTo(bool visibility) {
  aura::Window* keyboard_window =
      keyboard::KeyboardController::GetInstance()
          ->GetContainerWindowWithoutCreationForTest();
  if (!keyboard_window)
    return false;
  WindowVisibilityChangeWaiter waiter(keyboard_window, visibility);
  waiter.Wait();
  return true;
}

}  // namespace

namespace keyboard {

bool WaitUntilShown() {
  return WaitVisibilityChangesTo(true);
}

bool WaitUntilHidden() {
  return WaitVisibilityChangesTo(false);
}

}  // namespace keyboard
