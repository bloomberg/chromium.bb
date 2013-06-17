// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_imm32.h"

#include "base/basictypes.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_client.h"


namespace ui {

InputMethodIMM32::InputMethodIMM32(internal::InputMethodDelegate* delegate,
                                   HWND toplevel_window_handle)
    : InputMethodWin(delegate, toplevel_window_handle),
      enabled_(false) {
}

void InputMethodIMM32::OnFocus() {
  InputMethodWin::OnFocus();
  UpdateIMEState();
}

void InputMethodIMM32::OnBlur() {
  ConfirmCompositionText();
  InputMethodWin::OnBlur();
}

bool InputMethodIMM32::OnUntranslatedIMEMessage(
    const base::NativeEvent& event, InputMethod::NativeEventResult* result) {
  LRESULT original_result = 0;
  BOOL handled = FALSE;
  switch (event.message) {
    case WM_IME_SETCONTEXT:
      original_result = OnImeSetContext(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_STARTCOMPOSITION:
      original_result = OnImeStartComposition(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_COMPOSITION:
      original_result = OnImeComposition(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_ENDCOMPOSITION:
      original_result = OnImeEndComposition(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_REQUEST:
      original_result = OnImeRequest(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      original_result = OnChar(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
      original_result = OnDeadChar(
          event.message, event.wParam, event.lParam, &handled);
      break;
    default:
      NOTREACHED() << "Unknown IME message:" << event.message;
      break;
  }
  if (result)
    *result = original_result;
  return !!handled;
}

void InputMethodIMM32::OnTextInputTypeChanged(const TextInputClient* client) {
  if (IsTextInputClientFocused(client)) {
    ime_input_.CancelIME(GetAttachedWindowHandle(client));
    UpdateIMEState();
  }
  InputMethodWin::OnTextInputTypeChanged(client);
}

void InputMethodIMM32::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!enabled_ || !IsTextInputClientFocused(client))
    return;
  // The current text input type should not be NONE if |client| is focused.
  DCHECK(!IsTextInputTypeNone());
  gfx::Rect screen_bounds(GetTextInputClient()->GetCaretBounds());

  HWND attached_window = GetAttachedWindowHandle(client);
  // TODO(ime): see comment in TextInputClient::GetCaretBounds(), this
  // conversion shouldn't be necessary.
  RECT r;
  GetClientRect(attached_window, &r);
  POINT window_point = { screen_bounds.x(), screen_bounds.y() };
  ScreenToClient(attached_window, &window_point);
  ime_input_.UpdateCaretRect(
      attached_window,
      gfx::Rect(gfx::Point(window_point.x, window_point.y),
                screen_bounds.size()));
}

void InputMethodIMM32::CancelComposition(const TextInputClient* client) {
  if (enabled_ && IsTextInputClientFocused(client))
    ime_input_.CancelIME(GetAttachedWindowHandle(client));
}

void InputMethodIMM32::SetFocusedTextInputClient(TextInputClient* client) {
  InputMethodWin::SetFocusedTextInputClient(client);
}

void InputMethodIMM32::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  ConfirmCompositionText();
}

void InputMethodIMM32::OnDidChangeFocusedClient(TextInputClient* focused_before,
                                                TextInputClient* focused) {
  // Force to update the input type since client's TextInputStateChanged()
  // function might not be called if text input types before the client loses
  // focus and after it acquires focus again are the same.
  OnTextInputTypeChanged(focused);

  UpdateIMEState();

  // Force to update caret bounds, in case the client thinks that the caret
  // bounds has not changed.
  OnCaretBoundsChanged(focused);
}

LRESULT InputMethodIMM32::OnImeSetContext(UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam,
                                          BOOL* handled) {
  active_ = (wparam == TRUE);
  HWND attached_window = GetAttachedWindowHandle(GetTextInputClient());
  if (active_ && IsWindow(attached_window))
    ime_input_.CreateImeWindow(attached_window);

  OnInputMethodChanged();
  if (!IsWindow(attached_window))
    return 0;
  return ime_input_.SetImeWindowStyle(
      attached_window, message, wparam, lparam, handled);
}

LRESULT InputMethodIMM32::OnImeStartComposition(UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam,
                                                BOOL* handled) {
  // We have to prevent WTL from calling ::DefWindowProc() because the function
  // calls ::ImmSetCompositionWindow() and ::ImmSetCandidateWindow() to
  // over-write the position of IME windows.
  *handled = TRUE;

  if (IsTextInputTypeNone())
    return 0;

  // Reset the composition status and create IME windows.
  HWND attached_window = GetAttachedWindowHandle(GetTextInputClient());
  ime_input_.CreateImeWindow(attached_window);
  ime_input_.ResetComposition(attached_window);
  return 0;
}

LRESULT InputMethodIMM32::OnImeComposition(UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam,
                                           BOOL* handled) {
  // We have to prevent WTL from calling ::DefWindowProc() because we do not
  // want for the IMM (Input Method Manager) to send WM_IME_CHAR messages.
  *handled = TRUE;

  if (IsTextInputTypeNone())
    return 0;

  HWND attached_window = GetAttachedWindowHandle(GetTextInputClient());

  // At first, update the position of the IME window.
  ime_input_.UpdateImeWindow(attached_window);

  // Retrieve the result string and its attributes of the ongoing composition
  // and send it to a renderer process.
  ui::CompositionText composition;
  if (ime_input_.GetResult(attached_window, lparam, &composition.text)) {
    GetTextInputClient()->InsertText(composition.text);
    ime_input_.ResetComposition(attached_window);
    // Fall though and try reading the composition string.
    // Japanese IMEs send a message containing both GCS_RESULTSTR and
    // GCS_COMPSTR, which means an ongoing composition has been finished
    // by the start of another composition.
  }
  // Retrieve the composition string and its attributes of the ongoing
  // composition and send it to a renderer process.
  if (ime_input_.GetComposition(attached_window, lparam, &composition))
    GetTextInputClient()->SetCompositionText(composition);

  return 0;
}

LRESULT InputMethodIMM32::OnImeEndComposition(UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam,
                                              BOOL* handled) {
  // Let WTL call ::DefWindowProc() and release its resources.
  *handled = FALSE;

  if (IsTextInputTypeNone())
    return 0;

  if (GetTextInputClient()->HasCompositionText())
    GetTextInputClient()->ClearCompositionText();

  HWND attached_window = GetAttachedWindowHandle(GetTextInputClient());
  ime_input_.ResetComposition(attached_window);
  ime_input_.DestroyImeWindow(attached_window);
  return 0;
}

void InputMethodIMM32::ConfirmCompositionText() {
  if (!IsTextInputTypeNone()) {
    HWND attached_window = GetAttachedWindowHandle(GetTextInputClient());

    ime_input_.CleanupComposition(attached_window);
    // Though above line should confirm the client's composition text by sending
    // a result text to us, in case the input method and the client are in
    // inconsistent states, we check the client's composition state again.
    if (GetTextInputClient()->HasCompositionText())
      GetTextInputClient()->ConfirmCompositionText();
  }
}

void InputMethodIMM32::UpdateIMEState() {
  // Use switch here in case we are going to add more text input types.
  // We disable input method in password field.
  switch (GetTextInputType()) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      ime_input_.DisableIME(GetAttachedWindowHandle(GetTextInputClient()));
      enabled_ = false;
      break;
    default:
      ime_input_.EnableIME(GetAttachedWindowHandle(GetTextInputClient()));
      enabled_ = true;
      break;
  }
}

}  // namespace ui
