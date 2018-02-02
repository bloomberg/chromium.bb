// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/chromeos/ksv/keyboard_shortcut_viewer_metadata.h"

#include "base/logging.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/events/keyboard_layout_util.h"
#include "ui/chromeos/ksv/keyboard_shortcut_item.h"
#include "ui/chromeos/strings/grit/ui_chromeos_strings.h"
#include "ui/events/event_constants.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/keycodes/dom/keycode_converter.h"
#include "ui/events/keycodes/keyboard_code_conversion.h"

namespace keyboard_shortcut_viewer {

namespace {

// This order is significant as it determines the order in which the categories
// will be displayed in the view.
constexpr ShortcutCategory kCategoryDisplayOrder[] = {
    ShortcutCategory::kPopular,        ShortcutCategory::kTabAndWindow,
    ShortcutCategory::kPageAndBrowser, ShortcutCategory::kSystemAndDisplay,
    ShortcutCategory::kTextEditing,    ShortcutCategory::kAccessibility};

// Gets the keyboard codes for modifiers.
ui::KeyboardCode GetKeyCodeForModifier(ui::EventFlags modifier) {
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

// Provides I18n string for key codes which have no mapping to a meaningful
// description or they require a special one we explicitly specify. For example,
// ui::VKEY_COMMAND could return a string "Meta", but we want to display it as
// "Search" or "Launcher".
base::Optional<base::string16> GetSpecialStringForKeyboardCode(
    ui::KeyboardCode key_code) {
  int msg_id = 0;
  switch (key_code) {
    case ui::VKEY_CONTROL:
      msg_id = IDS_KSV_MODIFIER_CONTROL;
      break;
    case ui::VKEY_LMENU:
      msg_id = IDS_KSV_MODIFIER_ALT;
      break;
    case ui::VKEY_SHIFT:
      msg_id = IDS_KSV_MODIFIER_SHIFT;
      break;
    case ui::VKEY_COMMAND:
      msg_id = ui::DeviceUsesKeyboardLayout2() ? IDS_KSV_MODIFIER_LAUNCHER
                                               : IDS_KSV_MODIFIER_SEARCH;
      break;
    default:
      return base::nullopt;
  }
  return l10n_util::GetStringUTF16(msg_id);
}

}  // namespace

base::span<const ShortcutCategory> GetShortcutCategories() {
  return base::span<const ShortcutCategory>(kCategoryDisplayOrder);
}

base::string16 GetStringForCategory(ShortcutCategory category) {
  int msg_id = 0;
  switch (category) {
    case ShortcutCategory::kPopular:
      msg_id = IDS_KSV_CATEGORY_POPULAR;
      break;
    case ShortcutCategory::kTabAndWindow:
      msg_id = IDS_KSV_CATEGORY_TAB_WINDOW;
      break;
    case ShortcutCategory::kPageAndBrowser:
      msg_id = IDS_KSV_CATEGORY_PAGE_BROWSER;
      break;
    case ShortcutCategory::kSystemAndDisplay:
      msg_id = IDS_KSV_CATEGORY_SYSTEM_DISPLAY;
      break;
    case ShortcutCategory::kTextEditing:
      msg_id = IDS_KSV_CATEGORY_TEXT_EDITING;
      break;
    case ShortcutCategory::kAccessibility:
      msg_id = IDS_KSV_CATEGORY_ACCESSIBILITY;
      break;
    default:
      NOTREACHED();
      return base::string16();
  }
  return l10n_util::GetStringUTF16(msg_id);
}

base::string16 GetStringForKeyboardCode(ui::KeyboardCode key_code) {
  const base::Optional<base::string16> key_label =
      GetSpecialStringForKeyboardCode(key_code);
  if (key_label)
    return key_label.value();

  ui::DomCode dom_code = ui::UsLayoutKeyboardCodeToDomCode(key_code);
  ui::DomKey dom_key;
  ui::KeyboardCode keycode_ignored;
  const bool has_mapping = ui::DomCodeToUsLayoutDomKey(
      dom_code, 0 /* flags */, &dom_key, &keycode_ignored);
  DCHECK(has_mapping);
  return base::UTF8ToUTF16(ui::KeycodeConverter::DomKeyToKeyString(dom_key));
}

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
          if (accelerator_id.modifiers & modifier) {
            item.shortcut_key_codes.emplace_back(
                GetKeyCodeForModifier(modifier));
          }
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
