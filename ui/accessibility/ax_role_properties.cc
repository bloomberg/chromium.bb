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
    case AX_ROLE_IMAGE_MAP_LINK:
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

}  // namespace ui
