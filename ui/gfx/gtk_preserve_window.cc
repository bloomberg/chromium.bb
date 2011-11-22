// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gfx/gtk_preserve_window.h"

#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "ui/base/gtk/gtk_compat.h"

G_BEGIN_DECLS

#define GTK_PRESERVE_WINDOW_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), \
                                            GTK_TYPE_PRESERVE_WINDOW, \
                                            GtkPreserveWindowPrivate))

typedef struct _GtkPreserveWindowPrivate GtkPreserveWindowPrivate;

struct _GtkPreserveWindowPrivate {
  // If true, don't create/destroy windows on realize/unrealize.
  gboolean preserve_window;

  // Whether or not we delegate the resize of the GdkWindow
  // to someone else.
  gboolean delegate_resize;
};

G_DEFINE_TYPE(GtkPreserveWindow, gtk_preserve_window, GTK_TYPE_FIXED)

static void gtk_preserve_window_destroy(GtkObject* object);
static void gtk_preserve_window_realize(GtkWidget* widget);
static void gtk_preserve_window_unrealize(GtkWidget* widget);
static void gtk_preserve_window_size_allocate(GtkWidget* widget,
                                              GtkAllocation* allocation);

static void gtk_preserve_window_class_init(GtkPreserveWindowClass *klass) {
  GtkWidgetClass* widget_class = reinterpret_cast<GtkWidgetClass*>(klass);
  widget_class->realize = gtk_preserve_window_realize;
  widget_class->unrealize = gtk_preserve_window_unrealize;
  widget_class->size_allocate = gtk_preserve_window_size_allocate;

  GtkObjectClass* object_class = reinterpret_cast<GtkObjectClass*>(klass);
  object_class->destroy = gtk_preserve_window_destroy;

  GObjectClass* gobject_class = G_OBJECT_CLASS(klass);
  g_type_class_add_private(gobject_class, sizeof(GtkPreserveWindowPrivate));
}

static void gtk_preserve_window_init(GtkPreserveWindow* widget) {
  GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(widget);
  priv->preserve_window = FALSE;

  // These widgets always have their own window.
  gtk_fixed_set_has_window(GTK_FIXED(widget), TRUE);
}

GtkWidget* gtk_preserve_window_new() {
  return GTK_WIDGET(g_object_new(GTK_TYPE_PRESERVE_WINDOW, NULL));
}

static void gtk_preserve_window_destroy(GtkObject* object) {
  GtkWidget* widget = reinterpret_cast<GtkWidget*>(object);
  GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(widget);

  if (widget->window) {
    gdk_window_set_user_data(widget->window, NULL);
    // If the window is preserved, someone else must destroy it.
    if (!priv->preserve_window)
      gdk_window_destroy(widget->window);
    widget->window = NULL;
  }

  GTK_OBJECT_CLASS(gtk_preserve_window_parent_class)->destroy(object);
}

static void gtk_preserve_window_realize(GtkWidget* widget) {
  g_return_if_fail(GTK_IS_PRESERVE_WINDOW(widget));

  if (widget->window) {
    gdk_window_reparent(widget->window,
                        gtk_widget_get_parent_window(widget),
                        widget->allocation.x,
                        widget->allocation.y);
    GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(widget);
    if (!priv->delegate_resize) {
      gdk_window_resize(widget->window,
                        widget->allocation.width,
                        widget->allocation.height);
    }
    widget->style = gtk_style_attach(widget->style, widget->window);
    gtk_style_set_background(widget->style, widget->window, GTK_STATE_NORMAL);

    gint event_mask = gtk_widget_get_events(widget);
    event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;
    gdk_window_set_events(widget->window, (GdkEventMask) event_mask);
    gdk_window_set_user_data(widget->window, widget);

    // Deprecated as of GTK 2.22. Used for compatibility.
    // It should be: gtk_widget_set_realized(widget, TRUE)
    GTK_WIDGET_SET_FLAGS(widget, GTK_REALIZED);
  } else {
    GTK_WIDGET_CLASS(gtk_preserve_window_parent_class)->realize(widget);
  }
}

