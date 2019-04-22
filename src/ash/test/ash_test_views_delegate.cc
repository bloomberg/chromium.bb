// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_views_delegate.h"

#include "ash/shell.h"
#include "ui/base/ui_base_features.h"

namespace ash {

AshTestViewsDelegate::AshTestViewsDelegate() = default;

AshTestViewsDelegate::~AshTestViewsDelegate() = default;

void AshTestViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  if (running_outside_ash_) {
    // With Mash, MusClient::CreateNativeWidget uses a DesktopNativeWidgetAura,
    // which doesn't need a context.
    if (!features::IsUsingWindowService()) {
      DCHECK(ash::Shell::HasInstance());
      if (!params->parent && !params->context)
        params->context = Shell::GetRootWindowForNewWindows();
    }
  } else {
    CHECK(params->native_widget || params->context || params->parent)
        << "Widgets must be created with a context or parent. In tests use "
        << "CurrentContext(). In non-test code you likely want to use the "
        << "parent the Widget will be added to, or possibly "
        << "Shell::GetRootWindowForNewWindows().";
  }

  TestViewsDelegate::OnBeforeWidgetInit(params, delegate);
}

views::TestViewsDelegate::ProcessMenuAcceleratorResult
AshTestViewsDelegate::ProcessAcceleratorWhileMenuShowing(
    const ui::Accelerator& accelerator) {
  if (accelerator == close_menu_accelerator_)
    return ProcessMenuAcceleratorResult::CLOSE_MENU;

  return ProcessMenuAcceleratorResult::LEAVE_MENU_OPEN;
}

}  // namespace ash
