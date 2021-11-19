// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

#include <utility>

#include "base/bind.h"
#include "base/process/process.h"
#include "base/task/current_thread.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/public/mojom/wayland/wayland_overlay_config.mojom.h"
#include "ui/ozone/public/overlay_plane.h"

namespace mojo {
// static
ui::ozone::mojom::WaylandOverlayConfigPtr
TypeConverter<ui::ozone::mojom::WaylandOverlayConfigPtr,
              ui::OverlayPlane>::Convert(const ui::OverlayPlane& input) {
  ui::ozone::mojom::WaylandOverlayConfigPtr wayland_overlay_config{
      ui::ozone::mojom::WaylandOverlayConfig::New()};
  wayland_overlay_config->z_order = input.overlay_plane_data.z_order;
  wayland_overlay_config->transform = input.overlay_plane_data.plane_transform;
  wayland_overlay_config->bounds_rect = input.overlay_plane_data.display_bounds;
  wayland_overlay_config->crop_rect = input.overlay_plane_data.crop_rect;
  wayland_overlay_config->enable_blend = input.overlay_plane_data.enable_blend;
  wayland_overlay_config->opacity = input.overlay_plane_data.opacity;
  wayland_overlay_config->damage_region = input.overlay_plane_data.damage_rect;
  wayland_overlay_config->access_fence_handle =
      !input.gpu_fence || input.gpu_fence->GetGpuFenceHandle().is_null()
          ? gfx::GpuFenceHandle()
          : input.gpu_fence->GetGpuFenceHandle().Clone();
  wayland_overlay_config->priority_hint =
      input.overlay_plane_data.priority_hint;

  const auto& rounded_corners = input.overlay_plane_data.rounded_corners;
  wayland_overlay_config->rounded_corners.clear();
  // Push the corners in the following order - top left, top right, bottom
  // right, and bottom left.
  wayland_overlay_config->rounded_corners.push_back(
      rounded_corners.GetCornerRadii(gfx::RRectF::Corner::kUpperLeft).x());
  wayland_overlay_config->rounded_corners.push_back(
      rounded_corners.GetCornerRadii(gfx::RRectF::Corner::kUpperRight).x());
  wayland_overlay_config->rounded_corners.push_back(
      rounded_corners.GetCornerRadii(gfx::RRectF::Corner::kLowerRight).x());
  wayland_overlay_config->rounded_corners.push_back(
      rounded_corners.GetCornerRadii(gfx::RRectF::Corner::kLowerLeft).x());

  return wayland_overlay_config;
}
}  // namespace mojo

namespace ui {

WaylandBufferManagerGpu::WaylandBufferManagerGpu() = default;
WaylandBufferManagerGpu::~WaylandBufferManagerGpu() = default;

void WaylandBufferManagerGpu::Initialize(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
    const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
        buffer_formats_with_modifiers,
    bool supports_dma_buf,
    bool supports_viewporter,
    bool supports_acquire_fence,
    bool supports_non_backed_solid_color_buffers) {
  supported_buffer_formats_with_modifiers_ = buffer_formats_with_modifiers;

#if defined(WAYLAND_GBM)
  if (!supports_dma_buf)
    set_gbm_device(nullptr);
#endif
  supports_viewporter_ = supports_viewporter;
  supports_acquire_fence_ = supports_acquire_fence;
  supports_non_backed_solid_color_buffers_ =
      supports_non_backed_solid_color_buffers;

  BindHostInterface(std::move(remote_host));

  io_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
}

void WaylandBufferManagerGpu::OnSubmission(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id,
                                           gfx::SwapResult swap_result,
                                           gfx::GpuFenceHandle release_fence) {
  base::AutoLock scoped_lock(lock_);
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK_LE(commit_thread_runners_.count(widget), 1u);
  // Return back to the same thread where the commit request came from.
  auto it = commit_thread_runners_.find(widget);
  if (it == commit_thread_runners_.end())
    return;
  it->second->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::SubmitSwapResultOnOriginThread,
                     base::Unretained(this), widget, buffer_id, swap_result,
                     std::move(release_fence)));
}

void WaylandBufferManagerGpu::OnPresentation(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  base::AutoLock scoped_lock(lock_);
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  DCHECK_LE(commit_thread_runners_.count(widget), 1u);
  // Return back to the same thread where the commit request came from.
  auto it = commit_thread_runners_.find(widget);
  if (it == commit_thread_runners_.end())
    return;
  it->second->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::SubmitPresentationOnOriginThread,
                     base::Unretained(this), widget, buffer_id, feedback));
}

