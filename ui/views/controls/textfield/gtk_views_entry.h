// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_GTK_VIEWS_ENTRY_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_GTK_VIEWS_ENTRY_H_
#pragma once

#include <gdk/gdk.h>
#include <gtk/gtkentry.h>

namespace views {
class NativeTextfieldGtk;
}

// GtkViewsEntry is a subclass of GtkEntry that can draw text when the text in
// the entry is empty. For example, this could show the text 'password' in a
// password field when the text is empty. GtkViewsEntry is used internally by
// NativeTextfieldGtk.

G_BEGIN_DECLS

#define GTK_TYPE_VIEWS_ENTRY        (gtk_views_entry_get_type ())
#define GTK_VIEWS_ENTRY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), GTK_TYPE_VIEWS_ENTRY, GtkViewsEntry))
#define GTK_VIEWS_ENTRY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), GTK_TYPE_VIEWS_ENTRY, GtkViewsEntryClass))
#define GTK_IS_VIEWS_ENTRY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTK_TYPE_VIEWS_ENTRY))
#define GTK_IS_VIEWS_ENTRY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), GTK_TYPE_VIEWS_ENTRY))
#define GTK_VIEWS_ENTRY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), GTK_TYPE_VIEWS_ENTRY, GtkViewsEntry))

typedef struct _GtkViewsEntry        GtkViewsEntry;
typedef struct _GtkViewsEntryClass   GtkViewsEntryClass;

struct _GtkViewsEntry {
  GtkEntry entry;
  views::NativeTextfieldGtk* host;
};

struct _GtkViewsEntryClass {
  GtkEntryClass parent_class;
};

GtkWidget* gtk_views_entry_new(views::NativeTextfieldGtk* host);

GType gtk_views_entry_get_type();

G_END_DECLS

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_GTK_VIEWS_ENTRY_H_
