// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_WINDOW_COMPOSITOR_FRAME_SINK_H_
#define SERVICES_UI_PUBLIC_CPP_WINDOW_COMPOSITOR_FRAME_SINK_H_

#include "base/macros.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/output/context_provider.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "cc/surfaces/surface_id.h"
#include "mojo/public/cpp/bindings/binding.h"

namespace ui {

class ClientCompositorFrameSink
    : public cc::CompositorFrameSink,
      public cc::mojom::MojoCompositorFrameSinkClient,
      public cc::ExternalBeginFrameSourceClient {
 public:
  ClientCompositorFrameSink(
      scoped_refptr<cc::ContextProvider> context_provider,
      gpu::GpuMemoryBufferManager* gpu_memory_buffer_manager,
      cc::mojom::MojoCompositorFrameSinkPtrInfo compositor_frame_sink_info,
      cc::mojom::MojoCompositorFrameSinkClientRequest client_request,
      bool enable_surface_synchronization);

  ~ClientCompositorFrameSink() override;

  // cc::CompositorFrameSink implementation.
  bool BindToClient(cc::CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SetLocalSurfaceId(const cc::LocalSurfaceId& local_surface_id) override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;
  void DidNotProduceFrame(const cc::BeginFrameAck& ack) override;

 private:
  // cc::mojom::MojoCompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck(
      const cc::ReturnedResourceArray& resources) override;
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;

  // cc::ExternalBeginFrameSourceClient implementation.
  void OnNeedsBeginFrames(bool needs_begin_frames) override;

  gfx::Size last_submitted_frame_size_;
  cc::LocalSurfaceId local_surface_id_;
  cc::LocalSurfaceIdAllocator id_allocator_;
  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;
  cc::mojom::MojoCompositorFrameSinkPtrInfo compositor_frame_sink_info_;
  cc::mojom::MojoCompositorFrameSinkClientRequest client_request_;
  cc::mojom::MojoCompositorFrameSinkPtr compositor_frame_sink_;
  std::unique_ptr<mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient>>
      client_binding_;
  std::unique_ptr<base::ThreadChecker> thread_checker_;
  const bool enable_surface_synchronization_;

  DISALLOW_COPY_AND_ASSIGN(ClientCompositorFrameSink);
};

}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_WINDOW_COMPOSITOR_FRAME_SINK_H_
