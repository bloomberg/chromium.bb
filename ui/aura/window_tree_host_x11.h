// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_X11_H_
#define UI_AURA_WINDOW_TREE_HOST_X11_H_

#include <X11/Xlib.h>

#include <vector>

// Get rid of a macro from Xlib.h that conflicts with Aura's RootWindow class.
#undef RootWindow

#include "base/memory/scoped_ptr.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/env_observer.h"
#include "ui/aura/window_tree_host.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/event_source.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"

namespace ui {
class MouseEvent;
}

namespace aura {

namespace internal {
class TouchEventCalibrate;
}

class AURA_EXPORT WindowTreeHostX11 : public WindowTreeHost,
                                      public ui::EventSource,
                                      public ui::PlatformEventDispatcher,
                                      public EnvObserver {
 public:
  explicit WindowTreeHostX11(const gfx::Rect& bounds);
  virtual ~WindowTreeHostX11();

  // ui::PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const ui::PlatformEvent& event) OVERRIDE;
  virtual uint32_t DispatchEvent(const ui::PlatformEvent& event) OVERRIDE;

  // WindowTreeHost:
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void ToggleFullScreen() OVERRIDE;
  virtual gfx::Rect GetBounds() const OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;
  virtual gfx::Insets GetInsets() const OVERRIDE;
  virtual void SetInsets(const gfx::Insets& insets) OVERRIDE;
  virtual gfx::Point GetLocationOnNativeScreen() const OVERRIDE;
  virtual void SetCapture() OVERRIDE;
  virtual void ReleaseCapture() OVERRIDE;
  virtual bool QueryMouseLocation(gfx::Point* location_return) OVERRIDE;
  virtual bool ConfineCursorToRootWindow() OVERRIDE;
  virtual void UnConfineCursor() OVERRIDE;
  virtual void PostNativeEvent(const base::NativeEvent& event) OVERRIDE;
  virtual void OnDeviceScaleFactorChanged(float device_scale_factor) OVERRIDE;
  virtual void SetCursorNative(gfx::NativeCursor cursor_type) OVERRIDE;
  virtual void MoveCursorToNative(const gfx::Point& location) OVERRIDE;
  virtual void OnCursorVisibilityChangedNative(bool show) OVERRIDE;

  // EnvObserver overrides.
  virtual void OnWindowInitialized(Window* window) OVERRIDE;
  virtual void OnHostInitialized(WindowTreeHost* host) OVERRIDE;

  // ui::EventSource overrides.
  virtual ui::EventProcessor* GetEventProcessor() OVERRIDE;

 private:
  // Dispatches XI2 events. Note that some events targetted for the X root
  // window are dispatched to the aura root window (e.g. touch events after
  // calibration).
  void DispatchXI2Event(const base::NativeEvent& event);

  // Returns true if there's an X window manager present... in most cases.  Some
  // window managers (notably, ion3) don't implement enough of ICCCM for us to
  // detect that they're there.
  bool IsWindowManagerPresent();

  // Sets the cursor on |xwindow_| to |cursor|.  Does not check or update
  // |current_cursor_|.
  void SetCursorInternal(gfx::NativeCursor cursor);

  // Translates the native mouse location into screen coordinates and and
  // dispatches the event via WindowEventDispatcher.
  void TranslateAndDispatchMouseEvent(ui::MouseEvent* event);

  // Update is_internal_display_ based on delegate_ state
  void UpdateIsInternalDisplay();

  // Set the CrOS touchpad "tap paused" property. It is used to temporarily
  // turn off the Tap-to-click feature when the mouse pointer is invisible.
  void SetCrOSTapPaused(bool state);

  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;
  ::Window xwindow_;

  // The native root window.
  ::Window x_root_window_;

  // Current Aura cursor.
  gfx::NativeCursor current_cursor_;

  // Is the window mapped to the screen?
  bool window_mapped_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  // The insets that specifies the effective area within the |window_|.
  gfx::Insets insets_;

  // True if the root host resides on the internal display
  bool is_internal_display_;

  scoped_ptr<XID[]> pointer_barriers_;

  scoped_ptr<internal::TouchEventCalibrate> touch_calibrate_;

  ui::X11AtomCache atom_cache_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostX11);
};

namespace test {

// Set the default value of the override redirect flag used to
// create a X window for WindowTreeHostX11.
AURA_EXPORT void SetUseOverrideRedirectWindowByDefault(bool override_redirect);

}  // namespace test
}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_X11_H_
