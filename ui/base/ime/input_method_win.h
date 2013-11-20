// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_INPUT_METHOD_WIN_H_
#define UI_BASE_IME_INPUT_METHOD_WIN_H_

#include <windows.h>

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/input_method_base.h"
#include "ui/base/ime/win/imm32_manager.h"

namespace ui {

// A common InputMethod implementation shared between IMM32 and TSF.
class UI_EXPORT InputMethodWin : public InputMethodBase {
 public:
  InputMethodWin(internal::InputMethodDelegate* delegate,
                 HWND toplevel_window_handle);

  // Overridden from InputMethod:
  virtual void Init(bool focused) OVERRIDE;
  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnInputLocaleChanged() OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;

 protected:
  // Overridden from InputMethodBase:
  // If a derived class overrides this method, it should call parent's
  // implementation.
  virtual void OnDidChangeFocusedClient(TextInputClient* focused_before,
                                        TextInputClient* focused) OVERRIDE;

  // Some IMEs rely on WM_IME_REQUEST message even when TSF is enabled. So
  // OnImeRequest (and its actual implementations as OnDocumentFeed,
  // OnReconvertString, and OnQueryCharPosition) are placed in this base class.
  LRESULT OnImeRequest(UINT message,
                       WPARAM wparam,
                       LPARAM lparam,
                       BOOL* handled);
  // For both WM_CHAR and WM_SYSCHAR
  LRESULT OnChar(HWND window_handle,
                 UINT message,
                 WPARAM wparam,
                 LPARAM lparam,
                 BOOL* handled);
  // For both WM_DEADCHAR and WM_SYSDEADCHAR
  // TODO(yukawa): Stop handling WM_DEADCHAR and WM_SYSDEADCHAR when non-Aura
  // build is deprecated.
  LRESULT OnDeadChar(UINT message, WPARAM wparam, LPARAM lparam, BOOL* handled);

  LRESULT OnDocumentFeed(RECONVERTSTRING* reconv);
  LRESULT OnReconvertString(RECONVERTSTRING* reconv);
  LRESULT OnQueryCharPosition(IMECHARPOSITION* char_positon);

  // Returns the window handle to which |text_input_client| is bound.
  // On Aura environment, |toplevel_window_handle_| is always returned.
  HWND GetAttachedWindowHandle(const TextInputClient* text_input_client) const;

  // Returns true if the Win32 native window bound to |client| is considered
  // to be ready for receiving keyboard input.
  bool IsWindowFocused(const TextInputClient* client) const;

  // Indicates if the current input locale has an IME.
  bool active_;

  // Windows IMM32 wrapper.
  // (See "ui/base/ime/win/ime_input.h" for its details.)
  ui::IMM32Manager imm32_manager_;

 private:
  bool DispatchFabricatedKeyEvent(const ui::KeyEvent& event);

  // The toplevel window handle.
  // On non-Aura environment, this value is not used and always NULL.
  const HWND toplevel_window_handle_;

  // Name of the current input locale.
  std::string locale_;

  // The current input text direction.
  base::i18n::TextDirection direction_;

  // The new text direction and layout alignment requested by the user by
  // pressing ctrl-shift. It'll be sent to the text input client when the key
  // is released.
  base::i18n::TextDirection pending_requested_direction_;

  // Represents if WM_CHAR[wparam=='\r'] should be dispatched to the focused
  // text input client or ignored silently. This flag is introduced as a quick
  // workaround against crbug.com/319100
  // TODO(yukawa, IME): Figure out long-term solution.
  bool accept_carriage_return_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodWin);
};

}  // namespace ui

#endif  // UI_BASE_IME_INPUT_METHOD_WIN_H_
