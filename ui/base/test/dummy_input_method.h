// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_TEST_DUMMY_INPUT_METHOD_H_
#define UI_BASE_TEST_DUMMY_INPUT_METHOD_H_

#include "ui/base/ime/input_method.h"

namespace ui {
namespace test {

class DummyInputMethod : public ui::InputMethod {
 public:
  DummyInputMethod() {}
  virtual ~DummyInputMethod() {}

  // ui::InputMethod overrides:
  virtual void SetDelegate(
      ui::internal::InputMethodDelegate* delegate) OVERRIDE {}
  virtual void Init(bool focused) OVERRIDE {}
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual void SetFocusedTextInputClient(
      ui::TextInputClient* client) OVERRIDE {}
  virtual ui::TextInputClient* GetTextInputClient() const OVERRIDE {
    return NULL;
  }
  virtual void DispatchKeyEvent(
      const base::NativeEvent& native_key_event) OVERRIDE {}
  virtual void OnTextInputTypeChanged(
      const ui::TextInputClient* client) OVERRIDE {}
  virtual void OnCaretBoundsChanged(
      const ui::TextInputClient* client) OVERRIDE {}
  virtual void CancelComposition(const ui::TextInputClient* client) OVERRIDE {}
  virtual std::string GetInputLocale() OVERRIDE { return ""; }
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE {
    return base::i18n::UNKNOWN_DIRECTION;
  }
  virtual bool IsActive() OVERRIDE { return true; }
  virtual ui::TextInputType GetTextInputType() const OVERRIDE {
    return ui::TEXT_INPUT_TYPE_NONE;
  }
  virtual bool CanComposeInline() const OVERRIDE {
    return true;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInputMethod);
};

}  // namespace test
}  // namespace ui

#endif  // UI_BASE_TEST_DUMMY_INPUT_METHOD_H_

