// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/root_window_host_linux.h"

#include <X11/Xatom.h>
#include <X11/cursorfont.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xfixes.h>
#include <X11/extensions/Xrandr.h>
#include <algorithm>

#include "base/message_pump_x.h"
#include "base/stl_util.h"
#include "base/stringprintf.h"
#include "ui/aura/client/user_gesture_client.h"
#include "ui/aura/dispatcher_linux.h"
#include "ui/aura/env.h"
#include "ui/aura/event.h"
#include "ui/aura/root_window.h"
#include "ui/base/cursor/cursor.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/base/x/x11_util.h"
#include "ui/base/view_prop.h"
#include "ui/gfx/compositor/layer.h"

using std::max;
using std::min;

namespace aura {

namespace {

// Standard Linux mouse buttons for going back and forward.
const int kBackMouseButton = 8;
const int kForwardMouseButton = 9;

const char kRootWindowHostLinuxKey[] = "__AURA_ROOT_WINDOW_HOST_LINUX__";

// The events reported for slave devices can have incorrect information for some
// fields. This utility function is used to check for such inconsistencies.
void CheckXEventForConsistency(XEvent* xevent) {
#if defined(USE_XI2_MT)
  static bool expect_master_event = false;
  static XIDeviceEvent slave_event;
  static gfx::Point slave_location;
  static int slave_button;

  // Note: If an event comes from a slave pointer device, then it will be
  // followed by the same event, but reported from its master pointer device.
  // However, if the event comes from a floating slave device (e.g. a
  // touchscreen), then it will not be followed by a duplicate event, since the
  // floating slave isn't attached to a master.

  bool was_expecting_master_event = expect_master_event;
  expect_master_event = false;

  if (!xevent || xevent->type != GenericEvent)
    return;

  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xevent->xcookie.data);
  if (xievent->evtype != XI_Motion &&
      xievent->evtype != XI_ButtonPress &&
      xievent->evtype != XI_ButtonRelease) {
    return;
  }

