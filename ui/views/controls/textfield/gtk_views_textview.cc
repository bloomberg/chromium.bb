// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/gtk_views_textview.h"

#include "base/utf_string_conversions.h"
#include "ui/gfx/canvas_skia_paint.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "ui/views/controls/textfield/native_textfield_gtk.h"
#include "ui/views/controls/textfield/textfield.h"

G_BEGIN_DECLS

G_DEFINE_TYPE(GtkViewsTextView, gtk_views_textview, GTK_TYPE_TEXT_VIEW)

static gint gtk_views_textview_expose_event(GtkWidget *widget,
                                            GdkEventExpose *event) {
  GtkTextView* text_view = GTK_TEXT_VIEW(widget);
  GdkWindow* text_window = gtk_text_view_get_window(text_view,
                                                     GTK_TEXT_WINDOW_TEXT);
  if (event->window == text_window) {
    gint width, height;
    gdk_drawable_get_size (text_window, &width, &height);
    gtk_paint_flat_box(widget->style, text_window,
        static_cast<GtkStateType>(GTK_WIDGET_STATE(widget)), GTK_SHADOW_NONE,
        &event->area, widget, "textview",
        0, 0, width, height);
  }

  gint result = GTK_WIDGET_CLASS(gtk_views_textview_parent_class)->expose_event(
      widget, event);

  GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(text_view);
  views::NativeTextfieldGtk* host = GTK_VIEWS_TEXTVIEW(widget)->host;

  GtkTextIter start;
  GtkTextIter end;
  gtk_text_buffer_get_bounds(text_buffer, &start, &end);

  // Paint the info text to text window if GtkTextView contains no text.
  if (host && event->window == text_window &&
      !host->textfield()->text_to_display_when_empty().empty() &&
      gtk_text_iter_equal(&start, &end)) {
    gfx::CanvasSkiaPaint canvas(event);
    if (!canvas.is_empty()) {
      gfx::Insets insets =
          views::NativeTextfieldGtk::GetTextViewInnerBorder(text_view);
      gfx::Font font = host->textfield()->font();
      const string16 text = host->textfield()->text_to_display_when_empty();
      canvas.DrawStringInt(
          text, font,
          gfx::GdkColorToSkColor(widget->style->text[GTK_STATE_INSENSITIVE]),
          insets.left(), insets.top(),
          widget->allocation.width - insets.width(), font.GetHeight());
    }
  }

  // Draw border and focus.
  if (event->window == widget->window) {
    gint width;
    gint height;
    gdk_drawable_get_size (widget->window, &width, &height);

    bool has_focus = GTK_WIDGET_HAS_FOCUS(widget);

    gtk_paint_shadow(widget->style, widget->window,
                     has_focus ? GTK_STATE_ACTIVE : GTK_STATE_NORMAL,
                     GTK_SHADOW_OUT,
                     &event->area, widget, "textview",
                     0, 0, width, height);
    if (has_focus) {
      gtk_paint_focus(widget->style, widget->window,
                      static_cast<GtkStateType>(GTK_WIDGET_STATE(widget)),
                      &event->area, widget, "textview",
                      0, 0, width, height);
    }
  }

  return result;
}

static void gtk_views_textview_class_init(
    GtkViewsTextViewClass* views_textview_class) {
  GtkWidgetClass* widget_class =
      reinterpret_cast<GtkWidgetClass*>(views_textview_class);
  widget_class->expose_event = gtk_views_textview_expose_event;
}

static void gtk_views_textview_init(GtkViewsTextView* text_view) {
  text_view->host = NULL;
}

GtkWidget* gtk_views_textview_new(views::NativeTextfieldGtk* host) {
  gpointer text_view = g_object_new(GTK_TYPE_VIEWS_TEXTVIEW, NULL);
  GTK_VIEWS_TEXTVIEW(text_view)->host = host;
  return GTK_WIDGET(text_view);
}

G_END_DECLS
