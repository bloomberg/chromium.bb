// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_FOCUS_WATCHER_MUS_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_FOCUS_WATCHER_MUS_H_

#include "ash/public/cpp/immersive/immersive_focus_watcher.h"
#include "ui/aura/client/focus_change_observer.h"
#include "ui/aura/client/transient_window_client_observer.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ash {
class ImmersiveFullscreenController;
class ImmersiveRevealedLock;
}  // namespace ash

// ImmersiveFocusWatcherMus is responsible for grabbing a reveal lock based on
// activation and/or focus. This implementation grabs a lock if views focus is
// in the top view, a bubble is showing that is anchored to the top view, or
// the focused window is a transient child of the top view's widget.
class ImmersiveFocusWatcherMus
    : public ash::ImmersiveFocusWatcher,
      public views::FocusChangeListener,
      public aura::client::TransientWindowClientObserver,
      public ::wm::ActivationChangeObserver {
 public:
  explicit ImmersiveFocusWatcherMus(
      ash::ImmersiveFullscreenController* controller);
  ~ImmersiveFocusWatcherMus() override;

  // ImmersiveFocusWatcher:
  void UpdateFocusRevealedLock() override;
  void ReleaseLock() override;

 private:
  class BubbleObserver;

  views::Widget* GetWidget();
  aura::Window* GetWidgetWindow();

  // Recreate |bubble_observer_| and start observing any bubbles anchored to a
  // child of |top_container_|.
  void RecreateBubbleObserver();

  // views::FocusChangeListener overrides:
  void OnWillChangeFocus(views::View* focused_before,
                         views::View* focused_now) override;
  void OnDidChangeFocus(views::View* focused_before,
                        views::View* focused_now) override;

  // aura::client::TransientWindowClientObserver overrides:
  void OnTransientChildWindowAdded(aura::Window* window,
                                   aura::Window* transient) override;
  void OnTransientChildWindowRemoved(aura::Window* window,
                                     aura::Window* transient) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(
      ::wm::ActivationChangeObserver::ActivationReason reason,
      aura::Window* gaining_active,
      aura::Window* losing_active) override;

  ash::ImmersiveFullscreenController* immersive_fullscreen_controller_;

  // Lock which keeps the top-of-window views revealed based on the focused view
  // and the active widget. Acquiring the lock never triggers a reveal because
  // a view is not focusable till a reveal has made it visible.
  std::unique_ptr<ash::ImmersiveRevealedLock> lock_;

  // Manages bubbles which are anchored to a child of
  // |ImmersiveFullscreenController::top_container_|.
  std::unique_ptr<BubbleObserver> bubble_observer_;

  DISALLOW_COPY_AND_ASSIGN(ImmersiveFocusWatcherMus);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_IMMERSIVE_FOCUS_WATCHER_MUS_H_