  if (xievent->sourceid == xievent->deviceid) {
    slave_event = *xievent;
    slave_location = ui::EventLocationFromNative(xevent);
    slave_button = ui::EventButtonFromNative(xevent);
    expect_master_event = true;
  } else if (was_expecting_master_event) {
    CHECK_EQ(slave_location.x(), ui::EventLocationFromNative(xevent).x());
    CHECK_EQ(slave_location.y(), ui::EventLocationFromNative(xevent).y());

    CHECK_EQ(slave_event.type, xievent->type);
    CHECK_EQ(slave_event.evtype, xievent->evtype);
    CHECK_EQ(slave_button, ui::EventButtonFromNative(xevent));
    CHECK_EQ(slave_event.flags, xievent->flags);
    CHECK_EQ(slave_event.buttons.mask_len, xievent->buttons.mask_len);
    CHECK_EQ(slave_event.valuators.mask_len, xievent->valuators.mask_len);
    CHECK_EQ(slave_event.mods.base, xievent->mods.base);
    CHECK_EQ(slave_event.mods.latched, xievent->mods.latched);
    CHECK_EQ(slave_event.mods.locked, xievent->mods.locked);
    CHECK_EQ(slave_event.mods.effective, xievent->mods.effective);
  }
#endif  // defined(USE_XI2_MT)
}

// Returns X font cursor shape from an Aura cursor.
int CursorShapeFromNative(gfx::NativeCursor native_cursor) {
  switch (native_cursor.native_type()) {
    case ui::kCursorNull:
      return XC_left_ptr;
    case ui::kCursorPointer:
      return XC_left_ptr;
    case ui::kCursorCross:
      return XC_crosshair;
    case ui::kCursorHand:
      return XC_hand2;
    case ui::kCursorIBeam:
      return XC_xterm;
    case ui::kCursorWait:
      return XC_watch;
    case ui::kCursorHelp:
      return XC_question_arrow;
    case ui::kCursorEastResize:
      return XC_right_side;
    case ui::kCursorNorthResize:
      return XC_top_side;
    case ui::kCursorNorthEastResize:
      return XC_top_right_corner;
    case ui::kCursorNorthWestResize:
      return XC_top_left_corner;
    case ui::kCursorSouthResize:
      return XC_bottom_side;
    case ui::kCursorSouthEastResize:
      return XC_bottom_right_corner;
    case ui::kCursorSouthWestResize:
      return XC_bottom_left_corner;
    case ui::kCursorWestResize:
      return XC_left_side;
    case ui::kCursorNorthSouthResize:
      return XC_sb_v_double_arrow;
    case ui::kCursorEastWestResize:
      return XC_sb_h_double_arrow;
    case ui::kCursorNorthEastSouthWestResize:
    case ui::kCursorNorthWestSouthEastResize:
      // There isn't really a useful cursor available for these.
      return XC_left_ptr;
    case ui::kCursorColumnResize:
      return XC_sb_h_double_arrow;
    case ui::kCursorRowResize:
      return XC_sb_v_double_arrow;
    case ui::kCursorMiddlePanning:
      return XC_fleur;
    case ui::kCursorEastPanning:
      return XC_sb_right_arrow;
    case ui::kCursorNorthPanning:
      return XC_sb_up_arrow;
    case ui::kCursorNorthEastPanning:
      return XC_top_right_corner;
    case ui::kCursorNorthWestPanning:
      return XC_top_left_corner;
    case ui::kCursorSouthPanning:
      return XC_sb_down_arrow;
    case ui::kCursorSouthEastPanning:
      return XC_bottom_right_corner;
    case ui::kCursorSouthWestPanning:
      return XC_bottom_left_corner;
    case ui::kCursorWestPanning:
      return XC_sb_left_arrow;
    case ui::kCursorMove:
      return XC_fleur;
    case ui::kCursorVerticalText:
    case ui::kCursorCell:
    case ui::kCursorContextMenu:
    case ui::kCursorAlias:
    case ui::kCursorProgress:
    case ui::kCursorNoDrop:
    case ui::kCursorCopy:
    case ui::kCursorNone:
    case ui::kCursorNotAllowed:
    case ui::kCursorZoomIn:
    case ui::kCursorZoomOut:
    case ui::kCursorGrab:
    case ui::kCursorGrabbing:
      // TODO(jamescook): Need cursors for these.  crbug.com/111650
      return XC_left_ptr;
    case ui::kCursorCustom:
      NOTREACHED();
      return XC_left_ptr;
  }
  NOTREACHED();
  return XC_left_ptr;
}

// Coalesce all pending motion events that are at the top of the queue, and
// return the number eliminated, storing the last one in |last_event|.
int CoalescePendingXIMotionEvents(const XEvent* xev, XEvent* last_event) {
  XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(xev->xcookie.data);
  int num_coalesed = 0;
  Display* display = xev->xany.display;

  while (XPending(display)) {
    XEvent next_event;
    XPeekEvent(display, &next_event);

    // If we can't get the cookie, abort the check.
    if (!XGetEventData(next_event.xgeneric.display, &next_event.xcookie))
      return num_coalesed;

    // If this isn't from a valid device, throw the event away, as
    // that's what the message pump would do. Device events come in pairs
    // with one from the master and one from the slave so there will
    // always be at least one pending.
    if (!ui::TouchFactory::GetInstance()->ShouldProcessXI2Event(&next_event)) {
      CheckXEventForConsistency(&next_event);
      XFreeEventData(display, &next_event.xcookie);
      XNextEvent(display, &next_event);
      continue;
    }

    if (next_event.type == GenericEvent &&
        next_event.xgeneric.evtype == XI_Motion &&
        !ui::GetScrollOffsets(&next_event, NULL, NULL)) {
      XIDeviceEvent* next_xievent =
          static_cast<XIDeviceEvent*>(next_event.xcookie.data);
      // Confirm that the motion event is targeted at the same window
      // and that no buttons or modifiers have changed.
      if (xievent->event == next_xievent->event &&
          xievent->child == next_xievent->child &&
          xievent->buttons.mask_len == next_xievent->buttons.mask_len &&
          (memcmp(xievent->buttons.mask,
                  next_xievent->buttons.mask,
                  xievent->buttons.mask_len) == 0) &&
          xievent->mods.base == next_xievent->mods.base &&
          xievent->mods.latched == next_xievent->mods.latched &&
          xievent->mods.locked == next_xievent->mods.locked &&
          xievent->mods.effective == next_xievent->mods.effective) {
        XFreeEventData(display, &next_event.xcookie);
        // Free the previous cookie.
        if (num_coalesed > 0)
          XFreeEventData(display, &last_event->xcookie);
        // Get the event and its cookie data.
        XNextEvent(display, last_event);
        XGetEventData(display, &last_event->xcookie);
        CheckXEventForConsistency(last_event);
        ++num_coalesed;
        continue;
      } else {
        // This isn't an event we want so free its cookie data.
        XFreeEventData(display, &next_event.xcookie);
      }
    }
    break;
  }
  return num_coalesed;
}

// We emulate Windows' WM_KEYDOWN and WM_CHAR messages.  WM_CHAR events are only
// generated for certain keys; see
// http://msdn.microsoft.com/en-us/library/windows/desktop/ms646268.aspx.  Per
// discussion on http://crbug.com/108480, char events should furthermore not be
// generated for Tab, Escape, and Backspace.
bool ShouldSendCharEventForKeyboardCode(ui::KeyboardCode keycode) {
  if ((keycode >= ui::VKEY_0 && keycode <= ui::VKEY_9) ||
      (keycode >= ui::VKEY_A && keycode <= ui::VKEY_Z) ||
      (keycode >= ui::VKEY_NUMPAD0 && keycode <= ui::VKEY_NUMPAD9)) {
    return true;
  }

  switch (keycode) {
    case ui::VKEY_RETURN:
    case ui::VKEY_SPACE:
    // In addition to the keys listed at MSDN, we include other
    // graphic-character and numpad keys.
    case ui::VKEY_MULTIPLY:
    case ui::VKEY_ADD:
    case ui::VKEY_SUBTRACT:
    case ui::VKEY_DECIMAL:
    case ui::VKEY_DIVIDE:
    case ui::VKEY_OEM_1:
    case ui::VKEY_OEM_2:
    case ui::VKEY_OEM_3:
    case ui::VKEY_OEM_4:
    case ui::VKEY_OEM_5:
    case ui::VKEY_OEM_6:
    case ui::VKEY_OEM_7:
    case ui::VKEY_OEM_102:
    case ui::VKEY_OEM_PLUS:
    case ui::VKEY_OEM_COMMA:
    case ui::VKEY_OEM_MINUS:
    case ui::VKEY_OEM_PERIOD:
      return true;
    default:
      return false;
  }
}

// A list of atoms that we'll intern on host creation to save roundtrips to the
// X11 server. Must be kept in sync with RootWindowHostLinux::AtomList
const char* kAtomList[] = {
  "WM_DELETE_WINDOW",
  "_NET_WM_PING",
  "_NET_WM_PID",
  "WM_S0"
};

}  // namespace

