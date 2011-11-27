// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_GTK_VIEWS_TEXTVIEW_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_GTK_VIEWS_TEXTVIEW_H_
#pragma once

#include <gdk/gdk.h>
#include <gtk/gtktextview.h>

namespace views {
class NativeTextfieldGtk;
}

// Similar to GtkViewsEntry, GtkViewsTextView is a subclass of GtkTextView
// with a border and ability to show a custom info when text view has no text.

G_BEGIN_DECLS

#define GTK_TYPE_VIEWS_TEXTVIEW        (gtk_views_textview_get_type())
#define GTK_VIEWS_TEXTVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_VIEWS_TEXTVIEW, \
    GtkViewsTextView))
#define GTK_VIEWS_TEXTVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_VIEWS_TEXTVIEW, \
    GtkViewsTextViewClass))
#define GTK_IS_VIEWS_TEXTVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VIEWS_TEXTVIEW))
#define GTK_IS_VIEWS_TEXTVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VIEWS_TEXTVIEW))
#define GTK_VIEWS_TEXTVIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VIEWS_TEXTVIEW, \
    GtkViewsTextView))

typedef struct _GtkViewsTextView        GtkViewsTextView;
typedef struct _GtkViewsTextViewClass   GtkViewsTextViewClass;

struct _GtkViewsTextView {
  GtkTextView text_view;
  views::NativeTextfieldGtk* host;
};

struct _GtkViewsTextViewClass {
  GtkTextViewClass parent_class;
};

GtkWidget* gtk_views_textview_new(views::NativeTextfieldGtk* host);

GType gtk_views_textview_get_type();

G_END_DECLS

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_GTK_VIEWS_TEXTVIEW_H_
