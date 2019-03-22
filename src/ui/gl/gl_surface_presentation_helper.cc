// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_presentation_helper.h"

#include "base/threading/thread_task_runner_handle.h"
#include "build/build_config.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/egl_timestamps.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_fence.h"
#include "ui/gl/gpu_timing.h"

namespace gl {

GLSurfacePresentationHelper::ScopedSwapBuffers::ScopedSwapBuffers(
    GLSurfacePresentationHelper* helper,
    const GLSurface::PresentationCallback& callback)
    : ScopedSwapBuffers(helper, callback, -1) {}

GLSurfacePresentationHelper::ScopedSwapBuffers::ScopedSwapBuffers(
    GLSurfacePresentationHelper* helper,
    const GLSurface::PresentationCallback& callback,
    int frame_id)
    : helper_(helper) {
  if (helper_)
    helper_->PreSwapBuffers(callback, frame_id);
}

GLSurfacePresentationHelper::ScopedSwapBuffers::~ScopedSwapBuffers() {
  if (helper_)
    helper_->PostSwapBuffers(result_);
}

GLSurfacePresentationHelper::Frame::Frame(Frame&& other) = default;

GLSurfacePresentationHelper::Frame::Frame(
    int frame_id,
    const GLSurface::PresentationCallback& callback)
    : frame_id(frame_id), callback(callback) {}

GLSurfacePresentationHelper::Frame::Frame(
    std::unique_ptr<GPUTimer>&& timer,
    const GLSurface::PresentationCallback& callback)
    : timer(std::move(timer)), callback(callback) {}

GLSurfacePresentationHelper::Frame::Frame(
    std::unique_ptr<GLFence>&& fence,
    const GLSurface::PresentationCallback& callback)
    : fence(std::move(fence)), callback(callback) {}

GLSurfacePresentationHelper::Frame::Frame(
    const GLSurface::PresentationCallback& callback)
    : callback(callback) {}

GLSurfacePresentationHelper::Frame::~Frame() = default;

GLSurfacePresentationHelper::Frame& GLSurfacePresentationHelper::Frame::
operator=(Frame&& other) = default;

bool GLSurfacePresentationHelper::GetFrameTimestampInfoIfAvailable(
    const Frame& frame,
    base::TimeTicks* timestamp,
    base::TimeDelta* interval,
    uint32_t* flags) {
  DCHECK(frame.timer || frame.fence || egl_timestamp_client_);

  if (egl_timestamp_client_) {
    return egl_timestamp_client_->GetFrameTimestampInfoIfAvailable(
        timestamp, interval, flags, frame.frame_id);
  } else if (frame.timer) {
    if (!frame.timer->IsAvailable())
      return false;
    int64_t start = 0;
    int64_t end = 0;
    frame.timer->GetStartEndTimestamps(&start, &end);
    *timestamp = base::TimeTicks() + base::TimeDelta::FromMicroseconds(start);
  } else {
    if (!frame.fence->HasCompleted())
      return false;
    *timestamp = base::TimeTicks::Now();
  }
  // Below logic is used to calculate final values of timestamp, interval and
  // flags when using timer/fence to report the timestamps.
  const bool fixed_vsync = !vsync_provider_;
  const bool hw_clock = vsync_provider_ && vsync_provider_->IsHWClock();
  *interval = vsync_interval_;
  *flags = 0;
  if (vsync_interval_.is_zero() || fixed_vsync) {
    // If VSync parameters are fixed or not available, we just run
    // presentation callbacks with timestamp from GPUTimers.
    return true;
  } else if (*timestamp < vsync_timebase_) {
    // We got a VSync whose timestamp is after GPU finished rendering this
    // back buffer.
    *flags = gfx::PresentationFeedback::kVSync |
             gfx::PresentationFeedback::kHWCompletion;
    auto delta = vsync_timebase_ - *timestamp;
    if (delta < vsync_interval_) {
      // The |vsync_timebase_| is the closest VSync's timestamp after the GPU
      // finished rendering.
      *timestamp = vsync_timebase_;
      if (hw_clock)
        *flags |= gfx::PresentationFeedback::kHWClock;
    } else {
      // The |vsync_timebase_| isn't the closest VSync's timestamp after the
      // GPU finished rendering. We have to compute the closest VSync's
      // timestmp.
      *timestamp =
          timestamp->SnappedToNextTick(vsync_timebase_, vsync_interval_);
    }
  } else {
    // The |vsync_timebase_| is earlier than |timestamp|, we will compute the
    // next vSync's timestamp and use it to run callback.
    if (!vsync_interval_.is_zero()) {
      *timestamp =
          timestamp->SnappedToNextTick(vsync_timebase_, vsync_interval_);
      *flags = gfx::PresentationFeedback::kVSync;
    }
  }
  return true;
}

void GLSurfacePresentationHelper::Frame::Destroy(bool has_context) {
  if (timer) {
    timer->Destroy(has_context);
  } else if (fence) {
    if (has_context)
      fence = nullptr;
    else
      fence->Invalidate();
  }
  callback.Run(gfx::PresentationFeedback::Failure());
}

GLSurfacePresentationHelper::GLSurfacePresentationHelper(
    gfx::VSyncProvider* vsync_provider)
    : vsync_provider_(vsync_provider), weak_ptr_factory_(this) {}

GLSurfacePresentationHelper::GLSurfacePresentationHelper(
    base::TimeTicks timebase,
    base::TimeDelta interval)
    : vsync_provider_(nullptr),
      vsync_timebase_(timebase),
      vsync_interval_(interval),
      weak_ptr_factory_(this) {}

GLSurfacePresentationHelper::~GLSurfacePresentationHelper() {
  // Discard pending frames and run presentation callback with empty
  // PresentationFeedback.
  bool has_context = gl_context_ && gl_context_->IsCurrent(surface_);
  for (auto& frame : pending_frames_) {
    frame.Destroy(has_context);
  }
  pending_frames_.clear();
}

void GLSurfacePresentationHelper::OnMakeCurrent(GLContext* context,
                                                GLSurface* surface) {
  DCHECK(context);
  DCHECK(surface);
  DCHECK(surface == surface_ || !surface_);
  if (context == gl_context_)
    return;

  surface_ = surface;
  // If context is changed, we assume SwapBuffers issued for previous context
  // will be discarded.
  if (gpu_timing_client_)
    gpu_timing_client_ = nullptr;
  for (auto& frame : pending_frames_) {
    frame.Destroy();
  }
  pending_frames_.clear();

  gl_context_ = context;

  // Get an egl timestamp client.
  egl_timestamp_client_ = surface_->GetEGLTimestampClient();

  // If there is an egl timestamp client, check if egl timestamps are supported
  // or not. If supported, then return as there is no need to use gpu timestamp
  // client or fence.
  if (egl_timestamp_client_) {
    if (egl_timestamp_client_->IsEGLTimestampSupported())
      return;
    else
      egl_timestamp_client_ = nullptr;
  }

  gpu_timing_client_ = context->CreateGPUTimingClient();
  if (!gpu_timing_client_->IsAvailable())
    gpu_timing_client_ = nullptr;

// https://crbug.com/854298 : disable GLFence on Android as they seem to cause
// issues on some devices.
#if !defined(OS_ANDROID)
  gl_fence_supported_ = GLFence::IsSupported();
#endif
}

void GLSurfacePresentationHelper::PreSwapBuffers(
    const GLSurface::PresentationCallback& callback,
    int frame_id) {
  if (egl_timestamp_client_) {
    pending_frames_.emplace_back(frame_id, callback);
  } else if (gpu_timing_client_) {
    std::unique_ptr<GPUTimer> timer;
    timer = gpu_timing_client_->CreateGPUTimer(false /* prefer_elapsed_time */);
    timer->QueryTimeStamp();
    pending_frames_.push_back(Frame(std::move(timer), callback));
  } else if (gl_fence_supported_) {
    auto fence = GLFence::Create();
    pending_frames_.push_back(Frame(std::move(fence), callback));
  } else {
    pending_frames_.push_back(Frame(callback));
  }
}

void GLSurfacePresentationHelper::PostSwapBuffers(gfx::SwapResult result) {
  DCHECK(!pending_frames_.empty());
  auto& frame = pending_frames_.back();
  frame.result = result;
  ScheduleCheckPendingFrames(false /* align_with_next_vsync */);
}

void GLSurfacePresentationHelper::CheckPendingFrames() {
  DCHECK(gl_context_ || pending_frames_.empty());

  if (vsync_provider_ &&
      vsync_provider_->SupportGetVSyncParametersIfAvailable()) {
    if (!vsync_provider_->GetVSyncParametersIfAvailable(&vsync_timebase_,
                                                        &vsync_interval_)) {
      vsync_timebase_ = base::TimeTicks();
      vsync_interval_ = base::TimeDelta();
      LOG(ERROR) << "GetVSyncParametersIfAvailable() failed!";
    }
  }

  if (pending_frames_.empty())
    return;

  if (!gl_context_->MakeCurrent(surface_)) {
    gl_context_ = nullptr;
    egl_timestamp_client_ = nullptr;
    gpu_timing_client_ = nullptr;
    for (auto& frame : pending_frames_)
      frame.Destroy();
    pending_frames_.clear();
    return;
  }

  bool need_update_vsync = false;
  bool disjoint_occurred =
      gpu_timing_client_ && gpu_timing_client_->CheckAndResetTimerErrors();
  if (disjoint_occurred ||
      (!egl_timestamp_client_ && !gpu_timing_client_ && !gl_fence_supported_)) {
    // If EGLTimestamps, GPUTimer and GLFence are not available or disjoint
    // occurred, we will compute the next VSync's timestamp and use it to run
    // presentation callback.
    uint32_t flags = 0;
    auto timestamp = base::TimeTicks::Now();
    if (!vsync_interval_.is_zero()) {
      timestamp = timestamp.SnappedToNextTick(vsync_timebase_, vsync_interval_);
      flags = gfx::PresentationFeedback::kVSync;
    }
    gfx::PresentationFeedback feedback(timestamp, vsync_interval_, flags);
    for (auto& frame : pending_frames_) {
      if (frame.timer)
        frame.timer->Destroy(true /* has_context */);
      if (frame.result == gfx::SwapResult::SWAP_ACK)
        frame.callback.Run(feedback);
      else
        frame.callback.Run(gfx::PresentationFeedback::Failure());
    }
    pending_frames_.clear();
    // We want to update VSync, if we can not get VSync parameters
    // synchronously. Otherwise we will update the VSync parameters with the
    // next SwapBuffers.
    if (vsync_provider_ &&
        !vsync_provider_->SupportGetVSyncParametersIfAvailable()) {
      need_update_vsync = true;
    }
  }

  while (!pending_frames_.empty()) {
    auto& frame = pending_frames_.front();
    // Helper lambda for running the presentation callback and releasing the
    // frame.
    auto frame_presentation_callback =
        [this, &frame](const gfx::PresentationFeedback& feedback) {
          if (frame.timer)
            frame.timer->Destroy(true /* has_context */);
          frame.callback.Run(feedback);
          pending_frames_.pop_front();
        };

    if (frame.result != gfx::SwapResult::SWAP_ACK) {
      frame_presentation_callback(gfx::PresentationFeedback::Failure());
      continue;
    }

    base::TimeTicks timestamp;
    base::TimeDelta interval;
    uint32_t flags = 0;
    // Get timestamp info for a frame if available. If timestamp is not
    // available, it means this frame is not yet done.
    if (!GetFrameTimestampInfoIfAvailable(frame, &timestamp, &interval, &flags))
      break;

    frame_presentation_callback(
        gfx::PresentationFeedback(timestamp, interval, flags));
  }

  if (pending_frames_.empty() && !need_update_vsync)
    return;
  ScheduleCheckPendingFrames(true /* align_with_next_vsync */);
}

void GLSurfacePresentationHelper::CheckPendingFramesCallback() {
  DCHECK(check_pending_frame_scheduled_);
  check_pending_frame_scheduled_ = false;
  CheckPendingFrames();
}

void GLSurfacePresentationHelper::UpdateVSyncCallback(
    const base::TimeTicks timebase,
    const base::TimeDelta interval) {
  DCHECK(check_pending_frame_scheduled_);
  check_pending_frame_scheduled_ = false;
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
  CheckPendingFrames();
}

void GLSurfacePresentationHelper::ScheduleCheckPendingFrames(
    bool align_with_next_vsync) {
  if (check_pending_frame_scheduled_)
    return;
  check_pending_frame_scheduled_ = true;

  if (!align_with_next_vsync) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&GLSurfacePresentationHelper::CheckPendingFramesCallback,
                       weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  if (vsync_provider_ &&
      !vsync_provider_->SupportGetVSyncParametersIfAvailable()) {
    // In this case, the |vsync_provider_| will call the callback when the next
    // VSync is received.
    vsync_provider_->GetVSyncParameters(
        base::BindRepeating(&GLSurfacePresentationHelper::UpdateVSyncCallback,
                            weak_ptr_factory_.GetWeakPtr()));
    return;
  }

  // If the |vsync_provider_| can not notify us for the next VSync
  // asynchronically, we have to compute the next VSync time and post a delayed
  // task so we can check the VSync later.
  base::TimeDelta interval = vsync_interval_.is_zero()
                                 ? base::TimeDelta::FromSeconds(1) / 60
                                 : vsync_interval_;
  auto now = base::TimeTicks::Now();
  auto next_vsync = now.SnappedToNextTick(vsync_timebase_, interval);
  base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&GLSurfacePresentationHelper::CheckPendingFramesCallback,
                     weak_ptr_factory_.GetWeakPtr()),
      next_vsync - now);
}

}  // namespace gl
