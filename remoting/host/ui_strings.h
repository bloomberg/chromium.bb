// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_HOST_UI_STRINGS_H_
#define REMOTING_HOST_UI_STRINGS_H_

#include <string>

// This struct contains localized strings to be displayed in host dialogs.
// For the web-app, these are loaded from the appropriate messages.json
// file when the plugin is created. For remoting_simple_host, they are
// left set to the default (English) values.
//
// Since we don't anticipate having a significant host-side UI presented
// in this way, a namespace containing all available strings should be
// a reasonable way to implement this.

namespace remoting {

struct UiStrings {
  UiStrings();
  ~UiStrings();

  // The direction (left-to-right or right-to-left) for the current language.
  enum Direction { RTL, LTR };
  Direction direction;

  // The product name (Chromoting or Chrome Remote Desktop).
  std::string product_name;

  // The message in the disconnect dialog.
  std::string disconnect_message;

  // The label on the disconnect dialog button, without the keyboard shortcut.
  std::string disconnect_button_text;

  // The label on the disconnect dialog button, with the keyboard shortcut.
  std::string disconnect_button_text_plus_shortcut;

  // The keyboard shortcut for disconnecting clients.
  std::string disconnect_button_shortcut;

  // The confirmation prompt displayed by the continue window.
  std::string continue_prompt;

  // The label on the 'Continue' button of the continue window.
  std::string continue_button_text;

  // The label on the 'Stop Sharing' button of the continue window.
  std::string stop_sharing_button_text;
};

}

#endif // REMOTING_HOST_UI_STRINGS_H_
