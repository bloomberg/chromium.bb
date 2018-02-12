// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_CHROMEOS_KSV_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
#define UI_CHROMEOS_KSV_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_

#include <vector>

#include "base/containers/span.h"
#include "base/strings/string16.h"
#include "ui/chromeos/ksv/ksv_export.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace keyboard_shortcut_viewer {

struct KeyboardShortcutItem;
enum class ShortcutCategory;

// Returns a list of Ash and Chrome keyboard shortcuts metadata.
KSV_EXPORT const std::vector<KeyboardShortcutItem>&
GetKeyboardShortcutItemList();

// Returns the categories shown on the side tabs.
base::span<const ShortcutCategory> GetShortcutCategories();

base::string16 GetStringForCategory(ShortcutCategory category);

// Returns the string of a DomeKey for a given VKEY. VKEY needs to be mapped to
// a physical key |dom_code| and then the |dom_code| needs to be mapped to a
// meaning or character of |dom_key| based on the corresponding keyboard layout.
// TODO(https://crbug.com/803502): Get strings for non US keyboard layout.
base::string16 GetStringForKeyboardCode(ui::KeyboardCode key_code);

}  // namespace keyboard_shortcut_viewer

#endif  // UI_CHROMEOS_KSV_KEYBOARD_SHORTCUT_VIEWER_METADATA_H_
