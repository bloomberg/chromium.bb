// Copyright (c) 2009 The Chromium Authors. All rights reserved. Use of this
// source code is governed by a BSD-style license that can be found in the
// LICENSE file.

#ifndef CONTROLS_MENU_VIEWS_MENU_2_H_
#define CONTROLS_MENU_VIEWS_MENU_2_H_

#include "app/menus/menu_model.h"
#include "base/scoped_ptr.h"
#include "views/controls/menu/menu_wrapper.h"

namespace gfx {
class Point;
}

namespace views {

class NativeMenuGtk;

// A menu. Populated from a model, and relies on a delegate to execute commands.
class Menu2 {
 public:
  explicit Menu2(menus::MenuModel* model);
  virtual ~Menu2() {}

  // How the menu is aligned relative to the point it is shown at.
  enum Alignment {
    ALIGN_TOPLEFT,
    ALIGN_TOPRIGHT
  };

  // Runs the menu at the specified point. This method blocks until done.
  // RunContextMenuAt is the same, but the alignment is the default for a
  // context menu.
  void RunMenuAt(const gfx::Point& point, Alignment alignment);
  void RunContextMenuAt(const gfx::Point& point);

  // Cancels the active menu.
  void CancelMenu();

  // Called when the model supplying data to this menu has changed, and the menu
  // must be rebuilt.
  void Rebuild();

  // Called when the states of the menu items in the menu should be refreshed
  // from the model.
  void UpdateStates();

  // For submenus.
  gfx::NativeMenu GetNativeMenu() const;

  // Accessors.
  menus::MenuModel* model() const { return model_; }

 private:
  friend class NativeMenuGtk;

  menus::MenuModel* model_;

  // The object that actually implements the menu.
  scoped_ptr<MenuWrapper> wrapper_;

  DISALLOW_COPY_AND_ASSIGN(Menu2);
};

}  // namespace views

#endif  // CONTROLS_MENU_VIEWS_MENU_2_H_
