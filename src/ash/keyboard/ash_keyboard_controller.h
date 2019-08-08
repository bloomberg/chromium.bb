// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
#define ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/keyboard/ui/keyboard_controller_observer.h"
#include "ash/public/interfaces/keyboard_controller.mojom.h"
#include "ash/session/session_observer.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"

namespace gfx {
class Rect;
}

namespace keyboard {
class KeyboardController;
class KeyboardUIFactory;
}

namespace ash {

class SessionControllerImpl;
class VirtualKeyboardController;

// Contains and observes a keyboard::KeyboardController instance. Ash specific
// behavior, including implementing the mojo interface, is implemented in this
// class. TODO(stevenjb): Consider re-factoring keyboard::KeyboardController so
// that this can inherit from that class instead.
class ASH_EXPORT AshKeyboardController
    : public mojom::KeyboardController,
      public keyboard::KeyboardControllerObserver,
      public SessionObserver {
 public:
  // |session_controller| is expected to outlive AshKeyboardController.
  explicit AshKeyboardController(SessionControllerImpl* session_controller);
  ~AshKeyboardController() override;

  // Called from RegisterInterfaces to bind this to the Ash service.
  void BindRequest(mojom::KeyboardControllerRequest request);

  // Create or destroy the virtual keyboard. Called from Shell. TODO(stevenjb):
  // Fix dependencies so that the virtual keyboard can be created with the
  // keyboard controller.
  void CreateVirtualKeyboard(
      std::unique_ptr<keyboard::KeyboardUIFactory> keyboard_ui_factory);
  void DestroyVirtualKeyboard();

  // Forwards events to mojo observers.
  void SendOnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds);
  void SendOnLoadKeyboardContentsRequested();
  void SendOnKeyboardUIDestroyed();

  // mojom::KeyboardController:
  void KeyboardContentsLoaded(const gfx::Size& size) override;
  void GetKeyboardConfig(GetKeyboardConfigCallback callback) override;
  void SetKeyboardConfig(
      keyboard::mojom::KeyboardConfigPtr keyboard_config) override;
  void IsKeyboardEnabled(IsKeyboardEnabledCallback callback) override;
  void SetEnableFlag(keyboard::mojom::KeyboardEnableFlag flag) override;
  void ClearEnableFlag(keyboard::mojom::KeyboardEnableFlag flag) override;
  void GetEnableFlags(GetEnableFlagsCallback callback) override;
  void ReloadKeyboardIfNeeded() override;
  void RebuildKeyboardIfEnabled() override;
  void IsKeyboardVisible(IsKeyboardVisibleCallback callback) override;
  void ShowKeyboard() override;
  void HideKeyboard(mojom::HideReason reason) override;
  void SetContainerType(keyboard::mojom::ContainerType container_type,
                        const base::Optional<gfx::Rect>& target_bounds,
                        SetContainerTypeCallback callback) override;
  void SetKeyboardLocked(bool locked) override;
  void SetOccludedBounds(const std::vector<gfx::Rect>& bounds) override;
  void SetHitTestBounds(const std::vector<gfx::Rect>& bounds) override;
  void SetDraggableArea(const gfx::Rect& bounds) override;
  void AddObserver(
      mojom::KeyboardControllerObserverAssociatedPtrInfo observer) override;

  // SessionObserver:
  void OnSessionStateChanged(session_manager::SessionState state) override;

  keyboard::KeyboardController* keyboard_controller() {
    return keyboard_controller_.get();
  }

  VirtualKeyboardController* virtual_keyboard_controller() {
    return virtual_keyboard_controller_.get();
  }

  // Called whenever a root window is closing.
  // If the root window contains the virtual keyboard window, deactivates
  // the keyboard so that its window doesn't get destroyed as well.
  void OnRootWindowClosing(aura::Window* root_window);

 private:
  // keyboard::KeyboardControllerObserver
  void OnKeyboardConfigChanged() override;
  void OnKeyboardVisibilityStateChanged(bool is_visible) override;
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& screen_bounds) override;
  void OnKeyboardWorkspaceOccludedBoundsChanged(
      const gfx::Rect& screen_bounds) override;
  void OnKeyboardEnableFlagsChanged(
      std::set<keyboard::mojom::KeyboardEnableFlag>& keyboard_enable_flags)
      override;
  void OnKeyboardEnabledChanged(bool is_enabled) override;

  SessionControllerImpl* session_controller_;  // unowned
  std::unique_ptr<keyboard::KeyboardController> keyboard_controller_;
  std::unique_ptr<VirtualKeyboardController> virtual_keyboard_controller_;
  mojo::BindingSet<mojom::KeyboardController> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::KeyboardControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(AshKeyboardController);
};

}  // namespace ash

#endif  // ASH_KEYBOARD_ASH_KEYBOARD_CONTROLLER_H_
