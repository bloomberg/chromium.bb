// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller_mojo_impl.h"

#include "ui/gfx/geometry/rect.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_observer.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard {

class KeyboardControllerMojoImpl::ControllerObserver
    : public KeyboardControllerObserver {
 public:
  ControllerObserver(KeyboardControllerMojoImpl* service,
                     ::keyboard::KeyboardController* controller)
      : service_(service), controller_(controller) {
    controller_->AddObserver(this);
  }

  ~ControllerObserver() override { controller_->RemoveObserver(this); }

  // KeyboardControllerObserver:
  void OnKeyboardConfigChanged() override {
    service_->NotifyConfigChanged(
        mojom::KeyboardConfig::New(controller_->keyboard_config()));
  }
  void OnKeyboardVisibilityStateChanged(bool is_visible) override {
    service_->NotifyKeyboardVisibilityChanged(is_visible);
  }
  void OnKeyboardVisibleBoundsChanged(const gfx::Rect& bounds) override {
    service_->NotifyKeyboardVisibleBoundsChanged(bounds);
  }
  void OnKeyboardDisabled() override {
    service_->NotifyKeyboardEnabledChanged(false);
  }

 private:
  KeyboardControllerMojoImpl* service_;
  ::keyboard::KeyboardController* controller_;
};

KeyboardControllerMojoImpl::KeyboardControllerMojoImpl(
    ::keyboard::KeyboardController* controller)
    : controller_(controller),
      controller_observer_(
          std::make_unique<ControllerObserver>(this, controller)) {}

KeyboardControllerMojoImpl::~KeyboardControllerMojoImpl() {}

void KeyboardControllerMojoImpl::BindRequest(
    mojom::KeyboardControllerRequest request) {
  bindings_.AddBinding(this, std::move(request));
}

void KeyboardControllerMojoImpl::AddObserver(
    mojom::KeyboardControllerObserverAssociatedPtrInfo observer) {
  mojom::KeyboardControllerObserverAssociatedPtr observer_ptr;
  observer_ptr.Bind(std::move(observer));
  observers_.AddPtr(std::move(observer_ptr));
}

void KeyboardControllerMojoImpl::GetKeyboardConfig(
    GetKeyboardConfigCallback callback) {
  std::move(callback).Run(
      mojom::KeyboardConfig::New(controller_->keyboard_config()));
}

void KeyboardControllerMojoImpl::SetKeyboardConfig(
    mojom::KeyboardConfigPtr keyboard_config) {
  controller_->UpdateKeyboardConfig(*keyboard_config);
}

void KeyboardControllerMojoImpl::NotifyConfigChanged(
    mojom::KeyboardConfigPtr config) {
  observers_.ForAllPtrs([&config](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardConfigChanged(config.Clone());
  });
}

void KeyboardControllerMojoImpl::NotifyKeyboardVisibilityChanged(
    bool visibile) {
  observers_.ForAllPtrs(
      [visibile](mojom::KeyboardControllerObserver* observer) {
        observer->OnKeyboardVisibilityChanged(visibile);
      });
}

void KeyboardControllerMojoImpl::NotifyKeyboardVisibleBoundsChanged(
    const gfx::Rect& bounds) {
  observers_.ForAllPtrs([&bounds](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardVisibleBoundsChanged(bounds);
  });
}

void KeyboardControllerMojoImpl::NotifyKeyboardEnabledChanged(bool enabled) {
  observers_.ForAllPtrs([enabled](mojom::KeyboardControllerObserver* observer) {
    observer->OnKeyboardEnabledChanged(enabled);
  });
}

}  // namespace keyboard
