// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_MOJO_IMPL_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_MOJO_IMPL_H_

#include <memory>

#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/keyboard/keyboard_export.h"
#include "ui/keyboard/public/keyboard_controller.mojom.h"

namespace gfx {
class Rect;
}

namespace keyboard {

class KeyboardController;

class KEYBOARD_EXPORT KeyboardControllerMojoImpl
    : public mojom::KeyboardController {
 public:
  explicit KeyboardControllerMojoImpl(
      ::keyboard::KeyboardController* controller);
  ~KeyboardControllerMojoImpl() override;

  void BindRequest(mojom::KeyboardControllerRequest request);

  // mojom::KeyboardController:
  void AddObserver(
      mojom::KeyboardControllerObserverAssociatedPtrInfo observer) override;
  void GetKeyboardConfig(GetKeyboardConfigCallback callback) override;
  void SetKeyboardConfig(mojom::KeyboardConfigPtr keyboard_config) override;

 private:
  class ControllerObserver;

  void NotifyConfigChanged(mojom::KeyboardConfigPtr config);
  void NotifyKeyboardVisibilityChanged(bool visible);
  void NotifyKeyboardVisibleBoundsChanged(const gfx::Rect& bounds);
  void NotifyKeyboardDisabled();

  ::keyboard::KeyboardController* controller_;
  std::unique_ptr<ControllerObserver> controller_observer_;
  mojo::BindingSet<mojom::KeyboardController> bindings_;
  mojo::AssociatedInterfacePtrSet<mojom::KeyboardControllerObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerMojoImpl);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_MOJO_IMPL_H_
