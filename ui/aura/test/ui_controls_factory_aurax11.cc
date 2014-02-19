// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <X11/keysym.h>
#include <X11/Xlib.h>

// X macro fail.
#if defined(RootWindow)
#undef RootWindow
#endif

#include "base/logging.h"
#include "ui/aura/client/screen_position_client.h"
#include "ui/aura/env.h"
#include "ui/aura/root_window.h"
#include "ui/aura/test/ui_controls_factory_aura.h"
#include "ui/aura/window.h"
#include "ui/base/test/ui_controls_aura.h"
#include "ui/base/x/x11_util.h"
#include "ui/compositor/dip_util.h"
#include "ui/events/keycodes/keyboard_code_conversion_x.h"

namespace aura {
namespace test {
namespace {

using ui_controls::DOWN;
using ui_controls::LEFT;
using ui_controls::MIDDLE;
using ui_controls::MouseButton;
using ui_controls::RIGHT;
using ui_controls::UIControlsAura;
using ui_controls::UP;

// Mask of the buttons currently down.
unsigned button_down_mask = 0;

// Event waiter executes the specified closure|when a matching event
// is found.
// TODO(oshima): Move this to base.
class EventWaiter : public base::MessageLoopForUI::Observer {
 public:
  typedef bool (*EventWaiterMatcher)(const base::NativeEvent& event);

  EventWaiter(const base::Closure& closure, EventWaiterMatcher matcher)
      : closure_(closure),
        matcher_(matcher) {
    base::MessageLoopForUI::current()->AddObserver(this);
  }

  virtual ~EventWaiter() {
    base::MessageLoopForUI::current()->RemoveObserver(this);
  }

  // MessageLoop::Observer implementation:
  virtual base::EventStatus WillProcessEvent(
      const base::NativeEvent& event) OVERRIDE {
    if ((*matcher_)(event)) {
      base::MessageLoop::current()->PostTask(FROM_HERE, closure_);
      delete this;
    }
    return base::EVENT_CONTINUE;
  }

  virtual void DidProcessEvent(const base::NativeEvent& event) OVERRIDE {
  }

 private:
  base::Closure closure_;
  EventWaiterMatcher matcher_;
  DISALLOW_COPY_AND_ASSIGN(EventWaiter);
};

// Returns atom that indidates that the XEvent is marker event.
Atom MarkerEventAtom() {
  return XInternAtom(gfx::GetXDisplay(), "marker_event", False);
}

// Returns true when the event is a marker event.
bool Matcher(const base::NativeEvent& event) {
  return event->xany.type == ClientMessage &&
      event->xclient.message_type == MarkerEventAtom();
}

class UIControlsX11 : public UIControlsAura {
 public:
  UIControlsX11(aura::RootWindow* root_window) : root_window_(root_window) {
  }

  virtual bool SendKeyPress(gfx::NativeWindow window,
                            ui::KeyboardCode key,
                            bool control,
                            bool shift,
                            bool alt,
                            bool command) OVERRIDE {
    DCHECK(!command);  // No command key on Aura
    return SendKeyPressNotifyWhenDone(
        window, key, control, shift, alt, command, base::Closure());
  }
  virtual bool SendKeyPressNotifyWhenDone(
      gfx::NativeWindow window,
      ui::KeyboardCode key,
      bool control,
      bool shift,
      bool alt,
      bool command,
      const base::Closure& closure) OVERRIDE {
    DCHECK(!command);  // No command key on Aura
    XEvent xevent = {0};
    xevent.xkey.type = KeyPress;
    if (control)
      SetKeycodeAndSendThenMask(&xevent, XK_Control_L, ControlMask);
    if (shift)
      SetKeycodeAndSendThenMask(&xevent, XK_Shift_L, ShiftMask);
    if (alt)
      SetKeycodeAndSendThenMask(&xevent, XK_Alt_L, Mod1Mask);
    xevent.xkey.keycode =
        XKeysymToKeycode(gfx::GetXDisplay(),
                         ui::XKeysymForWindowsKeyCode(key, shift));
    root_window_->host()->PostNativeEvent(&xevent);

    // Send key release events.
    xevent.xkey.type = KeyRelease;
    root_window_->host()->PostNativeEvent(&xevent);
    if (alt)
      UnmaskAndSetKeycodeThenSend(&xevent, Mod1Mask, XK_Alt_L);
    if (shift)
      UnmaskAndSetKeycodeThenSend(&xevent, ShiftMask, XK_Shift_L);
    if (control)
      UnmaskAndSetKeycodeThenSend(&xevent, ControlMask, XK_Control_L);
    DCHECK(!xevent.xkey.state);
    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }

