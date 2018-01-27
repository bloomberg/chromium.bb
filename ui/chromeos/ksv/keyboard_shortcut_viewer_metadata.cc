// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/keyboard_shortcut_viewer_metadata.h"

#include "base/logging.h"
#include "base/macros.h"
#include "ui/chromeos/ksv/keyboard_shortcut_item.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/keyboard_codes.h"

namespace keyboard_shortcut_viewer {

namespace {

// Get the keyboard codes for modifiers.
ui::KeyboardCode GetModifierKeyCode(ui::EventFlags modifier) {
  switch (modifier) {
    case ui::EF_CONTROL_DOWN:
      return ui::VKEY_CONTROL;
    case ui::EF_ALT_DOWN:
      return ui::VKEY_LMENU;
    case ui::EF_SHIFT_DOWN:
      return ui::VKEY_SHIFT;
    case ui::EF_COMMAND_DOWN:
      return ui::VKEY_COMMAND;
    default:
      NOTREACHED();
      return ui::VKEY_UNKNOWN;
  }
}

}  // namespace

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

  CR_DEFINE_STATIC_LOCAL(bool, is_initialized, (false));
  // If the item's |shortcut_key_codes| is empty, we need to dynamically
  // populate the keycodes with |accelerator_ids| to construct the shortcut
  // string.
  if (!is_initialized) {
    is_initialized = true;
    for (auto& item : item_list) {
      if (item.shortcut_key_codes.empty()) {
        DCHECK(!item.accelerator_ids.empty());
        // Only use the first |accelerator_id| because the modifiers are the
        // same even if it is a grouped accelerators.
        const AcceleratorId& accelerator_id = item.accelerator_ids[0];
        // Insert |shortcut_key_codes| by the order of CTLR, ALT, SHIFT, SEARCH,
        // and then key, to be consistent with how we describe it in the
        // |shortcut_message_id| associated string template.
        for (auto modifier : {ui::EF_CONTROL_DOWN, ui::EF_ALT_DOWN,
                              ui::EF_SHIFT_DOWN, ui::EF_COMMAND_DOWN}) {
          if (accelerator_id.modifiers & modifier)
            item.shortcut_key_codes.emplace_back(GetModifierKeyCode(modifier));
        }
        // For non grouped accelerators, we need to populate the key as well.
        if (item.accelerator_ids.size() == 1)
          item.shortcut_key_codes.emplace_back(accelerator_id.keycode);
      }
    }
  }

  return item_list;
}

}  // namespace keyboard_shortcut_viewer
