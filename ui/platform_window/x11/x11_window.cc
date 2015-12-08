// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/platform_window/x11/x11_window.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include "base/strings/utf_string_conversions.h"
#include "ui/events/devices/x11/touch_factory_x11.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/events/platform/platform_event_dispatcher.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/events/platform/x11/x11_event_source.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/x/x11_atom_cache.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/platform_window/platform_window_delegate.h"

namespace ui {

namespace {

const char* kAtomsToCache[] = {
  "UTF8_STRING",
  "WM_DELETE_WINDOW",
  "_NET_WM_NAME",
  "_NET_WM_PID",
  "_NET_WM_PING",
  NULL
};

XID FindXEventTarget(XEvent* xevent) {
  XID target = xevent->xany.window;
  if (xevent->type == GenericEvent)
    target = static_cast<XIDeviceEvent*>(xevent->xcookie.data)->event;
  return target;
}

bool g_override_redirect = false;

}  // namespace

X11Window::X11Window(PlatformWindowDelegate* delegate)
    : delegate_(delegate),
      xdisplay_(gfx::GetXDisplay()),
      xwindow_(None),
      xroot_window_(DefaultRootWindow(xdisplay_)),
      atom_cache_(xdisplay_, kAtomsToCache),
      window_mapped_(false) {
  CHECK(delegate_);
  TouchFactory::SetTouchDeviceListFromCommandLine();
}

X11Window::~X11Window() {
  Destroy();
}

void X11Window::Destroy() {
  if (xwindow_ == None)
    return;

  // Stop processing events.
  PlatformEventSource::GetInstance()->RemovePlatformEventDispatcher(this);
  XID xwindow = xwindow_;
  XDisplay* xdisplay = xdisplay_;
  xwindow_ = None;
  delegate_->OnClosed();
  // |this| might be deleted because of the above call.

  XDestroyWindow(xdisplay, xwindow);
}

void X11Window::ProcessXInput2Event(XEvent* xev) {
  if (!TouchFactory::GetInstance()->ShouldProcessXI2Event(xev))
    return;
  EventType event_type = EventTypeFromNative(xev);
  switch (event_type) {
    case ET_KEY_PRESSED:
    case ET_KEY_RELEASED: {
      KeyEvent key_event(xev);
      delegate_->DispatchEvent(&key_event);
      break;
    }
    case ET_MOUSE_PRESSED:
    case ET_MOUSE_MOVED:
    case ET_MOUSE_DRAGGED:
    case ET_MOUSE_RELEASED: {
      MouseEvent mouse_event(xev);
      delegate_->DispatchEvent(&mouse_event);
      break;
    }
    case ET_MOUSEWHEEL: {
      MouseWheelEvent wheel_event(xev);
      delegate_->DispatchEvent(&wheel_event);
      break;
    }
    case ET_SCROLL_FLING_START:
    case ET_SCROLL_FLING_CANCEL:
    case ET_SCROLL: {
      ScrollEvent scroll_event(xev);
      delegate_->DispatchEvent(&scroll_event);
      break;
    }
    case ET_TOUCH_MOVED:
    case ET_TOUCH_PRESSED:
    case ET_TOUCH_CANCELLED:
    case ET_TOUCH_RELEASED: {
      TouchEvent touch_event(xev);
      delegate_->DispatchEvent(&touch_event);
      break;
    }
    default:
      break;
  }
}

void X11Window::Show() {
  if (window_mapped_)
    return;

  CHECK(PlatformEventSource::GetInstance());
  PlatformEventSource::GetInstance()->AddPlatformEventDispatcher(this);

  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = None;
  swa.bit_gravity = NorthWestGravity;
  swa.override_redirect = g_override_redirect;
  xwindow_ = XCreateWindow(xdisplay_,
                           xroot_window_,
                           requested_bounds_.x(),
                           requested_bounds_.y(),
                           requested_bounds_.width(),
                           requested_bounds_.height(),
                           0,               // border width
                           CopyFromParent,  // depth
                           InputOutput,
                           CopyFromParent,  // visual
                           CWBackPixmap | CWBitGravity | CWOverrideRedirect,
                           &swa);

  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask | EnterWindowMask |
                    LeaveWindowMask | ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);

