// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_VIEWS_H_
#define UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_VIEWS_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "ui/views/controls/menu/menu_delegate.h"
#include "ui/views/controls/menu/menu_wrapper.h"

namespace ui {
class MenuModel;
}

namespace views {

class MenuItemView;
class MenuRunner;

// A views implementation of MenuWrapper, used currently for touchui.
class NativeMenuViews : public MenuWrapper,
                        public MenuDelegate {
 public:
  explicit NativeMenuViews(Menu2* menu);
  virtual ~NativeMenuViews();

  // Overridden from MenuWrapper:
  virtual void RunMenuAt(const gfx::Point& point, int alignment) OVERRIDE;
  virtual void CancelMenu() OVERRIDE;
  virtual void Rebuild() OVERRIDE;
  virtual void UpdateStates() OVERRIDE;
  virtual gfx::NativeMenu GetNativeMenu() const OVERRIDE;
  virtual MenuAction GetMenuAction() const OVERRIDE;
  virtual void AddMenuListener(MenuListener* listener) OVERRIDE;
  virtual void RemoveMenuListener(MenuListener* listener) OVERRIDE;
  virtual void SetMinimumWidth(int width) OVERRIDE;

  // Overridden from MenuDelegate:
  virtual bool IsItemChecked(int id) const OVERRIDE;
  virtual bool IsCommandEnabled(int id) const OVERRIDE;
  virtual void ExecuteCommand(int id) OVERRIDE;
  virtual bool GetAccelerator(int id, ui::Accelerator* accelerator) OVERRIDE;

 private:
  void AddMenuItemsFromModel(MenuItemView* parent, ui::MenuModel* model);
  void UpdateMenuFromModel(SubmenuView* menu, ui::MenuModel* model);

  // The attached model and delegate. Does not assume ownership.
  ui::MenuModel* model_;
  MenuItemView* root_;
  scoped_ptr<MenuRunner> menu_runner_;

  DISALLOW_COPY_AND_ASSIGN(NativeMenuViews);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_MENU_NATIVE_MENU_VIEWS_H_
