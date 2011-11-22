// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_GTK_GTK_COMPAT_H_
#define UI_BASE_GTK_GTK_COMPAT_H_
#pragma once

#include <gtk/gtk.h>

// Google Chrome must depend on GTK 2.18, at least until the next LTS drops
// (and we might have to extend which version of GTK we want to target due to
// RHEL). To make our porting job for GTK3 easier, we define all the methods
// that replace deprecated APIs in this file and then include it everywhere.
//
// This file is organized first by version, and then with each version,
// alphabetically by method.

#if !GTK_CHECK_VERSION(2, 20, 0)
inline gboolean gtk_widget_get_realized(GtkWidget* widget) {
  return GTK_WIDGET_REALIZED(widget);
}

inline gboolean gtk_widget_is_toplevel(GtkWidget* widget) {
  return GTK_WIDGET_TOPLEVEL(widget);
}
#endif

#if !GTK_CHECK_VERSION(2, 24, 0)
inline void gdk_pixmap_get_size(GdkPixmap* pixmap, gint* width, gint* height) {
  gdk_drawable_get_size(GDK_DRAWABLE(pixmap), width, height);
}

inline GdkScreen* gdk_window_get_screen(GdkWindow* window) {
  return gdk_drawable_get_screen(GDK_DRAWABLE(window));
}
#endif

#endif  // UI_BASE_GTK_GTK_COMPAT_H_
