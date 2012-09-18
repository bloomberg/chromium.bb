// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/widget/desktop_root_window_host_linux.h"

#include <X11/extensions/XInput2.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include "base/message_pump_aurax11.h"
#include "base/stringprintf.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/client/user_action_client.h"
#include "ui/aura/desktop/desktop_activation_client.h"
#include "ui/aura/desktop/desktop_dispatcher_client.h"
#include "ui/aura/focus_manager.h"
#include "ui/aura/root_window.h"
#include "ui/aura/shared/compound_event_filter.h"
#include "ui/aura/shared/input_method_event_filter.h"
#include "ui/base/touch/touch_factory.h"
#include "ui/base/x/x11_util.h"
#include "ui/views/widget/desktop_capture_client.h"
#include "ui/views/widget/x11_desktop_handler.h"
#include "ui/views/widget/x11_window_event_filter.h"

namespace views {

namespace {

// Standard Linux mouse buttons for going back and forward.
const int kBackMouseButton = 8;
const int kForwardMouseButton = 9;

const char* kAtomsToCache[] = {
  "WM_DELETE_WINDOW",
  "_NET_WM_PING",
  "_NET_WM_PID",
  "WM_S0",
  NULL
};

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, public:

DesktopRootWindowHostLinux::DesktopRootWindowHostLinux()
    : xdisplay_(base::MessagePumpAuraX11::GetDefaultXDisplay()),
      xwindow_(0),
      x_root_window_(DefaultRootWindow(xdisplay_)),
      atom_cache_(xdisplay_, kAtomsToCache),
      window_mapped_(false),
      focus_when_shown_(false) {
}

DesktopRootWindowHostLinux::~DesktopRootWindowHostLinux() {
  base::MessagePumpAuraX11::Current()->RemoveDispatcherForWindow(xwindow_);
  XDestroyWindow(xdisplay_, xwindow_);
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, private:

void DesktopRootWindowHostLinux::InitX11Window(const gfx::Rect& bounds) {
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
  base::MessagePumpAuraX11::Current()->AddDispatcherForWindow(this, xwindow_);

  // TODO(erg): Maybe need to set a ViewProp here like in RWHL::RWHL().

  long event_mask = ButtonPressMask | ButtonReleaseMask | FocusChangeMask |
                    KeyPressMask | KeyReleaseMask |
                    EnterWindowMask | LeaveWindowMask |
                    ExposureMask | VisibilityChangeMask |
                    StructureNotifyMask | PropertyChangeMask |
                    PointerMotionMask;
  XSelectInput(xdisplay_, xwindow_, event_mask);
  XFlush(xdisplay_);

  // TODO(erg): Something about an invisible cursor here? Don't think I need
  // it, but this is where it was.

  // TODO(erg): We currently only request window deletion events. We also
  // should listen for activation events and anything else that GTK+ listens
  // for, and do something useful.
  ::Atom protocols[2];
  protocols[0] = atom_cache_.GetAtom("WM_DELETE_WINDOW");
  protocols[1] = atom_cache_.GetAtom("_NET_WM_PING");
  XSetWMProtocols(xdisplay_, xwindow_, protocols, 2);

  // We need a WM_CLIENT_MACHINE and WM_LOCALE_NAME value so we integrate with
  // the desktop environment.
  XSetWMProperties(xdisplay_, xwindow_, NULL, NULL, NULL, 0, NULL, NULL, NULL);

  // Likewise, the X server needs to know this window's pid so it knows which
  // program to kill if the window hangs.
  pid_t pid = getpid();
  XChangeProperty(xdisplay_,
                  xwindow_,
                  atom_cache_.GetAtom("_NET_WM_PID"),
                  XA_CARDINAL,
                  32,
                  PropModeReplace,
                  reinterpret_cast<unsigned char*>(&pid), 1);

  // TODO(erg): Now that we're forked from RootWindowHostLinux, we should be
  // doing a much better job about communicating things like the window title
  // and icon to the window manager, which should replace this piece of copied
  // code.
  static int root_window_number = 0;
  std::string name = StringPrintf("aura_root_%d", root_window_number++);
  XStoreName(xdisplay_, xwindow_, name.c_str());
}

// TODO(erg): This method should basically be everything I need form
// RootWindowHostLinux::RootWindowHostLinux().
void DesktopRootWindowHostLinux::InitRootWindow(
    const Widget::InitParams& params) {
  aura::RootWindow::CreateParams rw_params(params.bounds);
  rw_params.host = this;
  root_window_.reset(new aura::RootWindow(rw_params));
  root_window_->Init();
  root_window_->AddChild(content_window_);
  root_window_host_delegate_ = root_window_.get();

  capture_client_.reset(new DesktopCaptureClient);
  aura::client::SetCaptureClient(root_window_.get(), capture_client_.get());

  root_window_->set_focus_manager(
      X11DesktopHandler::get()->get_focus_manager());

  aura::DesktopActivationClient* activation_client =
      X11DesktopHandler::get()->get_activation_client();
  aura::client::SetActivationClient(
      root_window_.get(), activation_client);

  dispatcher_client_.reset(new aura::DesktopDispatcherClient);
  aura::client::SetDispatcherClient(root_window_.get(),
                                    dispatcher_client_.get());

  // No event filter for aura::Env. Create CompoundEvnetFilter per RootWindow.
  root_window_event_filter_ = new aura::shared::CompoundEventFilter;
  // Pass ownership of the filter to the root_window.
  root_window_->SetEventFilter(root_window_event_filter_);

  input_method_filter_.reset(new aura::shared::InputMethodEventFilter());
  input_method_filter_->SetInputMethodPropertyInRootWindow(root_window_.get());
  root_window_event_filter_->AddFilter(input_method_filter_.get());

  // TODO(erg): Unify this code once the other consumer goes away.
  x11_window_event_filter_.reset(
      new X11WindowEventFilter(root_window_.get(), activation_client));
  x11_window_event_filter_->SetUseHostWindowBorders(false);
  root_window_event_filter_->AddFilter(x11_window_event_filter_.get());
}

bool DesktopRootWindowHostLinux::IsWindowManagerPresent() {
  // Per ICCCM 2.8, "Manager Selections", window managers should take ownership
  // of WM_Sn selections (where n is a screen number).
  return XGetSelectionOwner(
      xdisplay_, atom_cache_.GetAtom("WM_S0")) != None;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, DesktopRootWindowHost implementation:

void DesktopRootWindowHostLinux::Init(aura::Window* content_window,
                                      const Widget::InitParams& params) {
  content_window_ = content_window;

  // TODO(erg): Check whether we *should* be building a RootWindowHost here, or
  // whether we should be proxying requests to another DRWHL.

  // TODO(erg): We can finally solve the role problem! Based on params, try to
  // determine whether this is a utility window such as a menu.

  InitX11Window(params.bounds);
  InitRootWindow(params);

  // TODO(erg): This should be done by a LayoutManager instead of being a
  // one-off hack.
  content_window_->SetBounds(params.bounds);

  // This needs to be the intersection of:
  // - NativeWidgetAura::InitNativeWidget()
  // - DesktopNativeWidgetHelperAura::PreInitialize()
}

void DesktopRootWindowHostLinux::Close() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::CloseNow() {
  NOTIMPLEMENTED();
}

aura::RootWindowHost* DesktopRootWindowHostLinux::AsRootWindowHost() {
  return this;
}

void DesktopRootWindowHostLinux::ShowWindowWithState(
    ui::WindowShowState show_state) {
  if (show_state != ui::SHOW_STATE_DEFAULT &&
      show_state != ui::SHOW_STATE_NORMAL) {
    // Only forwarding to Show().
    NOTIMPLEMENTED();
  }

  Show();
}

void DesktopRootWindowHostLinux::ShowMaximizedWithBounds(
    const gfx::Rect& restored_bounds) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsVisible() const {
  return window_mapped_;
}

void DesktopRootWindowHostLinux::SetSize(const gfx::Size& size) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::CenterWindow(const gfx::Size& size) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::GetWindowPlacement(
    gfx::Rect* bounds,
    ui::WindowShowState* show_state) const {
  // TODO(erg):
  NOTIMPLEMENTED();
}

gfx::Rect DesktopRootWindowHostLinux::GetWindowBoundsInScreen() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect();
}

gfx::Rect DesktopRootWindowHostLinux::GetClientAreaBoundsInScreen() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect(100, 100);
}

gfx::Rect DesktopRootWindowHostLinux::GetRestoredBounds() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return gfx::Rect();
}

void DesktopRootWindowHostLinux::SetShape(gfx::NativeRegion native_region) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::ShouldUseNativeFrame() {
  return false;
}

void DesktopRootWindowHostLinux::Activate() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Deactivate() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsActive() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return true;
}

