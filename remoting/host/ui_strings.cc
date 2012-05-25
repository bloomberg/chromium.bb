// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ui_strings.h"

#include "base/utf_string_conversions.h"

namespace remoting {

UiStrings::UiStrings() :
    direction(LTR),
    product_name(ASCIIToUTF16("Chromoting")),
    disconnect_message(
        ASCIIToUTF16("Your desktop is currently being shared with $1.")),
    disconnect_button_text(ASCIIToUTF16("Disconnect")),
    // This is the wrong shortcut on Mac OS X, but remoting_simple_host
    // doesn't have a bundle (and hence no dialog) on that platform and
    // the web-app will provide the correct localization for this string.
    disconnect_button_text_plus_shortcut(
#if defined(OS_MACOSX)
        // TODO(sergeyu): Currently the shortcut is disabled because it
        // doesn't work properly on Mac.
        ASCIIToUTF16("Disconnect")
#else  // !defined(OS_MACOSX)
        ASCIIToUTF16("Disconnect (Ctrl+Alt+Esc)")
#endif  // !defined(OS_MACOSX)
                                         ),
    continue_prompt(ASCIIToUTF16(
        "You are currently sharing this machine with another user. "
        "Please confirm that you want to continue sharing.")),
    continue_button_text(ASCIIToUTF16("Continue")),
    stop_sharing_button_text(ASCIIToUTF16("Stop Sharing")) {
}

UiStrings::~UiStrings() { }

}
