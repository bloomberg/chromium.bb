// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_

#include <stdint.h>

#include "base/macros.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/platform_window/platform_window.h"
#include "ui/platform_window/platform_window_delegate.h"
#include "ui/platform_window/x11/x11_window_export.h"

typedef struct _XDisplay XDisplay;
typedef unsigned long XID;

namespace ui {

class X11_WINDOW_EXPORT X11Window : public PlatformWindow,
                                    public PlatformEventDispatcher {
 public:
  explicit X11Window(PlatformWindowDelegate* delegate);
  ~X11Window() override;

 private:
  void Destroy();

  void ProcessXInput2Event(XEvent* xevent);

  // PlatformWindow:
  void Show() override;
  void Hide() override;
  void Close() override;
  void SetBounds(const gfx::Rect& bounds) override;
  gfx::Rect GetBounds() override;
  void SetTitle(const base::string16& title) override;
  void SetCapture() override;
  void ReleaseCapture() override;
  void ToggleFullscreen() override;
  void Maximize() override;
  void Minimize() override;
  void Restore() override;
  void SetCursor(PlatformCursor cursor) override;
  void MoveCursorTo(const gfx::Point& location) override;
  void ConfineCursorToBounds(const gfx::Rect& bounds) override;
  PlatformImeController* GetPlatformImeController() override;

  // PlatformEventDispatcher:
  bool CanDispatchEvent(const PlatformEvent& event) override;
  uint32_t DispatchEvent(const PlatformEvent& event) override;

  PlatformWindowDelegate* delegate_;

  XDisplay* xdisplay_;
  XID xwindow_;
  XID xroot_window_;
  X11AtomCache atom_cache_;

  base::string16 window_title_;

  // Setting the bounds is an asynchronous operation in X11. |requested_bounds_|
  // is the bounds requested using XConfigureWindow, and |confirmed_bounds_| is
  // the bounds the X11 server has set on the window.
  gfx::Rect requested_bounds_;
  gfx::Rect confirmed_bounds_;

  bool window_mapped_;

  DISALLOW_COPY_AND_ASSIGN(X11Window);
};

namespace test {

// Sets the value of the |override_redirect| flag when creating an X11 window.
// It is necessary to set this flag on for various tests, otherwise the call to
// X11Window::Show() blocks because it never receives the MapNotify event. It is
// unclear why this is necessary, but might be related to calls to
// XInitThreads().
X11_WINDOW_EXPORT void SetUseOverrideRedirectWindowByDefault(
    bool override_redirect);

}  // namespace test

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
