// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/view.h"

#include <gtk/gtk.h>

#include "base/logging.h"

namespace views {

// static
int View::GetDoubleClickTimeMS() {
  GdkDisplay* display = gdk_display_get_default();
  return display ? display->double_click_time : 500;
}

int View::GetMenuShowDelay() {
  return kShowFolderDropMenuDelay;
}

void View::NotifyAccessibilityEvent(AccessibilityTypes::Event event_type) {
  // Not implemented on GTK.
}

ViewAccessibilityWrapper* View::GetViewAccessibilityWrapper() {
  NOTIMPLEMENTED();
  return NULL;
}

int View::GetHorizontalDragThreshold() {
  static bool determined_threshold = false;
  static int drag_threshold = 8;
  if (determined_threshold)
    return drag_threshold;
  determined_threshold = true;
  GtkSettings* settings = gtk_settings_get_default();
  if (!settings)
    return drag_threshold;
  int value = 0;
  g_object_get(G_OBJECT(settings), "gtk-dnd-drag-threshold", &value, NULL);
  if (value)
    drag_threshold = value;
  return drag_threshold;
}

int View::GetVerticalDragThreshold() {
  // Vertical and horizontal drag threshold are the same in Gtk.
  return GetHorizontalDragThreshold();
}

}  // namespace views
