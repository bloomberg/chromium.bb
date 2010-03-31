// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "webkit/glue/plugins/mac_accelerated_surface_container_manager.h"

#include "base/logging.h"
#include "webkit/glue/plugins/mac_accelerated_surface_container.h"
#include "webkit/glue/plugins/webplugin.h"

MacAcceleratedSurfaceContainerManager::MacAcceleratedSurfaceContainerManager()
    : current_id_(0) {
}

gfx::PluginWindowHandle
MacAcceleratedSurfaceContainerManager::AllocateFakePluginWindowHandle() {
  MacAcceleratedSurfaceContainer* container =
      new MacAcceleratedSurfaceContainer();
  gfx::PluginWindowHandle res =
      static_cast<gfx::PluginWindowHandle>(++current_id_);
  plugin_window_to_container_map_.insert(std::make_pair(res, container));
  return res;
}

void MacAcceleratedSurfaceContainerManager::DestroyFakePluginWindowHandle(
    gfx::PluginWindowHandle id) {
  MacAcceleratedSurfaceContainer* container = MapIDToContainer(id);
  if (container)
    delete container;
  plugin_window_to_container_map_.erase(id);
}

void MacAcceleratedSurfaceContainerManager::SetSizeAndIOSurface(
    gfx::PluginWindowHandle id,
    int32 width,
    int32 height,
    uint64 io_surface_identifier) {
  MacAcceleratedSurfaceContainer* container = MapIDToContainer(id);
  if (container)
    container->SetSizeAndIOSurface(width, height,
                                      io_surface_identifier, this);
}

void MacAcceleratedSurfaceContainerManager::SetSizeAndTransportDIB(
    gfx::PluginWindowHandle id,
    int32 width,
    int32 height,
    TransportDIB::Handle transport_dib) {
  MacAcceleratedSurfaceContainer* container = MapIDToContainer(id);
  if (container)
    container->SetSizeAndTransportDIB(width, height,
                                      transport_dib, this);
}

void MacAcceleratedSurfaceContainerManager::MovePluginContainer(
    const webkit_glue::WebPluginGeometry& move) {
  MacAcceleratedSurfaceContainer* container = MapIDToContainer(move.window);
  if (container)
    container->MoveTo(move);
}

void MacAcceleratedSurfaceContainerManager::Draw(CGLContextObj context) {
  glClearColor(0, 0, 0, 0);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  GLenum target = GL_TEXTURE_RECTANGLE_ARB;
  glTexEnvi(target, GL_TEXTURE_ENV_MODE, GL_REPLACE);

  for (PluginWindowToContainerMap::const_iterator i =
          plugin_window_to_container_map_.begin();
       i != plugin_window_to_container_map_.end(); ++i) {
    MacAcceleratedSurfaceContainer* container = i->second;
    container->Draw(context);
  }

  // Unbind any texture from the texture target to ensure that the
  // next time through we will have to re-bind the texture and thereby
  // pick up modifications from the other process.
  glBindTexture(target, 0);

  glFlush();
}

void MacAcceleratedSurfaceContainerManager::EnqueueTextureForDeletion(
    GLuint texture) {
  if (texture) {
    textures_pending_deletion_.push_back(texture);
  }
}

MacAcceleratedSurfaceContainer*
    MacAcceleratedSurfaceContainerManager::MapIDToContainer(
        gfx::PluginWindowHandle id) {
  PluginWindowToContainerMap::const_iterator i =
      plugin_window_to_container_map_.find(id);
  if (i != plugin_window_to_container_map_.end())
    return i->second;

  LOG(ERROR) << "Request for plugin container for unknown window id " << id;

  return NULL;
}

