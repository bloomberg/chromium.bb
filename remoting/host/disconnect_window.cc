// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/disconnect_window.h"

namespace remoting {
// TODO(garykac): These strings should be localized.
const char DisconnectWindow::kTitle[] = "Remoting";
const char DisconnectWindow::kSharingWith[] = "Sharing with: ";
const char DisconnectWindow::kDisconnectButton[] = "Disconnect";

const char DisconnectWindow::kDisconnectKeysLinux[] = " (Ctrl+Alt+Esc)";
// For Mac: Ctrl+Option+Esc
// Ctrl: U+2303 = 0xE2 0x8C 0x83 (utf8)
// Option: U+2325 = 0xE2 0x8C 0xA5 (utf8)
// Escape: According to http://en.wikipedia.org/wiki/Esc_key, the ISO 9995
//         standard specifies the U+238B glyph for Escape. Although this
//         doesn't appear to be in common use.
const char DisconnectWindow::kDisconnectKeysMac[] =
    " (\xE2\x8C\x83+\xE2\x8C\xA5+Esc)";
const char DisconnectWindow::kDisconnectKeysWin[] = " (Ctrl+Alt+Esc)";
}
