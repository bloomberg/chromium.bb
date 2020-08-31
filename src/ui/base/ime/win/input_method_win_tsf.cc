// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/win/input_method_win_tsf.h"

#include "ui/base/ime/input_method_keyboard_controller.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_bridge.h"
#include "ui/base/ime/win/tsf_event_router.h"

namespace ui {

class InputMethodWinTSF::TSFEventObserver : public TSFEventRouterObserver {
 public:
  TSFEventObserver() = default;

  // Returns true if we know for sure that a candidate window (or IME suggest,
  // etc.) is open.
  bool IsCandidatePopupOpen() const { return is_candidate_popup_open_; }

  // Overridden from TSFEventRouterObserver:
  void OnCandidateWindowCountChanged(size_t window_count) override {
    is_candidate_popup_open_ = (window_count != 0);
  }

 private:
  // True if we know for sure that a candidate window is open.
  bool is_candidate_popup_open_ = false;

  DISALLOW_COPY_AND_ASSIGN(TSFEventObserver);
};

InputMethodWinTSF::InputMethodWinTSF(internal::InputMethodDelegate* delegate,
                                     HWND toplevel_window_handle)
    : InputMethodWinBase(delegate, toplevel_window_handle),
      tsf_event_observer_(new TSFEventObserver()),
      tsf_event_router_(new TSFEventRouter(tsf_event_observer_.get())) {}

InputMethodWinTSF::~InputMethodWinTSF() {}

void InputMethodWinTSF::OnFocus() {
  InputMethodBase::OnFocus();
  if (!ui::TSFBridge::GetInstance()) {
    // TSFBridge can be null for tests.
    return;
  }
  tsf_event_router_->SetManager(
      ui::TSFBridge::GetInstance()->GetThreadManager().Get());
  ui::TSFBridge::GetInstance()->SetInputMethodDelegate(
      InputMethodBase::delegate());
}

void InputMethodWinTSF::OnBlur() {
  InputMethodBase::OnBlur();
  if (!ui::TSFBridge::GetInstance()) {
    // TSFBridge can be null for tests.
    return;
  }
  tsf_event_router_->SetManager(nullptr);
  ui::TSFBridge::GetInstance()->RemoveInputMethodDelegate();
}

bool InputMethodWinTSF::OnUntranslatedIMEMessage(
    const MSG event,
    InputMethod::NativeEventResult* result) {
  LRESULT original_result = 0;
  BOOL handled = FALSE;
  // Even when TSF is enabled, following IMM32/Win32 messages must be handled.
  switch (event.message) {
    case WM_IME_REQUEST:
      // Some TSF-native TIPs (Text Input Processors) such as ATOK and Mozc
      // still rely on WM_IME_REQUEST message to implement reverse conversion.
      original_result =
          OnImeRequest(event.message, event.wParam, event.lParam, &handled);
      break;
    case WM_CHAR:
    case WM_SYSCHAR:
      // ui::InputMethod interface is responsible for handling Win32 character
      // messages. For instance, we will be here in the following cases.
      // - TIP is not activated. (e.g, the current language profile is English)
      // - TIP does not handle and WM_KEYDOWN and WM_KEYDOWN is translated into
      //   WM_CHAR by TranslateMessage API. (e.g, TIP is turned off)
      // - Another application sends WM_CHAR through SendMessage API.
      original_result = OnChar(event.hwnd, event.message, event.wParam,
                               event.lParam, event, &handled);
      break;
  }

  if (result)
    *result = original_result;
  return !!handled;
}

void InputMethodWinTSF::OnTextInputTypeChanged(const TextInputClient* client) {
  InputMethodBase::OnTextInputTypeChanged(client);
  if (!ui::TSFBridge::GetInstance() || !IsTextInputClientFocused(client) ||
      !IsWindowFocused(client)) {
    return;
  }
  ui::TSFBridge::GetInstance()->CancelComposition();
  ui::TSFBridge::GetInstance()->OnTextInputTypeChanged(client);
  InputMethodWinBase::UpdateEngineFocusAndInputContext();
}

void InputMethodWinTSF::OnCaretBoundsChanged(const TextInputClient* client) {
  if (!ui::TSFBridge::GetInstance() || !IsTextInputClientFocused(client) ||
      !IsWindowFocused(client)) {
    return;
  }
  NotifyTextInputCaretBoundsChanged(client);
  ui::TSFBridge::GetInstance()->OnTextLayoutChanged();
  InputMethodWinBase::UpdateCompositionBoundsForEngine(client);
}

void InputMethodWinTSF::CancelComposition(const TextInputClient* client) {
  if (ui::TSFBridge::GetInstance() && IsTextInputClientFocused(client) &&
      IsWindowFocused(client)) {
    ui::TSFBridge::GetInstance()->CancelComposition();
    InputMethodWinBase::CancelCompositionForEngine();
  }
}

void InputMethodWinTSF::DetachTextInputClient(TextInputClient* client) {
  if (!ui::TSFBridge::GetInstance()) {
    // TSFBridge can be null for tests.
    return;
  }
  InputMethodWinBase::DetachTextInputClient(client);
  ui::TSFBridge::GetInstance()->RemoveFocusedClient(client);
}

bool InputMethodWinTSF::IsInputLocaleCJK() const {
  if (!ui::TSFBridge::GetInstance()) {
    return false;
  }

  return ui::TSFBridge::GetInstance()->IsInputLanguageCJK();
}

bool InputMethodWinTSF::IsCandidatePopupOpen() const {
  return tsf_event_observer_->IsCandidatePopupOpen();
}

void InputMethodWinTSF::OnWillChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  if (IsWindowFocused(focused_before)) {
    ConfirmCompositionText(/* reset_engine */ true,
                           /* keep_selection */ false);
    ui::TSFBridge::GetInstance()->RemoveFocusedClient(focused_before);
  }
}

