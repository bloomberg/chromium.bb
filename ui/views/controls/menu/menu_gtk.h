// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_MENU_GTK_H_
#define UI_VIEWS_CONTROLS_MENU_MENU_GTK_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/menu/menu.h"

namespace views {

class MenuGtk : public Menu {
 public:
  // Construct a Menu using the specified controller to determine command
  // state.
  // delegate     A Menu::Delegate implementation that provides more
  //              information about the Menu presentation.
  // anchor       An alignment hint for the popup menu.
  // owner        The window that the menu is being brought up relative
  //              to. Not actually used for anything but must not be
  //              NULL.
  MenuGtk(Delegate* d, AnchorPoint anchor, gfx::NativeView owner);
  virtual ~MenuGtk();

  // Overridden from Menu:
  virtual Menu* AddSubMenuWithIcon(int index,
                                   int item_id,
                                   const string16& label,
                                   const SkBitmap& icon) OVERRIDE;
  virtual void AddSeparator(int index) OVERRIDE;
  virtual void EnableMenuItemByID(int item_id, bool enabled) OVERRIDE;
  virtual void EnableMenuItemAt(int index, bool enabled) OVERRIDE;
  virtual void SetMenuLabel(int item_id, const string16& label) OVERRIDE;
  virtual bool SetIcon(const SkBitmap& icon, int item_id) OVERRIDE;
  virtual void RunMenuAt(int x, int y) OVERRIDE;
  virtual void Cancel() OVERRIDE;
  virtual int ItemCount() OVERRIDE;

 protected:
  virtual void AddMenuItemInternal(int index,
                                   int item_id,
                                   const string16& label,
                                   const SkBitmap& icon,
                                   MenuItemType type) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(MenuGtk);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_MENU_GTK_H_
