// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/menu/native_menu_views.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/menu/menu_2.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/menu_listener.h"
#include "ui/views/controls/menu/menu_runner.h"
#include "ui/views/controls/menu/submenu_view.h"

namespace views {

NativeMenuViews::NativeMenuViews(Menu2* menu)
    : model_(menu->model()),
      ALLOW_THIS_IN_INITIALIZER_LIST(root_(new MenuItemView(this))),
      menu_runner_(new MenuRunner(root_)) {
}

NativeMenuViews::~NativeMenuViews() {
}

// MenuWrapper implementation:
void NativeMenuViews::RunMenuAt(const gfx::Point& point, int alignment) {
  // TODO: this should really return the value from MenuRunner.
  UpdateStates();
  if (menu_runner_->RunMenuAt(NULL, NULL, gfx::Rect(point, gfx::Size()),
          alignment == Menu2::ALIGN_TOPLEFT ? MenuItemView::TOPLEFT :
          MenuItemView::TOPRIGHT, MenuRunner::HAS_MNEMONICS) ==
      MenuRunner::MENU_DELETED)
    return;
}

void NativeMenuViews::CancelMenu() {
  NOTIMPLEMENTED();
}

void NativeMenuViews::Rebuild() {
  if (SubmenuView* submenu = root_->GetSubmenu())
    submenu->RemoveAllChildViews(true);
  AddMenuItemsFromModel(root_, model_);
}

void NativeMenuViews::UpdateStates() {
  SubmenuView* submenu = root_->CreateSubmenu();
  UpdateMenuFromModel(submenu, model_);
}

gfx::NativeMenu NativeMenuViews::GetNativeMenu() const {
  NOTIMPLEMENTED();
  return NULL;
}

MenuWrapper::MenuAction NativeMenuViews::GetMenuAction() const {
  NOTIMPLEMENTED();
  return MENU_ACTION_NONE;
}

void NativeMenuViews::AddMenuListener(MenuListener* listener) {
  NOTIMPLEMENTED();
}

void NativeMenuViews::RemoveMenuListener(MenuListener* listener) {
  NOTIMPLEMENTED();
}

void NativeMenuViews::SetMinimumWidth(int width) {
  NOTIMPLEMENTED();
}

// MenuDelegate implementation

bool NativeMenuViews::IsItemChecked(int cmd) const {
  int index;
  ui::MenuModel* model = model_;
  if (!ui::MenuModel::GetModelAndIndexForCommandId(cmd, &model, &index))
    return false;
  return model->IsItemCheckedAt(index);
}

bool NativeMenuViews::IsCommandEnabled(int cmd) const {
  int index;
  ui::MenuModel* model = model_;
  if (!ui::MenuModel::GetModelAndIndexForCommandId(cmd, &model, &index))
    return false;
  return model->IsEnabledAt(index);
}

void NativeMenuViews::ExecuteCommand(int cmd) {
  int index;
  ui::MenuModel* model = model_;
  if (!ui::MenuModel::GetModelAndIndexForCommandId(cmd, &model, &index))
    return;
  model->ActivatedAt(index);
}

bool NativeMenuViews::GetAccelerator(int id, ui::Accelerator* accelerator) {
  int index;
  ui::MenuModel* model = model_;
  if (!ui::MenuModel::GetModelAndIndexForCommandId(id, &model, &index))
    return false;

  ui::Accelerator menu_accelerator;
  if (!model->GetAcceleratorAt(index, &menu_accelerator))
    return false;

  *accelerator = ui::Accelerator(menu_accelerator.key_code(),
                                 menu_accelerator.modifiers());
  return true;
}

// private
void NativeMenuViews::AddMenuItemsFromModel(MenuItemView* parent,
                                            ui::MenuModel* model) {
  for (int i = 0; i < model->GetItemCount(); ++i) {
    int index = i + model->GetFirstItemIndex(NULL);
    MenuItemView* child = parent->AppendMenuItemFromModel(model, index,
        model->GetCommandIdAt(index));

    if (child && child->GetType() == MenuItemView::SUBMENU) {
      AddMenuItemsFromModel(child, model->GetSubmenuModelAt(index));
    }
  }
}

void NativeMenuViews::UpdateMenuFromModel(SubmenuView* menu,
                                          ui::MenuModel* model) {
  for (int i = 0, sep = 0; i < model->GetItemCount(); ++i) {
    int index = i + model->GetFirstItemIndex(NULL);
    if (model->GetTypeAt(index) == ui::MenuModel::TYPE_SEPARATOR) {
      ++sep;
      continue;
    }

    // The submenu excludes the separators when counting the menu-items
    // in it. So exclude the number of separators to get the correct index.
    MenuItemView* mitem = menu->GetMenuItemAt(index - sep);
    mitem->SetVisible(model->IsVisibleAt(index));
    mitem->SetEnabled(model->IsEnabledAt(index));
    if (model->IsItemDynamicAt(index)) {
      mitem->SetTitle(model->GetLabelAt(index));
    }

    SkBitmap icon;
    if (model->GetIconAt(index, &icon)) {
      // TODO(atwilson): Support removing the icon dynamically
      // (http://crbug.com/66508).
      mitem->SetIcon(icon);
    }

    if (model->GetTypeAt(index) == ui::MenuModel::TYPE_SUBMENU) {
      DCHECK(mitem->HasSubmenu());
      UpdateMenuFromModel(mitem->GetSubmenu(), model->GetSubmenuModelAt(index));
    }
  }
}

// MenuWrapper, public:

// static
MenuWrapper* MenuWrapper::CreateWrapper(Menu2* menu) {
  return new NativeMenuViews(menu);
}

}  // namespace views
