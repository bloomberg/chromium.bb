// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_NULL_INPUT_METHOD_H_
#define UI_VIEWS_IME_NULL_INPUT_METHOD_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ui/views/ime/input_method.h"

namespace views {

// An implementation of views::InputMethod which does nothing.
//
// We're working on removing views::InputMethod{,Base,Bridge} and going to use
// only ui::InputMethod.  Use this class instead of views::InputMethodBridge
// with ui::TextInputFocusManager to effectively eliminate the
// views::InputMethod layer.
class NullInputMethod : public InputMethod {
 public:
  NullInputMethod();

  // Overridden from InputMethod:
  void SetDelegate(internal::InputMethodDelegate* delegate) override;
  void Init(Widget* widget) override;
  void OnFocus() override;
  void OnBlur() override;
  bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                NativeEventResult* result) override;
  void DispatchKeyEvent(const ui::KeyEvent& key) override;
  void OnTextInputTypeChanged(View* view) override;
  void OnCaretBoundsChanged(View* view) override;
  void CancelComposition(View* view) override;
  void OnInputLocaleChanged() override;
  std::string GetInputLocale() override;
  bool IsActive() override;
  ui::TextInputClient* GetTextInputClient() const override;
  ui::TextInputType GetTextInputType() const override;
  bool IsCandidatePopupOpen() const override;
  void ShowImeIfNeeded() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(NullInputMethod);
};

}  // namespace views

#endif  // UI_VIEWS_IME_NULL_INPUT_METHOD_H_
