// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/gpu/gbm_surfaceless_wayland.h"

#include <sync/sync.h>
#include <cmath>
#include <memory>

#include "base/bind.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/gpu_fence.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gl/gl_bindings.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/wayland/gpu/wayland_buffer_manager_gpu.h"
#include "ui/ozone/platform/wayland/mojom/wayland_overlay_config.mojom.h"

namespace ui {

namespace {

// A test run showed only 9 inflight solid color buffers at the same time. Thus,
// allow to store max 12 buffers (including some margin) of solid color buffers
// and remove the rest.
static constexpr size_t kMaxSolidColorBuffers = 12;

static constexpr gfx::Size kSolidColorBufferSize{4, 4};

void WaitForGpuFences(std::vector<std::unique_ptr<gfx::GpuFence>> fences) {
  for (auto& fence : fences)
    fence->Wait();
}

}  // namespace

GbmSurfacelessWayland::SolidColorBufferHolder::SolidColorBufferHolder() =
    default;
GbmSurfacelessWayland::SolidColorBufferHolder::~SolidColorBufferHolder() =
    default;

BufferId
GbmSurfacelessWayland::SolidColorBufferHolder::GetOrCreateSolidColorBuffer(
    SkColor color,
    WaylandBufferManagerGpu* buffer_manager) {
  BufferId next_buffer_id = 0;

  // First try for an existing buffer.
  auto it = std::find_if(available_solid_color_buffers_.begin(),
                         available_solid_color_buffers_.end(),
                         [&color](const SolidColorBuffer& solid_color_buffer) {
                           return solid_color_buffer.color == color;
                         });
  if (it != available_solid_color_buffers_.end()) {
    // This is a prefect color match so use this directly.
    next_buffer_id = it->buffer_id;
    inflight_solid_color_buffers_.emplace_back(std::move(*it));
    available_solid_color_buffers_.erase(it);
  } else {
    // Worst case allocate a new buffer. This definitely will occur on
    // startup.
    next_buffer_id = buffer_manager->AllocateBufferID();
    // Create wl_buffer on the browser side.
    buffer_manager->CreateSolidColorBuffer(color, kSolidColorBufferSize,
                                           next_buffer_id);
    // Allocate a backing structure that will be used to figure out if such
    // buffer has already existed.
    inflight_solid_color_buffers_.emplace_back(
        SolidColorBuffer(color, next_buffer_id));
  }
  DCHECK_GT(next_buffer_id, 0u);
  return next_buffer_id;
}

void GbmSurfacelessWayland::SolidColorBufferHolder::OnSubmission(
    BufferId buffer_id,
    WaylandBufferManagerGpu* buffer_manager,
    gfx::AcceleratedWidget widget) {
  // Solid color buffers do not require on submission as skia doesn't track
  // them. Instead, they are tracked by GbmSurfacelessWayland. In the future,
  // when SharedImageFactory allows to create non-backed shared images, this
  // should be removed from here.
  auto it =
      std::find_if(inflight_solid_color_buffers_.begin(),
                   inflight_solid_color_buffers_.end(),
                   [&buffer_id](const SolidColorBuffer& solid_color_buffer) {
                     return solid_color_buffer.buffer_id == buffer_id;
                   });
  if (it != inflight_solid_color_buffers_.end()) {
    available_solid_color_buffers_.emplace_back(std::move(*it));
    inflight_solid_color_buffers_.erase(it);
    // Keep track of the number of created buffers and erase the least used
    // ones until the maximum number of available solid color buffer.
    while (available_solid_color_buffers_.size() > kMaxSolidColorBuffers) {
      buffer_manager->DestroyBuffer(
          widget, available_solid_color_buffers_.begin()->buffer_id);
      available_solid_color_buffers_.erase(
          available_solid_color_buffers_.begin());
    }
  }
}

void GbmSurfacelessWayland::SolidColorBufferHolder::EraseBuffers(
    WaylandBufferManagerGpu* buffer_manager,
    gfx::AcceleratedWidget widget) {
  for (const auto& buffer : available_solid_color_buffers_)
    buffer_manager->DestroyBuffer(widget, buffer.buffer_id);
  available_solid_color_buffers_.clear();
}

GbmSurfacelessWayland::GbmSurfacelessWayland(
    WaylandBufferManagerGpu* buffer_manager,
    gfx::AcceleratedWidget widget)
    : SurfacelessEGL(gfx::Size()),
      buffer_manager_(buffer_manager),
      widget_(widget),
      has_implicit_external_sync_(
          HasEGLExtension("EGL_ARM_implicit_external_sync")),
      solid_color_buffers_holder_(std::make_unique<SolidColorBufferHolder>()),
      weak_factory_(this) {
  buffer_manager_->RegisterSurface(widget_, this);
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());
}

