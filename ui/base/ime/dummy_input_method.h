// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_DUMMY_INPUT_METHOD_H_
#define UI_BASE_IME_DUMMY_INPUT_METHOD_H_

#include "ui/base/ime/input_method.h"

namespace ui {

class InputMethodObserver;

class DummyInputMethod : public InputMethod {
 public:
  DummyInputMethod();
  virtual ~DummyInputMethod();

  // InputMethod overrides:
  virtual void SetDelegate(
      internal::InputMethodDelegate* delegate) override;
  virtual void Init(bool focused) override;
  virtual void OnFocus() override;
  virtual void OnBlur() override;
  virtual bool OnUntranslatedIMEMessage(
      const base::NativeEvent& event, NativeEventResult* result) override;
  virtual void SetFocusedTextInputClient(TextInputClient* client) override;
  virtual void DetachTextInputClient(TextInputClient* client) override;
  virtual TextInputClient* GetTextInputClient() const override;
  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) override;
  virtual void OnTextInputTypeChanged(const TextInputClient* client) override;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) override;
  virtual void CancelComposition(const TextInputClient* client) override;
  virtual void OnInputLocaleChanged() override;
  virtual std::string GetInputLocale() override;
  virtual bool IsActive() override;
  virtual TextInputType GetTextInputType() const override;
  virtual TextInputMode GetTextInputMode() const override;
  virtual bool CanComposeInline() const override;
  virtual bool IsCandidatePopupOpen() const override;
  virtual void ShowImeIfNeeded() override;

  virtual void AddObserver(InputMethodObserver* observer) override;
  virtual void RemoveObserver(InputMethodObserver* observer) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DummyInputMethod);
};

}  // namespace ui

#endif  // UI_BASE_IME_DUMMY_INPUT_METHOD_H_
