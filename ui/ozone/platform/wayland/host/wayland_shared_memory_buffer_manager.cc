// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_shared_memory_buffer_manager.h"

#include <utility>

#include "base/trace_event/trace_event.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"

namespace ui {

WaylandShmBufferManager::WaylandShmBufferManager(WaylandConnection* connection)
    : connection_(connection) {}

WaylandShmBufferManager::~WaylandShmBufferManager() {
  DCHECK(shm_buffers_.empty());
}

bool WaylandShmBufferManager::CreateBufferForWidget(
    gfx::AcceleratedWidget widget,
    base::File file,
    size_t length,
    const gfx::Size& size) {
  base::ScopedFD fd(file.TakePlatformFile());
  if (!fd.is_valid() || length == 0 || size.IsEmpty() ||
      widget == gfx::kNullAcceleratedWidget) {
    return false;
  }

  auto it = shm_buffers_.find(widget);
  if (it != shm_buffers_.end())
    return false;

  auto buffer = connection_->shm()->CreateBuffer(std::move(fd), length, size);
  if (!buffer)
    return false;

  shm_buffers_.insert(std::make_pair(widget, std::move(buffer)));

  connection_->ScheduleFlush();
  return true;
}

bool WaylandShmBufferManager::PresentBufferForWidget(
    gfx::AcceleratedWidget widget,
    const gfx::Rect& damage) {
  auto it = shm_buffers_.find(widget);
  if (it == shm_buffers_.end())
    return false;

  // TODO(https://crbug.com/930662): This is just a naive implementation that
  // allows chromium to draw to the buffer at any time, even if it is being used
  // by the Wayland compositor. Instead, we should track buffer releases and
  // frame callbacks from Wayland to ensure perfect frames (while minimizing
  // copies).
  wl_surface* surface = connection_->GetWindow(widget)->surface();
  wl_surface_damage(surface, damage.x(), damage.y(), damage.width(),
                    damage.height());
  wl_surface_attach(surface, it->second.get(), 0, 0);
  wl_surface_commit(surface);
  connection_->ScheduleFlush();
  return true;
}

bool WaylandShmBufferManager::DestroyBuffer(gfx::AcceleratedWidget widget) {
  auto it = shm_buffers_.find(widget);
  if (it == shm_buffers_.end())
    return false;

  shm_buffers_.erase(it);
  connection_->ScheduleFlush();
  return true;
}

}  // namespace ui
