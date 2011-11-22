// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_GTK_VIEWS_WINDOW_H_
#define UI_VIEWS_WIDGET_GTK_VIEWS_WINDOW_H_
#pragma once

#include <gtk/gtkwindow.h>

// GtkViewsWindow is a subclass of GtkWindow that overrides its move_focus
// method so that we can handle focus traversing by ourselves.

G_BEGIN_DECLS

#define GTK_TYPE_VIEWS_WINDOW        (gtk_views_window_get_type ())
#define GTK_VIEWS_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_VIEWS_WINDOW, GtkViewsWindow))
#define GTK_VIEWS_WINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_VIEWS_WINDOW, \
                             GtkViewsWindowClass))
#define GTK_IS_VIEWS_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VIEWS_WINDOW))
#define GTK_IS_VIEWS_WINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VIEWS_WINDOW))
#define GTK_VIEWS_WINDOW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VIEWS_WINDOW, GtkViewsWindow))

typedef struct _GtkViewsWindow        GtkViewsWindow;
typedef struct _GtkViewsWindowClass   GtkViewsWindowClass;

struct _GtkViewsWindow {
  GtkWindow window;
};

struct _GtkViewsWindowClass {
  GtkWindowClass parent_class;
};

GtkWidget* gtk_views_window_new(GtkWindowType type);

G_END_DECLS

#endif  // UI_VIEWS_WIDGET_GTK_VIEWS_WINDOW_H_
