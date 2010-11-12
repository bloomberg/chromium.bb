// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/accelerator_handler.h"

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
  return false;
}
#endif  // HAVE_XINPUT2

}  // namespace

#if defined(HAVE_XINPUT2)
bool DispatchX2Event(RootView* root, XEvent* xev) {
  if (X2EventIsTouchEvent(xev)) {
    // TODO(sad): Create a TouchEvent, and send it off to |root|. If the event
    // is processed by |root|, then return. Otherwise let it fall through so it
    // can be used (if desired) as a mouse event.

    // TouchEvent touch(xev);
    // if (root->OnTouchEvent(touch))
    //  return true;
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
    if (XGetEventData(xev->xgeneric.display, &xev->xcookie)) {
      XGenericEventCookie* cookie = &xev->xcookie;
      XIDeviceEvent* xiev = static_cast<XIDeviceEvent*>(cookie->data);
      xwindow = xiev->event;
    } else {
      DLOG(WARNING) << "Error fetching XGenericEventCookie for event.";
      return false;
    }
  }
#endif

  GdkWindow* gwind = gdk_window_lookup_for_display(gdisp, xwindow);

  if (RootView* root = FindRootViewForGdkWindow(gwind)) {
    switch (xev->type) {
      case KeyPress:
      case KeyRelease: {
        KeyEvent keyev(xev);

        // If it's a keypress, check to see if it triggers an accelerator.
        if (xev->type == KeyPress) {
          FocusManager* focus_manager = root->GetFocusManager();
          if (focus_manager && !focus_manager->OnKeyEvent(keyev))
            return true;
        }

        return root->ProcessKeyEvent(keyev);
      }

      case ButtonPress:
      case ButtonRelease: {
        MouseEvent mouseev(xev);
        if (xev->type == ButtonPress) {
          return root->OnMousePressed(mouseev);
        } else {
          root->OnMouseReleased(mouseev, false);
          return true;  // Assume the event has been processed to make sure we
                        // don't process it twice.
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
        bool ret = DispatchX2Event(root, xev);
        XFreeEventData(xev->xgeneric.display, &xev->xcookie);
        return ret;
      }
#endif
    }
  }

  return false;
}

AcceleratorHandler::AcceleratorHandler() {}

bool AcceleratorHandler::Dispatch(GdkEvent* event) {
  gtk_main_do_event(event);
  return true;
}

bool AcceleratorHandler::Dispatch(XEvent* xev) {
  return DispatchXEvent(xev);
}

}  // namespace views
