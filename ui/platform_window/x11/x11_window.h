// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
#define UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_

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
  virtual ~X11Window();

 private:
  void Destroy();

  void ProcessXInput2Event(XEvent* xevent);

  // PlatformWindow:
  virtual void Show() override;
  virtual void Hide() override;
  virtual void Close() override;
  virtual void SetBounds(const gfx::Rect& bounds) override;
  virtual gfx::Rect GetBounds() override;
  virtual void SetCapture() override;
  virtual void ReleaseCapture() override;
  virtual void ToggleFullscreen() override;
  virtual void Maximize() override;
  virtual void Minimize() override;
  virtual void Restore() override;
  virtual void SetCursor(PlatformCursor cursor) override;
  virtual void MoveCursorTo(const gfx::Point& location) override;

  // PlatformEventDispatcher:
  virtual bool CanDispatchEvent(const PlatformEvent& event) override;
  virtual uint32_t DispatchEvent(const PlatformEvent& event) override;

  PlatformWindowDelegate* delegate_;

  XDisplay* xdisplay_;
  XID xwindow_;
  XID xroot_window_;
  X11AtomCache atom_cache_;

  // Setting the bounds is an asynchronous operation in X11. |requested_bounds_|
  // is the bounds requested using XConfigureWindow, and |confirmed_bounds_| is
  // the bounds the X11 server has set on the window.
  gfx::Rect requested_bounds_;
  gfx::Rect confirmed_bounds_;

  bool window_mapped_;

  DISALLOW_COPY_AND_ASSIGN(X11Window);
};

}  // namespace ui

#endif  // UI_PLATFORM_WINDOW_X11_X11_WINDOW_H_
