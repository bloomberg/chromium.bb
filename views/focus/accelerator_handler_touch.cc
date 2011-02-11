// Copyright (c) 2010 The Chromium Authors. All rights reserved.
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
#include "views/touchui/touch_factory.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace views {

namespace {

RootView* FindRootViewForGdkWindow(GdkWindow* gdk_window) {
  gpointer data = NULL;
  gdk_window_get_user_data(gdk_window, &data);
  GtkWidget* gtk_widget = reinterpret_cast<GtkWidget*>(data);
  if (!gtk_widget || !GTK_IS_WIDGET(gtk_widget)) {
    DLOG(WARNING) << "no GtkWidget found for that GdkWindow";
    return NULL;
  }
  WidgetGtk* widget_gtk = WidgetGtk::GetViewForNative(gtk_widget);

  if (!widget_gtk) {
    DLOG(WARNING) << "no WidgetGtk found for that GtkWidget";
    return NULL;
  }
  return widget_gtk->GetRootView();
}

#if defined(HAVE_XINPUT2)
bool X2EventIsTouchEvent(XEvent* xev) {
  // TODO(sad): Determine if the captured event is a touch-event.
  XGenericEventCookie* cookie = &xev->xcookie;
  switch (cookie->evtype) {
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion: {
      // Is the event coming from a touch device?
      return TouchFactory::GetInstance()->IsTouchDevice(
          static_cast<XIDeviceEvent*>(cookie->data)->sourceid);
    }
    default:
      return false;
  }
}
#endif  // HAVE_XINPUT2

}  // namespace

#if defined(HAVE_XINPUT2)
bool DispatchX2Event(RootView* root, XEvent* xev) {
  XGenericEventCookie* cookie = &xev->xcookie;
  bool touch_event = false;

  if (X2EventIsTouchEvent(xev)) {
    // Hide the cursor when a touch event comes in.
    TouchFactory::GetInstance()->SetCursorVisible(false, false);
    touch_event = true;

    // Create a TouchEvent, and send it off to |root|. If the event
    // is processed by |root|, then return. Otherwise let it fall through so it
    // can be used (if desired) as a mouse event.
    TouchEvent touch(xev);
    if (root->OnTouchEvent(touch) != views::View::TOUCH_STATUS_UNKNOWN)
      return true;
  }

  switch (cookie->evtype) {
    case XI_KeyPress:
    case XI_KeyRelease: {
      // TODO(sad): We don't capture XInput2 events from keyboard yet.
      break;
    }
    case XI_ButtonPress:
    case XI_ButtonRelease:
    case XI_Motion: {
      // Scrolling the wheel generates press/release events with button id's 4
      // and 5. In case of a wheelscroll, we do not want to show the cursor.
      XIDeviceEvent* xievent = static_cast<XIDeviceEvent*>(cookie->data);
      if (xievent->detail == 4 || xievent->detail == 5) {
        MouseWheelEvent wheelev(xev);
        return root->ProcessMouseWheelEvent(wheelev);
      }

      MouseEvent mouseev(xev);
      if (!touch_event) {
        // Show the cursor, and decide whether or not the cursor should be
        // automatically hidden after a certain time of inactivity.
        int button_flags = mouseev.flags() & (ui::EF_RIGHT_BUTTON_DOWN |
            ui::EF_MIDDLE_BUTTON_DOWN | ui::EF_LEFT_BUTTON_DOWN);
        bool start_timer = false;

        switch (cookie->evtype) {
          case XI_ButtonPress:
            start_timer = false;
            break;
          case XI_ButtonRelease:
            // For a release, start the timer if this was only button pressed
            // that is being released.
            if (button_flags == ui::EF_RIGHT_BUTTON_DOWN ||
                button_flags == ui::EF_LEFT_BUTTON_DOWN ||
                button_flags == ui::EF_MIDDLE_BUTTON_DOWN)
              start_timer = true;
            break;
          case XI_Motion:
            start_timer = !button_flags;
            break;
        }
        TouchFactory::GetInstance()->SetCursorVisible(true, start_timer);
      }

      // Dispatch the event.
      switch (cookie->evtype) {
        case XI_ButtonPress:
          return root->OnMousePressed(mouseev);
        case XI_ButtonRelease:
          root->OnMouseReleased(mouseev, false);
          return true;
        case XI_Motion: {
          if (mouseev.type() == ui::ET_MOUSE_DRAGGED) {
            return root->OnMouseDragged(mouseev);
          } else {
            root->OnMouseMoved(mouseev);
            return true;
          }
        }
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
    XGenericEventCookie* cookie = &xev->xcookie;
    XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(cookie->data);
    xwindow = xiev->event;
  }
#endif

  GdkWindow* gwind = gdk_window_lookup_for_display(gdisp, xwindow);

  if (RootView* root = FindRootViewForGdkWindow(gwind)) {
    switch (xev->type) {
      case KeyPress:
      case KeyRelease: {
        KeyEvent keyev(xev);
        return root->ProcessKeyEvent(keyev);
      }

      case ButtonPress:
      case ButtonRelease: {
        if (xev->xbutton.button == 4 || xev->xbutton.button == 5) {
          // Scrolling the wheel triggers button press/release events.
          MouseWheelEvent wheelev(xev);
          return root->ProcessMouseWheelEvent(wheelev);
        } else {
          MouseEvent mouseev(xev);
          if (xev->type == ButtonPress) {
            return root->OnMousePressed(mouseev);
          } else {
            root->OnMouseReleased(mouseev, false);
            return true;  // Assume the event has been processed to make sure we
                          // don't process it twice.
          }
        }
      }

      case MotionNotify: {
        MouseEvent mouseev(xev);
        if (mouseev.type() == ui::ET_MOUSE_DRAGGED) {
          return root->OnMouseDragged(mouseev);
        } else {
          root->OnMouseMoved(mouseev);
          return true;
        }
      }

#if defined(HAVE_XINPUT2)
      case GenericEvent: {
        return DispatchX2Event(root, xev);
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
