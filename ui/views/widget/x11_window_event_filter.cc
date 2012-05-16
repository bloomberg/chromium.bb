// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/x11_window_event_filter.h"

#include <X11/extensions/XInput.h>
#include <X11/extensions/XInput2.h>

#include "base/message_pump_x.h"
#include "ui/aura/root_window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/base/x/x11_atom_cache.h"
#include "ui/base/x/x11_atom_cache.h"

namespace {

// These constants are defined in the Extended Window Manager Hints
// standard...and aren't in any header that I can find.
const int k_NET_WM_MOVERESIZE_SIZE_TOPLEFT =     0;
const int k_NET_WM_MOVERESIZE_SIZE_TOP =         1;
const int k_NET_WM_MOVERESIZE_SIZE_TOPRIGHT =    2;
const int k_NET_WM_MOVERESIZE_SIZE_RIGHT =       3;
const int k_NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT = 4;
const int k_NET_WM_MOVERESIZE_SIZE_BOTTOM =      5;
const int k_NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT =  6;
const int k_NET_WM_MOVERESIZE_SIZE_LEFT =        7;
const int k_NET_WM_MOVERESIZE_MOVE =             8;

// This data structure represents additional hints that we send to the window
// manager and has a direct lineage back to Motif, which defined this de facto
// standard. This struct doesn't seem 64-bit safe though, but it's what GDK
// does.
typedef struct {
  unsigned long flags;
  unsigned long functions;
  unsigned long decorations;
  long input_mode;
  unsigned long status;
} MotifWmHints;

// The bitflag in |flags| in MotifWmHints that signals that the reader should
// pay attention to the value in |decorations|.
const unsigned long kHintsDecorations = (1L << 1);

}  // namespace

namespace views {

X11WindowEventFilter::X11WindowEventFilter(aura::RootWindow* root_window)
    : root_window_(root_window),
      xdisplay_(base::MessagePumpX::GetDefaultXDisplay()),
      xwindow_(root_window_->GetAcceleratedWidget()),
      x_root_window_(DefaultRootWindow(xdisplay_)) {
}

X11WindowEventFilter::~X11WindowEventFilter() {}

void X11WindowEventFilter::SetUseHostWindowBorders(bool use_os_border) {
  ui::X11AtomCache* cache = ui::X11AtomCache::GetInstance();
  MotifWmHints motif_hints;
  memset(&motif_hints, 0, sizeof(motif_hints));
  motif_hints.flags = kHintsDecorations;
  motif_hints.decorations = use_os_border ? 1 : 0;

  ::Atom hint_atom = cache->GetAtom(ui::ATOM__MOTIF_WM_HINTS);
  XChangeProperty(base::MessagePumpX::GetDefaultXDisplay(),
                  xwindow_,
                  hint_atom,
                  hint_atom,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&motif_hints),
                  sizeof(MotifWmHints)/sizeof(long));
}

bool X11WindowEventFilter::PreHandleKeyEvent(aura::Window* target,
                                             aura::KeyEvent* event) {
  return false;
}

bool X11WindowEventFilter::PreHandleMouseEvent(aura::Window* target,
                                               aura::MouseEvent* event) {
  if (event->type() != ui::ET_MOUSE_PRESSED)
    return false;

  int component =
      target->delegate()->GetNonClientComponent(event->location());
  if (component == HTCLIENT)
    return false;

  // Get the |x_root_window_| location out of the native event.
  gfx::Point root_location;
  const base::NativeEvent& native_event = event->native_event();
  switch (native_event->type) {
    case ButtonPress: {
      root_location.SetPoint(native_event->xbutton.x_root,
                             native_event->xbutton.y_root);
      break;
    }
    case GenericEvent: {
      XIDeviceEvent* xievent =
          static_cast<XIDeviceEvent*>(native_event->xcookie.data);
      root_location.SetPoint(xievent->root_x, xievent->root_y);
      break;
    }
    default: {
      NOTREACHED();
      return false;
    }
  }

  return DispatchHostWindowDragMovement(component, root_location);
}

ui::TouchStatus X11WindowEventFilter::PreHandleTouchEvent(
    aura::Window* target,
    aura::TouchEvent* event) {
  return ui::TOUCH_STATUS_UNKNOWN;
}

ui::GestureStatus X11WindowEventFilter::PreHandleGestureEvent(
    aura::Window* target,
    aura::GestureEvent* event) {
  return ui::GESTURE_STATUS_UNKNOWN;
}

bool X11WindowEventFilter::DispatchHostWindowDragMovement(
    int hittest,
    const gfx::Point& screen_location) {
  int direction = -1;
  switch (hittest) {
    case HTBOTTOM:
      direction = k_NET_WM_MOVERESIZE_SIZE_BOTTOM;
      break;
    case HTBOTTOMLEFT:
      direction = k_NET_WM_MOVERESIZE_SIZE_BOTTOMLEFT;
      break;
    case HTBOTTOMRIGHT:
      direction = k_NET_WM_MOVERESIZE_SIZE_BOTTOMRIGHT;
      break;
    case HTCAPTION:
      direction = k_NET_WM_MOVERESIZE_MOVE;
      break;
    case HTLEFT:
      direction = k_NET_WM_MOVERESIZE_SIZE_LEFT;
      break;
    case HTRIGHT:
      direction = k_NET_WM_MOVERESIZE_SIZE_RIGHT;
      break;
    case HTTOP:
      direction = k_NET_WM_MOVERESIZE_SIZE_TOP;
      break;
    case HTTOPLEFT:
      direction = k_NET_WM_MOVERESIZE_SIZE_TOPLEFT;
      break;
    case HTTOPRIGHT:
      direction = k_NET_WM_MOVERESIZE_SIZE_TOPRIGHT;
      break;
    default:
      return false;
  }

  // We most likely have an implicit grab right here. We need to dump it
  // because what we're about to do is tell the window manager
  // that it's now responsible for moving the window around; it immediately
  // grabs when it receives the event below.
  XUngrabPointer(xdisplay_, CurrentTime);

  XEvent event;
  memset(&event, 0, sizeof(event));
  event.xclient.type = ClientMessage;
  event.xclient.display = xdisplay_;
  event.xclient.window = xwindow_;
  event.xclient.message_type = ui::X11AtomCache::GetInstance()->GetAtom(
      ui::ATOM__NET_WM_MOVERESIZE);
  event.xclient.format = 32;
  event.xclient.data.l[0] = screen_location.x();
  event.xclient.data.l[1] = screen_location.y();
  event.xclient.data.l[2] = direction;
  event.xclient.data.l[3] = 0;
  event.xclient.data.l[4] = 0;

  XSendEvent(xdisplay_, x_root_window_, False,
             SubstructureRedirectMask | SubstructureNotifyMask,
             &event);

  return true;
}

}  // namespace views

