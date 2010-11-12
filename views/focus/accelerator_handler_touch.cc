// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/focus/accelerator_handler.h"

#include <gtk/gtk.h>
#include <X11/Xlib.h>

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

}  // namespace

bool DispatchXEvent(XEvent* xev) {
  GdkDisplay* gdisp = gdk_display_get_default();
  GdkWindow* gwind = gdk_window_lookup_for_display(gdisp, xev->xany.window);

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
