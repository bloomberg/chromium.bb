// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_ui_handler.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/values.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/keyboard/keyboard_controller.h"
#include "ui/keyboard/keyboard_util.h"

namespace keyboard {

KeyboardUIHandler::KeyboardUIHandler() {
}

KeyboardUIHandler::~KeyboardUIHandler() {
}

void KeyboardUIHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback(
      "insertText",
      base::Bind(&KeyboardUIHandler::HandleInsertTextMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "getInputContext",
      base::Bind(&KeyboardUIHandler::HandleGetInputContextMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "sendKeyEvent",
      base::Bind(&KeyboardUIHandler::HandleSendKeyEventMessage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback(
      "hideKeyboard",
      base::Bind(&KeyboardUIHandler::HandleHideKeyboard,
                 base::Unretained(this)));
}

void KeyboardUIHandler::HandleInsertTextMessage(const base::ListValue* args) {
  string16 text;
  if (!args->GetString(0, &text)) {
    LOG(ERROR) << "insertText failed: bad argument";
    return;
  }

  aura::RootWindow* root_window =
      web_ui()->GetWebContents()->GetView()->GetNativeView()->GetRootWindow();
  if (!root_window) {
    LOG(ERROR) << "insertText failed: no root window";
    return;
  }

  if (!keyboard::InsertText(text, root_window))
    LOG(ERROR) << "insertText failed";
}

void KeyboardUIHandler::HandleGetInputContextMessage(
    const base::ListValue* args) {
  int request_id;
  if (!args->GetInteger(0, &request_id)) {
    LOG(ERROR) << "getInputContext failed: bad argument";
    return;
  }
  base::DictionaryValue results;
  results.SetInteger("requestId", request_id);

  aura::RootWindow* root_window =
      web_ui()->GetWebContents()->GetView()->GetNativeView()->GetRootWindow();
  if (!root_window) {
    LOG(ERROR) << "getInputContext failed: no root window";
    return;
  }
  ui::InputMethod* input_method =
      root_window->GetProperty(aura::client::kRootWindowInputMethodKey);
  if (!input_method) {
    LOG(ERROR) << "getInputContext failed: no input method";
    return;
  }

  ui::TextInputClient* tic = input_method->GetTextInputClient();
  results.SetInteger("type",
                     tic ? tic->GetTextInputType() : ui::TEXT_INPUT_TYPE_NONE);

  web_ui()->CallJavascriptFunction("GetInputContextCallback",
                                   results);
}

void KeyboardUIHandler::HandleSendKeyEventMessage(
    const base::ListValue* args) {
  const base::DictionaryValue* params = NULL;
  std::string type;
  int char_value;
  int key_code;
  bool shift_modifier;

  if (!args->GetDictionary(0, &params) ||
      !params->GetString("type", &type) ||
      !params->GetInteger("charValue", &char_value) ||
      !params->GetInteger("keyCode", &key_code) ||
      !params->GetBoolean("shiftKey", &shift_modifier)) {
    LOG(ERROR) << "SendKeyEvent failed: bad argument";
    return;
  }

  aura::RootWindow* root_window =
      web_ui()->GetWebContents()->GetView()->GetNativeView()->GetRootWindow();
  if (!root_window) {
    LOG(ERROR) << "sendKeyEvent failed: no root window";
    return;
  }

  if (!keyboard::SendKeyEvent(type,
                              char_value,
                              key_code,
                              shift_modifier,
                              root_window)) {
    LOG(ERROR) << "sendKeyEvent failed";
  }
}

void KeyboardUIHandler::HandleHideKeyboard(const base::ListValue* args) {
  // TODO(stevet): Call into the keyboard controller to hide the keyboard
  // directly.
  NOTIMPLEMENTED();
  return;
}

}  // namespace keyboard
