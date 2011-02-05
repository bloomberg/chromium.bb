// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/plugins/npapi/gtk_plugin_container_manager.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "ui/gfx/gtk_util.h"
#include "webkit/plugins/npapi/gtk_plugin_container.h"
#include "webkit/plugins/npapi/webplugin.h"

namespace webkit {
namespace npapi {

GtkPluginContainerManager::GtkPluginContainerManager() : host_widget_(NULL) {}

GtkPluginContainerManager::~GtkPluginContainerManager() {}

GtkWidget* GtkPluginContainerManager::CreatePluginContainer(
    gfx::PluginWindowHandle id) {
  DCHECK(host_widget_);
  GtkWidget *widget = gtk_plugin_container_new();
  plugin_window_to_widget_map_.insert(std::make_pair(id, widget));

  // The Realize callback is responsible for adding the plug into the socket.
  // The reason is 2-fold:
  // - the plug can't be added until the socket is realized, but this may not
  // happen until the socket is attached to a top-level window, which isn't the
  // case for background tabs.
  // - when dragging tabs, the socket gets unrealized, which breaks the XEMBED
  // connection. We need to make it again when the tab is reattached, and the
  // socket gets realized again.
  //
  // Note, the RealizeCallback relies on the plugin_window_to_widget_map_ to
  // have the mapping.
  g_signal_connect(widget, "realize",
                   G_CALLBACK(RealizeCallback), this);

  // Don't destroy the widget when the plug is removed.
  g_signal_connect(widget, "plug-removed",
                   G_CALLBACK(gtk_true), NULL);

  gtk_container_add(GTK_CONTAINER(host_widget_), widget);
  gtk_widget_show(widget);

  return widget;
}

void GtkPluginContainerManager::DestroyPluginContainer(
    gfx::PluginWindowHandle id) {
  DCHECK(host_widget_);
  GtkWidget* widget = MapIDToWidget(id);
  if (widget)
    gtk_widget_destroy(widget);

  plugin_window_to_widget_map_.erase(id);
}

void GtkPluginContainerManager::MovePluginContainer(
    const WebPluginGeometry& move) {
  DCHECK(host_widget_);
  GtkWidget *widget = MapIDToWidget(move.window);
  if (!widget)
    return;

  DCHECK(!GTK_WIDGET_NO_WINDOW(widget));

  if (!move.visible) {
    gtk_widget_hide(widget);
    return;
  }

  gtk_widget_show(widget);

  if (!move.rects_valid)
    return;

  // TODO(piman): if the widget hasn't been realized (e.g. the tab has been
  // torn off and the parent gtk widget has been detached from the hierarchy),
  // we lose the cutout information.
  if (GTK_WIDGET_REALIZED(widget)) {
    GdkRectangle clip_rect = move.clip_rect.ToGdkRectangle();
    GdkRegion* clip_region = gdk_region_rectangle(&clip_rect);
    gfx::SubtractRectanglesFromRegion(clip_region, move.cutout_rects);
    gdk_window_shape_combine_region(widget->window, clip_region, 0, 0);
    gdk_region_destroy(clip_region);
  }

  // Update the window position.  Resizing is handled by WebPluginDelegate.
  // TODO(deanm): Verify that we only need to move and not resize.
  // TODO(evanm): we should cache the last shape and position and skip all
  // of this business in the common case where nothing has changed.
  int current_x, current_y;

  // Until the above TODO is resolved, we can grab the last position
  // off of the GtkFixed with a bit of hackery.
  GValue value = {0};
  g_value_init(&value, G_TYPE_INT);
  gtk_container_child_get_property(GTK_CONTAINER(host_widget_), widget,
                                   "x", &value);
  current_x = g_value_get_int(&value);
  gtk_container_child_get_property(GTK_CONTAINER(host_widget_), widget,
                                   "y", &value);
  current_y = g_value_get_int(&value);
  g_value_unset(&value);

  if (move.window_rect.x() != current_x ||
      move.window_rect.y() != current_y) {
    // Calling gtk_fixed_move unnecessarily is a no-no, as it causes the
    // parent window to repaint!
    gtk_fixed_move(GTK_FIXED(host_widget_),
                   widget,
                   move.window_rect.x(),
                   move.window_rect.y());
  }

  gtk_plugin_container_set_size(widget,
                                move.window_rect.width(),
                                move.window_rect.height());
}

GtkWidget* GtkPluginContainerManager::MapIDToWidget(
    gfx::PluginWindowHandle id) {
  PluginWindowToWidgetMap::const_iterator i =
      plugin_window_to_widget_map_.find(id);
  if (i != plugin_window_to_widget_map_.end())
    return i->second;

  LOG(ERROR) << "Request for widget host for unknown window id " << id;

  return NULL;
}

gfx::PluginWindowHandle GtkPluginContainerManager::MapWidgetToID(
     GtkWidget* widget) {
  for (PluginWindowToWidgetMap::const_iterator i =
          plugin_window_to_widget_map_.begin();
       i != plugin_window_to_widget_map_.end(); ++i) {
    if (i->second == widget)
      return i->first;
  }

  LOG(ERROR) << "Request for id for unknown widget";
  return 0;
}

// static
void GtkPluginContainerManager::RealizeCallback(GtkWidget* widget,
                                                void* user_data) {
  GtkPluginContainerManager* plugin_container_manager =
      static_cast<GtkPluginContainerManager*>(user_data);

  gfx::PluginWindowHandle id = plugin_container_manager->MapWidgetToID(widget);
  if (id)
    gtk_socket_add_id(GTK_SOCKET(widget), id);
}

}  // namespace npapi
}  // namespace webkit
