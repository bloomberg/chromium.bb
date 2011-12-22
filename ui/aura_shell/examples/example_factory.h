// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_EXAMPLES_EXAMPLE_FACTORY_H_
#define UI_AURA_SHELL_EXAMPLES_EXAMPLE_FACTORY_H_
#pragma once

#include "ui/aura_shell/shell_delegate.h"

namespace views {
class View;
}

namespace aura_shell {

class AppListModel;
class AppListViewDelegate;

namespace examples {

void CreatePointyBubble(views::View* anchor_view);

void CreateLockScreen();

// Creates a window showing samples of commonly used widgets.
void CreateWidgetsWindow();

void BuildAppListModel(AppListModel* model);

AppListViewDelegate* CreateAppListViewDelegate();

void CreateAppList(const gfx::Rect& bounds,
                   const ShellDelegate::SetWidgetCallback& callback);

}  // namespace examples
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_EXAMPLES_EXAMPLE_FACTORY_H_