void WaylandBufferManagerGpu::RegisterSurface(gfx::AcceleratedWidget widget,
                                              WaylandSurfaceGpu* surface) {
  if (!io_thread_runner_) {
    LOG(ERROR) << "WaylandBufferManagerGpu is not initialized. Can't register "
                  "a surface.";
    return;
  }

  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WaylandBufferManagerGpu::SaveTaskRunnerForWidgetOnIOThread,
          base::Unretained(this), widget, base::ThreadTaskRunnerHandle::Get()));

  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.emplace(widget, surface);
}

void WaylandBufferManagerGpu::UnregisterSurface(gfx::AcceleratedWidget widget) {
  if (!io_thread_runner_) {
    LOG(ERROR) << "WaylandBufferManagerGpu is not initialized. Can't register "
                  "a surface.";
    return;
  }

  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WaylandBufferManagerGpu::ForgetTaskRunnerForWidgetOnIOThread,
          base::Unretained(this), widget));

  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.erase(widget);
}

WaylandSurfaceGpu* WaylandBufferManagerGpu::GetSurface(
    gfx::AcceleratedWidget widget) {
  base::AutoLock scoped_lock(lock_);

  auto it = widget_to_surface_map_.find(widget);
  if (it != widget_to_surface_map_.end())
    return it->second;
  return nullptr;
}

void WaylandBufferManagerGpu::CreateDmabufBasedBuffer(
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  if (!remote_host_) {
    LOG(ERROR) << "Interface is not bound. Can't request "
                  "WaylandBufferManagerHost to create/commit/destroy buffers.";
    return;
  }

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CreateDmabufBasedBufferInternal,
                     base::Unretained(this), std::move(dmabuf_fd),
                     std::move(size), std::move(strides), std::move(offsets),
                     std::move(modifiers), current_format, planes_count,
                     buffer_id));
}

void WaylandBufferManagerGpu::CreateShmBasedBuffer(base::ScopedFD shm_fd,
                                                   size_t length,
                                                   gfx::Size size,
                                                   uint32_t buffer_id) {
  if (!remote_host_) {
    LOG(ERROR) << "Interface is not bound. Can't request "
                  "WaylandBufferManagerHost to create/commit/destroy buffers.";
    return;
  }

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CreateShmBasedBufferInternal,
                     base::Unretained(this), std::move(shm_fd), length,
                     std::move(size), buffer_id));
}

void WaylandBufferManagerGpu::CreateSolidColorBuffer(SkColor color,
                                                     const gfx::Size& size,
                                                     uint32_t buf_id) {
  if (!remote_host_) {
    LOG(ERROR) << "Interface is not bound. Can't request "
                  "WaylandBufferManagerHost to create/commit/destroy buffers.";
    return;
  }

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CreateSolidColorBufferInternal,
                     base::Unretained(this), color, size, buf_id));
}

void WaylandBufferManagerGpu::CommitBuffer(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id,
                                           const gfx::Rect& bounds_rect,
                                           float surface_scale_factor,
                                           const gfx::Rect& damage_region) {
  std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlay_configs;
  // This surface only commits one buffer per frame, use INT32_MIN to attach
  // the buffer to root_surface of wayland window.
  overlay_configs.push_back(ui::ozone::mojom::WaylandOverlayConfig::New(
      INT32_MIN, gfx::OverlayTransform::OVERLAY_TRANSFORM_NONE, buffer_id,
      surface_scale_factor, bounds_rect, gfx::RectF(1.f, 1.f) /* no crop */,
      damage_region, false, 1.0f /*opacity*/, gfx::GpuFenceHandle(),
      gfx::OverlayPriorityHint::kNone, std::vector<float>()));

  CommitOverlays(widget, std::move(overlay_configs));
}

void WaylandBufferManagerGpu::CommitOverlays(
    gfx::AcceleratedWidget widget,
    std::vector<ozone::mojom::WaylandOverlayConfigPtr> overlays) {
  if (!remote_host_) {
    LOG(ERROR) << "Interface is not bound. Can't request "
                  "WaylandBufferManagerHost to create/commit/destroy buffers.";
    return;
  }

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&WaylandBufferManagerGpu::CommitOverlaysInternal,
                     base::Unretained(this), widget, std::move(overlays)));
}

