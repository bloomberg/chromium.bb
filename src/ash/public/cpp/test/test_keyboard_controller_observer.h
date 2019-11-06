// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_
#define ASH_PUBLIC_CPP_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_

#include "ash/public/interfaces/keyboard_config.mojom.h"
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "mojo/public/cpp/bindings/associated_binding.h"

namespace ash {

// mojom::KeyboardControllerObserver implementation for tests. This class
// implements a test client observer for tests running with the Window Service.

class TestKeyboardControllerObserver
    : public mojom::KeyboardControllerObserver {
 public:
  explicit TestKeyboardControllerObserver(
      mojom::KeyboardController* controller);
  ~TestKeyboardControllerObserver() override;

  // mojom::KeyboardControllerObserver
  void OnKeyboardEnableFlagsChanged(
      const std::vector<keyboard::mojom::KeyboardEnableFlag>& flags) override;
  void OnKeyboardEnabledChanged(bool enabled) override;
  void OnKeyboardConfigChanged(
      keyboard::mojom::KeyboardConfigPtr config) override;
  void OnKeyboardVisibilityChanged(bool visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override;
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& bounds) override;
  void OnLoadKeyboardContentsRequested() override;
  void OnKeyboardUIDestroyed() override;

  const keyboard::mojom::KeyboardConfig& config() const { return config_; }
  void set_config(const keyboard::mojom::KeyboardConfig& config) {
    config_ = config;
  }
  const std::vector<keyboard::mojom::KeyboardEnableFlag>& enable_flags() const {
    return enable_flags_;
  }
  int destroyed_count() const { return destroyed_count_; }

 private:
  mojom::KeyboardController* controller_;
  mojom::KeyboardControllerObserverAssociatedPtr ptr_;
  std::vector<keyboard::mojom::KeyboardEnableFlag> enable_flags_;
  keyboard::mojom::KeyboardConfig config_;
  int destroyed_count_ = 0;
  mojo::AssociatedBinding<mojom::KeyboardControllerObserver>
      keyboard_controller_observer_binding_{this};

  DISALLOW_COPY_AND_ASSIGN(TestKeyboardControllerObserver);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_TEST_KEYBOARD_CONTROLLER_OBSERVER_H_
