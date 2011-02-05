// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/gtk_views_entry.h"

#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "views/controls/textfield/native_textfield_gtk.h"
#include "views/controls/textfield/textfield.h"

G_BEGIN_DECLS

G_DEFINE_TYPE(GtkViewsEntry, gtk_views_entry, GTK_TYPE_ENTRY)

static gint gtk_views_entry_expose_event(GtkWidget *widget,
                                         GdkEventExpose *event) {
  views::NativeTextfieldGtk* host = GTK_VIEWS_ENTRY(widget)->host;
#if defined(OS_CHROMEOS)
  // Draw textfield background over the default white rectangle.
  if (event->window == widget->window) {
    gfx::CanvasSkiaPaint canvas(event);
    if (!canvas.is_empty() && host) {
      host->textfield()->PaintBackground(&canvas);
    }
  }
#endif

  gint result = GTK_WIDGET_CLASS(gtk_views_entry_parent_class)->expose_event(
      widget, event);

  GtkEntry* entry = GTK_ENTRY(widget);

  // Internally GtkEntry creates an additional window (text_area) that the
  // text is drawn to. We only need paint after that window has painted.
  if (host && event->window == entry->text_area &&
      !host->textfield()->text_to_display_when_empty().empty() &&
      g_utf8_strlen(gtk_entry_get_text(entry), -1) == 0) {
    gfx::CanvasSkiaPaint canvas(event);
    if (!canvas.is_empty()) {
      gfx::Insets insets =
          views::NativeTextfieldGtk::GetEntryInnerBorder(entry);
      gfx::Font font = host->textfield()->font();
      const string16 text = host->textfield()->text_to_display_when_empty();
      canvas.DrawStringInt(
          text, font,
          gfx::GdkColorToSkColor(widget->style->text[GTK_STATE_INSENSITIVE]),
          insets.left(), insets.top(),
          widget->allocation.width - insets.width(), font.GetHeight());
    }
  }

  return result;
}

static void gtk_views_entry_class_init(GtkViewsEntryClass* views_entry_class) {
  GtkWidgetClass* widget_class =
      reinterpret_cast<GtkWidgetClass*>(views_entry_class);
  widget_class->expose_event = gtk_views_entry_expose_event;
}

static void gtk_views_entry_init(GtkViewsEntry* entry) {
  entry->host = NULL;
}

GtkWidget* gtk_views_entry_new(views::NativeTextfieldGtk* host) {
  gpointer entry = g_object_new(GTK_TYPE_VIEWS_ENTRY, NULL);
  GTK_VIEWS_ENTRY(entry)->host = host;
  return GTK_WIDGET(entry);
}

G_END_DECLS