  unsigned char mask[XIMaskLen(XI_LASTEVENT)];
  memset(mask, 0, sizeof(mask));

  XISetMask(mask, XI_TouchBegin);
  XISetMask(mask, XI_TouchUpdate);
  XISetMask(mask, XI_TouchEnd);
  XISetMask(mask, XI_ButtonPress);
  XISetMask(mask, XI_ButtonRelease);
  XISetMask(mask, XI_Motion);
  XISetMask(mask, XI_KeyPress);
  XISetMask(mask, XI_KeyRelease);

  XIEventMask evmask;
  evmask.deviceid = XIAllDevices;
  evmask.mask_len = sizeof(mask);
  evmask.mask = mask;
  XISelectEvents(xdisplay_, xwindow_, &evmask, 1);
  XFlush(xdisplay_);

  ::Atom protocols[2];
  protocols[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
  protocols[1] = atom_cache_.GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(
      xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  // XChangeProperty() expects "pid" to be long.
  static_assert(sizeof(long) >= sizeof(pid_t),
                "pid_t should not be larger than long");
  long pid = getpid();
  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_PID"),
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&pid),
                  1);
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  XSizeHints size_hints;
  size_hints.flags = PPosition | PWinGravity;
  size_hints.x = requested_bounds_.x();
  size_hints.y = requested_bounds_.y();
  // Set StaticGravity so that the window position is not affected by the
  // frame width when running with window manager.
  size_hints.win_gravity = StaticGravity;
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

  XMapWindow(xdisplay_, xwindow_);

  // We now block until our window is mapped. Some X11 APIs will crash and
  // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
  // asynchronous.
  if (X11EventSource::GetInstance())
    X11EventSource::GetInstance()->BlockUntilWindowMapped(xwindow_);
  window_mapped_ = true;

  // TODO(sky): provide real scale factor.
  delegate_->OnAcceleratedWidgetAvailable(xwindow_, 1.f);
}

void X11Window::Hide() {
  if (!window_mapped_)
    return;
  XWithdrawWindow(xdisplay_, xwindow_, 0);
  window_mapped_ = false;
}

void X11Window::Close() {
  Destroy();
}

void X11Window::SetBounds(const gfx::Rect& bounds) {
  requested_bounds_ = bounds;
  if (!window_mapped_)
    return;
  XWindowChanges changes = {0};
  unsigned value_mask = CWX | CWY | CWWidth | CWHeight;
  changes.x = bounds.x();
  changes.y = bounds.y();
  changes.width = bounds.width();
  changes.height = bounds.height();
  XConfigureWindow(xdisplay_, xwindow_, value_mask, &changes);
}

gfx::Rect X11Window::GetBounds() {
  return confirmed_bounds_;
}

void X11Window::SetTitle(const base::string16& title) {
  if (window_title_ == title)
    return;
  window_title_ = title;
  std::string utf8str = base::UTF16ToUTF8(title);
  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_NAME"),
                  atom_cache_.GetAtom("UTF8_STRING"),
                  8,
                  PropModeReplace,
                  reinterpret_cast<const unsigned char*>(utf8str.c_str()),
                  utf8str.size());
  XTextProperty xtp;
  char *c_utf8_str = const_cast<char *>(utf8str.c_str());
  if (Xutf8TextListToTextProperty(xdisplay_, &c_utf8_str, 1,
                                  XUTF8StringStyle, &xtp) == Success) {
    XSetWMName(xdisplay_, xwindow_, &xtp);
    XFree(xtp.value);
  }
}

void X11Window::SetCapture() {}

