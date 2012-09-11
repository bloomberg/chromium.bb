// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_HOST_H_
#define UI_AURA_ROOT_WINDOW_HOST_H_

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
class RootWindowHostDelegate;

// RootWindowHost bridges between a native window and the embedded RootWindow.
// It provides the accelerated widget and maps events from the native os to
// aura.
class RootWindowHost {
 public:
  virtual ~RootWindowHost() {}

  // Creates a new RootWindowHost. The caller owns the returned value.
  static RootWindowHost* Create(RootWindowHostDelegate* delegate,
                                const gfx::Rect& bounds);

  // Returns the actual size of the screen.
  // (gfx::Screen only reports on the virtual desktop exposed by Aura.)
  static gfx::Size GetNativeScreenSize();

  virtual RootWindow* GetRootWindow() = 0;

  // Returns the RootWindowHost for the specified accelerated widget, or NULL if
  // there is none associated.
  static RootWindowHost* GetForAcceleratedWidget(
      gfx::AcceleratedWidget accelerated_widget);

  // Returns the accelerated widget.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Shows the RootWindowHost.
  virtual void Show() = 0;

  // Hides the RootWindowHost.
  virtual void Hide() = 0;

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

  // Queries the mouse's current position relative to the host window and sets
  // it in |location_return|. Returns true if the cursor is within the host
  // window. The position set to |location_return| is constrained within the
  // host window.
  // This method is expensive, instead use gfx::Screen::GetCursorScreenPoint().
  virtual bool QueryMouseLocation(gfx::Point* location_return) = 0;

  // Clips the cursor to the bounds of the root window until UnConfineCursor().
  virtual bool ConfineCursorToRootWindow() = 0;
  virtual void UnConfineCursor() = 0;

  // Moves the cursor to the specified location relative to the root window.
  virtual void MoveCursorTo(const gfx::Point& location) = 0;

  // Sets if the window should be focused when shown.
  virtual void SetFocusWhenShown(bool focus_when_shown) = 0;

  // Grabs the snapshot of the root window by using the platform-dependent APIs.
  // The bounds need to be in physical pixels.
  virtual bool GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) = 0;

  // Posts |native_event| to the platform's event queue.
#if !defined(OS_MACOSX)
  virtual void PostNativeEvent(const base::NativeEvent& native_event) = 0;
#endif

  // Called when the device scale factor of the root window has chagned.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) = 0;

  // Stop listening events in preparation for shutdown.
  virtual void PrepareForShutdown() = 0;
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_HOST_H_
