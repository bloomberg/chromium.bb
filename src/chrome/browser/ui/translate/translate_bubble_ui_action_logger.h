// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_UI_ACTION_LOGGER_H_
#define CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_UI_ACTION_LOGGER_H_

namespace translate {

enum TranslateBubbleUiEvent {
  // Update TranslateBubbleUiEvent in enums.xml when making changes.
  // Start with 1 to match existing UMA values: see http://crbug.com/612558
  // The user clicked the advanced option.
  SET_STATE_OPTIONS = 1,

  // The user clicked "Done" and went back from the advanced option.
  LEAVE_STATE_OPTIONS = 2,

  // The user clicked the advanced link.
  // [DEPRECATED] ADVANCED_LINK_CLICKED = 3,

  // The user checked the "always translate" checkbox.
  ALWAYS_TRANSLATE_CHECKED = 4,

  // The user unchecked the "always translate" checkbox.
  ALWAYS_TRANSLATE_UNCHECKED = 5,

  // The user selected "Nope" in the "Options" menu.
  // [DEPRECATED] NOPE_MENU_CLICKED = 6,

  // The user selected "Never translate language" in the "Options" menu.
  NEVER_TRANSLATE_LANGUAGE_MENU_CLICKED = 7,

  // The user selected "Never translate this site" in the "Options" menu.
  NEVER_TRANSLATE_SITE_MENU_CLICKED = 8,

  // The user clicked the "Translate" button.
  TRANSLATE_BUTTON_CLICKED = 9,

  // The user clicked the "Done" button.
  DONE_BUTTON_CLICKED = 10,

  // The user clicked the "Cancel" button.
  CANCEL_BUTTON_CLICKED = 11,

  // The user clicked the "Closed" [X] button.
  CLOSE_BUTTON_CLICKED = 12,

  // The user clicked the "Try Again" button.
  TRY_AGAIN_BUTTON_CLICKED = 13,

  // The user clicked the "Show Original" button.
  SHOW_ORIGINAL_BUTTON_CLICKED = 14,

  // The user clicked the "Settings" link.
  // [DEPRECATED] SETTINGS_LINK_CLICKED = 15,

  // The user changed the "Source language".
  SOURCE_LANGUAGE_MENU_CLICKED = 16,

  // The user changed the "Target language".
  TARGET_LANGUAGE_MENU_CLICKED = 17,

  // The user activated the translate page action icon.
  PAGE_ACTION_ICON_ACTIVATED = 18,

  // The user deactivated the translate page action icon.
  PAGE_ACTION_ICON_DEACTIVATED = 19,

  // The translate bubble was shown to the user.
  BUBBLE_SHOWN = 20,

  // The translate bugbble could not be shown to the user, for various reasons.
  BUBBLE_NOT_SHOWN_WINDOW_NOT_VALID = 21,
  BUBBLE_NOT_SHOWN_WINDOW_MINIMIZED = 22,
  BUBBLE_NOT_SHOWN_WINDOW_NOT_ACTIVE = 23,
  BUBBLE_NOT_SHOWN_WEB_CONTENTS_NOT_ACTIVE = 24,
  BUBBLE_NOT_SHOWN_EDITABLE_FIELD_IS_ACTIVE = 25,

  // The user clicked the advanced menu item.
  ADVANCED_MENU_CLICKED = 26,

  // The user clicked the advanced button.
  // [DEPRECATED] ADVANCED_BUTTON_CLICKED = 27,

  TRANSLATE_BUBBLE_UI_EVENT_MAX
};

// Logs metrics for the user's TranslateBubbleUiEvent |action|.
void ReportUiAction(translate::TranslateBubbleUiEvent action);

}  // namespace translate

#endif  // CHROME_BROWSER_UI_TRANSLATE_TRANSLATE_BUBBLE_UI_ACTION_LOGGER_H_
