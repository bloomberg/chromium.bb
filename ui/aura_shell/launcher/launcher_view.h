// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_VIEW_H_
#define UI_AURA_SHELL_LAUNCHER_VIEW_H_
#pragma once

#include "ui/aura_shell/launcher/launcher_model_observer.h"
#include "views/controls/button/button.h"
#include "views/widget/widget_delegate.h"

namespace views {
class ImageButton;
}

namespace aura_shell {

struct LauncherItem;
class LauncherModel;

namespace internal {

class LauncherView : public views::WidgetDelegateView,
                     public LauncherModelObserver,
                     public views::ButtonListener {
 public:
  explicit LauncherView(LauncherModel* model);
  virtual ~LauncherView();

  void Init();

 private:
  // Creates the view used to represent |item|.
  views::View* CreateViewForItem(const LauncherItem& item);

  // Resizes the widget to fit the view.
  void Resize();

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from LauncherModelObserver:
  virtual void LauncherItemAdded(int index) OVERRIDE;
  virtual void LauncherItemRemoved(int index) OVERRIDE;
  virtual void LauncherItemImagesChanged(int index) OVERRIDE;

  // Overriden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // The model; owned by Launcher.
  LauncherModel* model_;

  views::ImageButton* new_browser_button_;

  views::ImageButton* show_apps_button_;

  DISALLOW_COPY_AND_ASSIGN(LauncherView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_VIEW_H_
