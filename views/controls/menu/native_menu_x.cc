// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/menu/native_menu_x.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "gfx/canvas_skia.h"
#include "gfx/skia_util.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/menu/submenu_view.h"

namespace views {

NativeMenuX::NativeMenuX(Menu2* menu)
    : model_(menu->model()),
      ALLOW_THIS_IN_INITIALIZER_LIST(root_(new MenuItemView(this))) {
}

NativeMenuX::~NativeMenuX() {
}

// MenuWrapper implementation:
void NativeMenuX::RunMenuAt(const gfx::Point& point, int alignment) {
  UpdateStates();
  root_->RunMenuAt(NULL, NULL, gfx::Rect(point, gfx::Size()),
      alignment == Menu2::ALIGN_TOPLEFT ? MenuItemView::TOPLEFT :
      MenuItemView::TOPRIGHT, true);
}

void NativeMenuX::CancelMenu() {
  NOTIMPLEMENTED();
}

void NativeMenuX::Rebuild() {
  if (SubmenuView* submenu = root_->GetSubmenu()) {
    submenu->RemoveAllChildViews(true);
  }
  AddMenuItemsFromModel(root_.get(), model_);
}

void NativeMenuX::UpdateStates() {
  SubmenuView* submenu = root_->CreateSubmenu();
  UpdateMenuFromModel(submenu, model_);
}

gfx::NativeMenu NativeMenuX::GetNativeMenu() const {
  NOTIMPLEMENTED();
  return NULL;
}

MenuWrapper::MenuAction NativeMenuX::GetMenuAction() const {
  NOTIMPLEMENTED();
  return MENU_ACTION_NONE;
}

void NativeMenuX::AddMenuListener(MenuListener* listener) {
  NOTIMPLEMENTED();
}

void NativeMenuX::RemoveMenuListener(MenuListener* listener) {
  NOTIMPLEMENTED();
}

void NativeMenuX::SetMinimumWidth(int width) {
  NOTIMPLEMENTED();
}

// MenuDelegate implementation

bool NativeMenuX::IsItemChecked(int cmd) const {
  int index;
  menus::MenuModel* model = model_;
  if (!menus::MenuModel::GetModelAndIndexForCommandId(cmd, &model, &index))
    return false;
  return model->IsItemCheckedAt(index);
}

bool NativeMenuX::IsCommandEnabled(int cmd) const {
  int index;
  menus::MenuModel* model = model_;
  if (!menus::MenuModel::GetModelAndIndexForCommandId(cmd, &model, &index))
    return false;
  return model->IsEnabledAt(index);
}

void NativeMenuX::ExecuteCommand(int cmd) {
  int index;
  menus::MenuModel* model = model_;
  if (!menus::MenuModel::GetModelAndIndexForCommandId(cmd, &model, &index))
    return;
  model->ActivatedAt(index);
}

bool NativeMenuX::GetAccelerator(int id, views::Accelerator* accelerator) {
  int index;
  menus::MenuModel* model = model_;
  if (!menus::MenuModel::GetModelAndIndexForCommandId(id, &model, &index))
    return false;

  menus::Accelerator menu_accelerator;
  if (!model->GetAcceleratorAt(index, &menu_accelerator))
    return false;

  *accelerator = views::Accelerator(menu_accelerator.GetKeyCode(),
                                    menu_accelerator.modifiers());
  return true;
}

// private
void NativeMenuX::AddMenuItemsFromModel(MenuItemView* parent,
                                        menus::MenuModel* model) {
  for (int i = 0; i < model->GetItemCount(); ++i) {
    int index = i + model->GetFirstItemIndex(NULL);
    MenuItemView* child = parent->AppendMenuItemFromModel(model, index,
        model->GetCommandIdAt(index));

    if (child && child->GetType() == MenuItemView::SUBMENU) {
      AddMenuItemsFromModel(child, model->GetSubmenuModelAt(index));
    }
  }
}

void NativeMenuX::UpdateMenuFromModel(SubmenuView* menu,
                                      menus::MenuModel* model) {
  for (int i = 0, sep = 0; i < model->GetItemCount(); ++i) {
    int index = i + model->GetFirstItemIndex(NULL);
    if (model->GetTypeAt(index) == menus::MenuModel::TYPE_SEPARATOR) {
      ++sep;
      continue;
    }

    // The submenu excludes the separators when counting the menu-items
    // in it. So exclude the number of separators to get the correct index.
    MenuItemView* mitem = menu->GetMenuItemAt(index - sep);
    mitem->SetVisible(model->IsVisibleAt(index));
    mitem->SetEnabled(model->IsEnabledAt(index));
    if (model->IsLabelDynamicAt(index)) {
      mitem->SetTitle(UTF16ToWide(model->GetLabelAt(index)));
    }

    SkBitmap icon;
    if (model->GetIconAt(index, &icon)) {
      mitem->SetIcon(icon);
    }

    if (model->GetTypeAt(index) == menus::MenuModel::TYPE_SUBMENU) {
      DCHECK(mitem->HasSubmenu());
      UpdateMenuFromModel(mitem->GetSubmenu(), model->GetSubmenuModelAt(index));
    }
  }
}

// MenuWrapper, public:

#if !defined(OS_CHROMEOS)
// static
MenuWrapper* MenuWrapper::CreateWrapper(Menu2* menu) {
  return new NativeMenuX(menu);
}
#endif

}  // namespace views
