// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_TSF_H_
#define UI_BASE_IME_INPUT_METHOD_TSF_H_

#include <windows.h>

#include <string>

#include "ui/base/ime/input_method_win.h"

namespace ui {

// An InputMethod implementation based on Windows TSF API.
class UI_EXPORT InputMethodTSF : public InputMethodWin {
 public:
  InputMethodTSF(internal::InputMethodDelegate* delegate,
                 HWND toplevel_window_handle);

  // Overridden from InputMethod:
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE;
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual void SetFocusedTextInputClient(TextInputClient* client) OVERRIDE;
  virtual bool IsCandidatePopupOpen() const OVERRIDE;

  // Overridden from InputMethodBase:
  virtual void OnWillChangeFocusedClient(TextInputClient* focused_before,
                                         TextInputClient* focused) OVERRIDE;
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) OVERRIDE;

 private:
  // Asks the client to confirm current composition text.
  void ConfirmCompositionText();

  // Returns true if the Win32 native window bound to |client| has Win32 input
  // focus.
  bool IsWindowFocused(const TextInputClient* client) const;

  DISALLOW_COPY_AND_ASSIGN(InputMethodTSF);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_TSF_H_
