// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_presentation_helper.h"

#include "base/threading/thread_task_runner_handle.h"
#include "ui/gfx/vsync_provider.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gpu_timing.h"

namespace gl {

GLSurfacePresentationHelper::Frame::Frame(Frame&& other) = default;

GLSurfacePresentationHelper::Frame::Frame(
    std::unique_ptr<GPUTimer>&& timer,
    const GLSurface::PresentationCallback& callback)
    : timer(std::move(timer)), callback(callback) {}

GLSurfacePresentationHelper::Frame::~Frame() = default;

GLSurfacePresentationHelper::Frame& GLSurfacePresentationHelper::Frame::
operator=(Frame&& other) = default;

GLSurfacePresentationHelper::GLSurfacePresentationHelper(
    gfx::VSyncProvider* vsync_provider,
    bool hw_clock)
    : vsync_provider_(vsync_provider),
      hw_clock_(hw_clock),
      weak_ptr_factory_(this) {
  DCHECK(vsync_provider_);
}

GLSurfacePresentationHelper::GLSurfacePresentationHelper(
    base::TimeTicks timebase,
    base::TimeDelta interval)
    : vsync_provider_(nullptr),
      hw_clock_(false),
      vsync_timebase_(timebase),
      vsync_interval_(interval),
      weak_ptr_factory_(this) {}

GLSurfacePresentationHelper::~GLSurfacePresentationHelper() {
  // Discard pending frames and run presentation callback with empty
  // PresentationFeedback.
  bool has_context = gl_context_ && gl_context_->IsCurrent(surface_);
  for (auto& frame : pending_frames_) {
    frame.timer->Destroy(has_context);
    frame.callback.Run(gfx::PresentationFeedback());
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
  if (gpu_timing_client_) {
    gpu_timing_client_ = nullptr;
    for (auto& frame : pending_frames_) {
      frame.timer->Destroy(false /* has_context */);
      frame.callback.Run(gfx::PresentationFeedback());
    }
    pending_frames_.clear();
  }

  gl_context_ = context;
  gpu_timing_client_ = context->CreateGPUTimingClient();
  if (!gpu_timing_client_->IsAvailable())
    gpu_timing_client_ = nullptr;
}

void GLSurfacePresentationHelper::PreSwapBuffers(
    const GLSurface::PresentationCallback& callback) {
  std::unique_ptr<GPUTimer> timer;
  if (gpu_timing_client_) {
    timer = gpu_timing_client_->CreateGPUTimer(false /* prefer_elapsed_time */);
    timer->QueryTimeStamp();
  }
  pending_frames_.push_back(Frame(std::move(timer), callback));
}

void GLSurfacePresentationHelper::PostSwapBuffers() {
  if (!waiting_for_vsync_parameters_)
    CheckPendingFrames();
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

  // If the |gl_context_| is not current anymore, we assume SwapBuffers issued
  // for previous context will be discarded.
  if (gl_context_ && !gl_context_->IsCurrent(surface_)) {
    gpu_timing_client_ = nullptr;
    for (auto& frame : pending_frames_) {
      frame.timer->Destroy(false /* has_context */);
      frame.callback.Run(gfx::PresentationFeedback());
    }
    pending_frames_.clear();
    return;
  }

  bool disjoint_occurred =
      gpu_timing_client_ && gpu_timing_client_->CheckAndResetTimerErrors();
  if (disjoint_occurred || !gpu_timing_client_) {
    // If GPUTimer is not avaliable or disjoint occurred, we just run
    // presentation callback with current system time.
    for (auto& frame : pending_frames_) {
      frame.callback.Run(gfx::PresentationFeedback(
          base::TimeTicks::Now(), vsync_interval_, 0 /* flags */));
    }
    pending_frames_.clear();
    return;
  }

  bool need_update_vsync = false;
  while (!pending_frames_.empty()) {
    auto& frame = pending_frames_.front();
    if (!frame.timer->IsAvailable())
      break;

    int64_t start = 0;
    int64_t end = 0;
    frame.timer->GetStartEndTimestamps(&start, &end);
    auto timestamp =
        base::TimeTicks() + base::TimeDelta::FromMicroseconds(start);

    // Helper lambda for running the presentation callback and releasing the
    // frame.
    auto frame_presentation_callback =
        [this, &frame](const gfx::PresentationFeedback& feedback) {
          frame.timer->Destroy(true /* has_context */);
          frame.callback.Run(feedback);
          pending_frames_.pop_front();
        };

    const bool fixed_vsync = !vsync_provider_;
    if (vsync_interval_.is_zero() || fixed_vsync) {
      // If VSync parameters are fixed or not avaliable, we just run
      // presentation callbacks with timestamp from GPUTimers.
      frame_presentation_callback(
          gfx::PresentationFeedback(timestamp, vsync_interval_, 0 /* flags */));
    } else if (timestamp < vsync_timebase_) {
      // We got a VSync whose timestamp is after GPU finished renderering this
      // back buffer.
      uint32_t flags = gfx::PresentationFeedback::kVSync |
                       gfx::PresentationFeedback::kHWCompletion;
      auto delta = vsync_timebase_ - timestamp;
      if (delta < vsync_interval_) {
        // The |vsync_timebase_| is the closest VSync's timestamp after the GPU
        // finished renderering.
        timestamp = vsync_timebase_;
        if (hw_clock_)
          flags |= gfx::PresentationFeedback::kHWClock;
      } else {
        // The |vsync_timebase_| isn't the closest VSync's timestamp after the
        // GPU finished renderering. We have to compute the closest VSync's
        // timestmp.
        timestamp =
            timestamp.SnappedToNextTick(vsync_timebase_, vsync_interval_);
      }
      frame_presentation_callback(
          gfx::PresentationFeedback(timestamp, vsync_interval_, flags));
    } else {
      // The |vsync_timebase_| is earlier than |timestamp| of the current
      // pending frame. We need update vsync parameters.
      need_update_vsync = true;
      // We compute the next VSync's timestamp and use it to run
      // callback.
      auto delta = timestamp - vsync_timebase_;
      auto offset = delta % vsync_interval_;
      timestamp += vsync_interval_ - offset;
      uint32_t flags = gfx::PresentationFeedback::kVSync;
      frame_presentation_callback(
          gfx::PresentationFeedback(timestamp, vsync_interval_, flags));
    }
  }

  if (waiting_for_vsync_parameters_ || !need_update_vsync ||
      pending_frames_.empty())
    return;

  waiting_for_vsync_parameters_ = true;
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

void GLSurfacePresentationHelper::CheckPendingFramesCallback() {
  DCHECK(waiting_for_vsync_parameters_);
  waiting_for_vsync_parameters_ = false;
  CheckPendingFrames();
}

void GLSurfacePresentationHelper::UpdateVSyncCallback(
    const base::TimeTicks timebase,
    const base::TimeDelta interval) {
  DCHECK(waiting_for_vsync_parameters_);
  waiting_for_vsync_parameters_ = false;
  vsync_timebase_ = timebase;
  vsync_interval_ = interval;
  CheckPendingFrames();
}

}  // namespace gl
