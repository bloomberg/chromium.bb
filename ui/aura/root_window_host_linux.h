// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_ROOT_WINDOW_HOST_LINUX_H_
#define UI_AURA_ROOT_WINDOW_HOST_LINUX_H_

#include <X11/Xlib.h>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "ui/aura/root_window_host.h"
#include "ui/base/x/x11_atom_cache.h"
#include "ui/gfx/rect.h"

namespace ui {
class ViewProp;
}

namespace aura {

namespace internal {
class TouchEventCalibrate;
}

class RootWindowHostLinux : public RootWindowHost,
                            public MessageLoop::Dispatcher {
 public:
  RootWindowHostLinux(RootWindowHostDelegate* delegate,
                      const gfx::Rect& bounds);
  virtual ~RootWindowHostLinux();

  // Overridden from Dispatcher overrides:
  virtual bool Dispatch(const base::NativeEvent& event) OVERRIDE;

 private:
  bool DispatchEventForRootWindow(const base::NativeEvent& event);

  // Dispatches XI2 events. Note that some events targetted for the X root
  // window are dispatched to the aura root window (e.g. touch events after
  // calibration).
  void DispatchXI2Event(const base::NativeEvent& event);

  // RootWindowHost Overrides.
  virtual RootWindow* GetRootWindow() OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor_type) OVERRIDE;
  virtual void ShowCursor(bool show) OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void MoveCursorTo(const gfx::Point& location) OVERRIDE;
  virtual void SetFocusWhenShown(bool focus_when_shown) OVERRIDE;
  virtual bool GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void PrepareForShutdown() OVERRIDE;

  // Returns true if there's an X window manager present... in most cases.  Some
  // window managers (notably, ion3) don't implement enough of ICCCM for us to
  // detect that they're there.
  bool IsWindowManagerPresent();

  // Sets the cursor on |xwindow_| to |cursor|.  Does not check or update
  // |current_cursor_|.
  void SetCursorInternal(gfx::NativeCursor cursor);

  RootWindowHostDelegate* delegate_;

  // The display and the native X window hosting the root window.
  Display* xdisplay_;
  ::Window xwindow_;

  // The native root window.
  ::Window x_root_window_;

  // Current Aura cursor.
  gfx::NativeCursor current_cursor_;

  // Is the window mapped to the screen?
  bool window_mapped_;

  // Is the cursor currently shown?
  bool cursor_shown_;

  // The invisible cursor.
  ::Cursor invisible_cursor_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  // The bounds of |x_root_window_|.
  gfx::Rect x_root_bounds_;

  // True if the window should be focused when the window is shown.
  bool focus_when_shown_;

  scoped_array<XID> pointer_barriers_;

  scoped_ptr<ui::ViewProp> prop_;

  scoped_ptr<internal::TouchEventCalibrate> touch_calibrate_;

  ui::X11AtomCache atom_cache_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowHostLinux);
};

}  // namespace aura

#endif  // UI_AURA_ROOT_WINDOW_HOST_LINUX_H_
