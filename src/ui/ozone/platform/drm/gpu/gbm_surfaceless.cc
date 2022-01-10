// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/drm/gpu/gbm_surfaceless.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/check_op.h"
#include "base/notreached.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool.h"
#include "base/trace_event/trace_event.h"
#include "ui/gfx/gpu_fence_handle.h"
#include "ui/gfx/presentation_feedback.h"
#include "ui/gl/gl_bindings.h"
#include "ui/ozone/common/egl_util.h"
#include "ui/ozone/platform/drm/gpu/drm_device.h"
#include "ui/ozone/platform/drm/gpu/drm_framebuffer.h"
#include "ui/ozone/platform/drm/gpu/drm_window_proxy.h"
#include "ui/ozone/platform/drm/gpu/gbm_surface_factory.h"

#if BUILDFLAG(USE_OPENGL_APITRACE)
#include "ui/gl/gl_implementation.h"
#endif

namespace ui {

namespace {

void WaitForFence(EGLDisplay display, EGLSyncKHR fence) {
  eglClientWaitSyncKHR(display, fence, EGL_SYNC_FLUSH_COMMANDS_BIT_KHR,
                       EGL_FOREVER_KHR);
  eglDestroySyncKHR(display, fence);
}

}  // namespace

GbmSurfaceless::GbmSurfaceless(GbmSurfaceFactory* surface_factory,
                               std::unique_ptr<DrmWindowProxy> window,
                               gfx::AcceleratedWidget widget)
    : SurfacelessEGL(gfx::Size()),
      surface_factory_(surface_factory),
      window_(std::move(window)),
      widget_(widget),
      has_implicit_external_sync_(
          HasEGLExtension("EGL_ARM_implicit_external_sync")),
      has_image_flush_external_(
          HasEGLExtension("EGL_EXT_image_flush_external")) {
  surface_factory_->RegisterSurface(window_->widget(), this);
  supports_plane_gpu_fences_ = window_->SupportsGpuFences();
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());
}

void GbmSurfaceless::QueueOverlayPlane(DrmOverlayPlane plane) {
  is_on_external_drm_device_ = !plane.buffer->drm_device()->is_primary_device();
  planes_.push_back(std::move(plane));
}

bool GbmSurfaceless::Initialize(gl::GLSurfaceFormat format) {
  if (!SurfacelessEGL::Initialize(format))
    return false;
  return true;
}