void WaylandBufferManagerGpu::DestroyBuffer(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id) {
  if (!remote_host_) {
    LOG(ERROR) << "Interface is not bound. Can't request "
                  "WaylandBufferManagerHost to create/commit/destroy buffers.";
    return;
  }

  // Do the mojo call on the IO child thread.
  io_thread_runner_->PostTask(
      FROM_HERE, base::BindOnce(&WaylandBufferManagerGpu::DestroyBufferInternal,
                                base::Unretained(this), widget, buffer_id));
}

void WaylandBufferManagerGpu::AddBindingWaylandBufferManagerGpu(
    mojo::PendingReceiver<ozone::mojom::WaylandBufferManagerGpu> receiver) {
  receiver_set_.Add(this, std::move(receiver));
}

const std::vector<uint64_t>&
WaylandBufferManagerGpu::GetModifiersForBufferFormat(
    gfx::BufferFormat buffer_format) const {
  auto it = supported_buffer_formats_with_modifiers_.find(buffer_format);
  if (it != supported_buffer_formats_with_modifiers_.end()) {
    return it->second;
  }
  static std::vector<uint64_t> dummy;
  return dummy;
}

uint32_t WaylandBufferManagerGpu::AllocateBufferID() {
  return ++next_buffer_id_;
}

void WaylandBufferManagerGpu::CreateDmabufBasedBufferInternal(
    base::ScopedFD dmabuf_fd,
    gfx::Size size,
    const std::vector<uint32_t>& strides,
    const std::vector<uint32_t>& offsets,
    const std::vector<uint64_t>& modifiers,
    uint32_t current_format,
    uint32_t planes_count,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  remote_host_->CreateDmabufBasedBuffer(
      mojo::PlatformHandle(std::move(dmabuf_fd)), size, strides, offsets,
      modifiers, current_format, planes_count, buffer_id);
}

void WaylandBufferManagerGpu::CreateShmBasedBufferInternal(
    base::ScopedFD shm_fd,
    size_t length,
    gfx::Size size,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  remote_host_->CreateShmBasedBuffer(mojo::PlatformHandle(std::move(shm_fd)),
                                     length, size, buffer_id);
}

void WaylandBufferManagerGpu::CreateSolidColorBufferInternal(
    SkColor color,
    const gfx::Size& size,
    uint32_t buf_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  remote_host_->CreateSolidColorBuffer(size, color, buf_id);
}

void WaylandBufferManagerGpu::CommitOverlaysInternal(
    gfx::AcceleratedWidget widget,
    std::vector<ozone::mojom::WaylandOverlayConfigPtr> overlays) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  remote_host_->CommitOverlays(widget, std::move(overlays));
}

void WaylandBufferManagerGpu::DestroyBufferInternal(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id) {
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  remote_host_->DestroyBuffer(widget, buffer_id);
}

void WaylandBufferManagerGpu::BindHostInterface(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host) {
  // WaylandBufferManagerHost may bind host again after an error. See
  // WaylandBufferManagerHost::BindInterface for more details.
  if (remote_host_.is_bound()) {
    remote_host_.reset();
    associated_receiver_.reset();
  }
  remote_host_.Bind(std::move(remote_host));

  // Setup associated interface.
  mojo::PendingAssociatedRemote<ozone::mojom::WaylandBufferManagerGpu>
      client_remote;
  associated_receiver_.Bind(client_remote.InitWithNewEndpointAndPassReceiver());
  DCHECK(remote_host_);
  remote_host_->SetWaylandBufferManagerGpu(std::move(client_remote));
}

void WaylandBufferManagerGpu::SaveTaskRunnerForWidgetOnIOThread(
    gfx::AcceleratedWidget widget,
    scoped_refptr<base::SingleThreadTaskRunner> origin_runner) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  commit_thread_runners_.emplace(widget, origin_runner);
}

void WaylandBufferManagerGpu::ForgetTaskRunnerForWidgetOnIOThread(
    gfx::AcceleratedWidget widget) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  DCHECK(io_thread_runner_->BelongsToCurrentThread());
  commit_thread_runners_.erase(widget);
}

void WaylandBufferManagerGpu::SubmitSwapResultOnOriginThread(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    gfx::SwapResult swap_result,
    gfx::GpuFenceHandle release_fence) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the swap result is provided.
  if (surface)
    surface->OnSubmission(buffer_id, swap_result, std::move(release_fence));
}

void WaylandBufferManagerGpu::SubmitPresentationOnOriginThread(
    gfx::AcceleratedWidget widget,
    uint32_t buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  auto* surface = GetSurface(widget);
  // The surface might be destroyed by the time the presentation feedback is
  // provided.
  if (surface)
    surface->OnPresentation(buffer_id, feedback);
}

}  // namespace ui
