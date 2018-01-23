// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/keyboard_shortcut_viewer_metadata.h"

#include "base/macros.h"
#include "ui/chromeos/ksv/keyboard_shortcut_item.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/events/event_constants.h"

namespace keyboard_shortcut_viewer {

const std::vector<KeyboardShortcutItem>& GetKeyboardShortcutItemList() {
  CR_DEFINE_STATIC_LOCAL(
      std::vector<KeyboardShortcutItem>, item_list,
      // TODO(crbug.com/793997): These are two examples how we organize the
      // metadata. Full metadata will be provided.
      ({{// |categories|
         {ShortcutCategory::kPopular, ShortcutCategory::kSystemAndDisplay},
         IDS_KSV_DESCRIPTION_LOCK_SCREEN,
         IDS_KSV_SHORTCUT_ONE_MODIFIER_ONE_KEY,
         // |accelerator_ids|
         {{ui::VKEY_L, ui::EF_COMMAND_DOWN}}},

        // Some accelerators are grouped into one metadata for the
        // KeyboardShortcutViewer due to the similarity. E.g. the shortcut for
        // "Change screen resolution" is "Ctrl + Shift and + or -", which groups
        // two accelerators.
        {// |categories|
         {ShortcutCategory::kSystemAndDisplay},
         IDS_KSV_DESCRIPTION_CHANGE_SCREEN_RESOLUTION,
         IDS_KSV_SHORTCUT_CHANGE_SCREEN_RESOLUTION,
         // |accelerator_ids|
         {{ui::VKEY_OEM_MINUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN},
          {ui::VKEY_OEM_PLUS, ui::EF_CONTROL_DOWN | ui::EF_SHIFT_DOWN}}}}));
  return item_list;
}

}  // namespace keyboard_shortcut_viewer
