// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/ui_strings.h"

#include "base/utf_string_conversions.h"

namespace remoting {

UiStrings::UiStrings() :
    direction(LTR),
    product_name(ASCIIToUTF16("Chromoting")),
    // Audio is currently only supported for Me2Me, and not on Mac. However,
    // for IT2Me, these strings are replaced during l10n, so it's fine to
    // hard-code a mention of audio here.
#if defined(OS_MACOSX)
    disconnect_message(
        ASCIIToUTF16("Your desktop is currently shared with $1.")),
#else
    disconnect_message(ASCIIToUTF16(
        "Your desktop and any audio output are currently shared with $1.")),
#endif
    disconnect_button_text(ASCIIToUTF16("Disconnect")),
    continue_prompt(ASCIIToUTF16(
        "You are currently sharing this machine with another user. "
        "Please confirm that you want to continue sharing.")),
    continue_button_text(ASCIIToUTF16("Continue")),
    stop_sharing_button_text(ASCIIToUTF16("Stop Sharing")) {
}

UiStrings::~UiStrings() {}

}  // namespace remoting
