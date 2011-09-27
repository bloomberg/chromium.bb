// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_DESKTOP_H_
#define UI_AURA_DESKTOP_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/task.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/cursor.h"
#include "ui/aura/root_window.h"
#include "ui/gfx/compositor/compositor.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Size;
}

namespace aura {

class DesktopHost;
class MouseEvent;

// Desktop is responsible for hosting a set of windows.
class AURA_EXPORT Desktop : public ui::CompositorDelegate {
 public:
  Desktop();
  ~Desktop();

  // Initializes the desktop.
  void Init();

  // Shows the desktop host.
  void Show();

  // Sets the size of the desktop.
  void SetSize(const gfx::Size& size);

  // Shows the specified cursor.
  void SetCursor(CursorType cursor_type);

  // Shows the desktop host and runs an event loop for it.
  void Run();

  // Draws the necessary set of windows.
  void Draw();

  // Handles a mouse event. Returns true if handled.
  bool OnMouseEvent(const MouseEvent& event);

  // Handles a key event. Returns true if handled.
  bool OnKeyEvent(const KeyEvent& event);

  // Called when the host changes size.
  void OnHostResized(const gfx::Size& size);

  // Compositor we're drawing to.
  ui::Compositor* compositor() { return compositor_.get(); }

  Window* window() { return window_.get(); }

  Window* toplevel_window_container() { return toplevel_window_container_; }
  void set_toplevel_window_container(Window* toplevel_window_container) {
    toplevel_window_container_ = toplevel_window_container;
  }

  static void set_compositor_factory_for_testing(ui::Compositor*(*factory)()) {
    compositor_factory_ = factory;
  }
  static ui::Compositor* (*compositor_factory())() {
    return compositor_factory_;
  }

  // Sets the active window to |window| and the focused window to |to_focus|.
  // If |to_focus| is NULL, |window| is focused.
  void SetActiveWindow(Window* window, Window* to_focus);
  Window* active_window() { return active_window_; }

  // Activates the topmost window. Does nothing if the topmost window is already
  // active.
  void ActivateTopmostWindow();

  // Invoked from RootWindow when |window| is being destroyed.
  void WindowDestroying(Window* window);

  static Desktop* GetInstance();

 private:
  // Returns the topmost window to activate. This ignores |ignore|.
  Window* GetTopmostWindowToActivate(Window* ignore);

  // Overridden from ui::CompositorDelegate
  virtual void ScheduleCompositorPaint();

  scoped_refptr<ui::Compositor> compositor_;

  scoped_ptr<internal::RootWindow> window_;
  Window* toplevel_window_container_;

  scoped_ptr<DesktopHost> host_;

  static Desktop* instance_;

  // Used to schedule painting.
  ScopedRunnableMethodFactory<Desktop> schedule_paint_;

  // Factory used to create Compositors. Settable by tests.
  static ui::Compositor*(*compositor_factory_)();

  Window* active_window_;

  // Are we in the process of being destroyed? Used to avoid processing during
  // destruction.
  bool in_destructor_;

  DISALLOW_COPY_AND_ASSIGN(Desktop);
};

}  // namespace aura

#endif  // UI_AURA_DESKTOP_H_
