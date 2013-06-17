// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_tsf.h"

#include "ui/base/ime/text_input_client.h"

namespace ui {

InputMethodTSF::InputMethodTSF(internal::InputMethodDelegate* delegate,
                               HWND toplevel_window_handle)
    : InputMethodWin(delegate, toplevel_window_handle) {
}

void InputMethodTSF::OnFocus() {
  InputMethodWin::OnFocus();
  NOTIMPLEMENTED();
}

void InputMethodTSF::OnBlur() {
  NOTIMPLEMENTED();
  InputMethodWin::OnBlur();
}

bool InputMethodTSF::OnUntranslatedIMEMessage(
    const base::NativeEvent& event, InputMethod::NativeEventResult* result) {
  LRESULT original_result = 0;
  BOOL handled = FALSE;
  // Even when TSF is enabled, following IMM32/Win32 messages must be handled.
  switch (event.message) {
    case WM_IME_REQUEST:
      // Some TSF-native TIPs (Text Input Processors) such as ATOK and Mozc
      // still rely on WM_IME_REQUEST message to implement reverse conversion.
      original_result = OnImeRequest(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      // ui::InputMethod interface is responsible for handling Win32 character
      // messages. For instance, we will be here in the following cases.
      // - TIP is not activated. (e.g, the current language profile is English)
      // - TIP does not handle and WM_KEYDOWN and WM_KEYDOWN is translated into
      //   WM_CHAR by TranslateMessage API. (e.g, TIP is turned off)
      // - Another application sends WM_CHAR through SendMessage API.
      original_result = OnChar(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
      // See the comment in WM_CHAR/WM_SYSCHAR.
      original_result = OnDeadChar(
          event.message, event.wParam, event.lParam, &handled);
      break;
  }
  if (result)
    *result = original_result;
  return !!handled;
}

void InputMethodTSF::OnTextInputTypeChanged(const TextInputClient* client) {
  NOTIMPLEMENTED();
  InputMethodWin::OnTextInputTypeChanged(client);
}

void InputMethodTSF::OnCaretBoundsChanged(const TextInputClient* client) {
  NOTIMPLEMENTED();
}

void InputMethodTSF::CancelComposition(const TextInputClient* client) {
  NOTIMPLEMENTED();
}

void InputMethodTSF::SetFocusedTextInputClient(TextInputClient* client) {
  NOTIMPLEMENTED();
  InputMethodWin::SetFocusedTextInputClient(client);
}

void InputMethodTSF::OnWillChangeFocusedClient(TextInputClient* focused_before,
                                               TextInputClient* focused) {
  NOTIMPLEMENTED();
}

void InputMethodTSF::OnDidChangeFocusedClient(TextInputClient* focused_before,
                                              TextInputClient* focused) {
  NOTIMPLEMENTED();
}

void InputMethodTSF::ConfirmCompositionText() {
  NOTIMPLEMENTED();
}

}  // namespace ui
