// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOCK_INPUT_METHOD_H_
#define UI_BASE_IME_MOCK_INPUT_METHOD_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ui_base_export.h"

namespace ui {

class KeyEvent;
class TextInputClient;

// A mock ui::InputMethod implementation for testing. You can get the instance
// of this class as the global input method with calling
// SetUpInputMethodFactoryForTesting() which is declared in
// ui/base/ime/input_method_factory.h
class UI_BASE_EXPORT MockInputMethod : NON_EXPORTED_BASE(public InputMethod) {
 public:
  explicit MockInputMethod(internal::InputMethodDelegate* delegate);
  virtual ~MockInputMethod();

  // Overriden from InputMethod.
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) override;
  virtual void Init(bool focused) override;
  virtual void OnFocus() override;
  virtual void OnBlur() override;
  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) override;
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
  TextInputClient* text_input_client_;
  ObserverList<InputMethodObserver> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethod);
};

}  // namespace ui

#endif  // UI_BASE_IME_MOCK_INPUT_METHOD_H_
