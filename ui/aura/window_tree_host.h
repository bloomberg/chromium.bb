// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_H_
#define UI_AURA_WINDOW_TREE_HOST_H_

#include <vector>

#include "base/event_types.h"
#include "base/message_loop/message_loop.h"
#include "ui/aura/aura_export.h"
#include "ui/base/cursor/cursor.h"
#include "ui/gfx/native_widget_types.h"

namespace gfx {
class Insets;
class Point;
class Rect;
class Size;
class Transform;
}

namespace ui {
class Compositor;
}

namespace aura {

class RootWindow;
class WindowTreeHostDelegate;
class RootWindowTransformer;

// WindowTreeHost bridges between a native window and the embedded RootWindow.
// It provides the accelerated widget and maps events from the native os to
// aura.
class AURA_EXPORT WindowTreeHost {
 public:
  virtual ~WindowTreeHost();

  // Creates a new WindowTreeHost. The caller owns the returned value.
  static WindowTreeHost* Create(const gfx::Rect& bounds);

  void InitHost();

  void InitCompositor();

  // TODO(beng): these will become trivial accessors in a future CL.
  aura::Window* window();
  const aura::Window* window() const;

  ui::Compositor* compositor() { return compositor_.get(); }

  void SetRootWindowTransformer(scoped_ptr<RootWindowTransformer> transformer);
  gfx::Transform GetRootTransform() const;

  void SetTransform(const gfx::Transform& transform);

  gfx::Transform GetInverseRootTransform() const;

  // Updates the root window's size using |host_size|, current
  // transform and insets.
  void UpdateRootWindowSize(const gfx::Size& host_size);

  // Returns the actual size of the screen.
  // (gfx::Screen only reports on the virtual desktop exposed by Aura.)
  static gfx::Size GetNativeScreenSize();

  void set_delegate(WindowTreeHostDelegate* delegate) {
    delegate_ = delegate;
  }

  // Converts |point| from the root window's coordinate system to native
  // screen's.
  void ConvertPointToNativeScreen(gfx::Point* point) const;

  // Converts |point| from native screen coordinate system to the root window's.
  void ConvertPointFromNativeScreen(gfx::Point* point) const;

  // Converts |point| from the root window's coordinate system to the
  // host window's.
  void ConvertPointToHost(gfx::Point* point) const;

  // Converts |point| from the host window's coordinate system to the
  // root window's.
  void ConvertPointFromHost(gfx::Point* point) const;

  // Cursor.
  // Sets the currently-displayed cursor. If the cursor was previously hidden
  // via ShowCursor(false), it will remain hidden until ShowCursor(true) is
  // called, at which point the cursor that was last set via SetCursor() will be
  // used.
  void SetCursor(gfx::NativeCursor cursor);

  // Invoked when the cursor's visibility has changed.
  void OnCursorVisibilityChanged(bool visible);

  // Moves the cursor to the specified location relative to the root window.
  void MoveCursorTo(const gfx::Point& location);

  // Moves the cursor to the |host_location| given in host coordinates.
  void MoveCursorToHostLocation(const gfx::Point& host_location);

  gfx::NativeCursor last_cursor() const { return last_cursor_; }

  virtual RootWindow* GetRootWindow() = 0;

  // Returns the accelerated widget.
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() = 0;

  // Shows the WindowTreeHost.
  virtual void Show() = 0;

  // Hides the WindowTreeHost.
  virtual void Hide() = 0;

  // Toggles the host's full screen state.
  virtual void ToggleFullScreen() = 0;

  // Gets/Sets the size of the WindowTreeHost.
  virtual gfx::Rect GetBounds() const = 0;
  virtual void SetBounds(const gfx::Rect& bounds) = 0;

  // Sets/Gets the insets that specifies the effective root window area
  // in the host window.
  virtual gfx::Insets GetInsets() const = 0;
  virtual void SetInsets(const gfx::Insets& insets) = 0;

  // Sets the OS capture to the root window.
  virtual void SetCapture() = 0;

  // Releases OS capture of the root window.
  virtual void ReleaseCapture() = 0;

  // Queries the mouse's current position relative to the host window and sets
  // it in |location_return|. Returns true if the cursor is within the host
  // window. The position set to |location_return| is constrained within the
  // host window. If the cursor is disabled, returns false and (0, 0) is set to
  // |location_return|.
  // This method is expensive, instead use gfx::Screen::GetCursorScreenPoint().
  virtual bool QueryMouseLocation(gfx::Point* location_return) = 0;

  // Clips the cursor to the bounds of the root window until UnConfineCursor().
  // We would like to be able to confine the cursor to that window. However,
  // currently, we do not have such functionality in X. So we just confine
  // to the root window. This is ok because this option is currently only
  // being used in fullscreen mode, so root_window bounds = window bounds.
  virtual bool ConfineCursorToRootWindow() = 0;
  virtual void UnConfineCursor() = 0;

  // Posts |native_event| to the platform's event queue.
  virtual void PostNativeEvent(const base::NativeEvent& native_event) = 0;

  // Called when the device scale factor of the root window has chagned.
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) = 0;

  // Stop listening events in preparation for shutdown.
  virtual void PrepareForShutdown() = 0;

 protected:
  friend class TestScreen;  // TODO(beng): see if we can remove/consolidate.

  WindowTreeHost();
  void DestroyCompositor();

  void CreateCompositor(gfx::AcceleratedWidget accelerated_widget);

  // Returns the location of the RootWindow on native screen.
  virtual gfx::Point GetLocationOnNativeScreen() const = 0;

  void NotifyHostResized(const gfx::Size& new_size);

  // Sets the currently displayed cursor.
  virtual void SetCursorNative(gfx::NativeCursor cursor) = 0;

  // Moves the cursor to the specified location relative to the root window.
  virtual void MoveCursorToNative(const gfx::Point& location) = 0;

  // kCalled when the cursor visibility has changed.
  virtual void OnCursorVisibilityChangedNative(bool show) = 0;

  WindowTreeHostDelegate* delegate_;

 private:
  // Moves the cursor to the specified location. This method is internally used
  // by MoveCursorTo() and MoveCursorToHostLocation().
  void MoveCursorToInternal(const gfx::Point& root_location,
                            const gfx::Point& host_location);

  scoped_ptr<ui::Compositor> compositor_;

  scoped_ptr<RootWindowTransformer> transformer_;

  // Last cursor set.  Used for testing.
  gfx::NativeCursor last_cursor_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHost);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_H_
