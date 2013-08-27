// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
#define UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/ime/input_method_base.h"

namespace ui {
class InputMethod;
}  // namespace ui

namespace views {

// A "bridge" InputMethod implementation for views top-level widgets, which just
// sends/receives IME related events to/from a system-wide ui::InputMethod
// object.
class InputMethodBridge : public InputMethodBase {
 public:
  // |shared_input_method| indicates if |host| is shared among other top level
  // widgets.
  InputMethodBridge(internal::InputMethodDelegate* delegate,
                    ui::InputMethod* host,
                    bool shared_input_method);
  virtual ~InputMethodBridge();

  // Overridden from InputMethod:
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE;
  virtual void DispatchKeyEvent(const ui::KeyEvent& key) OVERRIDE;
  virtual void OnTextInputTypeChanged(View* view) OVERRIDE;
  virtual void OnCaretBoundsChanged(View* view) OVERRIDE;
  virtual void CancelComposition(View* view) OVERRIDE;
  virtual void OnInputLocaleChanged() OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual bool IsCandidatePopupOpen() const OVERRIDE;

  // Overridden from FocusChangeListener.
  virtual void OnWillChangeFocus(View* focused_before, View* focused) OVERRIDE;
  virtual void OnDidChangeFocus(View* focused_before, View* focused) OVERRIDE;

  ui::InputMethod* GetHostInputMethod() const;

 private:
  void UpdateViewFocusState();

  ui::InputMethod* const host_;

  const bool shared_input_method_;

  bool context_focused_;

  DISALLOW_COPY_AND_ASSIGN(InputMethodBridge);
};

}  // namespace views

#endif  // UI_VIEWS_IME_INPUT_METHOD_BRIDGE_H_
