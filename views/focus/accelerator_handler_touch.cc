// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/accelerator_handler.h"

#include <bitset>
#include <gtk/gtk.h>
#if defined(HAVE_XINPUT2)
#include <X11/extensions/XInput2.h>
#else
#include <X11/Xlib.h>
#endif

#include "views/accelerator.h"
#include "views/events/event.h"
#include "views/focus/focus_manager.h"
#include "views/ime/input_method.h"
#include "views/touchui/touch_factory.h"
#include "views/view.h"
#include "views/widget/native_widget.h"

namespace views {

namespace {

Widget* FindWidgetForGdkWindow(GdkWindow* gdk_window) {
  gpointer data = NULL;
  gdk_window_get_user_data(gdk_window, &data);
  GtkWidget* gtk_widget = reinterpret_cast<GtkWidget*>(data);
  if (!gtk_widget || !GTK_IS_WIDGET(gtk_widget)) {
    DLOG(WARNING) << "no GtkWidget found for that GdkWindow";
    return NULL;
  }
  NativeWidget* widget = NativeWidget::GetNativeWidgetForNativeView(gtk_widget);

  if (!widget) {
    DLOG(WARNING) << "no NativeWidgetGtk found for that GtkWidget";
    return NULL;
  }
  return widget->GetWidget();
}

}  // namespace

#if defined(HAVE_XINPUT2)
bool DispatchX2Event(Widget* widget, XEvent* xev) {
  XGenericEventCookie* cookie = &xev->xcookie;
  switch (cookie->evtype) {
    case XI_KeyPress:
    case XI_KeyRelease: {
      // TODO(sad): We don't capture XInput2 events from keyboard yet.
      break;
    }
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion: {
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(cookie->data);
      Event::FromNativeEvent2 from_native;

      // Scrolling the wheel generates press/release events with button id's 4
      // and 5. In case of a wheelscroll, we do not want to show the cursor.
      if (xievent->detail == 4 || xievent->detail == 5) {
        MouseWheelEvent wheelev(xev, from_native);
        return widget->OnMouseEvent(wheelev);
      }

      // Is the event coming from a touch device?
      if (TouchFactory::GetInstance()->IsTouchDevice(xievent->sourceid)) {
        // Hide the cursor when a touch event comes in.
        TouchFactory::GetInstance()->SetCursorVisible(false, false);

        // With XInput 2.0, XI_ButtonPress and XI_ButtonRelease events are
        // ignored, as XI_Motion events contain enough data to detect finger
        // press and release. See more notes in TouchFactory::TouchParam.
        if (cookie->evtype == XI_ButtonPress ||
            cookie->evtype == XI_ButtonRelease)
          return false;

        // If the TouchEvent is processed by |root|, then return. Otherwise let
        // it fall through so it can be used as a MouseEvent, if desired.
        TouchEvent touch(xev, from_native);
        View* root = widget->GetRootView();
        if (root->OnTouchEvent(touch) != views::View::TOUCH_STATUS_UNKNOWN)
          return true;

        // We do not want to generate a mouse event for an unprocessed touch
        // event here. That is already done by the gesture manager in
        // RootView::OnTouchEvent.
        return false;
      } else {
        MouseEvent mouseev(xev, from_native);

        // Show the cursor. Start a timer to hide the cursor after a delay on
        // move (not drag) events, or if the only button pressed is released.
        bool start_timer = mouseev.type() == ui::ET_MOUSE_MOVED;
        start_timer |= mouseev.type() == ui::ET_MOUSE_RELEASED &&
                       (mouseev.IsOnlyLeftMouseButton() ||
                        mouseev.IsOnlyMiddleMouseButton() ||
                        mouseev.IsOnlyRightMouseButton());
        TouchFactory::GetInstance()->SetCursorVisible(true, start_timer);

        return widget->OnMouseEvent(mouseev);
      }
    }
  }
  return false;
}

#endif  // HAVE_XINPUT2

bool DispatchXEvent(XEvent* xev) {
  GdkDisplay* gdisp = gdk_display_get_default();
  XID xwindow = xev->xany.window;

#if defined(HAVE_XINPUT2)
  if (xev->type == GenericEvent) {
    if (!TouchFactory::GetInstance()->ShouldProcessXI2Event(xev))
      return true;  // Consume the event.

    XGenericEventCookie* cookie = &xev->xcookie;
    if (cookie->evtype == XI_HierarchyChanged) {
      TouchFactory::GetInstance()->UpdateDeviceList(cookie->display);
      return true;
    }

    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(cookie->data);
    xwindow = xiev->event;
  }
#endif

  GdkWindow* gwind = gdk_window_lookup_for_display(gdisp, xwindow);
  Widget* widget = FindWidgetForGdkWindow(gwind);
  if (widget) {
    Event::FromNativeEvent2 from_native;
    switch (xev->type) {
      case KeyPress:
      case KeyRelease: {
        KeyEvent keyev(xev, from_native);
        InputMethod* ime = widget->GetInputMethod();
        // Always dispatch key events to the input method first, to make sure
        // that the input method's hotkeys work all time.
        if (ime) {
          ime->DispatchKeyEvent(keyev);
          return true;
        }
        return widget->OnKeyEvent(keyev);
      }
      case ButtonPress:
      case ButtonRelease:
        if (xev->xbutton.button == 4 || xev->xbutton.button == 5) {
          // Scrolling the wheel triggers button press/release events.
          MouseWheelEvent wheelev(xev, from_native);
          return widget->OnMouseEvent(wheelev);
        }
        // fallthrough
      case MotionNotify: {
        MouseEvent mouseev(xev, from_native);
        return widget->OnMouseEvent(mouseev);
      }

#if defined(HAVE_XINPUT2)
      case GenericEvent: {
        return DispatchX2Event(widget, xev);
      }
#endif
    }
  }

  return false;
}

#if defined(HAVE_XINPUT2)
void SetTouchDeviceList(std::vector<unsigned int>& devices) {
  TouchFactory::GetInstance()->SetTouchDeviceList(devices);
}
#endif

AcceleratorHandler::AcceleratorHandler() {}

bool AcceleratorHandler::Dispatch(GdkEvent* event) {
  gtk_main_do_event(event);
  return true;
}

base::MessagePumpGlibXDispatcher::DispatchStatus
    AcceleratorHandler::DispatchX(XEvent* xev) {
  return DispatchXEvent(xev) ?
      base::MessagePumpGlibXDispatcher::EVENT_PROCESSED :
      base::MessagePumpGlibXDispatcher::EVENT_IGNORED;
}

}  // namespace views