RootWindowHostLinux::RootWindowHostLinux(const gfx::Rect& bounds)
    : root_window_(NULL),
      xdisplay_(base::MessagePumpX::GetDefaultXDisplay()),
      xwindow_(0),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      current_cursor_(ui::kCursorNull),
      cursor_shown_(true),
      bounds_(bounds),
      focus_when_shown_(false),
      pointer_barriers_(NULL) {
  XSetWindowAttributes swa;
  memset(&swa, 0, sizeof(swa));
  swa.background_pixmap = None;
  xwindow_ = XCreateWindow(
      xdisplay_, x_root_window_,
      bounds.x(), bounds.y(), bounds.width(), bounds.height(),
      0,               // border width
      CopyFromParent,  // depth
      InputOutput,
      CopyFromParent,  // visual
      CWBackPixmap,
      &swa);
  static_cast<DispatcherLinux*>(Env::GetInstance()->GetDispatcher())->
      WindowDispatcherCreated(xwindow_, this);

  prop_.reset(new ui::ViewProp(xwindow_, kRootWindowHostLinuxKey, this));

  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask |
                    EnterWindowMask | LeaveWindowMask |
                    ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);
  XFlush(xdisplay_);

  if (base::MessagePumpForUI::HasXInput2())
    ui::TouchFactory::GetInstance()->SetupXI2ForXWindow(xwindow_);

  // Initialize invisible cursor.
  char nodata[] = { 0, 0, 0, 0, 0, 0, 0, 0 };
  XColor black;
  black.red = black.green = black.blue = 0;
  Pixmap blank = XCreateBitmapFromData(xdisplay_, xwindow_,
                                       nodata, 8, 8);
  invisible_cursor_ = XCreatePixmapCursor(xdisplay_, blank, blank,
                                          &black, &black, 0, 0);
  XFreePixmap(xdisplay_, blank);

  if (RootWindow::hide_host_cursor())
    XDefineCursor(xdisplay_, x_root_window_, invisible_cursor_);

  // Grab all the atoms we need now to minimize roundtrips to the X11 server.
  XInternAtoms(xdisplay_, const_cast<char**>(kAtomList), ATOM_COUNT, False,
               cached_atoms_);

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  ::Atom protocols[2];
  protocols[0] = cached_atoms_[ATOM_WM_DELETE_WINDOW];
  protocols[1] = cached_atoms_[ATOM__NET_WM_PING];
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  pid_t pid = getpid();
  XChangeProperty(xdisplay_,
                  xwindow_,
                  cached_atoms_[ATOM__NET_WM_PID],
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&pid), 1);

  // crbug.com/120229 - set the window title so gtalk can find the primary root
  // window to broadcast.
  // TODO(jhorwich) Remove this once Chrome supports window-based broadcasting.
  static int root_window_number = 0;
  std::string name = StringPrintf("aura_root_%d", root_window_number++);
  XStoreName(xdisplay_, xwindow_, name.c_str());
}

