// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_MOCK_INPUT_METHOD_H_
#define UI_BASE_IME_MOCK_INPUT_METHOD_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ui_export.h"

namespace ui {

class TextInputClient;

// A mock InputMethod implementation for testing classes that use the
// ui::InputMethod interface such as aura::DesktopHost.
class UI_EXPORT MockInputMethod : public InputMethod {
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
  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE;
  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE;
  virtual void CancelComposition(const TextInputClient* client) OVERRIDE;
  virtual std::string GetInputLocale() OVERRIDE;
  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE;
  virtual bool IsActive() OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;

  // If called, the next key press will not generate a Char event. Instead, it
  // will generate the VKEY_PROCESSKEY RawKeyDown event.
  void ConsumeNextKey();

  // Sends VKEY_PROCESSKEY.
  void SendFakeProcessKeyEvent(bool pressed, int flags) const;

 private:
  internal::InputMethodDelegate* delegate_;
  TextInputClient* text_input_client_;
  bool consume_next_key_;

  DISALLOW_COPY_AND_ASSIGN(MockInputMethod);
};

}  // namespace ui

#endif  // UI_BASE_IME_MOCK_INPUT_METHOD_H_
