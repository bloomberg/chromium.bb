// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_VIEW_H_
#define UI_AURA_SHELL_LAUNCHER_VIEW_H_
#pragma once

#include "views/widget/widget_delegate.h"
#include "views/controls/button/button.h"

namespace aura_shell {
namespace internal {

class LauncherButton;

class LauncherView : public views::WidgetDelegateView,
                     public views::ButtonListener {
 public:
  LauncherView();
  virtual ~LauncherView();

 private:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  LauncherButton* chrome_button_;
  LauncherButton* applist_button_;

  DISALLOW_COPY_AND_ASSIGN(LauncherView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_VIEW_H_
