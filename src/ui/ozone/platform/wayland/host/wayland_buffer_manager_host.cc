// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/host/wayland_buffer_manager_host.h"

#include <presentation-time-client-protocol.h>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/i18n/number_formatting.h"
#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/current_thread.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/ozone/platform/wayland/host/surface_augmenter.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_dmabuf.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_backing_solid_color.h"
#include "ui/ozone/platform/wayland/host/wayland_buffer_handle.h"
#include "ui/ozone/platform/wayland/host/wayland_connection.h"
#include "ui/ozone/platform/wayland/host/wayland_drm.h"
#include "ui/ozone/platform/wayland/host/wayland_shm.h"
#include "ui/ozone/platform/wayland/host/wayland_window.h"
#include "ui/ozone/platform/wayland/host/wayland_zwp_linux_dmabuf.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"

namespace ui {

namespace {

std::string NumberToString(uint32_t number) {
  return base::UTF16ToUTF8(base::FormatNumber(number));
}

}  // namespace

WaylandBufferManagerHost::WaylandBufferManagerHost(
    WaylandConnection* connection)
    : connection_(connection), receiver_(this) {}

WaylandBufferManagerHost::~WaylandBufferManagerHost() = default;

void WaylandBufferManagerHost::SetTerminateGpuCallback(
    base::OnceCallback<void(std::string)> terminate_callback) {
  terminate_gpu_cb_ = std::move(terminate_callback);
}

mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
WaylandBufferManagerHost::BindInterface() {
  // Allow to rebind the interface if it hasn't been destroyed yet.
  if (receiver_.is_bound())
    OnChannelDestroyed();

  mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost>
      buffer_manager_host;
  receiver_.Bind(buffer_manager_host.InitWithNewPipeAndPassReceiver());
  return buffer_manager_host;
}

void WaylandBufferManagerHost::OnChannelDestroyed() {
  DCHECK(base::CurrentUIThread::IsSet());

  buffer_backings_.clear();
  for (auto* window : connection_->wayland_window_manager()->GetAllWindows())
    window->OnChannelDestroyed();

  buffer_manager_gpu_associated_.reset();
  receiver_.reset();
}

wl::BufferFormatsWithModifiersMap
WaylandBufferManagerHost::GetSupportedBufferFormats() const {
#if defined(WAYLAND_GBM)
  if (connection_->zwp_dmabuf())
    return connection_->zwp_dmabuf()->supported_buffer_formats();
  else if (connection_->drm())
    return connection_->drm()->supported_buffer_formats();
#endif
  return {};
}

bool WaylandBufferManagerHost::SupportsDmabuf() const {
  return !!connection_->zwp_dmabuf() ||
         (connection_->drm() && connection_->drm()->SupportsDrmPrime());
}

bool WaylandBufferManagerHost::SupportsAcquireFence() const {
  return !!connection_->linux_explicit_synchronization_v1();
}

bool WaylandBufferManagerHost::SupportsViewporter() const {
  return !!connection_->viewporter();
}

bool WaylandBufferManagerHost::SupportsNonBackedSolidColorBuffers() const {
  return !!connection_->surface_augmenter();
}

bool WaylandBufferManagerHost::SupportsSubpixelAccuratePosition() const {
  return connection_->surface_augmenter() &&
         connection_->surface_augmenter()->SupportsSubpixelAccuratePosition();
}

void WaylandBufferManagerHost::SetWaylandBufferManagerGpu(
    mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
        buffer_manager_gpu_associated) {
  buffer_manager_gpu_associated_.Bind(std::move(buffer_manager_gpu_associated));
}

void WaylandBufferManagerHost::CreateDmabufBasedBuffer(
    mojo::PlatformHandle dmabuf_fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT2("wayland", "WaylandBufferManagerHost::CreateDmabufBasedBuffer",
               "Format", format, "Buffer id", buffer_id);

  base::ScopedFD fd = dmabuf_fd.TakeFD();

  // Validate data and ask surface to create a buffer associated with the
  // |buffer_id|.
  if (!ValidateDataFromGpu(fd, size, strides, offsets, modifiers, format,
                           planes_count, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Check if any of the surfaces has already had a buffer with the same id.
  auto result = buffer_backings_.emplace(
      buffer_id, std::make_unique<WaylandBufferBackingDmabuf>(
                     connection_, std::move(fd), size, std::move(strides),
                     std::move(offsets), std::move(modifiers), format,
                     planes_count, buffer_id));

  if (!result.second) {
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
    TerminateGpuProcess();
    return;
  }

  auto* backing = result.first->second.get();
  backing->EnsureBufferHandle();
}

void WaylandBufferManagerHost::CreateShmBasedBuffer(mojo::PlatformHandle shm_fd,
                                                    uint64_t length,
                                                    const gfx::Size& size,
                                                    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateShmBasedBuffer",
               "Buffer id", buffer_id);

  base::ScopedFD fd = shm_fd.TakeFD();
  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(fd, length, size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // Check if any of the surfaces has already had a buffer with the same id.
  auto result = buffer_backings_.emplace(
      buffer_id, std::make_unique<WaylandBufferBackingShm>(
                     connection_, std::move(fd), length, size, buffer_id));

  if (!result.second) {
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
    TerminateGpuProcess();
    return;
  }

  auto* backing = result.first->second.get();
  backing->EnsureBufferHandle();
}

void WaylandBufferManagerHost::CreateSolidColorBuffer(const gfx::Size& size,
                                                      SkColor color,
                                                      uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());
  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::CreateSolidColorBuffer",
               "Buffer id", buffer_id);

  // Validate data and create a buffer associated with the |buffer_id|.
  if (!ValidateDataFromGpu(size, buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  // OzonePlatform::PlatformInitProperties has a control variable that tells
  // viz to create a backing solid color buffers if the protocol is not
  // available. But in order to avoid a missusage of that variable and this
  // method (malformed requests), explicitly terminate the gpu.
  if (!connection_->surface_augmenter()) {
    error_message_ = "Surface augmenter protocol is not available.";
    TerminateGpuProcess();
    return;
  }

  auto result = buffer_backings_.emplace(
      buffer_id, std::make_unique<WaylandBufferBackingSolidColor>(
                     connection_, color, size, buffer_id));

  if (!result.second) {
    error_message_ = base::StrCat(
        {"A buffer with id= ", NumberToString(buffer_id), " already exists"});
    TerminateGpuProcess();
    return;
  }

  auto* backing = result.first->second.get();
  backing->EnsureBufferHandle();
}

WaylandBufferHandle* WaylandBufferManagerHost::EnsureBufferHandle(
    WaylandSurface* requestor,
    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(error_message_.empty());
  DCHECK(requestor);

  auto it = buffer_backings_.find(buffer_id);
  if (it == buffer_backings_.end())
    return nullptr;

  return it->second->EnsureBufferHandle(requestor);
}

WaylandBufferHandle* WaylandBufferManagerHost::GetBufferHandle(
    WaylandSurface* requestor,
    uint32_t buffer_id) {
  DCHECK(base::CurrentUIThread::IsSet());
  DCHECK(requestor);

  auto it = buffer_backings_.find(buffer_id);
  if (it == buffer_backings_.end())
    return nullptr;

  return it->second->GetBufferHandle(requestor);
}

void WaylandBufferManagerHost::CommitOverlays(
    gfx::AcceleratedWidget widget,
    std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlays) {
  DCHECK(base::CurrentUIThread::IsSet());

  TRACE_EVENT0("wayland", "WaylandBufferManagerHost::CommitOverlays");

  DCHECK(error_message_.empty());

  if (widget == gfx::kNullAcceleratedWidget) {
    error_message_ = "Invalid widget.";
    TerminateGpuProcess();
  }
  WaylandWindow* window =
      connection_->wayland_window_manager()->GetWindow(widget);
  // In tab dragging, window may have been destroyed when buffers reach here. We
  // omit buffer commits and OnSubmission, because the corresponding buffer
  // queue in gpu process should be destroyed soon.
  if (!window)
    return;

  for (auto& overlay : overlays) {
    if (!ValidateOverlayData(*overlay)) {
      TerminateGpuProcess();
      return;
    }
  }

  window->CommitOverlays(overlays);
}

void WaylandBufferManagerHost::DestroyBuffer(
    [[maybe_unused]] gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  // TODO(fangzhoug): Remove |widget| from the argument list of the mojo
  // interface.
  DCHECK(base::CurrentUIThread::IsSet());

  TRACE_EVENT1("wayland", "WaylandBufferManagerHost::DestroyBuffer",
               "Buffer id", buffer_id);

  DCHECK(error_message_.empty());
  if (!ValidateBufferExistence(buffer_id)) {
    TerminateGpuProcess();
    return;
  }

  buffer_backings_.erase(buffer_id);
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(
    const base::ScopedFD& fd,
    const gfx::Size& size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  if (!ValidateDataFromGpu(size, buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (planes_count < 1)
    reason = "Planes count cannot be less than 1";

  if (planes_count != strides.size() || planes_count != offsets.size() ||
      planes_count != modifiers.size()) {
    reason = base::StrCat({"Number of strides(", NumberToString(strides.size()),
                           ")/offsets(", NumberToString(offsets.size()),
                           ")/modifiers(", NumberToString(modifiers.size()),
                           ") does not correspond to the number of planes(",
                           NumberToString(planes_count), ")"});
  }

  for (auto stride : strides) {
    if (stride == 0)
      reason = "Strides are invalid";
  }

  if (!IsValidBufferFormat(format))
    reason = "Buffer format is invalid";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }
  return true;
}

bool WaylandBufferManagerHost::ValidateBufferIdFromGpu(uint32_t buffer_id) {
  std::string reason;
  if (buffer_id < 1)
    reason = base::StrCat({"Invalid buffer id: ", NumberToString(buffer_id)});

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(const base::ScopedFD& fd,
                                                   size_t length,
                                                   const gfx::Size& size,
                                                   uint32_t buffer_id) {
  if (!ValidateDataFromGpu(size, buffer_id))
    return false;

  std::string reason;
  if (!fd.is_valid())
    reason = "Buffer fd is invalid";

  if (length == 0)
    reason = "The shm pool length cannot be less than 1";

  if (!reason.empty()) {
    error_message_ = std::move(reason);
    return false;
  }

  return true;
}

bool WaylandBufferManagerHost::ValidateDataFromGpu(const gfx::Size& size,
                                                   uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  std::string reason;
  if (size.IsEmpty())
    error_message_ = "Buffer size is invalid";

  return error_message_.empty();
}

bool WaylandBufferManagerHost::ValidateBufferExistence(uint32_t buffer_id) {
  if (!ValidateBufferIdFromGpu(buffer_id))
    return false;

  auto it = buffer_backings_.find(buffer_id);
  if (it == buffer_backings_.end()) {
    error_message_ = base::StrCat(
        {"Buffer with ", NumberToString(buffer_id), " id does not exist"});
  }

  return error_message_.empty();
}

bool WaylandBufferManagerHost::ValidateOverlayData(
    const ui::ozone::mojom::WaylandOverlayConfig& overlay_data) {
  if (!ValidateBufferExistence(overlay_data.buffer_id))
    return false;

  std::string reason;
  if (std::isnan(overlay_data.bounds_rect.x()) ||
      std::isnan(overlay_data.bounds_rect.y()) ||
      std::isnan(overlay_data.bounds_rect.width()) ||
      std::isnan(overlay_data.bounds_rect.height()) ||
      std::isinf(overlay_data.bounds_rect.x()) ||
      std::isinf(overlay_data.bounds_rect.y()) ||
      std::isinf(overlay_data.bounds_rect.width()) ||
      std::isinf(overlay_data.bounds_rect.height())) {
    error_message_ = "Overlay bounds_rect is invalid (NaN or infinity).";
    return false;
  }

  return true;
}

void WaylandBufferManagerHost::OnSubmission(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id,
                                            const gfx::SwapResult& swap_result,
                                            gfx::GpuFenceHandle release_fence) {
  DCHECK(base::CurrentUIThread::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnSubmission(widget, buffer_id, swap_result,
                                               std::move(release_fence));
}

void WaylandBufferManagerHost::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(base::CurrentUIThread::IsSet());

  DCHECK(buffer_manager_gpu_associated_);
  buffer_manager_gpu_associated_->OnPresentation(widget, buffer_id, feedback);
}

void WaylandBufferManagerHost::TerminateGpuProcess() {
  DCHECK(!error_message_.empty());
  std::move(terminate_gpu_cb_).Run(std::move(error_message_));
  // The GPU process' failure results in calling ::OnChannelDestroyed.
}

}  // namespace ui
