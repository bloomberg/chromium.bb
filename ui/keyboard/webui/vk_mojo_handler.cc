// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/webui/vk_mojo_handler.h"

#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_controller_proxy.h"
#include "ui/keyboard/keyboard_util.h"
#include "ui/keyboard/webui/keyboard.mojom.h"

namespace keyboard {

VKMojoHandler::VKMojoHandler() {
  GetInputMethod()->AddObserver(this);
}

VKMojoHandler::~VKMojoHandler() {
  GetInputMethod()->RemoveObserver(this);
}

ui::InputMethod* VKMojoHandler::GetInputMethod() {
  return KeyboardController::GetInstance()->proxy()->GetInputMethod();
}

void VKMojoHandler::OnConnectionEstablished() {
  OnTextInputStateChanged(GetInputMethod()->GetTextInputClient());
}

void VKMojoHandler::SendKeyEvent(const mojo::String& event_type,
                                 int32_t char_value,
                                 int32_t key_code,
                                 const mojo::String& key_name,
                                 int32_t modifiers) {
  aura::Window* window =
      KeyboardController::GetInstance()->GetContainerWindow();
  std::string type = event_type.To<std::string>();
  std::string name = key_name.To<std::string>();
  keyboard::SendKeyEvent(
      type, char_value, key_code, name, modifiers, window->GetHost());
}

void VKMojoHandler::HideKeyboard() {
  KeyboardController::GetInstance()->HideKeyboard(
      KeyboardController::HIDE_REASON_MANUAL);
}

void VKMojoHandler::OnTextInputTypeChanged(const ui::TextInputClient* client) {
}

void VKMojoHandler::OnFocus() {
}

void VKMojoHandler::OnBlur() {
}

void VKMojoHandler::OnCaretBoundsChanged(const ui::TextInputClient* client) {
}

void VKMojoHandler::OnTextInputStateChanged(
    const ui::TextInputClient* text_client) {
  ui::TextInputType type =
      text_client ? text_client->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE;
  std::string type_name = "none";
  switch (type) {
    case ui::TEXT_INPUT_TYPE_NONE:
      type_name = "none";
      break;

    case ui::TEXT_INPUT_TYPE_PASSWORD:
      type_name = "password";
      break;

    case ui::TEXT_INPUT_TYPE_EMAIL:
      type_name = "email";
      break;

    case ui::TEXT_INPUT_TYPE_NUMBER:
      type_name = "number";
      break;

    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      type_name = "tel";
      break;

    case ui::TEXT_INPUT_TYPE_URL:
      type_name = "url";
      break;

    case ui::TEXT_INPUT_TYPE_DATE:
      type_name = "date";
      break;

    case ui::TEXT_INPUT_TYPE_TEXT:
    case ui::TEXT_INPUT_TYPE_SEARCH:
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TEXT_INPUT_TYPE_MONTH:
    case ui::TEXT_INPUT_TYPE_TIME:
    case ui::TEXT_INPUT_TYPE_WEEK:
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      type_name = "text";
      break;
  }
  client()->OnTextInputTypeChanged(type_name);
}

void VKMojoHandler::OnInputMethodDestroyed(
    const ui::InputMethod* input_method) {
}

void VKMojoHandler::OnShowImeIfNeeded() {
}

}  // namespace keyboard
