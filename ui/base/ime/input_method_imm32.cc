// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/input_method_imm32.h"

#include "base/basictypes.h"
#include "ui/base/ime/composition_text.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_input_scope.h"


namespace ui {

InputMethodIMM32::InputMethodIMM32(internal::InputMethodDelegate* delegate,
                                   HWND toplevel_window_handle)
    : InputMethodWin(delegate, toplevel_window_handle),
      enabled_(false), is_candidate_popup_open_(false),
      composing_window_handle_(NULL) {
  // In non-Aura environment, appropriate callbacks to OnFocus() and OnBlur()
  // are not implemented yet. To work around this limitation, here we use
  // "always focused" model.
  // TODO(ime): Fix the caller of OnFocus() and OnBlur() so that appropriate
  // focus event will be passed.
  InputMethodWin::OnFocus();
}

void InputMethodIMM32::OnFocus() {
  // Ignore OnFocus event for "always focused" model. See the comment in the
  // constructor.
  // TODO(ime): Implement OnFocus once the callers are fixed.
}

void InputMethodIMM32::OnBlur() {
  // Ignore OnBlur event for "always focused" model. See the comment in the
  // constructor.
  // TODO(ime): Implement OnFocus once the callers are fixed.
}

bool InputMethodIMM32::OnUntranslatedIMEMessage(
    const base::NativeEvent& event, InputMethod::NativeEventResult* result) {
  LRESULT original_result = 0;
  BOOL handled = FALSE;
  switch (event.message) {
    case WM_IME_SETCONTEXT:
      original_result = OnImeSetContext(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_STARTCOMPOSITION:
      original_result = OnImeStartComposition(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_COMPOSITION:
      original_result = OnImeComposition(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_ENDCOMPOSITION:
      original_result = OnImeEndComposition(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_REQUEST:
      original_result = OnImeRequest(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      original_result = OnChar(
          event.hwnd, event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_DEADCHAR:
    case WM_SYSDEADCHAR:
      original_result = OnDeadChar(
          event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_IME_NOTIFY:
      original_result = OnImeNotify(
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
  if (IsTextInputClientFocused(client) && IsWindowFocused(client)) {
    imm32_manager_.CancelIME(GetAttachedWindowHandle(client));
    UpdateIMEState();
  }
  InputMethodWin::OnTextInputTypeChanged(client);
}

void InputMethodIMM32::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!enabled_ || !IsTextInputClientFocused(client) ||
      !IsWindowFocused(client)) {
    return;
  }

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
  imm32_manager_.UpdateCaretRect(
      attached_window,
      gfx::Rect(gfx::Point(window_point.x, window_point.y),
                screen_bounds.size()));
}

void InputMethodIMM32::CancelComposition(const TextInputClient* client) {
  if (enabled_ && IsTextInputClientFocused(client))
    imm32_manager_.CancelIME(GetAttachedWindowHandle(client));
}

bool InputMethodIMM32::IsCandidatePopupOpen() const {
  return is_candidate_popup_open_;
}

void InputMethodIMM32::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  if (IsWindowFocused(focused_before)) {
    ConfirmCompositionText();
  }
}

void InputMethodIMM32::OnDidChangeFocusedClient(TextInputClient* focused_before,
                                                TextInputClient* focused) {
  if (IsWindowFocused(focused)) {
    // Force to update the input type since client's TextInputStateChanged()
    // function might not be called if text input types before the client loses
    // focus and after it acquires focus again are the same.
    OnTextInputTypeChanged(focused);

    UpdateIMEState();

    // Force to update caret bounds, in case the client thinks that the caret
    // bounds has not changed.
    OnCaretBoundsChanged(focused);
  }
}

LRESULT InputMethodIMM32::OnImeSetContext(HWND window_handle,
                                          UINT message,
                                          WPARAM wparam,
                                          LPARAM lparam,
                                          BOOL* handled) {
  if (!!wparam)
    imm32_manager_.CreateImeWindow(window_handle);

  OnInputMethodChanged();
  return imm32_manager_.SetImeWindowStyle(
      window_handle, message, wparam, lparam, handled);
}

LRESULT InputMethodIMM32::OnImeStartComposition(HWND window_handle,
                                                UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam,
                                                BOOL* handled) {
  // We have to prevent WTL from calling ::DefWindowProc() because the function
  // calls ::ImmSetCompositionWindow() and ::ImmSetCandidateWindow() to
  // over-write the position of IME windows.
  *handled = TRUE;

  // Reset the composition status and create IME windows.
  composing_window_handle_ = window_handle;
  imm32_manager_.CreateImeWindow(window_handle);
  imm32_manager_.ResetComposition(window_handle);
  return 0;
}

LRESULT InputMethodIMM32::OnImeComposition(HWND window_handle,
                                           UINT message,
                                           WPARAM wparam,
                                           LPARAM lparam,
                                           BOOL* handled) {
  // We have to prevent WTL from calling ::DefWindowProc() because we do not
  // want for the IMM (Input Method Manager) to send WM_IME_CHAR messages.
  *handled = TRUE;

  // At first, update the position of the IME window.
  imm32_manager_.UpdateImeWindow(window_handle);

  // Retrieve the result string and its attributes of the ongoing composition
  // and send it to a renderer process.
  ui::CompositionText composition;
  if (imm32_manager_.GetResult(window_handle, lparam, &composition.text)) {
    if (!IsTextInputTypeNone())
      GetTextInputClient()->InsertText(composition.text);
    imm32_manager_.ResetComposition(window_handle);
    // Fall though and try reading the composition string.
    // Japanese IMEs send a message containing both GCS_RESULTSTR and
    // GCS_COMPSTR, which means an ongoing composition has been finished
    // by the start of another composition.
  }
  // Retrieve the composition string and its attributes of the ongoing
  // composition and send it to a renderer process.
  if (imm32_manager_.GetComposition(window_handle, lparam, &composition) &&
      !IsTextInputTypeNone())
    GetTextInputClient()->SetCompositionText(composition);

  return 0;
}

LRESULT InputMethodIMM32::OnImeEndComposition(HWND window_handle,
                                              UINT message,
                                              WPARAM wparam,
                                              LPARAM lparam,
                                              BOOL* handled) {
  // Let WTL call ::DefWindowProc() and release its resources.
  *handled = FALSE;

  composing_window_handle_ = NULL;

  if (!IsTextInputTypeNone() && GetTextInputClient()->HasCompositionText())
    GetTextInputClient()->ClearCompositionText();

  imm32_manager_.ResetComposition(window_handle);
  imm32_manager_.DestroyImeWindow(window_handle);
  return 0;
}

LRESULT InputMethodIMM32::OnImeNotify(UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam,
                                      BOOL* handled) {
  *handled = FALSE;

  // Update |is_candidate_popup_open_|, whether a candidate window is open.
  switch (wparam) {
  case IMN_OPENCANDIDATE:
    is_candidate_popup_open_ = true;
    break;
  case IMN_CLOSECANDIDATE:
    is_candidate_popup_open_ = false;
    break;
  }

  return 0;
}

void InputMethodIMM32::ConfirmCompositionText() {
  if (composing_window_handle_)
    imm32_manager_.CleanupComposition(composing_window_handle_);

  if (!IsTextInputTypeNone()) {
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
  const HWND window_handle = GetAttachedWindowHandle(GetTextInputClient());
  const TextInputType text_input_type = GetTextInputType();
  const TextInputMode text_input_mode = GetTextInputMode();
  switch (text_input_type) {
    case ui::TEXT_INPUT_TYPE_NONE:
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      imm32_manager_.DisableIME(window_handle);
      enabled_ = false;
      break;
    default:
      imm32_manager_.EnableIME(window_handle);
      enabled_ = true;
      break;
  }

  imm32_manager_.SetTextInputMode(window_handle, text_input_mode);
  tsf_inputscope::SetInputScopeForTsfUnawareWindow(
      window_handle, text_input_type, text_input_mode);
}

}  // namespace ui
