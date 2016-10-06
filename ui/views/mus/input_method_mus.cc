// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/mus/input_method_mus.h"

#include <utility>

#include "services/ui/public/cpp/window.h"
#include "services/ui/public/interfaces/ime.mojom.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/events/event.h"
#include "ui/platform_window/mojo/ime_type_converters.h"
#include "ui/platform_window/mojo/text_input_state.mojom.h"
#include "ui/views/mus/text_input_client_impl.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// InputMethodMus, public:

InputMethodMus::InputMethodMus(ui::internal::InputMethodDelegate* delegate,
                               ui::Window* window)
    : window_(window) {
  SetDelegate(delegate);
}

InputMethodMus::~InputMethodMus() {}

void InputMethodMus::Init(shell::Connector* connector) {
  connector->ConnectToInterface("service:ui", &ime_server_);
}

////////////////////////////////////////////////////////////////////////////////
// InputMethodMus, ui::InputMethod implementation:

void InputMethodMus::OnFocus() {
  InputMethodBase::OnFocus();
  UpdateTextInputType();
}

void InputMethodMus::OnBlur() {
  InputMethodBase::OnBlur();
  UpdateTextInputType();
}

bool InputMethodMus::OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                              NativeEventResult* result) {
  // This method is not called on non-Windows platforms. See the comments for
  // ui::InputMethod::OnUntranslatedIMEMessage().
  return false;
}

void InputMethodMus::DispatchKeyEvent(ui::KeyEvent* event) {
  DCHECK(event->type() == ui::ET_KEY_PRESSED ||
         event->type() == ui::ET_KEY_RELEASED);

  // If no text input client, do nothing.
  if (!GetTextInputClient()) {
    ignore_result(DispatchKeyEventPostIME(event));
    return;
  }

  // TODO(moshayedi): crbug.com/641355. Currently if we stop propagation of
  // non-char events here, accelerators ddn't work. This is because we send the
  // event ack too early in NativeWidgetMus. We should send both char and
  // non-char events to the IME driver once we fix this.
  if (event->is_char()) {
    // IME driver will notify the text input client if it is not interested in
    // event, which in turn will call DispatchKeyEventPostIME().
    input_method_->ProcessKeyEvent(ui::Event::Clone(*event));
    event->StopPropagation();
    return;
  }

  ignore_result(DispatchKeyEventPostIME(event));
}

void InputMethodMus::OnTextInputTypeChanged(const ui::TextInputClient* client) {
  if (IsTextInputClientFocused(client))
    UpdateTextInputType();
  InputMethodBase::OnTextInputTypeChanged(client);

  if (input_method_) {
    input_method_->OnTextInputTypeChanged(
        static_cast<ui::mojom::TextInputType>(client->GetTextInputType()));
  }
}

void InputMethodMus::OnCaretBoundsChanged(const ui::TextInputClient* client) {
  if (input_method_)
    input_method_->OnCaretBoundsChanged(client->GetCaretBounds());
}

void InputMethodMus::CancelComposition(const ui::TextInputClient* client) {
  if (input_method_)
    input_method_->CancelComposition();
}

void InputMethodMus::OnInputLocaleChanged() {
  // TODO(moshayedi): crbug.com/637418. Not supported in ChromeOS. Investigate
  // whether we want to support this or not.
}

bool InputMethodMus::IsCandidatePopupOpen() const {
  // TODO(moshayedi): crbug.com/637416. Implement this properly when we have a
  // mean for displaying candidate list popup.
  return false;
}

void InputMethodMus::OnDidChangeFocusedClient(
    ui::TextInputClient* focused_before,
    ui::TextInputClient* focused) {
  InputMethodBase::OnDidChangeFocusedClient(focused_before, focused);
  UpdateTextInputType();

  text_input_client_ = base::MakeUnique<TextInputClientImpl>(focused, this);
  ime_server_->StartSession(text_input_client_->CreateInterfacePtrAndBind(),
                            GetProxy(&input_method_));
}

void InputMethodMus::UpdateTextInputType() {
  ui::TextInputType type = GetTextInputType();
  mojo::TextInputStatePtr state = mojo::TextInputState::New();
  state->type = mojo::ConvertTo<mojo::TextInputType>(type);
  if (window_) {
    if (type != ui::TEXT_INPUT_TYPE_NONE)
      window_->SetImeVisibility(true, std::move(state));
    else
      window_->SetTextInputState(std::move(state));
  }
}

}  // namespace views
