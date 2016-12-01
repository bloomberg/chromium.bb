// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ui/surfaces/display_compositor.h"

#include "cc/output/in_process_context_provider.h"
#include "cc/surfaces/surface.h"
#include "gpu/command_buffer/client/shared_memory_limits.h"
#include "gpu/ipc/gpu_in_process_thread_service.h"
#include "mojo/public/cpp/bindings/strong_binding.h"
#include "services/ui/common/mus_gpu_memory_buffer_manager.h"
#include "services/ui/surfaces/gpu_compositor_frame_sink.h"

namespace ui {

DisplayCompositor::DisplayCompositor(
    scoped_refptr<gpu::InProcessCommandBuffer::Service> gpu_service,
    std::unique_ptr<gpu::GpuMemoryBufferManager> gpu_memory_buffer_manager,
    gpu::ImageFactory* image_factory,
    cc::mojom::DisplayCompositorRequest request,
    cc::mojom::DisplayCompositorClientPtr client)
    : gpu_service_(std::move(gpu_service)),
      gpu_memory_buffer_manager_(std::move(gpu_memory_buffer_manager)),
      image_factory_(image_factory),
      client_(std::move(client)),
      binding_(this, std::move(request)) {
  manager_.AddObserver(this);
}

void DisplayCompositor::CreateCompositorFrameSink(
    const cc::FrameSinkId& frame_sink_id,
    gpu::SurfaceHandle surface_handle,
    cc::mojom::MojoCompositorFrameSinkRequest request,
    cc::mojom::MojoCompositorFrameSinkPrivateRequest private_request,
    cc::mojom::MojoCompositorFrameSinkClientPtr client) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We cannot create more than one CompositorFrameSink with a given
  // |frame_sink_id|.
  DCHECK_EQ(0u, compositor_frame_sinks_.count(frame_sink_id));
  scoped_refptr<cc::InProcessContextProvider> context_provider;
  if (surface_handle != gpu::kNullSurfaceHandle) {
    context_provider = new cc::InProcessContextProvider(
        gpu_service_, surface_handle, gpu_memory_buffer_manager_.get(),
        image_factory_, gpu::SharedMemoryLimits(),
        nullptr /* shared_context */);
  }
  compositor_frame_sinks_[frame_sink_id] =
      base::MakeUnique<GpuCompositorFrameSink>(
          this, frame_sink_id, surface_handle, gpu_memory_buffer_manager_.get(),
          std::move(context_provider), std::move(request),
          std::move(private_request), std::move(client));
}

void DisplayCompositor::AddRootSurfaceReference(const cc::SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  AddSurfaceReference(manager_.GetRootSurfaceId(), child_id);
}

void DisplayCompositor::AddSurfaceReference(const cc::SurfaceId& parent_id,
                                            const cc::SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  auto vector_iter = temp_references_.find(child_id.frame_sink_id());

  // If there are no temporary references for the FrameSinkId then we can just
  // add reference and return.
  if (vector_iter == temp_references_.end()) {
    manager_.AddSurfaceReference(parent_id, child_id);
    return;
  }

  // Get the vector<LocalFrameId> for the appropriate FrameSinkId and look for
  // |child_id.local_frame_id| in that vector. If found, there is a temporary
  // reference to |child_id|.
  std::vector<cc::LocalFrameId>& refs = vector_iter->second;
  auto temp_ref_iter =
      std::find(refs.begin(), refs.end(), child_id.local_frame_id());

  if (temp_ref_iter == refs.end()) {
    manager_.AddSurfaceReference(parent_id, child_id);
    return;
  }

  // All surfaces get a temporary reference to the top level root. If the parent
  // wants to add a reference to the top level root then we do nothing.
  // Otherwise remove the temporary reference and add the reference.
  if (parent_id != manager_.GetRootSurfaceId()) {
    manager_.AddSurfaceReference(parent_id, child_id);
    manager_.RemoveSurfaceReference(manager_.GetRootSurfaceId(), child_id);
  }

  // Remove temporary references for surfaces with the same FrameSinkId that
  // were created before |child_id|. The earlier surfaces were never embedded in
  // the parent and the parent is embedding a later surface, so we know the
  // parent doesn't need them anymore.
  for (auto iter = refs.begin(); iter != temp_ref_iter; ++iter) {
    cc::SurfaceId id = cc::SurfaceId(child_id.frame_sink_id(), *iter);
    manager_.RemoveSurfaceReference(manager_.GetRootSurfaceId(), id);
  }

  // Remove markers for temporary references up to |child_id|, as the temporary
  // references they correspond to were removed above. If |ref_iter| is the last
  // element in |refs| then we are removing all temporary references for the
  // FrameSinkId and can remove the map entry entirely.
  if (++temp_ref_iter == refs.end())
    temp_references_.erase(child_id.frame_sink_id());
  else
    refs.erase(refs.begin(), ++temp_ref_iter);
}

void DisplayCompositor::RemoveRootSurfaceReference(
    const cc::SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  RemoveSurfaceReference(manager_.GetRootSurfaceId(), child_id);
}

void DisplayCompositor::RemoveSurfaceReference(const cc::SurfaceId& parent_id,
                                               const cc::SurfaceId& child_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // TODO(kylechar): Each remove reference can trigger GC, it would be better if
  // we GC only once if removing multiple references.
  manager_.RemoveSurfaceReference(parent_id, child_id);
}

DisplayCompositor::~DisplayCompositor() {
  DCHECK(thread_checker_.CalledOnValidThread());
  // Remove all temporary references on shutdown.
  for (auto& map_entry : temp_references_) {
    const cc::FrameSinkId& frame_sink_id = map_entry.first;
    for (auto& local_frame_id : map_entry.second) {
      manager_.RemoveSurfaceReference(
          manager_.GetRootSurfaceId(),
          cc::SurfaceId(frame_sink_id, local_frame_id));
    }
  }
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

void DisplayCompositor::OnSurfaceCreated(const cc::SurfaceId& surface_id,
                                         const gfx::Size& frame_size,
                                         float device_scale_factor) {
  DCHECK(thread_checker_.CalledOnValidThread());
  // We can get into a situation where multiple CompositorFrames arrive for a
  // CompositorFrameSink before the DisplayCompositorClient can add any
  // references for the frame. When the second frame with a new size arrives,
  // the first will be destroyed and then if there are no references it will be
  // deleted during surface GC. A temporary reference, removed when a real
  // reference is received, is added to prevent this from happening.
  manager_.AddSurfaceReference(manager_.GetRootSurfaceId(), surface_id);
  temp_references_[surface_id.frame_sink_id()].push_back(
      surface_id.local_frame_id());

  if (client_)
    client_->OnSurfaceCreated(surface_id, frame_size, device_scale_factor);
}

void DisplayCompositor::OnSurfaceDamaged(const cc::SurfaceId& surface_id,
                                         bool* changed) {}

}  // namespace ui
