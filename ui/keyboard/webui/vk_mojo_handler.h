// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_WEBUI_VK_MOJO_HANDLER_H_
#define UI_KEYBOARD_WEBUI_VK_MOJO_HANDLER_H_

#include "base/macros.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/keyboard/webui/keyboard.mojom.h"

namespace keyboard {

class VKMojoHandler : public mojo::InterfaceImpl<KeyboardUIHandlerMojo>,
                      public ui::InputMethodObserver {
 public:
  VKMojoHandler();
  virtual ~VKMojoHandler();

 private:
  ui::InputMethod* GetInputMethod();

  // mojo::InterfaceImpl<>:
  virtual void OnConnectionEstablished() override;

  // KeyboardUIHandlerMojo:
  virtual void SendKeyEvent(const mojo::String& event_type,
                            int32_t char_value,
                            int32_t key_code,
                            const mojo::String& key_name,
                            int32_t modifiers) override;
  virtual void HideKeyboard() override;

  // ui::InputMethodObserver:
  virtual void OnTextInputTypeChanged(
      const ui::TextInputClient* client) override;
  virtual void OnFocus() override;
  virtual void OnBlur() override;
  virtual void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  virtual void OnTextInputStateChanged(
      const ui::TextInputClient* text_client) override;
  virtual void OnInputMethodDestroyed(
      const ui::InputMethod* input_method) override;
  virtual void OnShowImeIfNeeded() override;

  DISALLOW_COPY_AND_ASSIGN(VKMojoHandler);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_WEBUI_VK_MOJO_HANDLER_H_