void DesktopRootWindowHostLinux::Maximize() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Minimize() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

void DesktopRootWindowHostLinux::Restore() {
  // TODO(erg):
  NOTIMPLEMENTED();
}

bool DesktopRootWindowHostLinux::IsMaximized() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return false;
}

bool DesktopRootWindowHostLinux::IsMinimized() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return false;
}

bool DesktopRootWindowHostLinux::HasCapture() const {
  // TODO(erg):
  NOTIMPLEMENTED();
  return false;
}

void DesktopRootWindowHostLinux::SetAlwaysOnTop(bool always_on_top) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

InputMethod* DesktopRootWindowHostLinux::CreateInputMethod() {
  // TODO(erg):
  NOTIMPLEMENTED();
  return NULL;
}

internal::InputMethodDelegate*
    DesktopRootWindowHostLinux::GetInputMethodDelegate() {
  // TODO(erg):
  NOTIMPLEMENTED();
  return NULL;
}

void DesktopRootWindowHostLinux::SetWindowTitle(const string16& title) {
  // TODO(erg):
  NOTIMPLEMENTED();
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, aura::RootWindowHost implementation:

aura::RootWindow* DesktopRootWindowHostLinux::GetRootWindow() {
  return root_window_.get();
}

gfx::AcceleratedWidget DesktopRootWindowHostLinux::GetAcceleratedWidget() {
  return xwindow_;
}

void DesktopRootWindowHostLinux::Show() {
  if (!window_mapped_) {
    // Before we map the window, set size hints. Otherwise, some window managers
    // will ignore toplevel XMoveWindow commands.
    XSizeHints size_hints;
    size_hints.flags = PPosition;
    size_hints.x = bounds_.x();
    size_hints.y = bounds_.y();
    XSetWMNormalHints(xdisplay_, xwindow_, &size_hints);

    XMapWindow(xdisplay_, xwindow_);

    // We now block until our window is mapped. Some X11 APIs will crash and
    // burn if passed |xwindow_| before the window is mapped, and XMapWindow is
    // asynchronous.
    base::MessagePumpAuraX11::Current()->BlockUntilWindowMapped(xwindow_);
    window_mapped_ = true;
  }
}

void DesktopRootWindowHostLinux::Hide() {
  if (window_mapped_) {
    XWithdrawWindow(xdisplay_, xwindow_, 0);
    window_mapped_ = false;
  }
}

void DesktopRootWindowHostLinux::ToggleFullScreen() {
}

gfx::Rect DesktopRootWindowHostLinux::GetBounds() const {
  return gfx::Rect(100, 100);
}

void DesktopRootWindowHostLinux::SetBounds(const gfx::Rect& bounds) {
}

gfx::Point DesktopRootWindowHostLinux::GetLocationOnNativeScreen() const {
  return gfx::Point(1, 1);
}

void DesktopRootWindowHostLinux::SetCapture() {
}

void DesktopRootWindowHostLinux::ReleaseCapture() {
}

void DesktopRootWindowHostLinux::SetCursor(gfx::NativeCursor cursor) {
}

void DesktopRootWindowHostLinux::ShowCursor(bool show) {
}

bool DesktopRootWindowHostLinux::QueryMouseLocation(
    gfx::Point* location_return) {
  return false;
}

bool DesktopRootWindowHostLinux::ConfineCursorToRootWindow() {
  return false;
}

void DesktopRootWindowHostLinux::UnConfineCursor() {
}

void DesktopRootWindowHostLinux::MoveCursorTo(const gfx::Point& location) {
}

void DesktopRootWindowHostLinux::SetFocusWhenShown(bool focus_when_shown) {
  static const char* k_NET_WM_USER_TIME = "_NET_WM_USER_TIME";
  focus_when_shown_ = focus_when_shown;
  if (IsWindowManagerPresent() && !focus_when_shown_) {
    ui::SetIntProperty(xwindow_,
                       k_NET_WM_USER_TIME,
                       k_NET_WM_USER_TIME,
                       0);
  }
}

bool DesktopRootWindowHostLinux::GrabSnapshot(
      const gfx::Rect& snapshot_bounds,
      std::vector<unsigned char>* png_representation) {
  return false;
}

void DesktopRootWindowHostLinux::PostNativeEvent(
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

void DesktopRootWindowHostLinux::OnDeviceScaleFactorChanged(
    float device_scale_factor) {
}

void DesktopRootWindowHostLinux::PrepareForShutdown() {
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHostLinux, MessageLoop::Dispatcher implementation:

bool DesktopRootWindowHostLinux::Dispatch(const base::NativeEvent& event) {
  XEvent* xev = event;

  // May want to factor CheckXEventForConsistency(xev); into a common location
  // since it is called here.
  switch (xev->type) {
    case Expose:
      // TODO(erg): Can we only redraw the affected areas?
      root_window_host_delegate_->OnHostPaint();
      break;
    case KeyPress: {
      ui::KeyEvent keydown_event(xev, false);
      root_window_host_delegate_->OnHostKeyEvent(&keydown_event);
      break;
    }
    case KeyRelease: {
      ui::KeyEvent keyup_event(xev, false);
      root_window_host_delegate_->OnHostKeyEvent(&keyup_event);
      break;
    }
    case ButtonPress: {
      if (static_cast<int>(xev->xbutton.button) == kBackMouseButton ||
          static_cast<int>(xev->xbutton.button) == kForwardMouseButton) {
        aura::client::UserActionClient* gesture_client =
            aura::client::GetUserActionClient(root_window_.get());
        if (gesture_client) {
          gesture_client->OnUserAction(
              static_cast<int>(xev->xbutton.button) == kBackMouseButton ?
              aura::client::UserActionClient::BACK :
              aura::client::UserActionClient::FORWARD);
        }
        break;
      }
    }  // fallthrough
    case ButtonRelease: {
      ui::MouseEvent mouseev(xev);
      root_window_host_delegate_->OnHostMouseEvent(&mouseev);
      break;
    }
    case FocusOut:
      if (xev->xfocus.mode != NotifyGrab)
        root_window_host_delegate_->OnHostLostCapture();
      break;
    case ConfigureNotify: {
      DCHECK_EQ(xwindow_, xev->xconfigure.window);
      DCHECK_EQ(xwindow_, xev->xconfigure.event);
      // It's possible that the X window may be resized by some other means than
      // from within aura (e.g. the X window manager can change the size). Make
      // sure the root window size is maintained properly.
      gfx::Rect bounds(xev->xconfigure.x, xev->xconfigure.y,
                       xev->xconfigure.width, xev->xconfigure.height);
      bool size_changed = bounds_.size() != bounds.size();
      bool origin_changed = bounds_.origin() != bounds.origin();
      bounds_ = bounds;
      if (size_changed)
        root_window_host_delegate_->OnHostResized(bounds.size());
      if (origin_changed)
        root_window_host_delegate_->OnHostMoved(bounds_.origin());
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
        // case ui::ET_TOUCH_MOVED:
        //   num_coalesced = CoalescePendingMotionEvents(xev, &last_event);
        //   if (num_coalesced > 0)
        //     xev = &last_event;
        //   // fallthrough
        // case ui::ET_TOUCH_PRESSED:
        // case ui::ET_TOUCH_RELEASED: {
        //   ui::TouchEvent touchev(xev);
        //   root_window_host_delegate_->OnHostTouchEvent(&touchev);
        //   break;
        // }
        case ui::ET_MOUSE_MOVED:
        case ui::ET_MOUSE_DRAGGED:
        case ui::ET_MOUSE_PRESSED:
        case ui::ET_MOUSE_RELEASED:
        case ui::ET_MOUSE_ENTERED:
        case ui::ET_MOUSE_EXITED: {
          if (type == ui::ET_MOUSE_MOVED || type == ui::ET_MOUSE_DRAGGED) {
            // If this is a motion event, we want to coalesce all pending motion
            // events that are at the top of the queue.
            // num_coalesced = CoalescePendingMotionEvents(xev, &last_event);
            // if (num_coalesced > 0)
            //   xev = &last_event;
          } else if (type == ui::ET_MOUSE_PRESSED) {
            XIDeviceEvent* xievent =
                static_cast<XIDeviceEvent*>(xev->xcookie.data);
            int button = xievent->detail;
            if (button == kBackMouseButton || button == kForwardMouseButton) {
              aura::client::UserActionClient* gesture_client =
                  aura::client::GetUserActionClient(
                      root_window_host_delegate_->AsRootWindow());
              if (gesture_client) {
                bool reverse_direction =
                    ui::IsTouchpadEvent(xev) && ui::IsNaturalScrollEnabled();
                gesture_client->OnUserAction(
                    (button == kBackMouseButton && !reverse_direction) ||
                    (button == kForwardMouseButton && reverse_direction) ?
                    aura::client::UserActionClient::BACK :
                    aura::client::UserActionClient::FORWARD);
              }
              break;
            }
          }
          ui::MouseEvent mouseev(xev);
          root_window_host_delegate_->OnHostMouseEvent(&mouseev);
          break;
        }
        case ui::ET_MOUSEWHEEL: {
          ui::MouseWheelEvent mouseev(xev);
          root_window_host_delegate_->OnHostMouseEvent(&mouseev);
          break;
        }
        case ui::ET_SCROLL_FLING_START:
        case ui::ET_SCROLL_FLING_CANCEL:
        case ui::ET_SCROLL: {
          ui::ScrollEvent scrollev(xev);
          root_window_host_delegate_->OnHostScrollEvent(&scrollev);
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
      if (message_type == atom_cache_.GetAtom("WM_DELETE_WINDOW")) {
        // We have received a close message from the window manager.
        root_window_->OnRootWindowHostCloseRequested();
      } else if (message_type == atom_cache_.GetAtom("_NET_WM_PING")) {
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

      ui::MouseEvent mouseev(xev);
      root_window_host_delegate_->OnHostMouseEvent(&mouseev);
      break;
    }
  }
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// DesktopRootWindowHost, public:

// static
DesktopRootWindowHost* DesktopRootWindowHost::Create(
    internal::NativeWidgetDelegate* native_widget_delegate,
    const gfx::Rect& initial_bounds) {
  return new DesktopRootWindowHostLinux;
}

}  // namespace views
