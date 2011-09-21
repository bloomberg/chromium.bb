// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_H_
#define UI_AURA_ROOT_WINDOW_H_
#pragma once

#include "ui/aura/window.h"

namespace aura {
namespace internal {

class FocusManager;

// A Window subclass that handles event targeting for certain types of
// MouseEvent.
class RootWindow : public Window {
 public:
  RootWindow();
  virtual ~RootWindow();

  // Handles a mouse event. Returns true if handled.
  bool HandleMouseEvent(const MouseEvent& event);

  // Handles a key event. Returns true if handled.
  bool HandleKeyEvent(const KeyEvent& event);

  // Sets capture to the specified window.
  void SetCapture(Window* window);

  // If |window| has mouse capture, the current capture window is set to NULL.
  void ReleaseCapture(Window* window);

  // Returns the window that has mouse capture.
  Window* capture_window() { return capture_window_; }

  // Invoked when a child window is destroyed. Cleans up any references to the
  // Window.
  void WindowDestroying(Window* window);

  // Current handler for mouse events.
  Window* mouse_pressed_handler() { return mouse_pressed_handler_; }

  // Overridden from Window:
  virtual FocusManager* GetFocusManager() OVERRIDE;

 protected:
  // Overridden from Window:
  virtual internal::RootWindow* GetRoot() OVERRIDE;

 private:
  // Called whenever the mouse moves, tracks the current |mouse_moved_handler_|,
  // sending exited and entered events as its value changes.
  void HandleMouseMoved(const MouseEvent& event, Window* target);

  Window* mouse_pressed_handler_;
  Window* mouse_moved_handler_;
  scoped_ptr<FocusManager> focus_manager_;
  Window* capture_window_;

  DISALLOW_COPY_AND_ASSIGN(RootWindow);
};

}  // namespace internal
}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_H_
