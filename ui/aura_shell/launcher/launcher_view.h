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
class BoundsAnimator;
class ImageButton;
}

namespace aura_shell {

struct LauncherItem;
class LauncherModel;
class ViewModel;

namespace internal {

class LauncherView : public views::WidgetDelegateView,
                     public LauncherModelObserver,
                     public views::ButtonListener {
 public:
  explicit LauncherView(LauncherModel* model);
  virtual ~LauncherView();

  void Init();

 private:
  struct IdealBounds {
    gfx::Rect new_browser_bounds;
    gfx::Rect show_apps_bounds;
  };

  // Sets the bounds of each view to its ideal bounds.
  void LayoutToIdealBounds();

  // Calculates the ideal bounds. The bounds of each button corresponding to an
  // item in the model is set in |view_model_|, the bounds of the
  // |new_browser_button_| and |show_apps_button_| is set in |bounds|.
  void CalculateIdealBounds(IdealBounds* bounds);

  // Animates the bounds of each view to its ideal bounds.
  void AnimateToIdealBounds();

  // Creates the view used to represent |item|.
  views::View* CreateViewForItem(const LauncherItem& item);

  // Resizes the widget to fit the view.
  void Resize();

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from LauncherModelObserver:
  virtual void LauncherItemAdded(int model_index) OVERRIDE;
  virtual void LauncherItemRemoved(int model_index) OVERRIDE;
  virtual void LauncherItemImagesChanged(int model_index) OVERRIDE;

  // Overriden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // The model; owned by Launcher.
  LauncherModel* model_;

  // Used to manage the set of active launcher buttons. There is a view per
  // item in |model_|.
  scoped_ptr<ViewModel> view_model_;

  scoped_ptr<views::BoundsAnimator> bounds_animator_;

  views::ImageButton* new_browser_button_;

  views::ImageButton* show_apps_button_;

  DISALLOW_COPY_AND_ASSIGN(LauncherView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_VIEW_H_
