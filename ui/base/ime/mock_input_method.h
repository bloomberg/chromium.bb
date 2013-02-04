// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOCK_INPUT_METHOD_H_
#define UI_BASE_IME_MOCK_INPUT_METHOD_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_export.h"

namespace ui {

class KeyEvent;
class TextInputClient;

// A mock ui::InputMethod implementation for minimum input support.
class UI_EXPORT MockInputMethod : NON_EXPORTED_BASE(public InputMethod) {
 public:
  explicit MockInputMethod(internal::InputMethodDelegate* delegate);
  virtual ~MockInputMethod();

  // Overriden from InputMethod.
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) OVERRIDE;
  virtual void Init(bool focused) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void SetFocusedTextInputClient(TextInputClient* client) OVERRIDE;
  virtual TextInputClient* GetTextInputClient() const OVERRIDE;
  virtual void DispatchKeyEvent(const base::NativeEvent& native_event) OVERRIDE;
  virtual void DispatchFabricatedKeyEvent(const ui::KeyEvent& event) OVERRIDE;
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;

  int init_callcount() const {
    return init_callcount_;
  }

  int on_focus_callcount() const {
    return on_focus_callcount_;
  }

  int on_blur_callcaount() const {
    return on_blur_callcaount_;
  }

  int set_focused_text_input_client_callcount() const {
    return set_focused_text_input_client_callcount_;
  }

  int dispatch_keyevent_callcount() const {
    return dispatch_keyevent_callcount_;
  }

  int dispatch_fabricated_keyevent_callcount() const {
    return dispatch_fabricated_keyevent_callcount_;
  }

  int on_text_input_type_changed_callcount() const {
    return on_text_input_type_changed_callcount_;
  }

  int on_caret_bounds_changed_callcount() const {
    return on_caret_bounds_changed_callcount_;
  }

  int cancel_composition_callcount() const {
    return cancel_composition_callcount_;
  }

  int get_input_locale_callcount() const {
    return get_input_locale_callcount_;
  }

  int get_input_text_direction_callcount() const {
    return get_input_text_direction_callcount_;
  }

  int is_active_callcount() const {
    return is_active_callcount_;
  }

  ui::TextInputType latest_text_input_type() const {
    return latest_text_input_type_;
  }

 private:
  internal::InputMethodDelegate* delegate_;
  TextInputClient* text_input_client_;

  int init_callcount_;
  int on_focus_callcount_;
  int on_blur_callcaount_;
  int set_focused_text_input_client_callcount_;
  int get_text_input_client_callcount_;
  int dispatch_keyevent_callcount_;
  int dispatch_fabricated_keyevent_callcount_;
  int on_text_input_type_changed_callcount_;
  int on_caret_bounds_changed_callcount_;
  int cancel_composition_callcount_;
  int get_input_locale_callcount_;
  int get_input_text_direction_callcount_;
  int is_active_callcount_;

  ui::TextInputType latest_text_input_type_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethod);
};

}  // namespace ui

#endif  // UI_BASE_IME_MOCK_INPUT_METHOD_H_
