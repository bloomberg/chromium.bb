// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/fuchsia/input_method_keyboard_controller_fuchsia.h"

#include <utility>

#include "base/fuchsia/component_context.h"
#include "base/fuchsia/fuchsia_logging.h"
#include "base/logging.h"

namespace ui {

InputMethodKeyboardControllerFuchsia::InputMethodKeyboardControllerFuchsia(
    fuchsia::ui::input::ImeService* ime_service)
    : ime_service_(ime_service),
      ime_visibility_(
          base::fuchsia::ComponentContext::GetDefault()
              ->ConnectToService<fuchsia::ui::input::ImeVisibilityService>()) {
  DCHECK(ime_service_);

  ime_visibility_.set_error_handler([this](zx_status_t status) {
    ZX_LOG(WARNING, status) << "ImeVisibilityService connection lost.";

    // We can't observe visibility events anymore, so dismiss the keyboard and
    // assume that it's closed for good.
    DismissVirtualKeyboard();
    keyboard_visible_ = false;
  });

  ime_visibility_.events().OnKeyboardVisibilityChanged = [this](bool visible) {
    keyboard_visible_ = visible;
  };
}

InputMethodKeyboardControllerFuchsia::~InputMethodKeyboardControllerFuchsia() =
    default;

bool InputMethodKeyboardControllerFuchsia::DisplayVirtualKeyboard() {
  ime_service_->ShowKeyboard();
  return true;
}

void InputMethodKeyboardControllerFuchsia::DismissVirtualKeyboard() {
  ime_service_->HideKeyboard();
}

void InputMethodKeyboardControllerFuchsia::AddObserver(
    InputMethodKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED();
}

void InputMethodKeyboardControllerFuchsia::RemoveObserver(
    InputMethodKeyboardControllerObserver* observer) {
  NOTIMPLEMENTED();
}

bool InputMethodKeyboardControllerFuchsia::IsKeyboardVisible() {
  return keyboard_visible_;
}

}  // namespace ui