RootWindowHostLinux::~RootWindowHostLinux() {
  static_cast<DispatcherLinux*>(Env::GetInstance()->GetDispatcher())->
      WindowDispatcherDestroying(xwindow_);

  UnConfineCursor();

  XDestroyWindow(xdisplay_, xwindow_);

  // Clears XCursorCache.
  ui::GetXCursor(ui::kCursorClearXCursorCache);

  XFreeCursor(xdisplay_, invisible_cursor_);
}

bool RootWindowHostLinux::Dispatch(const base::NativeEvent& event) {
  XEvent* xev = event;

  CheckXEventForConsistency(xev);

  switch (xev->type) {
    case Expose:
      root_window_->ScheduleFullDraw();
      break;
    case KeyPress: {
      KeyEvent keydown_event(xev, false);
      root_window_->DispatchKeyEvent(&keydown_event);
      break;
    }
    case KeyRelease: {
      KeyEvent keyup_event(xev, false);
      root_window_->DispatchKeyEvent(&keyup_event);
      break;
    }
    case ButtonPress: {
      if (static_cast<int>(xev->xbutton.button) == kBackMouseButton ||
          static_cast<int>(xev->xbutton.button) == kForwardMouseButton) {
        client::UserGestureClient* gesture_client =
            client::GetUserGestureClient(root_window_);
        if (gesture_client) {
          gesture_client->OnUserGesture(
              static_cast<int>(xev->xbutton.button) == kBackMouseButton ?
              client::UserGestureClient::GESTURE_BACK :
              client::UserGestureClient::GESTURE_FORWARD);
        }
        break;
      }
    }  // fallthrough
    case ButtonRelease: {
      MouseEvent mouseev(xev);
      root_window_->DispatchMouseEvent(&mouseev);
      break;
    }
    case FocusOut:
      if (xev->xfocus.mode != NotifyGrab)
        root_window_->SetCapture(NULL, 0);
      break;
    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      // Update barrier and mouse location when the root window has
      // moved/resized.
      if (pointer_barriers_.get()) {
        UnConfineCursor();
        gfx::Point p = root_window_->last_mouse_location();
        XWarpPointer(xdisplay_, None,  xwindow_, 0, 0, 0, 0, p.x(), p.y());
        ConfineCursorToRootWindow();
      }
      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the root window size is maintained properly.
      gfx::Rect bounds(xev->xconfigure.x, xev->xconfigure.y,
                       xev->xconfigure.width, xev->xconfigure.height);
      bool size_changed = bounds_.size() != bounds.size();
      bounds_ = bounds;
      if (size_changed)
        root_window_->OnHostResized(bounds.size());
      break;
    }
    case GenericEvent: {
      ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(xev))
        break;

      ui::EventType type = ui::EventTypeFromNative(xev);
      XEvent last_event;
      int num_coalesced = 0;

      switch (type) {
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_TOUCH_RELEASED:
        case ui::ET_TOUCH_MOVED: {
          TouchEvent touchev(xev);
          root_window_->DispatchTouchEvent(&touchev);
          break;
        }
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED:
        case ui::ET_MOUSEWHEEL:
        case ui::ET_MOUSE_ENTERED:
        case ui::ET_MOUSE_EXITED: {
          if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED) {
            // If this is a motion event, we want to coalesce all pending motion
            // events that are at the top of the queue.
            num_coalesced = CoalescePendingXIMotionEvents(xev, &last_event);
            if (num_coalesced > 0)
              xev = &last_event;
          } else if (type == ui::ET_MOUSE_PRESSED) {
            XIDeviceEvent* xievent =
                static_cast<XIDeviceEvent*>(xev->xcookie.data);
            int button = xievent->detail;
            if (button == kBackMouseButton || button == kForwardMouseButton) {
              client::UserGestureClient* gesture_client =
                  client::GetUserGestureClient(root_window_);
              if (gesture_client) {
                bool reverse_direction =
                    ui::IsTouchpadEvent(xev) && ui::IsNaturalScrollEnabled();
                gesture_client->OnUserGesture(
                    (button == kBackMouseButton && !reverse_direction) ||
                    (button == kForwardMouseButton && reverse_direction) ?
                    client::UserGestureClient::GESTURE_BACK :
                    client::UserGestureClient::GESTURE_FORWARD);
              }
              break;
            }
          }
          MouseEvent mouseev(xev);
          root_window_->DispatchMouseEvent(&mouseev);
          break;
        }
        case ui::ET_SCROLL_FLING_START:
        case ui::ET_SCROLL_FLING_CANCEL:
        case ui::ET_SCROLL: {
          ScrollEvent scrollev(xev);
          root_window_->DispatchScrollEvent(&scrollev);
          break;
        }
        case ui::ET_UNKNOWN:
          break;
        default:
          NOTREACHED();
      }

      // If we coalesced an event we need to free its cookie.
      if (num_coalesced > 0)
        XFreeEventData(xev->xgeneric.display, &last_event.xcookie);
      break;
    }
    case MapNotify: {
      // If there's no window manager running, we need to assign the X input
      // focus to our host window.
      if (!IsWindowManagerPresent() && focus_when_shown_)
        XSetInputFocus(xdisplay_, xwindow_, RevertToNone, CurrentTime);
      break;
    }
    case ClientMessage: {
      Atom message_type = static_cast<Atom>(xev->xclient.data.l[0]);
      if (message_type == cached_atoms_[ATOM_WM_DELETE_WINDOW]) {
        // We have received a close message from the window manager.
        root_window_->OnRootWindowHostClosed();
      } else if (message_type == cached_atoms_[ATOM__NET_WM_PING]) {
        XEvent reply_event = *xev;
        reply_event.xclient.window = x_root_window_;

        XSendEvent(xdisplay_,
                   reply_event.xclient.window,
                   False,
                   SubstructureRedirectMask | SubstructureNotifyMask,
                   &reply_event);
      }
      break;
    }
    case MappingNotify: {
      switch (xev->xmapping.request) {
        case MappingModifier:
        case MappingKeyboard:
          XRefreshKeyboardMapping(&xev->xmapping);
          root_window_->OnKeyboardMappingChanged();
          break;
        case MappingPointer:
          ui::UpdateButtonMap();
          break;
        default:
          NOTIMPLEMENTED() << " Unknown request: " << xev->xmapping.request;
          break;
      }
      break;
    }
    case MotionNotify: {
      // Discard all but the most recent motion event that targets the same
      // window with unchanged state.
      XEvent last_event;
      while (XPending(xev->xany.display)) {
        XEvent next_event;
        XPeekEvent(xev->xany.display, &next_event);
        if (next_event.type == MotionNotify &&
            next_event.xmotion.window == xev->xmotion.window &&
            next_event.xmotion.subwindow == xev->xmotion.subwindow &&
            next_event.xmotion.state == xev->xmotion.state) {
          XNextEvent(xev->xany.display, &last_event);
          xev = &last_event;
        } else {
          break;
        }
      }

      MouseEvent mouseev(xev);
      root_window_->DispatchMouseEvent(&mouseev);
      break;
    }
  }
  return true;
}

