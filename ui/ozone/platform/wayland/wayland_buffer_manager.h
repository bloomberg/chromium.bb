// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_WAYLAND_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_WAYLAND_BUFFER_MANAGER_H_

#include <map>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/macros.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/ozone/platform/wayland/wayland_object.h"

struct zwp_linux_dmabuf_v1;
struct zwp_linux_buffer_params_v1;

namespace gfx {
enum class BufferFormat;
}  // namespace gfx

namespace ui {

class WaylandConnection;

// The manager uses zwp_linux_dmabuf protocol to create wl_buffers from added
// dmabuf buffers. Only used when GPU runs in own process.
class WaylandBufferManager {
 public:
  WaylandBufferManager(zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                       WaylandConnection* connection);
  ~WaylandBufferManager();

  std::string error_message() { return std::move(error_message_); }

  std::vector<gfx::BufferFormat> supported_buffer_formats() {
    return supported_buffer_formats_;
  }

  // Creates a wl_buffer based on the dmabuf |file| descriptor. On error, false
  // is returned and |error_message_| is set.
  bool CreateBuffer(base::File file,
                    uint32_t width,
                    uint32_t height,
                    const std::vector<uint32_t>& strides,
                    const std::vector<uint32_t>& offsets,
                    uint32_t format,
                    const std::vector<uint64_t>& modifiers,
                    uint32_t planes_count,
                    uint32_t buffer_id);

  // Assigns a wl_buffer with |buffer_id| to a window with the same |widget|. On
  // error, false is returned and |error_message_| is set.
  bool ScheduleBufferSwap(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  // Destroys a buffer with |buffer_id| in |buffers_|. On error, false is
  // returned and |error_message_| is set.
  bool DestroyBuffer(uint32_t buffer_id);

  // Destroys all the data and buffers stored in own containers.
  void ClearState();

 private:
  // This is an internal helper representation of a wayland buffer object, which
  // the GPU process creates when CreateBuffer is called. It's used for
  // asynchronous buffer creation and stores |params| parameter to find out,
  // which Buffer the wl_buffer corresponds to when CreateSucceeded is called.
  // What is more, the Buffer stores such information as a widget it is attached
  // to, its buffer id for simplier buffer management and other members specific
  // to this Buffer object on run-time.
  struct Buffer {
    Buffer();
    Buffer(uint32_t id, zwp_linux_buffer_params_v1* zwp_params);
    ~Buffer();

    // GPU GbmPixmapWayland corresponding buffer id.
    uint32_t buffer_id = 0;

    // Widget to attached/being attach WaylandWindow.
    gfx::AcceleratedWidget widget = gfx::kNullAcceleratedWidget;

    // Params that are used to create a wl_buffer.
    zwp_linux_buffer_params_v1* params = nullptr;

    // A wl_buffer backed by a dmabuf created on the GPU side.
    wl::Object<wl_buffer> wl_buffer;

    DISALLOW_COPY_AND_ASSIGN(Buffer);
  };

  bool SwapBuffer(Buffer* buffer);

  // Validates data sent from GPU. If invalid, returns false and sets an error
  // message to |error_message_|.
  bool ValidateDataFromGpu(const base::File& file,
                           uint32_t width,
                           uint32_t height,
                           const std::vector<uint32_t>& strides,
                           const std::vector<uint32_t>& offsets,
                           uint32_t format,
                           const std::vector<uint64_t>& modifiers,
                           uint32_t planes_count,
                           uint32_t buffer_id);
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           uint32_t buffer_id);

  void CreateSucceededInternal(struct zwp_linux_buffer_params_v1* params,
                               struct wl_buffer* new_buffer);

  // zwp_linux_dmabuf_v1_listener
  static void Modifiers(void* data,
                        struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                        uint32_t format,
                        uint32_t modifier_hi,
                        uint32_t modifier_lo);
  static void Format(void* data,
                     struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                     uint32_t format);

  // zwp_linux_buffer_params_v1_listener
  static void CreateSucceeded(void* data,
                              struct zwp_linux_buffer_params_v1* params,
                              struct wl_buffer* new_buffer);
  static void CreateFailed(void* data,
                           struct zwp_linux_buffer_params_v1* params);

  // Stores announced buffer formats supported by the compositor.
  std::vector<gfx::BufferFormat> supported_buffer_formats_;

  // A container of created buffers.
  base::flat_map<uint32_t, std::unique_ptr<Buffer>> buffers_;

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  wl::Object<zwp_linux_dmabuf_v1> zwp_linux_dmabuf_;

  // Non-owned pointer to the main connection.
  WaylandConnection* connection_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_BUFFER_MANAGER_H_
