// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop_host.h"

#include "base/message_loop.h"
#include "base/message_pump_x.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"

#include <X11/Xlib.h>

namespace aura {

namespace {

class DesktopHostLinux : public DesktopHost {
 public:
  explicit DesktopHostLinux(const gfx::Rect& bounds);
  virtual ~DesktopHostLinux();

 private:
  // base::MessageLoop::Dispatcher Override.
  virtual DispatchStatus Dispatch(XEvent* xev) OVERRIDE;

  // DesktopHost Overrides.
  virtual void SetDesktop(Desktop* desktop) OVERRIDE;
  virtual gfx::AcceleratedWidget GetAcceleratedWidget() OVERRIDE;
  virtual void Show() OVERRIDE;
  virtual gfx::Size GetSize() OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor_type) OVERRIDE;

  Desktop* desktop_;

  // The display and the native X window hosting the desktop.
  Display* xdisplay_;
  ::Window xwindow_;

  // The size of |xwindow_|.
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(DesktopHostLinux);
};

DesktopHostLinux::DesktopHostLinux(const gfx::Rect& bounds)
    : desktop_(NULL),
      xdisplay_(NULL),
      xwindow_(0),
      bounds_(bounds) {
  // This assumes that the message-pump creates and owns the display.
  xdisplay_ = base::MessagePumpX::GetDefaultXDisplay();
  xwindow_ = XCreateSimpleWindow(xdisplay_, DefaultRootWindow(xdisplay_),
                                 bounds.x(), bounds.y(),
                                 bounds.width(), bounds.height(),
                                 0, 0, 0);
  XMapWindow(xdisplay_, xwindow_);

  long event_mask = ButtonPressMask | ButtonReleaseMask |
                    KeyPressMask | KeyReleaseMask |
                    ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);
  XFlush(xdisplay_);
}

DesktopHostLinux::~DesktopHostLinux() {
  XDestroyWindow(xdisplay_, xwindow_);
}

base::MessagePumpDispatcher::DispatchStatus DesktopHostLinux::Dispatch(
    XEvent* xev) {
  bool handled = false;
  switch (xev->type) {
    case Expose:
      desktop_->Draw();
      handled = true;
      break;
    case KeyPress:
    case KeyRelease: {
      KeyEvent keyev(xev);
      handled = desktop_->OnKeyEvent(keyev);
      break;
    }
    case ButtonPress:
    case ButtonRelease:
    case MotionNotify: {
      MouseEvent mouseev(xev);
      handled = desktop_->OnMouseEvent(mouseev);
      break;
    }

    case ConfigureNotify: {
      DCHECK_EQ(xdisplay_, xev->xconfigure.display);
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      desktop_->OnHostResized(gfx::Size(xev->xconfigure.width,
                                        xev->xconfigure.height));
      handled = true;
      break;
    }
  }
  return handled ? EVENT_PROCESSED : EVENT_IGNORED;
}

void DesktopHostLinux::SetDesktop(Desktop* desktop) {
  desktop_ = desktop;
}

gfx::AcceleratedWidget DesktopHostLinux::GetAcceleratedWidget() {
  return xwindow_;
}

void DesktopHostLinux::Show() {
}

gfx::Size DesktopHostLinux::GetSize() {
  return bounds_.size();
}

void DesktopHostLinux::SetSize(const gfx::Size& size) {
  if (bounds_.size() == size)
    return;
  bounds_.set_size(size);
  XResizeWindow(xdisplay_, xwindow_, size.width(), size.height());
}

void DesktopHostLinux::SetCursor(gfx::NativeCursor cursor_type) {
  NOTIMPLEMENTED();
}

}  // namespace

// static
DesktopHost* DesktopHost::Create(const gfx::Rect& bounds) {
  return new DesktopHostLinux(bounds);
}

}  // namespace aura
