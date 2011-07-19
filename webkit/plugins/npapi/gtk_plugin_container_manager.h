// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef WEBKIT_PLUGINS_NPAPI_GTK_PLUGIN_CONTAINER_MANAGER_H_
#define WEBKIT_PLUGINS_NPAPI_GTK_PLUGIN_CONTAINER_MANAGER_H_

#include <gtk/gtk.h>
#include <map>

#include "ui/gfx/native_widget_types.h"

typedef struct _GtkWidget GtkWidget;

namespace webkit {
namespace npapi {

struct WebPluginGeometry;

// Helper class that creates and manages plugin containers (GtkSocket).
class GtkPluginContainerManager {
 public:
  GtkPluginContainerManager();
  ~GtkPluginContainerManager();

  // Sets the widget that will host the plugin containers. Must be a GtkFixed.
  void set_host_widget(GtkWidget *widget) { host_widget_ = widget; }

  // Creates a new plugin container, for a given plugin XID.
  GtkWidget* CreatePluginContainer(gfx::PluginWindowHandle id);

  // Destroys a plugin container, given the plugin XID.
  void DestroyPluginContainer(gfx::PluginWindowHandle id);

  // Takes an update from WebKit about a plugin's position and side and moves
  // the plugin accordingly.
  void MovePluginContainer(const WebPluginGeometry& move);

 private:
  // Maps a plugin XID to the corresponding container widget.
  GtkWidget* MapIDToWidget(gfx::PluginWindowHandle id);

  // Maps a container widget to the corresponding plugin XID.
  gfx::PluginWindowHandle MapWidgetToID(GtkWidget* widget);

  // Callback for when the plugin container gets realized, at which point it
  // plugs the plugin XID.
  static void RealizeCallback(GtkWidget *widget, void *user_data);

  // Parent of the plugin containers.
  GtkWidget* host_widget_;

  // A map that associates plugin containers to the plugin XID.
  typedef std::map<gfx::PluginWindowHandle, GtkWidget*> PluginWindowToWidgetMap;
  PluginWindowToWidgetMap plugin_window_to_widget_map_;
};

}  // namespace npapi
}  // namespace webkit

#endif  // WEBKIT_PLUGINS_NPAPI_GTK_PLUGIN_CONTAINER_MANAGER_H_
