// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/test/test_keyboard_controller_observer.h"

#include "base/run_loop.h"

namespace ash {

TestKeyboardControllerObserver::TestKeyboardControllerObserver(
    mojom::KeyboardController* controller)
    : controller_(controller) {
  keyboard_controller_observer_binding_.Bind(
      mojo::MakeRequestAssociatedWithDedicatedPipe(&ptr_));
  controller_->AddObserver(ptr_.PassInterface());
}

TestKeyboardControllerObserver::~TestKeyboardControllerObserver() = default;

void TestKeyboardControllerObserver::OnKeyboardEnableFlagsChanged(
    const std::vector<keyboard::mojom::KeyboardEnableFlag>& flags) {
  enable_flags_ = flags;
}

void TestKeyboardControllerObserver::OnKeyboardEnabledChanged(bool enabled) {
  if (!enabled)
    ++destroyed_count_;
}

void TestKeyboardControllerObserver::OnKeyboardConfigChanged(
    keyboard::mojom::KeyboardConfigPtr config) {
  config_ = *config;
}

void TestKeyboardControllerObserver::OnKeyboardVisibilityChanged(bool visible) {
}

void TestKeyboardControllerObserver::OnKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {}

void TestKeyboardControllerObserver::OnKeyboardOccludedBoundsChanged(
    const gfx::Rect& bounds) {}

}  // namespace ash
