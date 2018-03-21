// Copyright (c) 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/app_list_util.h"

#include "ui/app_list/app_list_constants.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/focus/focus_manager.h"

namespace app_list {

bool CanProcessLeftRightKeyTraversal(const ui::KeyEvent& event) {
  if (event.handled() || event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (event.key_code() != ui::VKEY_LEFT && event.key_code() != ui::VKEY_RIGHT)
    return false;

  if (event.IsShiftDown() || event.IsControlDown() || event.IsAltDown())
    return false;

  return true;
}

bool CanProcessUpDownKeyTraversal(const ui::KeyEvent& event) {
  if (event.handled() || event.type() != ui::ET_KEY_PRESSED)
    return false;

  if (event.key_code() != ui::VKEY_UP && event.key_code() != ui::VKEY_DOWN)
    return false;

  if (event.IsShiftDown() || event.IsControlDown() || event.IsAltDown())
    return false;

  return true;
}

bool ProcessLeftRightKeyTraversalForTextfield(views::Textfield* textfield,
                                              const ui::KeyEvent& key_event) {
  DCHECK(CanProcessLeftRightKeyTraversal(key_event));

  const bool move_focus_reverse = base::i18n::IsRTL()
                                      ? key_event.key_code() == ui::VKEY_RIGHT
                                      : key_event.key_code() == ui::VKEY_LEFT;
  if (textfield->text().empty()) {
    textfield->GetFocusManager()->AdvanceFocus(move_focus_reverse);
    return true;
  }

  if (textfield->HasSelection())
    return false;

  if (textfield->GetCursorPosition() != 0 &&
      textfield->GetCursorPosition() != textfield->text().length()) {
    return false;
  }

  // For RTL language, the beginning position of the cursor will be at the right
  // side and it grows towards left as we are typing.
  const bool text_rtl =
      textfield->GetTextDirection() == base::i18n::RIGHT_TO_LEFT;
  const bool cursor_at_beginning = textfield->GetCursorPosition() == 0;
  const bool move_cursor_reverse =
      (text_rtl && key_event.key_code() == ui::VKEY_RIGHT) ||
      (!text_rtl && key_event.key_code() == ui::VKEY_LEFT);

  if ((cursor_at_beginning && !move_cursor_reverse) ||
      (!cursor_at_beginning && move_cursor_reverse)) {
    // Cursor is at either the beginning or the end of the textfield, and it
    // will move inward.
    return false;
  }

  // Move focus outside the textfield.
  textfield->GetFocusManager()->AdvanceFocus(move_focus_reverse);
  return true;
}

int GetPreferredIconDimension(SearchResult* search_result) {
  switch (search_result->display_type()) {
    case ash::SearchResultDisplayType::kRecommendation:  // Falls
                                                         // through.
    case ash::SearchResultDisplayType::kTile:
      return kTileIconSize;
    case ash::SearchResultDisplayType::kList:
      return kListIconSize;
    case ash::SearchResultDisplayType::kNone:
    case ash::SearchResultDisplayType::kCard:
      return 0;
    case ash::SearchResultDisplayType::kLast:
      break;
  }
  NOTREACHED();
  return 0;
}

}  // namespace app_list
