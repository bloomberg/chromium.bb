// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/ime_driver/input_method_bridge_chromeos.h"

#include <memory>
#include <utility>

#include "chrome/browser/chromeos/accessibility/accessibility_input_method_observer.h"
#include "chrome/browser/ui/views/ime_driver/remote_text_input_client.h"
#include "ui/base/ime/chromeos/input_method_chromeos.h"
#include "ui/base/ime/ime_bridge.h"

namespace {

bool IsActiveInputContextHandler(ui::InputMethodChromeOS* input_method) {
  ui::IMEBridge* bridge = ui::IMEBridge::Get();
  return bridge && bridge->GetInputContextHandler() == input_method;
}

}  // namespace

InputMethodBridge::InputMethodBridge(
    std::unique_ptr<RemoteTextInputClient> client)
    : client_(std::move(client)),
      input_method_chromeos_(
          std::make_unique<ui::InputMethodChromeOS>(client_.get())),
      accessibility_input_method_observer_(
          std::make_unique<AccessibilityInputMethodObserver>(
              input_method_chromeos_.get())) {
  input_method_chromeos_->OnFocus();
  input_method_chromeos_->SetFocusedTextInputClient(client_.get());
}

InputMethodBridge::~InputMethodBridge() {
  // IME session is ending.
  if (IsActiveInputContextHandler(input_method_chromeos_.get()))
    accessibility_input_method_observer_->ResetCaretBounds();
}

void InputMethodBridge::OnTextInputStateChanged(
    ws::mojom::TextInputStatePtr text_input_state) {
  client_->SetTextInputState(std::move(text_input_state));

  if (IsActiveInputContextHandler(input_method_chromeos_.get()))
    input_method_chromeos_->OnTextInputTypeChanged(client_.get());
}

void InputMethodBridge::OnCaretBoundsChanged(const gfx::Rect& caret_bounds) {
  client_->SetCaretBounds(caret_bounds);

  if (IsActiveInputContextHandler(input_method_chromeos_.get()))
    input_method_chromeos_->OnCaretBoundsChanged(client_.get());
}

void InputMethodBridge::OnTextInputClientDataChanged(
    ws::mojom::TextInputClientDataPtr data) {
  client_->SetTextInputClientData(std::move(data));
}

void InputMethodBridge::ProcessKeyEvent(std::unique_ptr<ui::Event> event,
                                        ProcessKeyEventCallback callback) {
  DCHECK(event->IsKeyEvent());
  ui::KeyEvent* key_event = event->AsKeyEvent();
  if (IsActiveInputContextHandler(input_method_chromeos_.get()) &&
      !key_event->is_char()) {
    input_method_chromeos_->DispatchKeyEventAsync(key_event,
                                                  std::move(callback));
  } else {
    const bool handled = false;
    std::move(callback).Run(handled);
  }
}

void InputMethodBridge::CancelComposition() {
  if (IsActiveInputContextHandler(input_method_chromeos_.get()))
    input_method_chromeos_->CancelComposition(client_.get());
}

void InputMethodBridge::ShowVirtualKeyboardIfEnabled() {
  if (IsActiveInputContextHandler(input_method_chromeos_.get()))
    input_method_chromeos_->ShowVirtualKeyboardIfEnabled();
}
