// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

#include <utility>

#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "cc/output/in_process_context_provider.h"
#include "cc/output/texture_mailbox_deleter.h"
#include "cc/surfaces/display.h"
#include "cc/surfaces/display_scheduler.h"
#include "cc/surfaces/surface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/surfaces/display_output_surface.h"
#include "services/ui/surfaces/gpu_compositor_frame_sink.h"

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
      reference_manager_(&manager_),
      gpu_service_(std::move(gpu_service)),
      gpu_memory_buffer_manager_(std::move(gpu_memory_buffer_manager)),
      image_factory_(image_factory),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  manager_.AddObserver(this);
  if (client_)
    client_->OnDisplayCompositorCreated(GetRootSurfaceId());
}

void DisplayCompositor::AddSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  DCHECK(thread_checker_.CalledOnValidThread());
  for (const auto& reference : references) {
    reference_manager_->AddSurfaceReference(reference.parent_id(),
                                            reference.child_id());
  }
}

void DisplayCompositor::RemoveSurfaceReferences(
    const std::vector<cc::SurfaceReference>& references) {
  DCHECK(thread_checker_.CalledOnValidThread());

  // TODO(kylechar): Each remove reference can trigger GC, it would be better if
  // we GC only once if removing multiple references.
  for (const auto& reference : references) {
    reference_manager_->RemoveSurfaceReference(reference.parent_id(),
                                               reference.child_id());
  }
}

DisplayCompositor::~DisplayCompositor() {
  DCHECK(thread_checker_.CalledOnValidThread());
  manager_.RemoveObserver(this);
}

void DisplayCompositor::OnCompositorFrameSinkClientConnectionLost(
    const cc::FrameSinkId& frame_sink_id,
    bool destroy_compositor_frame_sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (destroy_compositor_frame_sink)
    compositor_frame_sinks_.erase(frame_sink_id);
  // TODO(fsamuel): Tell the display compositor host that the client connection
  // has been lost so that it can drop its private connection and allow a new
  // client instance to create a new CompositorFrameSink.
}

void DisplayCompositor::OnCompositorFrameSinkPrivateConnectionLost(
    const cc::FrameSinkId& frame_sink_id,
    bool destroy_compositor_frame_sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (destroy_compositor_frame_sink)
    compositor_frame_sinks_.erase(frame_sink_id);
}

void DisplayCompositor::CreateDisplayCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateRequest display_private_request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_NE(surface_handle, gpu::kNullSurfaceHandle);
  std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source(
      new cc::DelayBasedBeginFrameSource(
          base::MakeUnique<cc::DelayBasedTimeSource>(task_runner_.get())));
  std::unique_ptr<cc::Display> display =
      CreateDisplay(frame_sink_id, surface_handle, begin_frame_source.get());
  CreateCompositorFrameSinkInternal(
      frame_sink_id, surface_handle, std::move(display),
      std::move(begin_frame_source), std::move(request),
      std::move(private_request), std::move(client),
      std::move(display_private_request));
}

void DisplayCompositor::CreateOffscreenCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  CreateCompositorFrameSinkInternal(frame_sink_id, gpu::kNullSurfaceHandle,
                                    nullptr, nullptr, std::move(request),
                                    std::move(private_request),
                                    std::move(client), nullptr);
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

  return base::MakeUnique<cc::Display>(
      nullptr /* bitmap_manager */, gpu_memory_buffer_manager_.get(),
      cc::RendererSettings(), frame_sink_id, begin_frame_source,
      std::move(display_output_surface), std::move(scheduler),
      base::MakeUnique<cc::TextureMailboxDeleter>(task_runner_.get()));
}

void DisplayCompositor::CreateCompositorFrameSinkInternal(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    std::unique_ptr<cc::Display> display,
    std::unique_ptr<cc::SyntheticBeginFrameSource> begin_frame_source,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client,
    cc::mojom::DisplayPrivateRequest display_request) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We cannot create more than one CompositorFrameSink with a given
  // |frame_sink_id|.
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));

  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<GpuCompositorFrameSink>(
          this, frame_sink_id, std::move(display),
          std::move(begin_frame_source), std::move(request),
          std::move(private_request), std::move(client),
          std::move(display_request));
}

const cc::SurfaceId& DisplayCompositor::GetRootSurfaceId() const {
  return reference_manager_->GetRootSurfaceId();
}

void DisplayCompositor::OnSurfaceCreated(const cc::SurfaceInfo& surface_info) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK_GT(surface_info.device_scale_factor(), 0.0f);

  if (client_)
    client_->OnSurfaceCreated(surface_info);
}

void DisplayCompositor::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                         bool* changed) {}

}  // namespace ui
