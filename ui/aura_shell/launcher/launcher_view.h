// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_LAUNCHER_VIEW_H_
#define UI_AURA_SHELL_LAUNCHER_VIEW_H_
#pragma once

#include "ui/aura_shell/launcher/launcher_button_host.h"
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
                     public views::ButtonListener,
                     public LauncherButtonHost {
 public:
  explicit LauncherView(LauncherModel* model);
  virtual ~LauncherView();

  void Init();

 private:
  class FadeOutAnimationDelegate;
  class StartFadeAnimationDelegate;

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

  // Fades |view| from an opacity of 0 to 1. This is when adding a new item.
  void FadeIn(views::View* view);

  // Invoked when the mouse has moved enough to trigger a drag. Sets internal
  // state in preparation for the drag.
  void PrepareForDrag(const views::MouseEvent& event);

  // Invoked when the mouse is dragged. Updates the models as appropriate.
  void ContinueDrag(const views::MouseEvent& event);

  // If there is a drag operation in progress it's canceled.
  void CancelDrag(views::View* deleted_view);

  // Common setup done for all children.
  void ConfigureChildView(views::View* view);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // Overridden from LauncherModelObserver:
  virtual void LauncherItemAdded(int model_index) OVERRIDE;
  virtual void LauncherItemRemoved(int model_index) OVERRIDE;
  virtual void LauncherItemImagesChanged(int model_index) OVERRIDE;
  virtual void LauncherItemMoved(int start_index, int target_index) OVERRIDE;

  // Overridden from LauncherButtonHost:
  virtual void MousePressedOnButton(views::View* view,
                                    const views::MouseEvent& event) OVERRIDE;
  virtual void MouseDraggedOnButton(views::View* view,
                                    const views::MouseEvent& event) OVERRIDE;
  virtual void MouseReleasedOnButton(views::View* view,
                                     bool canceled) OVERRIDE;

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

  // Are we dragging? This is only set if the mouse is dragged far enough to
  // trigger a drag.
  bool dragging_;

  // The view being dragged. This is set immediately when the mouse is pressed.
  // |dragging_| is set only if the mouse is dragged far enough.
  views::View* drag_view_;

  // X coordinate of the mouse down event in |drag_view_|s coordinates.
  int drag_offset_;

  // Index |drag_view_| was initially at.
  int start_drag_index_;

  DISALLOW_COPY_AND_ASSIGN(LauncherView);
};

}  // namespace internal
}  // namespace aura_shell

#endif  // UI_AURA_SHELL_LAUNCHER_VIEW_H_
