// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ui_strings.h"

namespace remoting {

UiStrings::UiStrings() :
    direction(LTR),
    product_name("Chromoting"),
    // The default string doesn't include the user name, so this will be
    // missing for remoting_simple_host
    disconnect_message("Your desktop is currently being shared."),
    disconnect_button_text("Disconnect"),
    // This is the wrong shortcut on Mac OS X, but remoting_simple_host
    // doesn't have a bundle (and hence no dialog) on that platform and
    // the web-app will provide the correct localization for this string.
    disconnect_button_shortcut("Ctrl+Alt+Esc"),
    continue_prompt(
        "You are currently sharing this machine with another user. "
        "Please confirm that you want to continue sharing."),
    continue_button_text("Continue"),
    stop_sharing_button_text("Stop Sharing") {
}

UiStrings::~UiStrings() { }

}
