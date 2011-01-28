// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/screen.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/logging.h"

namespace views {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  gint x, y;
  gdk_display_get_pointer(gdk_display_get_default(), NULL, &x, &y, NULL);
  return gfx::Point(x, y);
}

gfx::Rect static GetPrimaryMonitorBounds() {
  guchar* raw_data = NULL;
  gint data_len = 0;
  gboolean success = gdk_property_get(gdk_get_default_root_window(),
                                      gdk_atom_intern("_NET_WORKAREA", FALSE),
                                      gdk_atom_intern("CARDINAL", FALSE),
                                      0, 0xFF, false, NULL, NULL, &data_len,
                                      &raw_data);
  int top_left_x = 0;
  int top_left_y = 0;
  int width = 0;
  int height = 0;

  if (success) {
    glong* data = reinterpret_cast<glong*>(raw_data);
    top_left_x = data[0];
    top_left_y = data[1];
    width = data[2];
    height = data[3];
    g_free(raw_data);
  } else {
    // If there's no window manager, we can ask X for Monitor info directly.
    XWindowAttributes attributes;
    Status status = XGetWindowAttributes(gdk_x11_get_default_xdisplay(),
                                         gdk_x11_get_default_root_xwindow(),
                                         &attributes);
    if (status) {
      top_left_x = attributes.x;
      top_left_y = attributes.y;
      width = attributes.width;
      height = attributes.height;
      success = true;
    }
  }
  DCHECK(success);
  return gfx::Rect(top_left_x, top_left_y, width, height);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeView view) {
  // TODO(beng): use |view|.
  return GetPrimaryMonitorBounds();
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeView view) {
  GtkWidget* top_level = gtk_widget_get_toplevel(view);
  DCHECK(GTK_IS_WINDOW(top_level));
  GtkWindow* window = GTK_WINDOW(top_level);
  GdkScreen* screen = gtk_window_get_screen(window);
  gint monitor_num = gdk_screen_get_monitor_at_window(screen,
                                                      top_level->window);
  GdkRectangle bounds;
  gdk_screen_get_monitor_geometry(screen, monitor_num, &bounds);
  return gfx::Rect(bounds);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestPoint(const gfx::Point& point) {
  // TODO(jamiewalch): Restrict this to the work area of the monitor.
  return GetMonitorAreaNearestPoint(point);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  GdkScreen* screen = gdk_screen_get_default();
  gint monitor = gdk_screen_get_monitor_at_point(screen, point.x(), point.y());
  GdkRectangle bounds;
  gdk_screen_get_monitor_geometry(screen, monitor, &bounds);
  return gfx::Rect(bounds);
}

gfx::NativeWindow Screen::GetWindowAtCursorScreenPoint() {
  GdkWindow* window = gdk_window_at_pointer(NULL, NULL);
  if (!window)
    return NULL;

  gpointer data = NULL;
  gdk_window_get_user_data(window, &data);
  GtkWidget* widget = reinterpret_cast<GtkWidget*>(data);
  if (!widget)
    return NULL;
  widget = gtk_widget_get_toplevel(widget);
  return GTK_IS_WINDOW(widget) ? GTK_WINDOW(widget) : NULL;
}

}  // namespace

