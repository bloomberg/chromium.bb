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
  bool SwapBuffer(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  // Destroys a buffer with |buffer_id| in |buffers_|. On error, false is
  // returned and |error_message_| is set.
  bool DestroyBuffer(uint32_t buffer_id);

  // Destroys all the data and buffers stored in own containers.
  void ClearState();

 private:
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

  // Stores a wl_buffer and it's id provided by the GbmBuffer object on the
  // GPU process side.
  base::flat_map<uint32_t, wl::Object<wl_buffer>> buffers_;

  // A temporary params-to-buffer id map, which is used to identify which
  // id wl_buffer should be assigned when storing it in the |buffers_| map
  // during CreateSucceeded call.
  base::flat_map<struct zwp_linux_buffer_params_v1*, uint32_t>
      params_to_id_map_;

  // It might happen that GPU asks to swap buffers, when a wl_buffer hasn't
  // been created yet. Thus, store the request in a pending map. Once a buffer
  // is created, it will be attached to the requested WaylandWindow based on the
  // gfx::AcceleratedWidget.
  base::flat_map<uint32_t, gfx::AcceleratedWidget> pending_buffer_map_;

  // Stores announced buffer formats supported by the compositor.
  std::vector<gfx::BufferFormat> supported_buffer_formats_;

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  wl::Object<zwp_linux_dmabuf_v1> zwp_linux_dmabuf_;

  // Non-owned pointer to the main connection.
  WaylandConnection* connection_ = nullptr;
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_WAYLAND_BUFFER_MANAGER_H_
