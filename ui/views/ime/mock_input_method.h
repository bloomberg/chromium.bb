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
  virtual ~MockInputMethod();

  // Overridden from InputMethod:
  virtual void Init(Widget* widget) override;
  virtual void OnFocus() override;
  virtual void OnBlur() override;
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) override;
  virtual void DispatchKeyEvent(const ui::KeyEvent& key) override;
  virtual void OnTextInputTypeChanged(View* view) override;
  virtual void OnCaretBoundsChanged(View* view) override;
  virtual void CancelComposition(View* view) override;
  virtual void OnInputLocaleChanged() override;
  virtual std::string GetInputLocale() override;
  virtual bool IsActive() override;
  virtual bool IsCandidatePopupOpen() const override;
  virtual void ShowImeIfNeeded() override;
  virtual bool IsMock() const override;

  bool focus_changed() const { return focus_changed_; }
  bool untranslated_ime_message_called() const {
    return untranslated_ime_message_called_;
  }
  bool text_input_type_changed() const { return text_input_type_changed_; }
  bool caret_bounds_changed() const { return caret_bounds_changed_; }
  bool cancel_composition_called() const { return cancel_composition_called_; }
  bool input_locale_changed() const { return input_locale_changed_; }

  // Clears all internal states and result.
  void Clear();

  void SetCompositionTextForNextKey(const ui::CompositionText& composition);
  void SetResultTextForNextKey(const base::string16& result);

  void SetInputLocale(const std::string& locale);
  void SetActive(bool active);

 private:
  // Overridden from InputMethodBase.
  virtual void OnWillChangeFocus(View* focused_before, View* focused) override;

  // Clears boolean states defined below.
  void ClearStates();

  // Clears only composition information and result text.
  void ClearResult();

  // Composition information for the next key event. It'll be cleared
  // automatically after dispatching the next key event.
  ui::CompositionText composition_;
  bool composition_changed_;

  // Result text for the next key event. It'll be cleared automatically after
  // dispatching the next key event.
  base::string16 result_text_;

  // Record call state of corresponding methods. They will be set to false
  // automatically before dispatching a key event.
  bool focus_changed_;
  bool untranslated_ime_message_called_;
  bool text_input_type_changed_;
  bool caret_bounds_changed_;
  bool cancel_composition_called_;
  bool input_locale_changed_;

  // To mock corresponding input method prooperties.
  std::string locale_;
  bool active_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethod);
};

}  // namespace views

#endif  // UI_VIEWS_IME_MOCK_INPUT_METHOD_H_
