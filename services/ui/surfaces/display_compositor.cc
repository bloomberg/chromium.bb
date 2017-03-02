// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/base/switches.h"
#include "cc/output/in_process_context_provider.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface.h"
#include "components/display_compositor/gpu_compositor_frame_sink.h"
#include "components/display_compositor/gpu_root_compositor_frame_sink.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/surfaces/display_output_surface.h"

#if defined(USE_OZONE)
#include "gpu/command_buffer/client/gles2_interface.h"
#include "services/ui/surfaces/display_output_surface_ozone.h"
#endif

namespace ui {

DisplayCompositor::DisplayCompositor(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
    std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    cc::mojom::DisplayCompositorRequest request,
    cc::mojom::DisplayCompositorClientPtr client)
    : manager_(cc::SurfaceManager::LifetimeType::REFERENCES),
      gpu_service_(std::move(gpu_service)),
      gpu_memory_buffer_manager_(std::move(gpu_memory_buffer_manager)),
      image_factory_(image_factory),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  manager_.AddObserver(this);
}

DisplayCompositor::~DisplayCompositor() {
  DCHECK(thread_checker_.CalledOnValidThread());
  manager_.RemoveObserver(this);
}

void DisplayCompositor::CreateRootCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    cc::mojom::MojoCompositorFrameSinkAssociatedRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateAssociatedRequest display_private_request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(surface_handle, gpu::kNullSurfaceHandle);
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));

  std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get())));
  std::unique_ptr<cc::Display> display =
      CreateDisplay(frame_sink_id, surface_handle, begin_frame_source.get());

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<display_compositor::GpuRootCompositorFrameSink>(
          this, &manager_, frame_sink_id, std::move(display),
          std::move(begin_frame_source), std::move(request),
          std::move(private_request), std::move(client),
          std::move(display_private_request));
}

void DisplayCompositor::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<display_compositor::GpuCompositorFrameSink>(
          this, &manager_, frame_sink_id, std::move(request),
          std::move(private_request), std::move(client));
}

void DisplayCompositor::RegisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  manager_.RegisterFrameSinkHierarchy(parent_frame_sink_id,
                                      child_frame_sink_id);
}

void DisplayCompositor::UnregisterFrameSinkHierarchy(
    const cc::FrameSinkId& parent_frame_sink_id,
    const cc::FrameSinkId& child_frame_sink_id) {
  manager_.UnregisterFrameSinkHierarchy(parent_frame_sink_id,
                                        child_frame_sink_id);
}

void DisplayCompositor::DropTemporaryReference(
    const cc::SurfaceId& surface_id) {
  manager_.DropTemporaryReference(surface_id);
}

std::unique_ptr<cc::Display> DisplayCompositor::CreateDisplay(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    cc::SyntheticBeginFrameSource* begin_frame_source) {
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
        begin_frame_source, gpu_memory_buffer_manager_.get(),
        GL_TEXTURE_2D, GL_RGB);
#else
    NOTREACHED();
#endif
  } else {
    display_output_surface = base::MakeUnique<DisplayOutputSurface>(
        std::move(context_provider), begin_frame_source);
  }

  int max_frames_pending =
      display_output_surface->capabilities().max_frames_pending;
  DCHECK_GT(max_frames_pending, 0);

  std::unique_ptr<cc::DisplayScheduler> scheduler(
      new cc::DisplayScheduler(task_runner_.get(), max_frames_pending));

  cc::RendererSettings settings;
  settings.show_overdraw_feedback =
      base::CommandLine::ForCurrentProcess()->HasSwitch(
          cc::switches::kShowOverdrawFeedback);

  return base::MakeUnique<cc::Display>(
      nullptr /* bitmap_manager */, gpu_memory_buffer_manager_.get(), settings,
      frame_sink_id, begin_frame_source, std::move(display_output_surface),
      std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get()));
}

void DisplayCompositor::DestroyCompositorFrameSink(cc::FrameSinkId sink_id) {
  compositor_frame_sinks_.erase(sink_id);
}

void DisplayCompositor::OnSurfaceCreated(const cc::SurfaceInfo& surface_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(surface_info.device_scale_factor(), 0.0f);

  // TODO(kylechar): |client_| will try to find an owner for the temporary
  // reference to the new surface. With surface synchronization this might not
  // be necessary, because a surface reference might already exist and no
  // temporary reference was created. It could be useful to let |client_| know
  // if it should find an owner.
  if (client_)
    client_->OnSurfaceCreated(surface_info);
}

void DisplayCompositor::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                         bool* changed) {}

void DisplayCompositor::OnClientConnectionLost(
    const cc::FrameSinkId& frame_sink_id,
    bool destroy_compositor_frame_sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (destroy_compositor_frame_sink)
    DestroyCompositorFrameSink(frame_sink_id);
  // TODO(fsamuel): Tell the display compositor host that the client connection
  // has been lost so that it can drop its private connection and allow a new
  // client instance to create a new CompositorFrameSink.
}

void DisplayCompositor::OnPrivateConnectionLost(
    const cc::FrameSinkId& frame_sink_id,
    bool destroy_compositor_frame_sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (destroy_compositor_frame_sink)
    DestroyCompositorFrameSink(frame_sink_id);
}

}  // namespace ui
