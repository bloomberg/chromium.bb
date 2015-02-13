// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_IME_MOCK_INPUT_METHOD_H_
#define UI_VIEWS_IME_MOCK_INPUT_METHOD_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/composition_text.h"
#include "ui/views/ime/input_method_base.h"
#include "ui/views/view.h"

namespace views {

// A mock InputMethod implementation for testing purpose.
class VIEWS_EXPORT MockInputMethod : public InputMethodBase {
 public:
  MockInputMethod();
  explicit MockInputMethod(internal::InputMethodDelegate* delegate);
  ~MockInputMethod() override;

  // Overridden from InputMethod:
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
  bool IsCandidatePopupOpen() const override;
  void ShowImeIfNeeded() override;

  bool untranslated_ime_message_called() const {
    return untranslated_ime_message_called_;
  }
  bool text_input_type_changed() const { return text_input_type_changed_; }
  bool cancel_composition_called() const { return cancel_composition_called_; }

  // Clears all internal states and result.
  void Clear();

  void SetCompositionTextForNextKey(const ui::CompositionText& composition);
  void SetResultTextForNextKey(const base::string16& result);

 private:
  // Overridden from InputMethodBase.
  void OnWillChangeFocus(View* focused_before, View* focused) override;

  // Clears boolean states defined below.
  void ClearStates();

  // Whether a mock composition or result is scheduled for the next key event.
  bool HasComposition();

  // Clears only composition information and result text.
  void ClearComposition();

  // Composition information for the next key event. It'll be cleared
  // automatically after dispatching the next key event.
  ui::CompositionText composition_;

  // Result text for the next key event. It'll be cleared automatically after
  // dispatching the next key event.
  base::string16 result_text_;

  // Record call state of corresponding methods. They will be set to false
  // automatically before dispatching a key event.
  bool untranslated_ime_message_called_;
  bool text_input_type_changed_;
  bool cancel_composition_called_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethod);
};

}  // namespace views

#endif  // UI_VIEWS_IME_MOCK_INPUT_METHOD_H_
