// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/widget/gtk_views_fixed.h"

// We store whether we use the widget's allocated size as a property. Ideally
// we would stash this in GtkFixedChild, but GtkFixed doesn't allow subclassing
// gtk_fixed_put. Alternatively we could subclass GtkContainer and use our own
// API (effectively duplicating GtkFixed), but that means folks could no longer
// use the GtkFixed API else where in Chrome. For now I'm going with this route.
static const char* kUseAllocatedSize = "__VIEWS_USE_ALLOCATED_SIZE__";

G_BEGIN_DECLS

G_DEFINE_TYPE(GtkViewsFixed, gtk_views_fixed, GTK_TYPE_FIXED)

static void gtk_views_fixed_size_allocate(GtkWidget* widget,
                                          GtkAllocation* allocation) {
  widget->allocation = *allocation;
  if (!GTK_WIDGET_NO_WINDOW(widget) && GTK_WIDGET_REALIZED(widget)) {
    gdk_window_move_resize(widget->window, allocation->x, allocation->y,
                           allocation->width, allocation->height);
  }

  int border_width = GTK_CONTAINER(widget)->border_width;
  GList* children = GTK_FIXED(widget)->children;
  while (children) {
    GtkFixedChild* child = reinterpret_cast<GtkFixedChild*>(children->data);
    children = children->next;

    if (GTK_WIDGET_VISIBLE(child->widget)) {
      GtkAllocation child_allocation;

      gpointer use_allocated_size = g_object_get_data(G_OBJECT(child->widget),
                                                      kUseAllocatedSize);
      if (use_allocated_size) {
        // NOTE: even though the size isn't changing, we have to call
        // size_allocate, otherwise things like buttons won't repaint.
        child_allocation.width = child->widget->allocation.width;
        child_allocation.height = child->widget->allocation.height;
      } else {
        GtkRequisition child_requisition;
        gtk_widget_get_child_requisition(child->widget, &child_requisition);
        child_allocation.width = child_requisition.width;
        child_allocation.height = child_requisition.height;
      }
      child_allocation.x = child->x + border_width;
      child_allocation.y = child->y + border_width;

      if (GTK_WIDGET_NO_WINDOW(widget)) {
        child_allocation.x += widget->allocation.x;
        child_allocation.y += widget->allocation.y;
      }

      gtk_widget_size_allocate(child->widget, &child_allocation);
    }
  }
}

static void gtk_views_fixed_class_init(GtkViewsFixedClass* views_fixed_class) {
  GtkWidgetClass* widget_class =
      reinterpret_cast<GtkWidgetClass*>(views_fixed_class);
  widget_class->size_allocate = gtk_views_fixed_size_allocate;
}

static void gtk_views_fixed_init(GtkViewsFixed* fixed) {
}

GtkWidget* gtk_views_fixed_new(void) {
  return GTK_WIDGET(g_object_new(GTK_TYPE_VIEWS_FIXED, NULL));
}

void gtk_views_fixed_set_use_allocated_size(GtkWidget* widget, bool value) {
  g_object_set_data(G_OBJECT(widget), kUseAllocatedSize,
                    reinterpret_cast<gpointer>(value ? 1 : 0));
}

G_END_DECLS