void GbmSurfacelessWayland::QueueOverlayPlane(OverlayPlane plane,
                                              BufferId buffer_id) {
  auto result =
      unsubmitted_frames_.back()->planes.emplace(buffer_id, std::move(plane));
  DCHECK(result.second);
}

bool GbmSurfacelessWayland::ScheduleOverlayPlane(
    gl::GLImage* image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  if (!image) {
    // Only solid color overlays can be non-backed.
    if (!overlay_plane_data.solid_color.has_value()) {
      LOG(WARNING) << "Only solid color overlay planes are allowed to be "
                      "scheduled without GLImage.";
      return false;
    }
    DCHECK(!gpu_fence);
    unsubmitted_frames_.back()->non_backed_overlays.emplace_back(
        overlay_plane_data);
  } else {
    unsubmitted_frames_.back()->overlays.emplace_back(
        image, std::move(gpu_fence), overlay_plane_data);
  }
  return true;
}

bool GbmSurfacelessWayland::IsOffscreen() {
  return false;
}

bool GbmSurfacelessWayland::SupportsAsyncSwap() {
  return true;
}

bool GbmSurfacelessWayland::SupportsPostSubBuffer() {
  return true;
}

gfx::SwapResult GbmSurfacelessWayland::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  // The actual sub buffer handling is handled at higher layers.
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GbmSurfacelessWayland::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  TRACE_EVENT0("wayland", "GbmSurfacelessWayland::SwapBuffersAsync");
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    // Notify the caller, the buffer is never presented on a screen.
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  // TODO(fangzhoug): remove glFlush since eglImageFlushExternalEXT called on
  // the image should be enough (https://crbug.com/720045).
  if (!no_gl_flush_for_tests_)
    glFlush();
  unsubmitted_frames_.back()->Flush();

  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->completion_callback = std::move(completion_callback);
  frame->presentation_callback = std::move(presentation_callback);
  frame->ScheduleOverlayPlanes(this);

  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());

  // If Wayland server supports linux_explicit_synchronization_protocol, fences
  // should be shipped with buffers. Otherwise, we will wait for fences.
  if (buffer_manager_->supports_acquire_fence() || !use_egl_fence_sync_ ||
      !frame->schedule_planes_succeeded) {
    frame->ready = true;
    MaybeSubmitFrames();
    return;
  }

  base::OnceClosure fence_wait_task;
  std::vector<std::unique_ptr<gfx::GpuFence>> fences;
  for (auto& plane : frame->planes) {
    if (plane.second.gpu_fence)
      fences.push_back(std::move(plane.second.gpu_fence));
  }

  fence_wait_task = base::BindOnce(&WaitForGpuFences, std::move(fences));

  base::OnceClosure fence_retired_callback = base::BindOnce(
      &GbmSurfacelessWayland::FenceRetired, weak_factory_.GetWeakPtr(), frame);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(fence_wait_task), std::move(fence_retired_callback));
}

void GbmSurfacelessWayland::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->damage_region_ = gfx::Rect(x, y, width, height);

  SwapBuffersAsync(std::move(completion_callback),
                   std::move(presentation_callback));
}

