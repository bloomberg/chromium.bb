// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"

#include <utility>

#include "base/bind.h"
#include "base/process/process.h"
#include "base/task/current_thread.h"
#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/geometry/rrect_f.h"
#include "ui/gfx/linux/drm_util_linux.h"
#include "ui/gfx/overlay_priority_hint.h"
#include "ui/ozone/platform/wayland/gpu/wayland_surface_gpu.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"
#include "ui/ozone/public/overlay_plane.h"

#if defined(WAYLAND_GBM)
#include "ui/gfx/linux/gbm_wrapper.h"  // nogncheck
#include "ui/ozone/platform/wayland/gpu/drm_render_node_handle.h"
#include "ui/ozone/platform/wayland/gpu/drm_render_node_path_finder.h"
#endif

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

  wayland_overlay_config->rounded_clip_bounds =
      input.overlay_plane_data.rounded_corners;
  return wayland_overlay_config;
}
}  // namespace mojo

namespace ui {

WaylandBufferManagerGpu::WaylandBufferManagerGpu() {
#if defined(WAYLAND_GBM)
  // The path_finder and the handle do syscalls, which are permitted before
  // the sandbox entry. After the gpu enters the sandbox, they fail. Thus,
  // open the handle early and store it.
  OpenAndStoreDrmRenderNodeFd();
#endif

  // The WaylandBufferManagerGpu takes the task runner where it was created.
  // However, it might be null in tests and be available later after
  // initialization is done. Though, when this code runs outside tests, a race
  // between setting a task runner via ::Initialize and ::RegisterSurface may
  // happen, and a surface will never be registered. Thus, the following two
  // cases are possible:
  // 1) The WaylandBufferManagerGpu runs normally outside tests.
  // ThreadTaskRunnerHandle is set and it is passed during construction and
  // never changes.
  // 2) The WaylandBufferManagerGpu runs in unit tests and when it's created,
  // the task runner is not available and must be set later when ::Initialize is
  // called. In this case, there is no race between ::Initialize and
  // ::RegisterSurface and it's fine to defer setting the task runner.
  //
  // TODO(msisov): think about making unit tests initialize Ozone after task
  // runner is set that would allow to always set the task runner.
  if (base::ThreadTaskRunnerHandle::IsSet()) {
    gpu_thread_runner_ = base::ThreadTaskRunnerHandle::Get();
  } else {
    // In tests, the further calls might happen on a different sequence.
    // Otherwise, ThreadTaskRunnerHandle should have already been set.
    DETACH_FROM_SEQUENCE(gpu_sequence_checker_);
  }
}

WaylandBufferManagerGpu::~WaylandBufferManagerGpu() = default;

void WaylandBufferManagerGpu::Initialize(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host,
    const base::flat_map<::gfx::BufferFormat, std::vector<uint64_t>>&
        buffer_formats_with_modifiers,
    bool supports_dma_buf,
    bool supports_viewporter,
    bool supports_acquire_fence,
    bool supports_non_backed_solid_color_buffers) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // See the comment in the constructor.
  if (!gpu_thread_runner_)
    gpu_thread_runner_ = base::ThreadTaskRunnerHandle::Get();

  supported_buffer_formats_with_modifiers_ = buffer_formats_with_modifiers;
  supports_viewporter_ = supports_viewporter;
  supports_acquire_fence_ = supports_acquire_fence;
  supports_non_backed_solid_color_buffers_ =
      supports_non_backed_solid_color_buffers;
  supports_dmabuf_ = supports_dma_buf;

  BindHostInterface(std::move(remote_host));
}

void WaylandBufferManagerGpu::OnSubmission(gfx::AcceleratedWidget widget,
                                           uint32_t buffer_id,
                                           gfx::SwapResult swap_result,
                                           gfx::GpuFenceHandle release_fence) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  base::AutoLock scoped_lock(lock_);
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
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  base::AutoLock scoped_lock(lock_);
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
  DCHECK(gpu_thread_runner_);
  gpu_thread_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(
          &WaylandBufferManagerGpu::SaveTaskRunnerForWidgetOnIOThread,
          base::Unretained(this), widget, base::ThreadTaskRunnerHandle::Get()));

  base::AutoLock scoped_lock(lock_);
  widget_to_surface_map_.emplace(widget, surface);
}

void WaylandBufferManagerGpu::UnregisterSurface(gfx::AcceleratedWidget widget) {
  DCHECK(gpu_thread_runner_);
  gpu_thread_runner_->PostTask(
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
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CreateDmabufBasedBuffer,
                       base::Unretained(this), std::move(dmabuf_fd),
                       std::move(size), std::move(strides), std::move(offsets),
                       std::move(modifiers), current_format, planes_count,
                       buffer_id));
    return;
  }

  DCHECK(remote_host_);
  remote_host_->CreateDmabufBasedBuffer(
      mojo::PlatformHandle(std::move(dmabuf_fd)), size, strides, offsets,
      modifiers, current_format, planes_count, buffer_id);
}