  virtual bool SendMouseMove(long screen_x, long screen_y) OVERRIDE {
    return SendMouseMoveNotifyWhenDone(screen_x, screen_y, base::Closure());
  }
  virtual bool SendMouseMoveNotifyWhenDone(
      long screen_x,
      long screen_y,
      const base::Closure& closure) OVERRIDE {
    gfx::Point root_point(screen_x, screen_y);
    aura::client::ScreenPositionClient* screen_position_client =
        aura::client::GetScreenPositionClient(root_window_->window());
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(root_window_->window(),
                                                     &root_point);
    }

    XEvent xevent = {0};
    XMotionEvent* xmotion = &xevent.xmotion;
    xmotion->type = MotionNotify;
    xmotion->x = root_point.x();
    xmotion->y = root_point.y();
    xmotion->state = button_down_mask;
    xmotion->same_screen = True;
    // RootWindow will take care of other necessary fields.
    root_window_->host()->PostNativeEvent(&xevent);
    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }
  virtual bool SendMouseEvents(MouseButton type, int state) OVERRIDE {
    return SendMouseEventsNotifyWhenDone(type, state, base::Closure());
  }
  virtual bool SendMouseEventsNotifyWhenDone(
      MouseButton type,
      int state,
      const base::Closure& closure) OVERRIDE {
    XEvent xevent = {0};
    XButtonEvent* xbutton = &xevent.xbutton;
    gfx::Point mouse_loc = aura::Env::GetInstance()->last_mouse_location();
    aura::client::ScreenPositionClient* screen_position_client =
          aura::client::GetScreenPositionClient(root_window_->window());
    if (screen_position_client) {
      screen_position_client->ConvertPointFromScreen(root_window_->window(),
                                                     &mouse_loc);
    }
    xbutton->x = mouse_loc.x();
    xbutton->y = mouse_loc.y();
    xbutton->same_screen = True;
    switch (type) {
      case LEFT:
        xbutton->button = Button1;
        xbutton->state = Button1Mask;
        break;
      case MIDDLE:
        xbutton->button = Button2;
        xbutton->state = Button2Mask;
        break;
      case RIGHT:
        xbutton->button = Button3;
        xbutton->state = Button3Mask;
        break;
    }
    // RootWindow will take care of other necessary fields.
    if (state & DOWN) {
      xevent.xbutton.type = ButtonPress;
      root_window_->host()->PostNativeEvent(&xevent);
      button_down_mask |= xbutton->state;
    }
    if (state & UP) {
      xevent.xbutton.type = ButtonRelease;
      root_window_->host()->PostNativeEvent(&xevent);
      button_down_mask = (button_down_mask | xbutton->state) ^ xbutton->state;
    }
    RunClosureAfterAllPendingUIEvents(closure);
    return true;
  }
  virtual bool SendMouseClick(MouseButton type) OVERRIDE {
    return SendMouseEvents(type, UP | DOWN);
  }
  virtual void RunClosureAfterAllPendingUIEvents(
      const base::Closure& closure) OVERRIDE {
    if (closure.is_null())
      return;
    static XEvent* marker_event = NULL;
    if (!marker_event) {
      marker_event = new XEvent();
      marker_event->xclient.type = ClientMessage;
      marker_event->xclient.display = NULL;
      marker_event->xclient.window = None;
      marker_event->xclient.format = 8;
    }
    marker_event->xclient.message_type = MarkerEventAtom();
    root_window_->host()->PostNativeEvent(marker_event);
    new EventWaiter(closure, &Matcher);
  }
 private:
  void SetKeycodeAndSendThenMask(XEvent* xevent,
                                 KeySym keysym,
                                 unsigned int mask) {
    xevent->xkey.keycode =
        XKeysymToKeycode(gfx::GetXDisplay(), keysym);
    root_window_->host()->PostNativeEvent(xevent);
    xevent->xkey.state |= mask;
  }

  void UnmaskAndSetKeycodeThenSend(XEvent* xevent,
                                   unsigned int mask,
                                   KeySym keysym) {
    xevent->xkey.state ^= mask;
    xevent->xkey.keycode =
        XKeysymToKeycode(gfx::GetXDisplay(), keysym);
    root_window_->host()->PostNativeEvent(xevent);
  }

  aura::RootWindow* root_window_;

  DISALLOW_COPY_AND_ASSIGN(UIControlsX11);
};

}  // namespace

UIControlsAura* CreateUIControlsAura(aura::RootWindow* root_window) {
  return new UIControlsX11(root_window);
}

}  // namespace test
}  // namespace aura
