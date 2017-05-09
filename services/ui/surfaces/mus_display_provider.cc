// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/mus_display_provider.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "cc/output/in_process_context_provider.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "components/display_compositor/host_shared_bitmap_manager.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/command_buffer/service/image_factory.h"
#include "services/ui/surfaces/display_output_surface.h"

#if defined(USE_OZONE)
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/ui/surfaces/display_output_surface_ozone.h"
#endif

namespace ui {

MusDisplayProvider::MusDisplayProvider(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
    std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory)
    : gpu_service_(std::move(gpu_service)),
      gpu_memory_buffer_manager_(std::move(gpu_memory_buffer_manager)),
      image_factory_(image_factory),
      task_runner_(base::ThreadTaskRunnerHandle::Get()) {}

MusDisplayProvider::~MusDisplayProvider() {}

std::unique_ptr<cc::Display> MusDisplayProvider::CreateDisplay(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    std::unique_ptr<cc::BeginFrameSource>* begin_frame_source) {
  auto synthetic_begin_frame_source =
      base::MakeUnique<cc::DelayBasedBeginFrameSource>(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get()));

  scoped_refptr<cc::InProcessContextProvider> context_provider =
      new cc::InProcessContextProvider(
          gpu_service_, surface_handle, gpu_memory_buffer_manager_.get(),
          image_factory_, gpu::SharedMemoryLimits(),
          nullptr /* shared_context */);

  // TODO(rjkroege): If there is something better to do than CHECK, add it.
  CHECK(context_provider->BindToCurrentThread());

  std::unique_ptr<cc::OutputSurface> display_output_surface;
  if (context_provider->ContextCapabilities().surfaceless) {
#if defined(USE_OZONE)
    display_output_surface = base::MakeUnique<DisplayOutputSurfaceOzone>(
        std::move(context_provider), surface_handle,
        synthetic_begin_frame_source.get(), gpu_memory_buffer_manager_.get(),
        GL_TEXTURE_2D, GL_RGB);
#else
    NOTREACHED();
#endif
  } else {
    display_output_surface = base::MakeUnique<DisplayOutputSurface>(
        std::move(context_provider), synthetic_begin_frame_source.get());
  }

  int max_frames_pending =
      display_output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  auto scheduler = base::MakeUnique<cc::DisplayScheduler>(task_runner_.get(),
                                                          max_frames_pending);

  cc::RendererSettings settings;
  settings.show_overdraw_feedback =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kShowOverdrawFeedback);

  // The ownership of the BeginFrameSource is transfered to the caller.
  *begin_frame_source = std::move(synthetic_begin_frame_source);

  return base::MakeUnique<cc::Display>(
      display_compositor::HostSharedBitmapManager::current(),
      gpu_memory_buffer_manager_.get(), settings, frame_sink_id,
      begin_frame_source->get(), std::move(display_output_surface),
      std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get()));
}

}  // namespace ui
