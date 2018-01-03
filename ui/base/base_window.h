// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_BASE_WINDOW_H_
#define UI_BASE_BASE_WINDOW_H_

#include "base/compiler_specific.h"
#include "build/build_config.h"
#include "ui/base/ui_base_types.h"  // WindowShowState
#include "ui/gfx/native_widget_types.h"

#if defined(OS_WIN)
// Names used in this class are also windows.h macros. That the names
// are the same as in the Windows API is no coincidence but for now we
// don't want the Windows macros to interfere so we undef them.
#undef IsMinimized
#undef IsMaximized
#undef IsRestored
#endif  // OS_WIN

namespace gfx {
class Rect;
}

namespace ui {

// Provides an interface to perform actions on windows, and query window
// state.
class UI_BASE_EXPORT BaseWindow {
 public:
  // Returns true if the window is currently the active/focused window.
  virtual bool IsActive() const = 0;

  // Returns true if the window is maximized (aka zoomed).
  virtual bool IsMaximized() const = 0;

  // Returns true if the window is minimized.
  virtual bool IsMinimized() const = 0;

  // Returns true if the window is full screen.
  virtual bool IsFullscreen() const = 0;

  // Returns true if the window is fully restored (not Fullscreen, Maximized,
  // Minimized).
  static bool IsRestored(const BaseWindow& window);

  // Return a platform dependent identifier for this window.
  virtual gfx::NativeWindow GetNativeWindow() const = 0;

  // Returns the nonmaximized bounds of the window (even if the window is
  // currently maximized or minimized) in terms of the screen coordinates.
  virtual gfx::Rect GetRestoredBounds() const = 0;

  // Returns the restore state for the window (platform dependent).
  virtual ui::WindowShowState GetRestoredState() const = 0;

  // Retrieves the window's current bounds, including its window.
  // This will only differ from GetRestoredBounds() for maximized
  // and minimized windows.
  virtual gfx::Rect GetBounds() const = 0;

  // Shows the window, or activates it if it's already visible.
  virtual void Show() = 0;

  // Hides the window.
  virtual void Hide() = 0;

  // Show the window, but do not activate it. Does nothing if window
  // is already visible.
  virtual void ShowInactive() = 0;

  // Closes the window as soon as possible. The close action may be delayed
  // if an operation is in progress (e.g. a drag operation).
  virtual void Close() = 0;

  // Activates (brings to front) the window. Restores the window from minimized
  // state if necessary.
  virtual void Activate() = 0;

  // Deactivates the window, making the next window in the Z order the active
  // window.
  virtual void Deactivate() = 0;

  // Maximizes/minimizes/restores the window.
  virtual void Maximize() = 0;
  virtual void Minimize() = 0;
  virtual void Restore() = 0;

  // Sets the window's size and position to the specified values.
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Flashes the taskbar item associated with this window.
  // Set |flash| to true to initiate flashing, false to stop flashing.
  virtual void FlashFrame(bool flash) = 0;

  // Returns true if a window is set to be always on top.
  virtual bool IsAlwaysOnTop() const = 0;

  // If set to true, the window will stay on top of other windows which do not
  // have this flag enabled.
  virtual void SetAlwaysOnTop(bool always_on_top) = 0;
};

}  // namespace ui

#endif  // UI_BASE_BASE_WINDOW_H_