void RootWindowHostLinux::SetRootWindow(RootWindow* root_window) {
  root_window_ = root_window;
}

RootWindow* RootWindowHostLinux::GetRootWindow() {
  return root_window_;
}

gfx::AcceleratedWidget RootWindowHostLinux::GetAcceleratedWidget() {
  return xwindow_;
}

void RootWindowHostLinux::Show() {
  // Before we map the window, set size hints. Otherwise, some window managers
  // will ignore toplevel XMoveWindow commands.
  XSizeHints size_hints;
  size_hints.flags = PPosition;
  size_hints.x = bounds_.x();
  size_hints.y = bounds_.y();
  XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

  XMapWindow(xdisplay_, xwindow_);
}

void RootWindowHostLinux::ToggleFullScreen() {
  NOTIMPLEMENTED();
}

gfx::Rect RootWindowHostLinux::GetBounds() const {
  return bounds_;
}

void RootWindowHostLinux::SetBounds(const gfx::Rect& bounds) {
  bool size_changed = bounds_.size() != bounds.size();
  if (bounds == bounds_) {
    root_window_->SchedulePaintInRect(root_window_->bounds());
    return;
  }

  if (bounds.size() != bounds_.size())
    XResizeWindow(xdisplay_, xwindow_, bounds.width(), bounds.height());

  if (bounds.origin() != bounds_.origin())
    XMoveWindow(xdisplay_, xwindow_, bounds.x(), bounds.y());

  // Assume that the resize will go through as requested, which should be the
  // case if we're running without a window manager.  If there's a window
  // manager, it can modify or ignore the request, but (per ICCCM) we'll get a
  // (possibly synthetic) ConfigureNotify about the actual size and correct
  // |bounds_| later.
  bounds_ = bounds;
  if (size_changed)
    root_window_->OnHostResized(bounds.size());
}

