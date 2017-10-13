// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_test_util.h"

#include "base/run_loop.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"

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

class ControllerStateChangeWaiter
    : public keyboard::KeyboardControllerObserver {
 public:
  explicit ControllerStateChangeWaiter(keyboard::KeyboardControllerState state)
      : controller_(keyboard::KeyboardController::GetInstance()),
        state_(state) {
    controller_->AddObserver(this);
  }
  ~ControllerStateChangeWaiter() override { controller_->RemoveObserver(this); }

  void Wait() { run_loop_.Run(); }

 private:
  void OnKeyboardBoundsChanging(const gfx::Rect& new_bounds) override {}
  void OnKeyboardClosed() override {}
  void OnStateChanged(const keyboard::KeyboardControllerState state) override {
    if (state == state_) {
      run_loop_.QuitWhenIdle();
    }
  }

  base::RunLoop run_loop_;
  keyboard::KeyboardController* controller_;
  keyboard::KeyboardControllerState state_;

  DISALLOW_COPY_AND_ASSIGN(ControllerStateChangeWaiter);
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

void WaitControllerStateChangesTo(KeyboardControllerState state) {
  ControllerStateChangeWaiter waiter(state);
  waiter.Wait();
}

gfx::Rect KeyboardBoundsFromRootBounds(const gfx::Rect& root_bounds,
                                       int keyboard_height) {
  return gfx::Rect(root_bounds.x(), root_bounds.bottom() - keyboard_height,
                   root_bounds.width(), keyboard_height);
}

bool FakeKeyboardUI::HasContentsWindow() const {
  return false;
}

bool FakeKeyboardUI::ShouldWindowOverscroll(aura::Window* window) const {
  return true;
}

aura::Window* FakeKeyboardUI::GetContentsWindow() {
  return nullptr;
}

ui::InputMethod* FakeKeyboardUI::GetInputMethod() {
  return &ime_;
}

}  // namespace keyboard
