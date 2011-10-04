// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/screen.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "base/logging.h"

namespace gfx {

// static
gfx::Point Screen::GetCursorScreenPoint() {
  gint x, y;
  gdk_display_get_pointer(gdk_display_get_default(), NULL, &x, &y, NULL);
  return gfx::Point(x, y);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeView view) {
  // Do not use the _NET_WORKAREA here, this is supposed to be an area on a
  // specific monitor, and _NET_WORKAREA is a hint from the WM that generally
  // spans across all monitors.  This would make the work area larger than the
  // monitor.
  // TODO(danakj) This is a work-around as there is no standard way to get this
  // area, but it is a rect that we should be computing.  The standard means
  // to compute this rect would be to watch all windows with
  // _NET_WM_STRUT(_PARTIAL) hints, and subtract their space from the physical
  // area of the monitor to construct a work area.
  return GetMonitorAreaNearestWindow(view);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeView view) {
  GdkScreen* screen = gdk_screen_get_default();
  gint monitor_num = 0;
  if (view) {
    GtkWidget* top_level = gtk_widget_get_toplevel(view);
    DCHECK(GTK_IS_WINDOW(top_level));
    GtkWindow* window = GTK_WINDOW(top_level);
    screen = gtk_window_get_screen(window);
    monitor_num = gdk_screen_get_monitor_at_window(screen, top_level->window);
  }
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

}  // namespace gfx
