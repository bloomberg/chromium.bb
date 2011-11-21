// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_SHELL_APP_LIST_H_
#define UI_AURA_SHELL_APP_LIST_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "ui/aura/event_filter.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "views/widget/widget.h"

namespace aura_shell {
namespace internal {

// AppList is a controller that manages app list UI for shell. To show the UI,
// it requests app list widget from ShellDelegate and shows it when ready.
// While the UI is visible, it monitors things such as app list widget's
// activation state and desktop mouse click to auto dismiss the UI.
class AppList : public aura::EventFilter,
                public ui::LayerAnimationObserver,
                public views::Widget::Observer {
 public:
  AppList();
  virtual ~AppList();

  // Show/hide app list window.
  void SetVisible(bool visible);

  // Whether app list window is visible (shown or being shown).
  bool IsVisible();

 private:
  // Sets app list widget. If we are in visible mode, start showing animation.
  // Otherwise, we just close the widget.
  void SetWidget(views::Widget* widget);

  // Forgets the widget.
  void ResetWidget();

  // Starts show/hide animation.
  void ScheduleAnimation();

  // aura::EventFilter overrides:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;

  // ui::LayerAnimationObserver overrides:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;

  // views::Widget::Observer overrides:
  virtual void OnWidgetClosing(views::Widget* widget) OVERRIDE;
  virtual void OnWidgetActivationChanged(views::Widget* widget,
      bool active) OVERRIDE;

  // Whether we should show or hide app list widget.
  bool is_visible_;

  // App list widget we get from ShellDelegate.
  views::Widget* widget_;

  // A weak ptr factory for callbacks that ShellDelegate used to set widget.
  base::WeakPtrFactory<AppList> set_widget_factory_;

  DISALLOW_COPY_AND_ASSIGN(AppList);
};

}  // namespace internal
}  // namespace aura_shell

#endif  //  UI_AURA_SHELL_APP_LIST_H_
