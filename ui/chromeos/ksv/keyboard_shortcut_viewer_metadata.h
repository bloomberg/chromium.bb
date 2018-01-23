// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_KSV_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
#define UI_CHROMEOS_KSV_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_

#include <vector>

#include "ui/chromeos/ksv/ksv_export.h"

namespace keyboard_shortcut_viewer {

struct KeyboardShortcutItem;

// Returns a list of Ash and Chrome keyboard shortcuts metadata.
KSV_EXPORT const std::vector<KeyboardShortcutItem>&
GetKeyboardShortcutItemList();

}  // namespace keyboard_shortcut_viewer

#endif  // UI_CHROMEOS_KSV_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