EGLConfig GbmSurfacelessWayland::GetConfig() {
  if (!config_) {
    EGLint config_attribs[] = {EGL_BUFFER_SIZE,
                               32,
                               EGL_ALPHA_SIZE,
                               8,
                               EGL_BLUE_SIZE,
                               8,
                               EGL_GREEN_SIZE,
                               8,
                               EGL_RED_SIZE,
                               8,
                               EGL_RENDERABLE_TYPE,
                               EGL_OPENGL_ES2_BIT,
                               EGL_SURFACE_TYPE,
                               EGL_DONT_CARE,
                               EGL_NONE};
    config_ = ChooseEGLConfig(GetDisplay(), config_attribs);
  }
  return config_;
}

void GbmSurfacelessWayland::SetRelyOnImplicitSync() {
  use_egl_fence_sync_ = false;
}

bool GbmSurfacelessWayland::SupportsPlaneGpuFences() const {
  return true;
}

bool GbmSurfacelessWayland::SupportsOverridePlatformSize() const {
  return true;
}

bool GbmSurfacelessWayland::SupportsViewporter() const {
  return buffer_manager_->supports_viewporter();
}

gfx::SurfaceOrigin GbmSurfacelessWayland::GetOrigin() const {
  // GbmSurfacelessWayland's y-axis is flipped compare to GL - (0,0) is at top
  // left corner.
  return gfx::SurfaceOrigin::kTopLeft;
}

bool GbmSurfacelessWayland::Resize(const gfx::Size& size,
                                   float scale_factor,
                                   const gfx::ColorSpace& color_space,
                                   bool has_alpha) {
  surface_scale_factor_ = scale_factor;

  // Remove all the buffers.
  solid_color_buffers_holder_->EraseBuffers(buffer_manager_, widget_);

  return gl::SurfacelessEGL::Resize(size, scale_factor, color_space, has_alpha);
}

GbmSurfacelessWayland::~GbmSurfacelessWayland() {
  buffer_manager_->UnregisterSurface(widget_);
}

GbmSurfacelessWayland::PendingFrame::PendingFrame() = default;

GbmSurfacelessWayland::PendingFrame::~PendingFrame() = default;

void GbmSurfacelessWayland::PendingFrame::ScheduleOverlayPlanes(
    GbmSurfacelessWayland* surfaceless) {
  DCHECK(surfaceless);
  for (auto& overlay : overlays) {
    if (!overlay.ScheduleOverlayPlane(surfaceless->widget_))
      return;
  }

  // Solid color overlays are non-backed. Thus, queue them directly.
  // TODO(msisov): reconsider this once Linux Wayland compositors also support
  // creation of non-backed solid color wl_buffers.
  for (auto& overlay_data : non_backed_overlays) {
    // This mustn't happen, but let's be explicit here and fail scheduling if
    // it is not a solid color overlay.
    if (!overlay_data.solid_color.has_value()) {
      schedule_planes_succeeded = false;
      return;
    }

    BufferId buf_id =
        surfaceless->solid_color_buffers_holder_->GetOrCreateSolidColorBuffer(
            overlay_data.solid_color.value(), surfaceless->buffer_manager_);
    // Invalid buffer id.
    if (buf_id == 0) {
      schedule_planes_succeeded = false;
      return;
    }
    surfaceless->QueueOverlayPlane(OverlayPlane(nullptr, nullptr, overlay_data),
                                   buf_id);
  }

  schedule_planes_succeeded = true;
  return;
}

void GbmSurfacelessWayland::PendingFrame::Flush() {
  for (const auto& overlay : overlays)
    overlay.Flush();
}