void X11Window::ReleaseCapture() {}

void X11Window::ToggleFullscreen() {}

void X11Window::Maximize() {}

void X11Window::Minimize() {}

void X11Window::Restore() {}

void X11Window::SetCursor(PlatformCursor cursor) {
  XDefineCursor(xdisplay_, xwindow_, cursor);
}

void X11Window::MoveCursorTo(const gfx::Point& location) {}

void X11Window::ConfineCursorToBounds(const gfx::Rect& bounds) {
}

PlatformImeController* X11Window::GetPlatformImeController() {
  return nullptr;
}

bool X11Window::CanDispatchEvent(const PlatformEvent& event) {
  return FindXEventTarget(event) == xwindow_;
}

uint32_t X11Window::DispatchEvent(const PlatformEvent& event) {
  XEvent* xev = event;
  switch (xev->type) {
    case EnterNotify: {
      // EnterNotify creates ET_MOUSE_MOVED. Mark as synthesized as this is
      // not real mouse move event.
      MouseEvent mouse_event(xev);
      CHECK_EQ(ET_MOUSE_MOVED, mouse_event.type());
      mouse_event.set_flags(mouse_event.flags() | EF_IS_SYNTHESIZED);
      delegate_->DispatchEvent(&mouse_event);
      break;
    }
    case LeaveNotify: {
      MouseEvent mouse_event(xev);
      delegate_->DispatchEvent(&mouse_event);
      break;
    }

    case Expose: {
      gfx::Rect damage_rect(xev->xexpose.x,
                            xev->xexpose.y,
                            xev->xexpose.width,
                            xev->xexpose.height);
      delegate_->OnDamageRect(damage_rect);
      break;
    }

    case KeyPress:
    case KeyRelease: {
      KeyEvent key_event(xev);
      delegate_->DispatchEvent(&key_event);
      break;
    }

    case ButtonPress:
    case ButtonRelease: {
      switch (EventTypeFromNative(xev)) {
        case ET_MOUSEWHEEL: {
          MouseWheelEvent mouseev(xev);
          delegate_->DispatchEvent(&mouseev);
          break;
        }
        case ET_MOUSE_PRESSED:
        case ET_MOUSE_RELEASED: {
          MouseEvent mouseev(xev);
          delegate_->DispatchEvent(&mouseev);
          break;
        }
        case ET_UNKNOWN:
          // No event is created for X11-release events for mouse-wheel
          // buttons.
          break;
        default:
          NOTREACHED();
      }
      break;
    }

    case FocusOut:
      if (xev->xfocus.mode != NotifyGrab)
        delegate_->OnLostCapture();
      break;

    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      gfx::Rect bounds(xev->xconfigure.x,
                       xev->xconfigure.y,
                       xev->xconfigure.width,
                       xev->xconfigure.height);
      if (confirmed_bounds_ != bounds) {
        confirmed_bounds_ = bounds;
        delegate_->OnBoundsChanged(confirmed_bounds_);
      }
      break;
    }

    case ClientMessage: {
      Atom message = static_cast<Atom>(xev->xclient.data.l[0]);
      if (message == atom_cache_.GetAtom("WM_DELETE_WINDOW")) {
        delegate_->OnCloseRequest();
      } else if (message == atom_cache_.GetAtom("_NET_WM_PING")) {
        XEvent reply_event = *xev;
        reply_event.xclient.window = xroot_window_;

        XSendEvent(xdisplay_,
                   reply_event.xclient.window,
                   False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &reply_event);
        XFlush(xdisplay_);
      }
      break;
    }

    case GenericEvent: {
      ProcessXInput2Event(xev);
      break;
    }
  }
  return POST_DISPATCH_STOP_PROPAGATION;
}

namespace test {

void SetUseOverrideRedirectWindowByDefault(bool override_redirect) {
  g_override_redirect = override_redirect;
}

}  // namespace test
}  // namespace ui
