// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_WIDGET_GTK_VIEWS_FIXED_H_
#define UI_VIEWS_WIDGET_GTK_VIEWS_FIXED_H_
#pragma once

#include <gdk/gdk.h>
#include <gtk/gtkfixed.h>

// GtkViewsFixed is a subclass of GtkFixed that can give child widgets
// a set size rather than their requisitioned size (which is actually
// a minimum size, and that can cause issues). This behavior is
// controlled by gtk_views_fixed_set_widget_size; the default is to
// use the Widget's requisitioned size.

G_BEGIN_DECLS

#define GTK_TYPE_VIEWS_FIXED        (gtk_views_fixed_get_type ())
#define GTK_VIEWS_FIXED(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_VIEWS_FIXED, GtkViewsFixed))
#define GTK_VIEWS_FIXED_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_VIEWS_FIXED, GtkViewsFixedClass))
#define GTK_IS_VIEWS_FIXED(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VIEWS_FIXED))
#define GTK_IS_VIEWS_FIXED_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VIEWS_FIXED))
#define GTK_VIEWS_FIXED_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VIEWS_FIXED, GtkViewsFixed))

typedef struct _GtkViewsFixed        GtkViewsFixed;
typedef struct _GtkViewsFixedClass   GtkViewsFixedClass;

struct _GtkViewsFixed {
  GtkFixed fixed;
};

struct _GtkViewsFixedClass {
  GtkFixedClass parent_class;
};

GtkWidget* gtk_views_fixed_new();

GType gtk_views_fixed_get_type();

// If width and height are 0, go back to using the requisitioned size.
// Queues up a re-size on the widget.
void gtk_views_fixed_set_widget_size(GtkWidget* widget, int width, int height);

bool gtk_views_fixed_get_widget_size(GtkWidget* widget,
                                     int* width, int* height);

G_END_DECLS

#endif  // UI_VIEWS_WIDGET_GTK_VIEWS_FIXED_H
