// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/models/menu_model.h"

namespace ui {

int MenuModel::GetFirstItemIndex(gfx::NativeMenu native_menu) const {
  return 0;
}

bool MenuModel::IsVisibleAt(int index) const {
  return true;
}

bool MenuModel::GetModelAndIndexForCommandId(int command_id,
                                             MenuModel** model, int* index) {
  int item_count = (*model)->GetItemCount();
  for (int i = 0; i < item_count; ++i) {
    if ((*model)->GetTypeAt(i) == TYPE_SUBMENU) {
      MenuModel* submenu_model = (*model)->GetSubmenuModelAt(i);
      if (GetModelAndIndexForCommandId(command_id, &submenu_model, index)) {
        *model = submenu_model;
        return true;
      }
    }
    if ((*model)->GetCommandIdAt(i) == command_id) {
      *index = i;
      return true;
    }
  }
  return false;
}

const gfx::Font* MenuModel::GetLabelFontAt(int index) const {
  return NULL;
}

// Default implementation ignores the disposition.
void MenuModel::ActivatedAtWithDisposition(int index, int disposition) {
  ActivatedAt(index);
}

}  // namespace ui
