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
  ~VKMojoHandler() override;

 private:
  ui::InputMethod* GetInputMethod();

  // mojo::InterfaceImpl<>:
  void OnConnectionEstablished() override;

  // KeyboardUIHandlerMojo:
  void SendKeyEvent(const mojo::String& event_type,
                    int32_t char_value,
                    int32_t key_code,
                    const mojo::String& key_name,
                    int32_t modifiers) override;
  void HideKeyboard() override;

  // ui::InputMethodObserver:
  void OnTextInputTypeChanged(const ui::TextInputClient* client) override;
  void OnFocus() override;
  void OnBlur() override;
  void OnCaretBoundsChanged(const ui::TextInputClient* client) override;
  void OnTextInputStateChanged(const ui::TextInputClient* text_client) override;
  void OnInputMethodDestroyed(const ui::InputMethod* input_method) override;
  void OnShowImeIfNeeded() override;

  DISALLOW_COPY_AND_ASSIGN(VKMojoHandler);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_WEBUI_VK_MOJO_HANDLER_H_