void GbmSurfacelessWayland::MaybeSubmitFrames() {
  while (!unsubmitted_frames_.empty() && unsubmitted_frames_.front()->ready) {
    auto submitted_frame = std::move(unsubmitted_frames_.front());
    unsubmitted_frames_.erase(unsubmitted_frames_.begin());

    if (!submitted_frame->schedule_planes_succeeded) {
      last_swap_buffers_result_ = false;

      std::move(submitted_frame->completion_callback)
          .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
      // Notify the caller, the buffer is never presented on a screen.
      std::move(submitted_frame->presentation_callback)
          .Run(gfx::PresentationFeedback::Failure());

      submitted_frame.reset();
      return;
    }

    std::vector<ui::ozone::mojom::WaylandOverlayConfigPtr> overlay_configs;
    for (auto& plane : submitted_frame->planes) {
      overlay_configs.push_back(
          ui::ozone::mojom::WaylandOverlayConfig::From(plane.second));
      overlay_configs.back()->buffer_id = plane.first;
      // The current scale factor of the surface, which is used to determine
      // the size in pixels of resources allocated by the GPU process.
      overlay_configs.back()->surface_scale_factor = surface_scale_factor_;
      // TODO(petermcneeley): For the primary plane, we receive damage via
      // PostSubBufferAsync. Damage sent via overlay information is currently
      // always a full damage. Take the intersection until we send correct
      // damage via overlay information.
      if (plane.second.overlay_plane_data.z_order == 0 &&
          submitted_frame->damage_region_.has_value()) {
        overlay_configs.back()->damage_region.Intersect(
            submitted_frame->damage_region_.value());
      }
#if DCHECK_IS_ON()
      if (plane.second.overlay_plane_data.z_order == INT32_MIN)
        background_buffer_id_ = plane.first;
#endif
      plane.second.gpu_fence.reset();
    }

    buffer_manager_->CommitOverlays(widget_, std::move(overlay_configs));
    submitted_frames_.push_back(std::move(submitted_frame));
  }
}

EGLSyncKHR GbmSurfacelessWayland::InsertFence(bool implicit) {
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};
  return eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR,
                          implicit ? attrib_list : nullptr);
}

void GbmSurfacelessWayland::FenceRetired(PendingFrame* frame) {
  frame->ready = true;
  MaybeSubmitFrames();
}

void GbmSurfacelessWayland::SetNoGLFlushForTests() {
  no_gl_flush_for_tests_ = true;
}

void GbmSurfacelessWayland::OnSubmission(BufferId buffer_id,
                                         const gfx::SwapResult& swap_result,
                                         gfx::GpuFenceHandle release_fence) {
  DCHECK(!submitted_frames_.empty());
  DCHECK(submitted_frames_.front()->planes.count(buffer_id) ||
         buffer_id == background_buffer_id_);

  auto submitted_frame = std::move(submitted_frames_.front());
  submitted_frames_.erase(submitted_frames_.begin());
  for (auto& plane : submitted_frame->planes) {
    // Let the holder mark this buffer as free to reuse.
    solid_color_buffers_holder_->OnSubmission(plane.first, buffer_manager_,
                                              widget_);
  }
  submitted_frame->planes.clear();
  submitted_frame->overlays.clear();

  std::move(submitted_frame->completion_callback)
      .Run(gfx::SwapCompletionResult(swap_result, std::move(release_fence)));

  submitted_frame->pending_presentation_buffer = buffer_id;
  pending_presentation_frames_.push_back(std::move(submitted_frame));

  if (swap_result != gfx::SwapResult::SWAP_ACK) {
    last_swap_buffers_result_ = false;
    return;
  }

  MaybeSubmitFrames();
}

void GbmSurfacelessWayland::OnPresentation(
    BufferId buffer_id,
    const gfx::PresentationFeedback& feedback) {
  DCHECK(!pending_presentation_frames_.empty());
  DCHECK_EQ(pending_presentation_frames_.front()->pending_presentation_buffer,
            buffer_id);

  std::move(pending_presentation_frames_.front()->presentation_callback)
      .Run(feedback);
  pending_presentation_frames_.erase(pending_presentation_frames_.begin());
}

}  // namespace ui