gfx::Point RootWindowHostLinux::GetLocationOnNativeScreen() const {
  return bounds_.origin();
}

void RootWindowHostLinux::SetCapture() {
}

void RootWindowHostLinux::ReleaseCapture() {
}

void RootWindowHostLinux::SetCursor(gfx::NativeCursor cursor) {
  if (cursor == current_cursor_)
    return;
  current_cursor_ = cursor;

  if (cursor_shown_)
    SetCursorInternal(cursor);
}

void RootWindowHostLinux::ShowCursor(bool show) {
  if (show == cursor_shown_)
    return;
  cursor_shown_ = show;
  SetCursorInternal(show ? current_cursor_ : ui::kCursorNone);
}

gfx::Point RootWindowHostLinux::QueryMouseLocation() {
  ::Window root_return, child_return;
  int root_x_return, root_y_return, win_x_return, win_y_return;
  unsigned int mask_return;
  XQueryPointer(xdisplay_,
                xwindow_,
                &root_return,
                &child_return,
                &root_x_return, &root_y_return,
                &win_x_return, &win_y_return,
                &mask_return);
  return gfx::Point(max(0, min(bounds_.width(), win_x_return)),
                    max(0, min(bounds_.height(), win_y_return)));
}

bool RootWindowHostLinux::ConfineCursorToRootWindow() {
#if XFIXES_MAJOR >= 5
  DCHECK(!pointer_barriers_.get());
  if (pointer_barriers_.get())
    return false;
  // Pointer barriers extend all the way across the screen to
  // avoid leaks at the corners.
  gfx::Size screen_size = RootWindowHost::GetNativeScreenSize();
  pointer_barriers_.reset(new XID[4]);
  // Horizontal barriers.
  pointer_barriers_[0] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_,
      0, bounds_.y(), screen_size.width(), bounds_.y(),
      BarrierPositiveY,
      0, NULL);  // default device
  pointer_barriers_[1] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_,
      0, bounds_.bottom(), screen_size.width(),  bounds_.bottom(),
      BarrierNegativeY,
      0, NULL);  // default device
  // Vertical barriers.
  pointer_barriers_[2] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_,
      bounds_.x(), 0, bounds_.x(), screen_size.height(),
      BarrierPositiveX,
      0, NULL);  // default device
  pointer_barriers_[3] = XFixesCreatePointerBarrier(
      xdisplay_, x_root_window_,
      bounds_.right(), 0, bounds_.right(), screen_size.height(),
      BarrierNegativeX,
      0, NULL);  // default device
