// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_ui_controller.h"

#include "content/public/browser/browser_context.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "content/public/browser/web_ui_data_source.h"
#include "grit/keyboard_resources.h"
#include "ui/keyboard/keyboard_constants.h"
#include "ui/keyboard/keyboard_ui_handler.h"

namespace {

content::WebUIDataSource* CreateKeyboardUIDataSource() {
  content::WebUIDataSource* source =
      content::WebUIDataSource::Create(keyboard::kKeyboardWebUIHost);

  source->SetDefaultResource(IDR_KEYBOARD_WEBUI_INDEX);
  source->AddResourcePath("elements/kb-altkey.html",
                          IDR_KEYBOARD_ELEMENTS_ALTKEY);
  source->AddResourcePath("elements/kb-altkey-container.html",
                          IDR_KEYBOARD_ELEMENTS_ALTKEY_CONTAINER);
  source->AddResourcePath("elements/kb-altkey-data.html",
                          IDR_KEYBOARD_ELEMENTS_ALTKEY_DATA);
  source->AddResourcePath("elements/kb-altkey-set.html",
                          IDR_KEYBOARD_ELEMENTS_ALTKEY_SET);
  source->AddResourcePath("elements/kb-key.html", IDR_KEYBOARD_ELEMENTS_KEY);
  source->AddResourcePath("elements/kb-key-base.html",
                          IDR_KEYBOARD_ELEMENTS_KEY_BASE);
  source->AddResourcePath("elements/kb-key-codes.html",
                          IDR_KEYBOARD_ELEMENTS_KEY_CODES);
  source->AddResourcePath("elements/kb-keyboard.html",
                          IDR_KEYBOARD_ELEMENTS_KEYBOARD);
  source->AddResourcePath("elements/kb-keyset.html",
                          IDR_KEYBOARD_ELEMENTS_KEYSET);
  source->AddResourcePath("elements/kb-row.html", IDR_KEYBOARD_ELEMENTS_ROW);
  source->AddResourcePath("elements/kb-shift-key.html",
                          IDR_KEYBOARD_ELEMENTS_SHIFT_KEY);
  source->AddResourcePath("images/back.svg",
                          IDR_KEYBOARD_IMAGES_BACK);
  source->AddResourcePath("images/brightness-down.svg",
                          IDR_KEYBOARD_IMAGES_BRIGHTNESS_DOWN);
  source->AddResourcePath("images/brightness-up.svg",
                          IDR_KEYBOARD_IMAGES_BRIGHTNESS_UP);
  source->AddResourcePath("images/change-window.svg",
                          IDR_KEYBOARD_IMAGES_CHANGE_WINDOW);
  source->AddResourcePath("images/down.svg",
                          IDR_KEYBOARD_IMAGES_DOWN);
  source->AddResourcePath("images/forward.svg",
                          IDR_KEYBOARD_IMAGES_FORWARD);
  source->AddResourcePath("images/fullscreen.svg",
                          IDR_KEYBOARD_IMAGES_FULLSCREEN);
  source->AddResourcePath("images/left.svg",
                          IDR_KEYBOARD_IMAGES_LEFT);
  source->AddResourcePath("images/microphone.svg",
                          IDR_KEYBOARD_IMAGES_MICROPHONE);
  source->AddResourcePath("images/microphone-green.svg",
                          IDR_KEYBOARD_IMAGES_MICROPHONE_GREEN);
  source->AddResourcePath("images/mute.svg",
                          IDR_KEYBOARD_IMAGES_MUTE);
  source->AddResourcePath("images/reload.svg",
                          IDR_KEYBOARD_IMAGES_RELOAD);
  source->AddResourcePath("images/right.svg",
                          IDR_KEYBOARD_IMAGES_RIGHT);
  source->AddResourcePath("images/shutdown.svg",
                          IDR_KEYBOARD_IMAGES_SHUTDOWN);
  source->AddResourcePath("images/up.svg",
                          IDR_KEYBOARD_IMAGES_UP);
  source->AddResourcePath("images/volume-down.svg",
                          IDR_KEYBOARD_IMAGES_VOLUME_DOWN);
  source->AddResourcePath("images/volume-up.svg",
                          IDR_KEYBOARD_IMAGES_VOLUME_UP);
  source->AddResourcePath("layouts/latin-accents.js",
                          IDR_KEYBOARD_LAYOUTS_LATIN_ACCENTS);
  source->AddResourcePath("layouts/numeric.html", IDR_KEYBOARD_LAYOUTS_NUMERIC);
  source->AddResourcePath("layouts/system-qwerty.html",
                          IDR_KEYBOARD_LAYOUTS_SYSTEM_QWERTY);
  source->AddResourcePath("main.js", IDR_KEYBOARD_MAIN_JS);
  source->AddResourcePath("polymer.min.js", IDR_KEYBOARD_POLYMER);
  source->AddResourcePath("voice_input.js", IDR_KEYBOARD_VOICE_INPUT_JS);

  // These files are specific to the WebUI version
  source->AddResourcePath("api_adapter.js", IDR_KEYBOARD_WEBUI_API_ADAPTER_JS);
  source->AddResourcePath("constants.js", IDR_KEYBOARD_WEBUI_CONSTANTS_JS);
  source->AddResourcePath("layouts/qwerty.html", IDR_KEYBOARD_WEBUI_QWERTY);
  source->AddResourcePath("main.css", IDR_KEYBOARD_WEBUI_MAIN_CSS);

  return source;
}

}  // namespace

namespace keyboard {

KeyboardUIController::KeyboardUIController(content::WebUI* web_ui)
    : WebUIController(web_ui) {
  content::BrowserContext* browser_context =
      web_ui->GetWebContents()->GetBrowserContext();
  web_ui->AddMessageHandler(new KeyboardUIHandler());
  content::WebUIDataSource::Add(
      browser_context,
      CreateKeyboardUIDataSource());
}

KeyboardUIController::~KeyboardUIController() {}

}  // namespace keyboard
