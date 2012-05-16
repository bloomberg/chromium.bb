// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_HOST_H_
#define UI_AURA_ROOT_WINDOW_HOST_H_
#pragma once

#include <vector>

#include "base/message_loop.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Point;
class Rect;
class Size;
}

namespace aura {

class RootWindow;

// RootWindowHost bridges between a native window and the embedded RootWindow.
// It provides the accelerated widget and maps events from the native os to
// aura.
class RootWindowHost {
 public:
  virtual ~RootWindowHost() {}

  // Creates a new RootWindowHost. The caller owns the returned value.
  static RootWindowHost* Create(const gfx::Rect& bounds);

  // Returns the actual size of the screen.
  // (gfx::Screen only reports on the virtual desktop exposed by Aura.)
  static gfx::Size GetNativeScreenSize();

  // Returns the RootWindowHost for the specified accelerated widget, or NULL if
  // there is none associated.
  static RootWindowHost* GetForAcceleratedWidget(
      gfx::AcceleratedWidget accelerated_widget);

  // Sets the RootWindow this RootWindowHost is hosting. RootWindowHost does not
  // own the RootWindow.
  virtual void SetRootWindow(RootWindow* root_window) = 0;
  virtual RootWindow* GetRootWindow() = 0;

  // Returns the accelerated widget.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Shows the RootWindowHost.
  virtual void Show() = 0;

  // Toggles the host's full screen state.
  virtual void ToggleFullScreen() = 0;

  // Gets/Sets the size of the RootWindowHost.
  virtual gfx::Rect GetBounds() const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Returns the location of the RootWindow on native screen.
  virtual gfx::Point GetLocationOnNativeScreen() const = 0;

  // Sets the OS capture to the root window.
  virtual void SetCapture() = 0;

  // Releases OS capture of the root window.
  virtual void ReleaseCapture() = 0;

  // Sets the currently displayed cursor.
  virtual void SetCursor(gfx::NativeCursor cursor) = 0;

  // Shows or hides the cursor.
  virtual void ShowCursor(bool show) = 0;

  // Queries the mouse's current position relative to the host window.
  // The position is constrained within the host window.
  // You should probably call RootWindow::last_mouse_location() instead; this
  // method can be expensive.
  virtual gfx::Point QueryMouseLocation() = 0;

  // Clips the cursor to the bounds of the root window until UnConfineCursor().
  virtual bool ConfineCursorToRootWindow() = 0;
  virtual void UnConfineCursor() = 0;

  // Moves the cursor to the specified location relative to the root window.
  virtual void MoveCursorTo(const gfx::Point& location) = 0;

  // Sets if the window should be focused when shown.
  virtual void SetFocusWhenShown(bool focus_when_shown) = 0;

  // Grabs the snapshot of the root window by using the platform-dependent APIs.
  virtual bool GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) = 0;

  // Posts |native_event| to the platform's event queue.
#if !defined(OS_MACOSX)
  virtual void PostNativeEvent(const base::NativeEvent& native_event) = 0;
#endif
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_HOST_H_
