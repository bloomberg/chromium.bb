// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SHARED_MEMORY_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SHARED_MEMORY_BUFFER_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_object.h"
#include "ui/ozone/platform/wayland/wayland_util.h"

namespace ui {

class WaylandConnection;

// Manages shared memory buffers, which are created by the GPU on the GPU
// process/thread side, when software rendering is used.
class WaylandShmBufferManager {
 public:
  explicit WaylandShmBufferManager(WaylandConnection* connection);
  ~WaylandShmBufferManager();

  // Creates a wl_buffer based on shared memory handle for the specified
  // |widget|.
  bool CreateBufferForWidget(gfx::AcceleratedWidget widget,
                             base::File file,
                             size_t length,
                             const gfx::Size& size);

  // Attaches and commits a |wl_buffer| created for the |widget| in the Create
  // method.
  bool PresentBufferForWidget(gfx::AcceleratedWidget widget,
                              const gfx::Rect& damage);

  // Destroyes a |wl_buffer|, which was created for the |widget| in the Create
  // method.
  bool DestroyBuffer(gfx::AcceleratedWidget widget);

 private:
  // Internal representation of a shared memory buffer.
  struct ShmBuffer {
    ShmBuffer();
    ShmBuffer(wl::Object<struct wl_buffer> buffer,
              wl::Object<struct wl_shm_pool> pool);
    ~ShmBuffer();

    // A wl_buffer backed by a shared memory handle passed from the gpu process.
    wl::Object<struct wl_buffer> shm_buffer;

    // Is used to create shared memory based buffer objects.
    wl::Object<struct wl_shm_pool> shm_pool;

    DISALLOW_COPY_AND_ASSIGN(ShmBuffer);
  };

  // A container of created buffers.
  base::flat_map<gfx::AcceleratedWidget, std::unique_ptr<ShmBuffer>>
      shm_buffers_;

  // Non-owned pointer to the main connection.
  WaylandConnection* connection_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WaylandShmBufferManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_SHARED_MEMORY_BUFFER_MANAGER_H_
