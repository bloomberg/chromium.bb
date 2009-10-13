// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/screen.h"

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
  DCHECK(success);
  glong* data = reinterpret_cast<glong*>(raw_data);
  return gfx::Rect(data[0], data[1], data[0] + data[2], data[1] + data[3]);
}

// static
gfx::Rect Screen::GetMonitorWorkAreaNearestWindow(gfx::NativeWindow window) {
  // TODO(beng): use |window|.
  return GetPrimaryMonitorBounds();
}

// static
gfx::Rect Screen::GetMonitorAreaNearestWindow(gfx::NativeWindow window) {
  GdkScreen* screen = gtk_window_get_screen(window);
  gint monitor_num = gdk_screen_get_monitor_at_window(screen,
      (GTK_WIDGET(window))->window);
  GdkRectangle bounds;
  gdk_screen_get_monitor_geometry(screen, monitor_num, &bounds);
  return gfx::Rect(bounds);
}

// static
gfx::Rect Screen::GetMonitorAreaNearestPoint(const gfx::Point& point) {
  GdkScreen* screen = gdk_screen_get_default();
  gint monitor = gdk_screen_get_monitor_at_point(screen, point.x(), point.y());
  GdkRectangle bounds;
  gdk_screen_get_monitor_geometry(screen, monitor, &bounds);
  return gfx::Rect(bounds);
}

}  // namespace