static void gtk_preserve_window_unrealize(GtkWidget* widget) {
  g_return_if_fail(GTK_IS_PRESERVE_WINDOW(widget));

  GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(widget);
  if (priv->preserve_window) {
    GtkWidgetClass* widget_class =
        GTK_WIDGET_CLASS(gtk_preserve_window_parent_class);
    GtkContainerClass* container_class =
        GTK_CONTAINER_CLASS(gtk_preserve_window_parent_class);

    // Deprecated as of GTK 2.22. Used for compatibility.
    // It should be: gtk_widget_get_mapped()
    if (GTK_WIDGET_MAPPED(widget)) {
      widget_class->unmap(widget);

      // Deprecated as of GTK 2.22. Used for compatibility.
      // It should be: gtk_widget_set_mapped(widget, FALSE)
      GTK_WIDGET_UNSET_FLAGS(widget, GTK_MAPPED);
    }

    // This is the behavior from GtkWidget, inherited by GtkFixed.
    // It is unclear why we should not call the potentially overridden
    // unrealize method (via the callback), but doing so causes errors.
    container_class->forall(
        GTK_CONTAINER(widget), FALSE,
        reinterpret_cast<GtkCallback>(gtk_widget_unrealize), NULL);

    gtk_style_detach(widget->style);
    gdk_window_reparent(widget->window, gdk_get_default_root_window(), 0, 0);
    gtk_selection_remove_all(widget);
    gdk_window_set_user_data(widget->window, NULL);

    // Deprecated as of GTK 2.22. Used for compatibility.
    // It should be: gtk_widget_set_realized(widget, FALSE)
    GTK_WIDGET_UNSET_FLAGS(widget, GTK_REALIZED);
  } else {
    GTK_WIDGET_CLASS(gtk_preserve_window_parent_class)->unrealize(widget);
  }
}

gboolean gtk_preserve_window_get_preserve(GtkPreserveWindow* window) {
  g_return_val_if_fail(GTK_IS_PRESERVE_WINDOW(window), FALSE);
  GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(window);

  return priv->preserve_window;
}

void gtk_preserve_window_set_preserve(GtkPreserveWindow* window,
                                      gboolean value) {
  g_return_if_fail(GTK_IS_PRESERVE_WINDOW(window));
  GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(window);
  priv->preserve_window = value;

  GtkWidget* widget = GTK_WIDGET(window);
  if (value && !widget->window) {
    GdkWindowAttr attributes;
    gint attributes_mask;

    // We may not know the width and height, so we rely on the fact
    // that a size-allocation will resize it later.
    attributes.width = 1;
    attributes.height = 1;

    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.override_redirect = TRUE;

    attributes.visual = gtk_widget_get_visual(widget);
    attributes.colormap = gtk_widget_get_colormap(widget);

    attributes.event_mask = gtk_widget_get_events(widget);
    attributes.event_mask |= GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK;

    attributes_mask = GDK_WA_VISUAL | GDK_WA_COLORMAP | GDK_WA_NOREDIR;
    widget->window = gdk_window_new(
        gdk_get_default_root_window(), &attributes, attributes_mask);
  } else if (!value && widget->window && !gtk_widget_get_realized(widget)) {
    gdk_window_destroy(widget->window);
    widget->window = NULL;
  }
}

void gtk_preserve_window_size_allocate(GtkWidget* widget,
                                       GtkAllocation* allocation) {
  g_return_if_fail(GTK_IS_PRESERVE_WINDOW(widget));

  widget->allocation = *allocation;

  if (gtk_widget_get_realized(widget)) {
    GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(widget);
    if (priv->delegate_resize) {
      gdk_window_move(widget->window, allocation->x, allocation->y);
    } else {
      gdk_window_move_resize(
          widget->window, allocation->x, allocation->y,
          allocation->width, allocation->height);
    }
  }

  // Propagate resize to children
  guint16 border_width = GTK_CONTAINER(widget)->border_width;
  GList *children = GTK_FIXED(widget)->children;
  while (children) {
    GtkFixedChild *child = reinterpret_cast<GtkFixedChild*>(children->data);
    if (gtk_widget_get_visible(child->widget)) {
      GtkRequisition child_requisition;
      gtk_widget_get_child_requisition(child->widget, &child_requisition);

      GtkAllocation child_allocation;
      child_allocation.x = child->x + border_width;
      child_allocation.y = child->y + border_width;
      child_allocation.width = child_requisition.width;
      child_allocation.height = child_requisition.height;

      gtk_widget_size_allocate(child->widget, &child_allocation);
    }
    children = children->next;
  }
}

void gtk_preserve_window_delegate_resize(GtkPreserveWindow* widget,
                                         gboolean delegate) {
  GtkPreserveWindowPrivate* priv = GTK_PRESERVE_WINDOW_GET_PRIVATE(widget);
  priv->delegate_resize = delegate;
}

G_END_DECLS
