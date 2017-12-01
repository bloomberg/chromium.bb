// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/accessibility/ax_role_properties.h"

namespace ui {

bool IsRoleClickable(AXRole role) {
  switch (role) {
    case AX_ROLE_BUTTON:
    case AX_ROLE_CHECK_BOX:
    case AX_ROLE_COLOR_WELL:
    case AX_ROLE_DISCLOSURE_TRIANGLE:
    case AX_ROLE_LINK:
    case AX_ROLE_LIST_BOX_OPTION:
    case AX_ROLE_MENU_BUTTON:
    case AX_ROLE_MENU_ITEM:
    case AX_ROLE_MENU_ITEM_CHECK_BOX:
    case AX_ROLE_MENU_ITEM_RADIO:
    case AX_ROLE_MENU_LIST_OPTION:
    case AX_ROLE_MENU_LIST_POPUP:
    case AX_ROLE_POP_UP_BUTTON:
    case AX_ROLE_RADIO_BUTTON:
    case AX_ROLE_SWITCH:
    case AX_ROLE_TAB:
    case AX_ROLE_TOGGLE_BUTTON:
      return true;
    default:
      return false;
  }
}

bool IsDocument(ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_ROOT_WEB_AREA:
    case ui::AX_ROLE_WEB_AREA:
      return true;
    default:
      return false;
  }
}

bool IsCellOrTableHeaderRole(ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_CELL:
    case ui::AX_ROLE_COLUMN_HEADER:
    case ui::AX_ROLE_ROW_HEADER:
      return true;
    default:
      return false;
  }
}

bool IsTableLikeRole(ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_TABLE:
    case ui::AX_ROLE_GRID:
    case ui::AX_ROLE_TREE_GRID:
      return true;
    default:
      return false;
  }
}

bool IsContainerWithSelectableChildrenRole(ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_COMBO_BOX_GROUPING:
    case ui::AX_ROLE_COMBO_BOX_MENU_BUTTON:
    case ui::AX_ROLE_GRID:
    case ui::AX_ROLE_LIST_BOX:
    case ui::AX_ROLE_MENU:
    case ui::AX_ROLE_MENU_BAR:
    case ui::AX_ROLE_RADIO_GROUP:
    case ui::AX_ROLE_TAB_LIST:
    case ui::AX_ROLE_TOOLBAR:
    case ui::AX_ROLE_TREE:
    case ui::AX_ROLE_TREE_GRID:
      return true;
    default:
      return false;
  }
}

bool IsRowContainer(ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_TREE:
    case ui::AX_ROLE_TREE_GRID:
    case ui::AX_ROLE_GRID:
    case ui::AX_ROLE_TABLE:
      return true;
    default:
      return false;
  }
}

bool IsControl(ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_BUTTON:
    case ui::AX_ROLE_CHECK_BOX:
    case ui::AX_ROLE_COLOR_WELL:
    case ui::AX_ROLE_COMBO_BOX_MENU_BUTTON:
    case ui::AX_ROLE_DISCLOSURE_TRIANGLE:
    case ui::AX_ROLE_LIST_BOX:
    case ui::AX_ROLE_MENU:
    case ui::AX_ROLE_MENU_BAR:
    case ui::AX_ROLE_MENU_BUTTON:
    case ui::AX_ROLE_MENU_ITEM:
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
    case ui::AX_ROLE_MENU_ITEM_RADIO:
    case ui::AX_ROLE_MENU_LIST_OPTION:
    case ui::AX_ROLE_MENU_LIST_POPUP:
    case ui::AX_ROLE_POP_UP_BUTTON:
    case ui::AX_ROLE_RADIO_BUTTON:
    case ui::AX_ROLE_SCROLL_BAR:
    case ui::AX_ROLE_SEARCH_BOX:
    case ui::AX_ROLE_SLIDER:
    case ui::AX_ROLE_SPIN_BUTTON:
    case ui::AX_ROLE_SWITCH:
    case ui::AX_ROLE_TAB:
    case ui::AX_ROLE_TEXT_FIELD:
    case ui::AX_ROLE_TEXT_FIELD_WITH_COMBO_BOX:
    case ui::AX_ROLE_TOGGLE_BUTTON:
    case ui::AX_ROLE_TREE:
      return true;
    default:
      return false;
  }
}

bool IsMenuRelated(ui::AXRole role) {
  switch (role) {
    case ui::AX_ROLE_MENU:
    case ui::AX_ROLE_MENU_BAR:
    case ui::AX_ROLE_MENU_BUTTON:
    case ui::AX_ROLE_MENU_ITEM:
    case ui::AX_ROLE_MENU_ITEM_CHECK_BOX:
    case ui::AX_ROLE_MENU_ITEM_RADIO:
    case ui::AX_ROLE_MENU_LIST_OPTION:
    case ui::AX_ROLE_MENU_LIST_POPUP:
      return true;
    default:
      return false;
  }
}

}  // namespace ui
