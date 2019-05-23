// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHARED_MEMORY_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHARED_MEMORY_BUFFER_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

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
                             const gfx::Size& size,
                             uint32_t buffer_id);

  // Attaches and commits a |wl_buffer| with |buffer_id| to a surface with the
  // |widget|.
  bool PresentBufferForWidget(gfx::AcceleratedWidget widget,
                              const gfx::Rect& damage,
                              uint32_t buffer_id);

  // Destroyes a |wl_buffer| with the |buffer_id| for a surface with the
  // |widget|.
  bool DestroyBuffer(gfx::AcceleratedWidget widget, uint32_t buffer_id);

 private:
  struct Buffer {
    Buffer() = delete;
    Buffer(gfx::AcceleratedWidget widget, wl::Object<wl_buffer> buffer);
    ~Buffer();

    // Widget this buffer is created for.
    gfx::AcceleratedWidget widget;

    // Actual wayland buffer object.
    wl::Object<wl_buffer> buffer;
  };

  // A container of created buffers.
  base::flat_map<uint32_t, std::unique_ptr<Buffer>> shm_buffers_;

  // Non-owned pointer to the main connection.
  WaylandConnection* connection_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(WaylandShmBufferManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_SHARED_MEMORY_BUFFER_MANAGER_H_