void WaylandBufferManagerGpu::CreateShmBasedBuffer(base::ScopedFD shm_fd,
                                                   size_t length,
                                                   gfx::Size size,
                                                   uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_);
  // Do the mojo call on the GpuMainThread.
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CreateShmBasedBuffer,
                       base::Unretained(this), std::move(shm_fd), length,
                       std::move(size), buffer_id));
    return;
  }

  DCHECK(remote_host_);
  remote_host_->CreateShmBasedBuffer(mojo::PlatformHandle(std::move(shm_fd)),
                                     length, size, buffer_id);
}

void WaylandBufferManagerGpu::CreateSolidColorBuffer(SkColor color,
                                                     const gfx::Size& size,
                                                     uint32_t buf_id) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CreateSolidColorBuffer,
                       base::Unretained(this), color, size, buf_id));
    return;
  }

  DCHECK(remote_host_);
  remote_host_->CreateSolidColorBuffer(size, color, buf_id);
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
      gfx::OverlayPriorityHint::kNone, gfx::RRectF()));

  CommitOverlays(widget, std::move(overlay_configs));
}

void WaylandBufferManagerGpu::CommitOverlays(
    gfx::AcceleratedWidget widget,
    std::vector<ozone::mojom::WaylandOverlayConfigPtr> overlays) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&WaylandBufferManagerGpu::CommitOverlays,
                       base::Unretained(this), widget, std::move(overlays)));
    return;
  }

  DCHECK(remote_host_);
  remote_host_->CommitOverlays(widget, std::move(overlays));
}

void WaylandBufferManagerGpu::DestroyBuffer(gfx::AcceleratedWidget widget,
                                            uint32_t buffer_id) {
  DCHECK(gpu_thread_runner_);
  if (!gpu_thread_runner_->BelongsToCurrentThread()) {
    // Do the mojo call on the GpuMainThread.
    gpu_thread_runner_->PostTask(
        FROM_HERE, base::BindOnce(&WaylandBufferManagerGpu::DestroyBuffer,
                                  base::Unretained(this), widget, buffer_id));
    return;
  }

  DCHECK(remote_host_);
  remote_host_->DestroyBuffer(widget, buffer_id);
}

#if defined(WAYLAND_GBM)
GbmDevice* WaylandBufferManagerGpu::GetGbmDevice() {
  if (!supports_dmabuf_)
    return nullptr;

  if (gbm_device_ || use_fake_gbm_device_for_test_)
    return gbm_device_.get();

  if (!drm_render_node_fd_.is_valid()) {
    supports_dmabuf_ = false;
    return nullptr;
  }

  gbm_device_ = CreateGbmDevice(drm_render_node_fd_.release());
  if (!gbm_device_) {
    supports_dmabuf_ = false;
    LOG(WARNING) << "Failed to initialize gbm device.";
    return nullptr;
  }
  return gbm_device_.get();
}
#endif  // defined(WAYLAND_GBM)

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

void WaylandBufferManagerGpu::BindHostInterface(
    mojo::PendingRemote<ozone::mojom::WaylandBufferManagerHost> remote_host) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK(!remote_host_.is_bound() && !associated_receiver_.is_bound());
  remote_host_.Bind(std::move(remote_host));
  // WaylandBufferManagerHost may bind host again after an error. See
  // WaylandBufferManagerHost::BindInterface for more details.
  remote_host_.set_disconnect_handler(base::BindOnce(
      &WaylandBufferManagerGpu::OnHostDisconnected, base::Unretained(this)));

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
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
  commit_thread_runners_.emplace(widget, origin_runner);
}

void WaylandBufferManagerGpu::ForgetTaskRunnerForWidgetOnIOThread(
    gfx::AcceleratedWidget widget) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  DCHECK_NE(widget, gfx::kNullAcceleratedWidget);
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

#if defined(WAYLAND_GBM)
void WaylandBufferManagerGpu::OpenAndStoreDrmRenderNodeFd() {
  DrmRenderNodePathFinder path_finder;
  const base::FilePath drm_node_path = path_finder.GetDrmRenderNodePath();
  if (drm_node_path.empty()) {
    LOG(WARNING) << "Failed to find drm render node path.";
    return;
  }

  DrmRenderNodeHandle handle;
  if (!handle.Initialize(drm_node_path)) {
    LOG(WARNING) << "Failed to initialize drm render node handle.";
    return;
  }

  drm_render_node_fd_ = handle.PassFD();
}
#endif

void WaylandBufferManagerGpu::OnHostDisconnected() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(gpu_sequence_checker_);

  // WaylandBufferManagerHost may bind host again after an error. See
  // WaylandBufferManagerHost::BindInterface for more details.
  remote_host_.reset();
  // When the remote host is disconnected, it also disconnects the associated
  // receiver. Thus, reset that as well.
  associated_receiver_.reset();
}

}  // namespace ui
