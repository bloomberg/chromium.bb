// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_H_
#define UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/files/file.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gfx/swap_result.h"
#include "ui/ozone/platform/wayland/common/wayland_object.h"
#include "ui/ozone/platform/wayland/common/wayland_util.h"

namespace ui {

class WaylandConnection;
class WaylandWindow;

// The manager uses zwp_linux_dmabuf protocol to create wl_buffers from added
// dmabuf buffers, and uses internal representation of surfaces, which store
// buffers associated with the WaylandWindow.
class WaylandBufferManager {
 public:
  explicit WaylandBufferManager(WaylandConnection* connection);
  ~WaylandBufferManager();

  std::string error_message() { return std::move(error_message_); }

  void OnWindowAdded(WaylandWindow* window);
  void OnWindowRemoved(WaylandWindow* window);

  // Creates a wl_buffer based on the dmabuf |file| descriptor. On error, false
  // is returned and |error_message_| is set.
  bool CreateBuffer(gfx::AcceleratedWidget widget,
                    base::File file,
                    const gfx::Size& size,
                    const std::vector<uint32_t>& strides,
                    const std::vector<uint32_t>& offsets,
                    const std::vector<uint64_t>& modifiers,
                    uint32_t format,
                    uint32_t planes_count,
                    uint32_t buffer_id);

  // Assigns a wl_buffer with |buffer_id| to a window with the same |widget|. On
  // error, false is returned and |error_message_| is set. A |damage_region|
  // identifies which part of the buffer is updated. If an empty region is
  // provided, the whole buffer is updated. Once a frame callback or
  // presentation callback is received, WaylandConnection::OnSubmission and
  // WaylandConnection::OnPresentation are called. Though, it is guaranteed
  // OnPresentation won't be called earlier than OnSubmission.
  bool CommitBuffer(gfx::AcceleratedWidget widget,
                    uint32_t buffer_id,
                    const gfx::Rect& damage_region);

  // Destroys a buffer with |buffer_id| in |buffers_|. On error, false is
  // returned and |error_message_| is set.
  bool DestroyBuffer(gfx::AcceleratedWidget widget, uint32_t buffer_id);

  // Destroys all the data and buffers stored in own containers.
  void ClearState();

 private:
  // This is an internal representation of a real surface, which holds a pointer
  // to WaylandWindow. Also, this object holds buffers, frame callbacks and
  // presentation callbacks for that window's surface.
  class Surface;

  Surface* GetSurface(gfx::AcceleratedWidget widget) const;

  // Validates data sent from GPU. If invalid, returns false and sets an error
  // message to |error_message_|.
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           const base::File& file,
                           const gfx::Size& size,
                           const std::vector<uint32_t>& strides,
                           const std::vector<uint32_t>& offsets,
                           const std::vector<uint64_t>& modifiers,
                           uint32_t format,
                           uint32_t planes_count,
                           uint32_t buffer_id);
  bool ValidateDataFromGpu(const gfx::AcceleratedWidget& widget,
                           uint32_t buffer_id);

  // Callback method. Receives a result for the request to create a wl_buffer
  // backend by dmabuf file descriptor from ::CreateBuffer call.
  void OnCreateBufferComplete(gfx::AcceleratedWidget widget,
                              uint32_t buffer_id,
                              wl::Object<struct wl_buffer> new_buffer);

  base::flat_map<gfx::AcceleratedWidget, std::unique_ptr<Surface>> surfaces_;

  // Set when invalid data is received from the GPU process.
  std::string error_message_;

  // Non-owned pointer to the main connection.
  WaylandConnection* const connection_;

  base::WeakPtrFactory<WaylandBufferManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(WaylandBufferManager);
};

}  // namespace ui

#endif  // UI_OZONE_PLATFORM_WAYLAND_HOST_WAYLAND_BUFFER_MANAGER_H_
