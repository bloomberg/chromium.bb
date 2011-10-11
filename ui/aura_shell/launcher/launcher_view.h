// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_VIEW_H_
#define UI_AURA_SHELL_LAUNCHER_VIEW_H_
#pragma once

#include "ui/aura_shell/launcher/launcher_model_observer.h"
#include "views/widget/widget_delegate.h"

namespace aura_shell {

class LauncherModel;

namespace internal {

class LauncherButton;

class LauncherView : public views::WidgetDelegateView,
                     public LauncherModelObserver {
 public:
  LauncherView();
  virtual ~LauncherView();

  // Populates this LauncherView from the contents of |model|.
  void Init(LauncherModel* model);

 private:
  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from LauncherModelObserver:
  virtual void LauncherItemAdded(int index) OVERRIDE;
  virtual void LauncherItemRemoved(int index) OVERRIDE;
  virtual void LauncherSelectionChanged() OVERRIDE;

  // The model, we don't own it.
  LauncherModel* model_;

  DISALLOW_COPY_AND_ASSIGN(LauncherView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_VIEW_H_
