// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/aura/desktop_host.h"

#include "base/message_loop.h"
#include "base/message_pump_x.h"
#include "ui/aura/cursor.h"
#include "ui/aura/desktop.h"
#include "ui/aura/event.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/base/x/x11_util.h"

#include <X11/cursorfont.h>
#include <X11/Xlib.h>

namespace aura {

namespace {

// Returns X font cursor shape from an Aura cursor.
int CursorShapeFromNative(gfx::NativeCursor native_cursor) {
  switch (native_cursor) {
    case aura::kCursorNull:
      return XC_left_ptr;
    case aura::kCursorPointer:
      return XC_left_ptr;
    case aura::kCursorCross:
      return XC_crosshair;
    case aura::kCursorHand:
      return XC_hand2;
    case aura::kCursorIBeam:
      return XC_xterm;
    case aura::kCursorWait:
      return XC_watch;
    case aura::kCursorHelp:
      return XC_question_arrow;
    case aura::kCursorEastResize:
      return XC_right_side;
    case aura::kCursorNorthResize:
      return XC_top_side;
    case aura::kCursorNorthEastResize:
      return XC_top_right_corner;
    case aura::kCursorNorthWestResize:
      return XC_top_left_corner;
    case aura::kCursorSouthResize:
      return XC_bottom_side;
    case aura::kCursorSouthEastResize:
      return XC_bottom_right_corner;
    case aura::kCursorSouthWestResize:
      return XC_bottom_left_corner;
    case aura::kCursorWestResize:
      return XC_left_side;
    case aura::kCursorNorthSouthResize:
      return XC_sb_v_double_arrow;
    case aura::kCursorEastWestResize:
      return XC_sb_h_double_arrow;
    case aura::kCursorNorthEastSouthWestResize:
    case aura::kCursorNorthWestSouthEastResize:
      // There isn't really a useful cursor available for these.
      NOTIMPLEMENTED();
      return XC_left_ptr;
    case aura::kCursorColumnResize:
      return XC_sb_h_double_arrow;
    case aura::kCursorRowResize:
      return XC_sb_v_double_arrow;
    case aura::kCursorMiddlePanning:
      return XC_fleur;
    case aura::kCursorEastPanning:
      return XC_sb_right_arrow;
    case aura::kCursorNorthPanning:
      return XC_sb_up_arrow;
    case aura::kCursorNorthEastPanning:
      return XC_top_right_corner;
    case aura::kCursorNorthWestPanning:
      return XC_top_left_corner;
    case aura::kCursorSouthPanning:
      return XC_sb_down_arrow;
    case aura::kCursorSouthEastPanning:
      return XC_bottom_right_corner;
    case aura::kCursorSouthWestPanning:
      return XC_bottom_left_corner;
    case aura::kCursorWestPanning:
      return XC_sb_left_arrow;
    case aura::kCursorMove:
      return XC_fleur;
    case aura::kCursorVerticalText:
    case aura::kCursorCell:
    case aura::kCursorContextMenu:
    case aura::kCursorAlias:
    case aura::kCursorProgress:
    case aura::kCursorNoDrop:
    case aura::kCursorCopy:
    case aura::kCursorNone:
    case aura::kCursorNotAllowed:
    case aura::kCursorZoomIn:
    case aura::kCursorZoomOut:
    case aura::kCursorGrab:
    case aura::kCursorGrabbing:
    case aura::kCursorCustom:
      // TODO(jamescook): Need cursors for these.
      NOTIMPLEMENTED();
      return XC_left_ptr;
  }
  NOTREACHED();
  return XC_left_ptr;
}

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
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual void SetSize(const gfx::Size& size) OVERRIDE;
  virtual void SetCursor(gfx::NativeCursor cursor_type) OVERRIDE;
  virtual gfx::Point QueryMouseLocation() OVERRIDE;

  Desktop* desktop_;

  // The display and the native X window hosting the desktop.
  Display* xdisplay_;
  ::Window xwindow_;

  // Current Aura cursor.
  gfx::NativeCursor current_cursor_;

  // The size of |xwindow_|.
  gfx::Rect bounds_;

  DISALLOW_COPY_AND_ASSIGN(DesktopHostLinux);
};

DesktopHostLinux::DesktopHostLinux(const gfx::Rect& bounds)
    : desktop_(NULL),
      xdisplay_(NULL),
      xwindow_(0),
      current_cursor_(aura::kCursorNull),
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

  if (base::MessagePumpForUI::HasXInput2())
    ui::TouchFactory::GetInstance()->SetupXI2ForXWindow(xwindow_);
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
    case ButtonRelease: {
      MouseEvent mouseev(xev);
      handled = desktop_->OnMouseEvent(mouseev);
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
      handled = desktop_->OnMouseEvent(mouseev);
      break;
    }
    case ConfigureNotify: {
      DCHECK_EQ(xdisplay_, xev->xconfigure.display);
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      DCHECK_EQ(xwindow_, xev->xconfigure.event);

      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the desktop size is maintained properly.
      gfx::Size size(xev->xconfigure.width, xev->xconfigure.height);
      if (bounds_.size() != size)
        bounds_.set_size(size);
      desktop_->OnHostResized(size);
      handled = true;
      break;
    }

    case GenericEvent: {
      ui::TouchFactory* factory = ui::TouchFactory::GetInstance();
      if (!factory->ShouldProcessXI2Event(xev))
        break;
      ui::EventType type = ui::EventTypeFromNative(xev);
      switch (type) {
        case ui::ET_TOUCH_PRESSED:
        case ui::ET_TOUCH_RELEASED:
        case ui::ET_TOUCH_MOVED: {
          TouchEvent touchev(xev);
          handled = desktop_->OnTouchEvent(touchev);
          break;
        }
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED:
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
        case ui::ET_MOUSEWHEEL:
        case ui::ET_MOUSE_ENTERED:
        case ui::ET_MOUSE_EXITED: {
          MouseEvent mouseev(xev);
          handled = desktop_->OnMouseEvent(mouseev);
          break;
        }
        case ui::ET_UNKNOWN:
          handled = false;
          break;
        default:
          NOTREACHED();
      }
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

gfx::Size DesktopHostLinux::GetSize() const {
  return bounds_.size();
}

void DesktopHostLinux::SetSize(const gfx::Size& size) {
  if (bounds_.size() == size)
    return;
  bounds_.set_size(size);
  XResizeWindow(xdisplay_, xwindow_, size.width(), size.height());
}

void DesktopHostLinux::SetCursor(gfx::NativeCursor cursor) {
  if (current_cursor_ == cursor)
    return;
  current_cursor_ = cursor;
  // Custom web cursors are handled directly.
  if (cursor == kCursorCustom)
    return;
  int cursor_shape = CursorShapeFromNative(cursor);
  ::Cursor xcursor = ui::GetXCursor(cursor_shape);
  XDefineCursor(xdisplay_, xwindow_, xcursor);
}

gfx::Point DesktopHostLinux::QueryMouseLocation() {
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
  return gfx::Point(win_x_return, win_y_return);
}

}  // namespace

// static
DesktopHost* DesktopHost::Create(const gfx::Rect& bounds) {
  return new DesktopHostLinux(bounds);
}

}  // namespace aura
