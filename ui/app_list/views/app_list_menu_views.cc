// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/app_list/views/app_list_menu_views.h"

#include "ui/app_list/app_list_view_delegate.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_config.h"
#include "ui/views/controls/menu/menu_item_view.h"
#include "ui/views/controls/menu/submenu_view.h"
#include "ui/views/layout/grid_layout.h"

using views::MenuItemView;

namespace app_list {

AppListMenuViews::AppListMenuViews(AppListViewDelegate* delegate)
    : AppListMenu(delegate) {
  menu_delegate_.reset(new views::MenuModelAdapter(menu_model()));
  menu_ = new MenuItemView(menu_delegate_.get());
  menu_runner_.reset(new views::MenuRunner(menu_, 0));
  menu_delegate_->BuildMenu(menu_);
}

AppListMenuViews::~AppListMenuViews() {}

void AppListMenuViews::RunMenuAt(views::MenuButton* button,
                                 const gfx::Point& point) {
  ignore_result(menu_runner_->RunMenuAt(button->GetWidget(),
                                        button,
                                        gfx::Rect(point, gfx::Size()),
                                        views::MENU_ANCHOR_TOPRIGHT,
                                        ui::MENU_SOURCE_NONE));
}

void AppListMenuViews::Cancel() {
  menu_runner_->Cancel();
}

}  // namespace app_list
