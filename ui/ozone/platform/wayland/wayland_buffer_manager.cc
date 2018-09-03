// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/wayland_buffer_manager.h"

#include <drm_fourcc.h>

#include <linux-dmabuf-unstable-v1-client-protocol.h>

#include "base/trace_event/trace_event.h"
#include "ui/ozone/common/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/wayland_connection.h"
#include "ui/ozone/platform/wayland/wayland_window.h"

namespace ui {

WaylandBufferManager::Buffer::Buffer() = default;
WaylandBufferManager::Buffer::Buffer(uint32_t id,
                                     zwp_linux_buffer_params_v1* zwp_params)
    : buffer_id(id), params(zwp_params) {}
WaylandBufferManager::Buffer::~Buffer() = default;

WaylandBufferManager::WaylandBufferManager(
    zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    WaylandConnection* connection)
    : zwp_linux_dmabuf_(zwp_linux_dmabuf), connection_(connection) {
  static const zwp_linux_dmabuf_v1_listener dmabuf_listener = {
      &WaylandBufferManager::Format, &WaylandBufferManager::Modifiers,
  };
  zwp_linux_dmabuf_v1_add_listener(zwp_linux_dmabuf_.get(), &dmabuf_listener,
                                   this);

  // A roundtrip after binding guarantees that the client has received all
  // supported formats.
  wl_display_roundtrip(connection->display());
}

WaylandBufferManager::~WaylandBufferManager() {
  DCHECK(buffers_.empty());
}

bool WaylandBufferManager::CreateBuffer(base::File file,
                                        uint32_t width,
                                        uint32_t height,
                                        const std::vector<uint32_t>& strides,
                                        const std::vector<uint32_t>& offsets,
                                        uint32_t format,
                                        const std::vector<uint64_t>& modifiers,
                                        uint32_t planes_count,
                                        uint32_t buffer_id) {
  TRACE_EVENT2("Wayland", "WaylandBufferManager::CreateZwpLinuxDmabuf",
               "Format", format, "Buffer id", buffer_id);

  static const struct zwp_linux_buffer_params_v1_listener params_listener = {
      WaylandBufferManager::CreateSucceeded,
      WaylandBufferManager::CreateFailed};
  if (!ValidateDataFromGpu(file, width, height, strides, offsets, format,
                           modifiers, planes_count, buffer_id)) {
    // base::File::Close() has an assertion that checks if blocking operations
    // are allowed. Thus, manually close the fd here.
    base::ScopedFD fd(file.TakePlatformFile());
    fd.reset();
    return false;
  }

  // Store |params| connected to |buffer_id| to track buffer creation and
  // identify, which buffer a client wants to use.
  DCHECK(zwp_linux_dmabuf_);
  struct zwp_linux_buffer_params_v1* params =
      zwp_linux_dmabuf_v1_create_params(zwp_linux_dmabuf_.get());

  std::unique_ptr<Buffer> buffer = std::make_unique<Buffer>(buffer_id, params);
  buffers_.insert(std::pair<uint32_t, std::unique_ptr<Buffer>>(
      buffer_id, std::move(buffer)));

  uint32_t fd = file.TakePlatformFile();
  for (size_t i = 0; i < planes_count; i++) {
    zwp_linux_buffer_params_v1_add(params, fd, i /* plane id */, offsets[i],
                                   strides[i], 0 /* modifier hi */,
                                   0 /* modifier lo */);
  }
  zwp_linux_buffer_params_v1_add_listener(params, &params_listener, this);
  zwp_linux_buffer_params_v1_create(params, width, height, format, 0);

  connection_->ScheduleFlush();
  return true;
}

// TODO(msisov): handle buffer swap failure or success.
bool WaylandBufferManager::ScheduleBufferSwap(gfx::AcceleratedWidget widget,
                                              uint32_t buffer_id) {
  TRACE_EVENT1("Wayland", "WaylandBufferManager::SwapBuffer", "Buffer id",
               buffer_id);

  if (!ValidateDataFromGpu(widget, buffer_id))
    return false;

  auto it = buffers_.find(buffer_id);
  if (it == buffers_.end()) {
    error_message_ =
        "Buffer with " + std::to_string(buffer_id) + " id does not exist";
    return false;
  }

  Buffer* buffer = it->second.get();
  DCHECK(buffer);

  // Assign a widget to this buffer, which is used to find a corresponding
  // WaylandWindow.
  buffer->widget = widget;

  if (buffer->wl_buffer) {
    // A wl_buffer might not exist by this time. Silently return.
    // TODO: check this.
    return SwapBuffer(buffer);
  }
  return true;
}

bool WaylandBufferManager::DestroyBuffer(uint32_t buffer_id) {
  TRACE_EVENT1("Wayland", "WaylandBufferManager::DestroyZwpLinuxDmabuf",
               "Buffer id", buffer_id);

  auto it = buffers_.find(buffer_id);
  if (it == buffers_.end()) {
    error_message_ = "Trying to destroy non-existing buffer";
    return false;
  }
  buffers_.erase(it);

  connection_->ScheduleFlush();
  return true;
}

void WaylandBufferManager::ClearState() {
  buffers_.clear();
}

// TODO(msisov): handle buffer swap failure or success.
bool WaylandBufferManager::SwapBuffer(Buffer* buffer) {
  WaylandWindow* window = connection_->GetWindow(buffer->widget);
  if (!window) {
    error_message_ = "A WaylandWindow with current widget does not exist";
    return false;
  }

  // TODO(msisov): it would be beneficial to use real damage regions to improve
  // performance.
  //
  // TODO(msisov): also start using wl_surface_frame callbacks for better
  // performance.
  wl_surface_damage(window->surface(), 0, 0, window->GetBounds().width(),
                    window->GetBounds().height());
  wl_surface_attach(window->surface(), buffer->wl_buffer.get(), 0, 0);
  wl_surface_commit(window->surface());

  connection_->ScheduleFlush();
  return true;
}

bool WaylandBufferManager::ValidateDataFromGpu(
    const base::File& file,
    uint32_t width,
    uint32_t height,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    uint32_t format,
    const std::vector<uint64_t>& modifiers,
    uint32_t planes_count,
    uint32_t buffer_id) {
  std::string reason;
  if (!file.IsValid())
    reason = "Buffer fd is invalid";

  if (width == 0 || height == 0)
    reason = "Buffer size is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = "Number of strides(" + std::to_string(strides.size()) +
             ")/offsets(" + std::to_string(offsets.size()) + ")/modifiers(" +
             std::to_string(modifiers.size()) +
             ") does not correspond to the number of planes(" +
             std::to_string(planes_count) + ")";
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  auto it = buffers_.find(buffer_id);
  if (it != buffers_.end()) {
    reason = "A buffer with " + std::to_string(buffer_id) +
             " id has already existed";
  }

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }
  return true;
}

bool WaylandBufferManager::ValidateDataFromGpu(
    const gfx::AcceleratedWidget& widget,
    uint32_t buffer_id) {
  std::string reason;

  if (widget == gfx::kNullAcceleratedWidget)
    reason = "Invalid accelerated widget";

  if (buffer_id < 1)
    reason = "Invalid buffer id: " + std::to_string(buffer_id);

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

void WaylandBufferManager::CreateSucceededInternal(
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  // Find which buffer id |params| belong to and store wl_buffer
  // with that id.
  Buffer* buffer = nullptr;
  for (auto& item : buffers_) {
    if (item.second.get()->params == params) {
      buffer = item.second.get();
      break;
    }
  }

  DCHECK(buffer);
  buffer->wl_buffer.reset(new_buffer);
  buffer->params = nullptr;
  zwp_linux_buffer_params_v1_destroy(params);

  if (buffer->widget != gfx::kNullAcceleratedWidget)
    ScheduleBufferSwap(buffer->widget, buffer->buffer_id);
}

// static
void WaylandBufferManager::Modifiers(
    void* data,
    struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
    uint32_t format,
    uint32_t modifier_hi,
    uint32_t modifier_lo) {
  NOTIMPLEMENTED();
}

// static
void WaylandBufferManager::Format(void* data,
                                  struct zwp_linux_dmabuf_v1* zwp_linux_dmabuf,
                                  uint32_t format) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);
  // Return on not the supported ARGB format.
  if (format == DRM_FORMAT_ARGB2101010)
    return;
  self->supported_buffer_formats_.push_back(
      GetBufferFormatFromFourCCFormat(format));
}

// static
void WaylandBufferManager::CreateSucceeded(
    void* data,
    struct zwp_linux_buffer_params_v1* params,
    struct wl_buffer* new_buffer) {
  WaylandBufferManager* self = static_cast<WaylandBufferManager*>(data);

  DCHECK(self);
  self->CreateSucceededInternal(params, new_buffer);
}

// static
void WaylandBufferManager::CreateFailed(
    void* data,
    struct zwp_linux_buffer_params_v1* params) {
  zwp_linux_buffer_params_v1_destroy(params);
  LOG(FATAL) << "zwp_linux_buffer_params.create failed";
}

}  // namespace ui
