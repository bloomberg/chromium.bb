// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/gtk/tooltip_window_gtk.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"

namespace ui {

TooltipWindowGtk::TooltipWindowGtk(GtkWidget* widget)
    : host_(widget),
      window_(NULL),
      alignment_(NULL),
      label_(NULL) {
  Init();
}

TooltipWindowGtk::~TooltipWindowGtk() {
  if (window_)
    gtk_widget_destroy(window_);
}

void TooltipWindowGtk::SetTooltipText(const string16& text) {
  gtk_label_set_text(label(), UTF16ToUTF8(text).c_str());
}

GtkLabel* TooltipWindowGtk::label() {
  return GTK_LABEL(label_);
}

void  TooltipWindowGtk::Init() {
  // Creates and setup tooltip window.
  window_ = gtk_window_new(GTK_WINDOW_POPUP);
  gtk_window_set_type_hint(GTK_WINDOW(window_), GDK_WINDOW_TYPE_HINT_TOOLTIP);
  gtk_widget_set_app_paintable(window_, TRUE);
  gtk_window_set_resizable(GTK_WINDOW(window_), FALSE);
  gtk_widget_set_name(window_, "gtk-tooltip");

  GdkColormap* rgba_colormap =
      gdk_screen_get_rgba_colormap(gdk_screen_get_default());
  if (rgba_colormap)
    gtk_widget_set_colormap(window_, rgba_colormap);

  g_signal_connect(G_OBJECT(window_), "expose-event",
                   G_CALLBACK(&OnPaintThunk), this);
  g_signal_connect(G_OBJECT(window_), "style-set",
                   G_CALLBACK(&OnStyleSetThunk), this);

  alignment_ = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
  gtk_container_add(GTK_CONTAINER(window_), alignment_);
  gtk_widget_show(alignment_);

  label_ = gtk_label_new("");
  gtk_label_set_line_wrap(GTK_LABEL(label_), TRUE);
  gtk_container_add(GTK_CONTAINER(alignment_), label_);
  gtk_widget_show(label_);

  // Associates the tooltip window with given widget
  gtk_widget_set_tooltip_window(host_, GTK_WINDOW(window_));
}

// Paints our customized tooltip window.
gboolean TooltipWindowGtk::OnPaint(GtkWidget* widget, GdkEventExpose* event) {
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  gtk_paint_flat_box(gtk_widget_get_style(widget),
      gtk_widget_get_window(widget),
      GTK_STATE_NORMAL,
      GTK_SHADOW_OUT,
      NULL,
      widget,
      "tooltip",
      0, 0,
      allocation.width,
      allocation.height);

  return FALSE;
}

// Style change handler.
void TooltipWindowGtk::OnStyleSet(GtkWidget* widget,
                                  GtkStyle* previous_style) {
  GtkStyle* style = gtk_widget_get_style(widget);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment_),
      style->ythickness,
      style->ythickness,
      style->xthickness,
      style->xthickness);

  gtk_widget_queue_draw(widget);
}

}  // namespace ui