void InputMethodWinTSF::OnDidChangeFocusedClient(
    TextInputClient* focused_before,
    TextInputClient* focused) {
  if (ui::TSFBridge::GetInstance() && IsWindowFocused(focused) &&
      IsTextInputClientFocused(focused)) {
    ui::TSFBridge::GetInstance()->SetFocusedClient(toplevel_window_handle_,
                                                   focused);
    // Force to update the input type since client's TextInputStateChanged()
    // function might not be called if text input types before the client loses
    // focus and after it acquires focus again are the same.
    OnTextInputTypeChanged(focused);

    // Force to update caret bounds, in case the client thinks that the caret
    // bounds has not changed.
    OnCaretBoundsChanged(focused);
  }
  InputMethodWinBase::OnDidChangeFocusedClient(focused_before, focused);
}

void InputMethodWinTSF::ConfirmCompositionText(bool reset_engine,
                                               bool keep_selection) {
  // TODO(b/134473433) Modify this function so that when keep_selection is
  // true, the selection is not changed when text committed
  if (keep_selection) {
    NOTIMPLEMENTED_LOG_ONCE();
  }
  if (IsTextInputTypeNone())
    return;

  if (reset_engine && GetTextInputClient()->HasCompositionText())
    InputMethodWinBase::ResetEngine();
  if (ui::TSFBridge::GetInstance())
    ui::TSFBridge::GetInstance()->ConfirmComposition();
}

void InputMethodWinTSF::ShowVirtualKeyboardIfEnabled() {
  // TODO(crbug.com/1031786): Enable this once TSF input pane policy bug is
  // fixed if (ui::TSFBridge::GetInstance())
  //   ui::TSFBridge::GetInstance()->SetInputPanelPolicy(
  //       /*inputPanelPolicyManual*/ false);
  if (auto* controller = GetInputMethodKeyboardController())
    controller->DisplayVirtualKeyboard();
}

}  // namespace ui
