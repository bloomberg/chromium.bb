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
#include "views/event.h"
#include "views/focus/focus_manager.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

namespace views {

#if defined(HAVE_XINPUT2)
// Functions related to determining touch devices.
class TouchFactory {
 public:
  // Keep a list of touch devices so that it is possible to determine if a
  // pointer event is a touch-event or a mouse-event.
  static void SetTouchDeviceListInternal(
      const std::vector<unsigned int>& devices) {
    for (std::vector<unsigned int>::const_iterator iter = devices.begin();
        iter != devices.end(); ++iter) {
      DCHECK(*iter < touch_devices.size());
      touch_devices[*iter] = true;
    }
  }

  // Is the device a touch-device?
  static bool IsTouchDevice(unsigned int deviceid) {
    return deviceid < touch_devices.size() ? touch_devices[deviceid] : false;
  }

 private:
  // A quick lookup table for determining if a device is a touch device.
  static std::bitset<128> touch_devices;

  DISALLOW_COPY_AND_ASSIGN(TouchFactory);
};

std::bitset<128> TouchFactory::touch_devices;
#endif

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
      return TouchFactory::IsTouchDevice(
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
  if (X2EventIsTouchEvent(xev)) {
    // Create a TouchEvent, and send it off to |root|. If the event
    // is processed by |root|, then return. Otherwise let it fall through so it
    // can be used (if desired) as a mouse event.

    TouchEvent touch(xev);
    if (root->OnTouchEvent(touch) != views::View::TOUCH_STATUS_UNKNOWN)
      return true;
  }

  XGenericEventCookie* cookie = &xev->xcookie;

  switch (cookie->evtype) {
    case XI_KeyPress:
    case XI_KeyRelease: {
      // TODO(sad): We don't capture XInput2 events from keyboard yet.
      break;
    }
    case XI_ButtonPress:
    case XI_ButtonRelease: {
      MouseEvent mouseev(xev);
      if (cookie->evtype == XI_ButtonPress) {
        return root->OnMousePressed(mouseev);
      } else {
        root->OnMouseReleased(mouseev, false);
        return true;
      }
    }

    case XI_Motion: {
      MouseEvent mouseev(xev);
      if (mouseev.GetType() == Event::ET_MOUSE_DRAGGED) {
        return root->OnMouseDragged(mouseev);
      } else {
        root->OnMouseMoved(mouseev);
        return true;
      }
      break;
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
        if (mouseev.GetType() == Event::ET_MOUSE_DRAGGED) {
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
  TouchFactory::SetTouchDeviceListInternal(devices);
}
#endif

AcceleratorHandler::AcceleratorHandler() {}

bool AcceleratorHandler::Dispatch(GdkEvent* event) {
  gtk_main_do_event(event);
  return true;
}

base::MessagePumpGlibXDispatcher::DispatchStatus AcceleratorHandler::Dispatch(
    XEvent* xev) {
  return DispatchXEvent(xev) ?
      base::MessagePumpGlibXDispatcher::EVENT_PROCESSED :
      base::MessagePumpGlibXDispatcher::EVENT_IGNORED;
}

}  // namespace views
