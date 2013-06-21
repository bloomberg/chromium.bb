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
#include "ui/aura/window.h"
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

}  // namespace keyboard
