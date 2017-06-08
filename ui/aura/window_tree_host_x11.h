// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_WINDOW_TREE_HOST_X11_H_
#define UI_AURA_WINDOW_TREE_HOST_X11_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "ui/aura/aura_export.h"
#include "ui/aura/window_tree_host.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/gfx/geometry/rect.h"

// X forward decls to avoid including Xlib.h in a header file.
typedef struct _XDisplay XDisplay;
typedef unsigned long XID;
typedef XID Window;

namespace ui {
class XScopedEventSelector;
}

namespace aura {

class AURA_EXPORT WindowTreeHostX11 : public WindowTreeHost,
                                      public ui::PlatformEventDispatcher {
 public:
  explicit WindowTreeHostX11(const gfx::Rect& bounds);
  ~WindowTreeHostX11() override;

  // ui::PlatformEventDispatcher:
  bool CanDispatchEvent(const ui::PlatformEvent& event) override;
  uint32_t DispatchEvent(const ui::PlatformEvent& event) override;

  // WindowTreeHost:
  ui::EventSource* GetEventSource() override;
  gfx::AcceleratedWidget GetAcceleratedWidget() override;
  void ShowImpl() override;
  void HideImpl() override;
  gfx::Rect GetBoundsInPixels() const override;
  void SetBoundsInPixels(const gfx::Rect& bounds) override;
  gfx::Point GetLocationOnScreenInPixels() const override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void SetCursorNative(gfx::NativeCursor cursor_type) override;
  void MoveCursorToScreenLocationInPixels(
      const gfx::Point& location_in_pixels) override;
  void OnCursorVisibilityChangedNative(bool show) override;

  // Deselects mouse and keyboard events on |xwindow_|.
  void DisableInput();

 protected:
  // Called when X Configure Notify event is recevied.
  virtual void OnConfigureNotify();

  // Translates the native mouse location into screen coordinates and
  // dispatches the event via WindowEventDispatcher.
  virtual void TranslateAndDispatchLocatedEvent(ui::LocatedEvent* event);

  ::Window x_root_window() { return x_root_window_; }
  XDisplay* xdisplay() { return xdisplay_; }
  const gfx::Rect& bounds() const { return bounds_; }

 private:
  // Dispatches XI2 events. Note that some events targetted for the X root
  // window are dispatched to the aura root window (e.g. touch events after
  // calibration).
  void DispatchXI2Event(const base::NativeEvent& event);

  // Sets the cursor on |xwindow_| to |cursor|.  Does not check or update
  // |current_cursor_|.
  void SetCursorInternal(gfx::NativeCursor cursor);

  // The display and the native X window hosting the root window.
  XDisplay* xdisplay_;
  ::Window xwindow_;

  // Events selected on |xwindow_|.
  std::unique_ptr<ui::XScopedEventSelector> xwindow_events_;

  // The native root window.
  ::Window x_root_window_;

  // Current Aura cursor.
  gfx::NativeCursor current_cursor_;

  // Is the window mapped to the screen?
  bool window_mapped_;

  // The bounds of |xwindow_|.
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeHostX11);
};

}  // namespace aura

#endif  // UI_AURA_WINDOW_TREE_HOST_X11_H_