#endif
  return true;
}

void RootWindowHostLinux::UnConfineCursor() {
#if XFIXES_MAJOR >= 5
  if (pointer_barriers_.get()) {
    XFixesDestroyPointerBarrier(xdisplay_, pointer_barriers_[0]);
    XFixesDestroyPointerBarrier(xdisplay_, pointer_barriers_[1]);
    XFixesDestroyPointerBarrier(xdisplay_, pointer_barriers_[2]);
    XFixesDestroyPointerBarrier(xdisplay_, pointer_barriers_[3]);
    pointer_barriers_.reset();
  }
#endif
}

void RootWindowHostLinux::MoveCursorTo(const gfx::Point& location) {
  XWarpPointer(xdisplay_, None, xwindow_, 0, 0, 0, 0, location.x(),
      location.y());
}

void RootWindowHostLinux::SetFocusWhenShown(bool focus_when_shown) {
  static const char* k_NET_WM_USER_TIME = "_NET_WM_USER_TIME";
  focus_when_shown_ = focus_when_shown;
  if (IsWindowManagerPresent() && !focus_when_shown_) {
    ui::SetIntProperty(xwindow_,
                       k_NET_WM_USER_TIME,
                       k_NET_WM_USER_TIME,
                       0);
  }
}

void RootWindowHostLinux::PostNativeEvent(
    const base::NativeEvent& native_event) {
  DCHECK(xwindow_);
  DCHECK(xdisplay_);
  XEvent xevent = *native_event;
  xevent.xany.display = xdisplay_;
  xevent.xany.window = xwindow_;

  switch (xevent.type) {
    case EnterNotify:
    case LeaveNotify:
    case MotionNotify:
    case KeyPress:
    case KeyRelease:
    case ButtonPress:
    case ButtonRelease: {
      // The fields used below are in the same place for all of events
      // above. Using xmotion from XEvent's unions to avoid repeating
      // the code.
      xevent.xmotion.root = x_root_window_;
      xevent.xmotion.time = CurrentTime;

      gfx::Point point(xevent.xmotion.x, xevent.xmotion.y);
      root_window_->ConvertPointToNativeScreen(&point);
      xevent.xmotion.x_root = point.x();
      xevent.xmotion.y_root = point.y();
    }
    default:
      break;
  }
  XSendEvent(xdisplay_, xwindow_, False, 0, &xevent);
}

bool RootWindowHostLinux::IsWindowManagerPresent() {
  // Per ICCCM 2.8, "Manager Selections", window managers should take ownership
  // of WM_Sn selections (where n is a screen number).
  return XGetSelectionOwner(xdisplay_, cached_atoms_[ATOM_WM_S0]) != None;
}

void RootWindowHostLinux::SetCursorInternal(gfx::NativeCursor cursor) {
  ::Cursor xcursor =
      cursor == ui::kCursorNone ?
      invisible_cursor_ :
      cursor == ui::kCursorCustom ?
      cursor.platform() :
      ui::GetXCursor(CursorShapeFromNative(cursor));
  XDefineCursor(xdisplay_, xwindow_, xcursor);
}

// static
RootWindowHost* RootWindowHost::Create(const gfx::Rect& bounds) {
  return new RootWindowHostLinux(bounds);
}

// static
RootWindowHost* RootWindowHost::GetForAcceleratedWidget(
    gfx::AcceleratedWidget accelerated_widget) {
  return reinterpret_cast<RootWindowHost*>(
      ui::ViewProp::GetValue(accelerated_widget, kRootWindowHostLinuxKey));
}

// static
gfx::Size RootWindowHost::GetNativeScreenSize() {
  ::Display* xdisplay = base::MessagePumpX::GetDefaultXDisplay();
  return gfx::Size(DisplayWidth(xdisplay, 0), DisplayHeight(xdisplay, 0));
}

}  // namespace aura