gfx::SwapResult GbmSurfaceless::SwapBuffers(PresentationCallback callback) {
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

bool GbmSurfaceless::ScheduleOverlayPlane(
    gl::GLImage* image,
    std::unique_ptr<gfx::GpuFence> gpu_fence,
    const gfx::OverlayPlaneData& overlay_plane_data) {
  unsubmitted_frames_.back()->overlays.push_back(
      gl::GLSurfaceOverlay(image, std::move(gpu_fence), overlay_plane_data));
  return true;
}

bool GbmSurfaceless::Resize(const gfx::Size& size,
                            float scale_factor,
                            const gfx::ColorSpace& color_space,
                            bool has_alpha) {
  if (window_)
    window_->SetColorSpace(color_space);

  return SurfacelessEGL::Resize(size, scale_factor, color_space, has_alpha);
}

bool GbmSurfaceless::IsOffscreen() {
  return false;
}

bool GbmSurfaceless::SupportsAsyncSwap() {
  return true;
}

bool GbmSurfaceless::SupportsPostSubBuffer() {
  return true;
}

bool GbmSurfaceless::SupportsPlaneGpuFences() const {
  return supports_plane_gpu_fences_;
}

gfx::SwapResult GbmSurfaceless::PostSubBuffer(int x,
                                              int y,
                                              int width,
                                              int height,
                                              PresentationCallback callback) {
  // The actual sub buffer handling is handled at higher layers.
  NOTREACHED();
  return gfx::SwapResult::SWAP_FAILED;
}

void GbmSurfaceless::SwapBuffersAsync(
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  TRACE_EVENT0("drm", "GbmSurfaceless::SwapBuffersAsync");
  // If last swap failed, don't try to schedule new ones.
  if (!last_swap_buffers_result_) {
    std::move(completion_callback)
        .Run(gfx::SwapCompletionResult(gfx::SwapResult::SWAP_FAILED));
    // Notify the caller, the buffer is never presented on a screen.
    std::move(presentation_callback).Run(gfx::PresentationFeedback::Failure());
    return;
  }

  if ((!has_image_flush_external_ && !supports_plane_gpu_fences_) ||
      requires_gl_flush_on_swap_buffers_) {
    glFlush();
  }

#if BUILDFLAG(USE_OPENGL_APITRACE)
  gl::TerminateFrame();  // Notify end of frame at buffer swap request.
#endif

  unsubmitted_frames_.back()->Flush();

  PendingFrame* frame = unsubmitted_frames_.back().get();
  frame->completion_callback = std::move(completion_callback);
  frame->presentation_callback = std::move(presentation_callback);
  unsubmitted_frames_.push_back(std::make_unique<PendingFrame>());

  // TODO(dcastagna): Remove the following workaround once we get explicit sync
  // on all Intel boards, currently we don't have it on legacy KMS.
  // We can not rely on implicit sync on external devices (crbug.com/692508).
  // NOTE: When on internal devices, |is_on_external_drm_device_| is set to true
  // by default conservatively, and it is correctly computed after the first
  // plane is enqueued in QueueOverlayPlane, that is called from
  // GbmSurfaceless::SubmitFrame.
  // This means |is_on_external_drm_device_| could be incorrectly set to true
  // the first time we're testing it.
  if (supports_plane_gpu_fences_ ||
      (!use_egl_fence_sync_ && !is_on_external_drm_device_)) {
    frame->ready = true;
    SubmitFrame();
    return;
  }

  // TODO: the following should be replaced by a per surface flush as it gets
  // implemented in GL drivers.
  EGLSyncKHR fence = InsertFence(has_implicit_external_sync_);
  CHECK_NE(fence, EGL_NO_SYNC_KHR) << "eglCreateSyncKHR failed";

  base::OnceClosure fence_wait_task =
      base::BindOnce(&WaitForFence, GetDisplay(), fence);

  base::OnceClosure fence_retired_callback = base::BindOnce(
      &GbmSurfaceless::FenceRetired, weak_factory_.GetWeakPtr(), frame);

  base::ThreadPool::PostTaskAndReply(
      FROM_HERE,
      {base::MayBlock(), base::TaskShutdownBehavior::CONTINUE_ON_SHUTDOWN},
      std::move(fence_wait_task), std::move(fence_retired_callback));
}

void GbmSurfaceless::PostSubBufferAsync(
    int x,
    int y,
    int width,
    int height,
    SwapCompletionCallback completion_callback,
    PresentationCallback presentation_callback) {
  // The actual sub buffer handling is handled at higher layers.
  SwapBuffersAsync(std::move(completion_callback),
                   std::move(presentation_callback));
}

EGLConfig GbmSurfaceless::GetConfig() {
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

void GbmSurfaceless::SetRelyOnImplicitSync() {
  use_egl_fence_sync_ = false;
}

void GbmSurfaceless::SetForceGlFlushOnSwapBuffers() {
  requires_gl_flush_on_swap_buffers_ = true;
}

gfx::SurfaceOrigin GbmSurfaceless::GetOrigin() const {
  return gfx::SurfaceOrigin::kTopLeft;
}

GbmSurfaceless::~GbmSurfaceless() {
  Destroy();  // The EGL surface must be destroyed before SurfaceOzone.
  surface_factory_->UnregisterSurface(window_->widget());
}

GbmSurfaceless::PendingFrame::PendingFrame() {}

GbmSurfaceless::PendingFrame::~PendingFrame() {}

bool GbmSurfaceless::PendingFrame::ScheduleOverlayPlanes(
    gfx::AcceleratedWidget widget) {
  for (auto& overlay : overlays)
    if (!overlay.ScheduleOverlayPlane(widget))
      return false;
  return true;
}

void GbmSurfaceless::PendingFrame::Flush() {
  for (const auto& overlay : overlays)
    overlay.Flush();
}

void GbmSurfaceless::SubmitFrame() {
  DCHECK(!unsubmitted_frames_.empty());

  if (unsubmitted_frames_.front()->ready && !submitted_frame_) {
    for (auto& overlay : unsubmitted_frames_.front()->overlays) {
      if (overlay.z_order() == 0 && overlay.gpu_fence()) {
        submitted_frame_gpu_fence_ = std::make_unique<gfx::GpuFence>(
            overlay.gpu_fence()->GetGpuFenceHandle().Clone());
        break;
      }
    }
    submitted_frame_ = std::move(unsubmitted_frames_.front());
    unsubmitted_frames_.erase(unsubmitted_frames_.begin());

    bool schedule_planes_succeeded =
        submitted_frame_->ScheduleOverlayPlanes(widget_);

    if (!schedule_planes_succeeded) {
      OnSubmission(gfx::SwapResult::SWAP_FAILED,
                   /*release_fence=*/gfx::GpuFenceHandle());
      OnPresentation(gfx::PresentationFeedback::Failure());
      return;
    }

    window_->SchedulePageFlip(std::move(planes_),
                              base::BindOnce(&GbmSurfaceless::OnSubmission,
                                             weak_factory_.GetWeakPtr()),
                              base::BindOnce(&GbmSurfaceless::OnPresentation,
                                             weak_factory_.GetWeakPtr()));
    planes_.clear();
  }
}

EGLSyncKHR GbmSurfaceless::InsertFence(bool implicit) {
  const EGLint attrib_list[] = {EGL_SYNC_CONDITION_KHR,
                                EGL_SYNC_PRIOR_COMMANDS_IMPLICIT_EXTERNAL_ARM,
                                EGL_NONE};
  return eglCreateSyncKHR(GetDisplay(), EGL_SYNC_FENCE_KHR,
                          implicit ? attrib_list : NULL);
}

void GbmSurfaceless::FenceRetired(PendingFrame* frame) {
  frame->ready = true;
  SubmitFrame();
}

void GbmSurfaceless::OnSubmission(gfx::SwapResult result,
                                  gfx::GpuFenceHandle release_fence) {
  submitted_frame_->swap_result = result;
  if (!release_fence.is_null()) {
    std::move(submitted_frame_->completion_callback)
        .Run(gfx::SwapCompletionResult(result, std::move(release_fence)));
  }
}

void GbmSurfaceless::OnPresentation(const gfx::PresentationFeedback& feedback) {
  gfx::PresentationFeedback feedback_copy = feedback;

  if (submitted_frame_gpu_fence_ && !feedback.failed()) {
    feedback_copy.ready_timestamp =
        submitted_frame_gpu_fence_->GetMaxTimestamp();
  }
  submitted_frame_gpu_fence_.reset();
  submitted_frame_->overlays.clear();

  gfx::SwapResult result = submitted_frame_->swap_result;
  if (submitted_frame_->completion_callback)
    std::move(submitted_frame_->completion_callback)
        .Run(gfx::SwapCompletionResult(result));
  std::move(submitted_frame_->presentation_callback).Run(feedback_copy);
  submitted_frame_.reset();

  if (result == gfx::SwapResult::SWAP_FAILED) {
    last_swap_buffers_result_ = false;
    return;
  }

  SubmitFrame();
}

}  // namespace ui
