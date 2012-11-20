// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_COREWM_FOCUS_CONTROLLER_H_
#define UI_VIEWS_COREWM_FOCUS_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "ui/aura/client/activation_client.h"
#include "ui/aura/window_observer.h"
#include "ui/base/events/event_dispatcher.h"
#include "ui/views/views_export.h"

namespace views {
namespace corewm {

// FocusController handles focus and activation changes for an environment
// encompassing one or more RootWindows. Within an environment there can be
// only one focused and one active window at a time. When focus or activation
// changes a FocusChangeEvent is dispatched.
// Changes to focus and activation can be from three sources:
// . the Aura Client API (implemented here in aura::client::ActivationClient).
//   (The FocusController must be set as the ActivationClient implementation
//    for all RootWindows).
// . input events (implemented here in ui::EventHandler).
//   (The FocusController must be registered as a pre-target handler for
//    the applicable environment owner, either a RootWindow or another type).
// . Window disposition changes (implemented here in aura::WindowObserver).
//   (The FocusController registers itself as an observer of the active and
//    focused windows).
class VIEWS_EXPORT FocusController : public aura::client::ActivationClient,
                                     public ui::EventHandler,
                                     public aura::WindowObserver,
                                     public ui::EventDispatcher {
 public:
  FocusController();
  virtual ~FocusController();

  // TODO(beng): FocusClient
  void SetFocusedWindow(aura::Window* window);
  aura::Window* focused_window() { return focused_window_; }

 private:
  // Overridden from aura::ActivationClient:
  virtual void AddObserver(
      aura::client::ActivationChangeObserver* observer) OVERRIDE;
  virtual void RemoveObserver(
      aura::client::ActivationChangeObserver* observer) OVERRIDE;
  virtual void ActivateWindow(aura::Window* window) OVERRIDE;
  virtual void DeactivateWindow(aura::Window* window) OVERRIDE;
  virtual aura::Window* GetActiveWindow() OVERRIDE;
  virtual bool OnWillFocusWindow(aura::Window* window,
                                 const ui::Event* event) OVERRIDE;
  virtual bool CanActivateWindow(aura::Window* window) const OVERRIDE;

  // Overridden from ui::EventHandler:
  virtual ui::EventResult OnKeyEvent(ui::KeyEvent* event) OVERRIDE;
  virtual ui::EventResult OnMouseEvent(ui::MouseEvent* event) OVERRIDE;
  virtual ui::EventResult OnScrollEvent(ui::ScrollEvent* event) OVERRIDE;
  virtual ui::EventResult OnTouchEvent(ui::TouchEvent* event) OVERRIDE;
  virtual ui::EventResult OnGestureEvent(ui::GestureEvent* event) OVERRIDE;

  // Overridden from aura::WindowObserver:
  virtual void OnWindowVisibilityChanged(aura::Window* window,
                                         bool visible) OVERRIDE;
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;
  virtual void OnWillRemoveWindow(aura::Window* window) OVERRIDE;

  // Overridden from ui::EventDispatcher:
  virtual bool CanDispatchToTarget(ui::EventTarget* target) OVERRIDE;

  aura::Window* active_window_;
  aura::Window* focused_window_;

  DISALLOW_COPY_AND_ASSIGN(FocusController);
};

}  // namespace corewm
}  // namespace views

#endif  // UI_VIEWS_COREWM_FOCUS_CONTROLLER_H_
