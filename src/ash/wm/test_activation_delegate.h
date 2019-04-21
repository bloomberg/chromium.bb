// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_TEST_ACTIVATION_DELEGATE_H_
#define ASH_WM_TEST_ACTIVATION_DELEGATE_H_

#include "base/compiler_specific.h"
#include "base/logging.h"
#include "base/macros.h"
#include "ui/events/event_handler.h"
#include "ui/wm/public/activation_change_observer.h"
#include "ui/wm/public/activation_delegate.h"

namespace aura {
class Window;
}

namespace ash {

// A test ActivationDelegate that can be used to track activation changes for
// an aura::Window.
class TestActivationDelegate : public ::wm::ActivationDelegate,
                               public ::wm::ActivationChangeObserver {
 public:
  TestActivationDelegate();
  explicit TestActivationDelegate(bool activate);

  // Associates this delegate with a Window.
  void SetWindow(aura::Window* window);

  bool window_was_active() const { return window_was_active_; }
  void set_activate(bool v) { activate_ = v; }
  int activated_count() const { return activated_count_; }
  int lost_active_count() const { return lost_active_count_; }
  int should_activate_count() const { return should_activate_count_; }
  void Clear() {
    activated_count_ = lost_active_count_ = should_activate_count_ = 0;
    window_was_active_ = false;
  }

  // Overridden from wm::ActivationDelegate:
  bool ShouldActivate() const override;

 private:
  // Overridden from wm:ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gained_active,
      aura::Window* lost_active) override;

  aura::Window* window_;
  bool window_was_active_;
  bool activate_;
  int activated_count_;
  int lost_active_count_;
  mutable int should_activate_count_;

  DISALLOW_COPY_AND_ASSIGN(TestActivationDelegate);
};

}  // namespace ash

#endif  // ASH_WM_TEST_ACTIVATION_DELEGATE_H_
