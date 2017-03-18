// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_PUBLIC_CPP_DISPLAY_CLIENT_COMPOSITOR_FRAME_SINK_H_
#define SERVICES_UI_PUBLIC_CPP_DISPLAY_CLIENT_COMPOSITOR_FRAME_SINK_H_

#include "cc/ipc/display_compositor.mojom.h"
#include "cc/ipc/mojo_compositor_frame_sink.mojom.h"
#include "cc/output/compositor_frame_sink.h"
#include "cc/scheduler/begin_frame_source.h"
#include "cc/surfaces/local_surface_id.h"
#include "cc/surfaces/local_surface_id_allocator.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class ThreadChecker;
}  // namespace base

namespace ui {
namespace ws {

// DisplayClientCompositorFrameSink submits CompositorFrames to a
// MojoCompositorFrameSink, with the client's frame being the root surface of
// the Display.
class DisplayClientCompositorFrameSink
    : public cc::CompositorFrameSink,
      public cc::mojom::MojoCompositorFrameSinkClient,
      public cc::ExternalBeginFrameSourceClient {
 public:
  DisplayClientCompositorFrameSink(
      const cc::FrameSinkId& frame_sink_id,
      cc::mojom::MojoCompositorFrameSinkAssociatedPtr compositor_frame_sink,
      cc::mojom::DisplayPrivateAssociatedPtr display_private,
      cc::mojom::MojoCompositorFrameSinkClientRequest client_request);

  ~DisplayClientCompositorFrameSink() override;

  // cc::CompositorFrameSink implementation:
  bool BindToClient(cc::CompositorFrameSinkClient* client) override;
  void DetachFromClient() override;
  void SubmitCompositorFrame(cc::CompositorFrame frame) override;

 private:
  // cc::mojom::MojoCompositorFrameSinkClient implementation:
  void DidReceiveCompositorFrameAck() override;
  void OnBeginFrame(const cc::BeginFrameArgs& begin_frame_args) override;
  void ReclaimResources(const cc::ReturnedResourceArray& resources) override;
  void WillDrawSurface(const cc::LocalSurfaceId& local_surface_id,
                       const gfx::Rect& damage_rect) override;

  // cc::ExternalBeginFrameSourceClient implementation:
  void OnNeedsBeginFrames(bool needs_begin_frame) override;
  void OnDidFinishFrame(const cc::BeginFrameAck& ack) override;

  gfx::Size last_submitted_frame_size_;
  cc::LocalSurfaceId local_surface_id_;
  cc::LocalSurfaceIdAllocator id_allocator_;
  cc::mojom::MojoCompositorFrameSinkClientRequest client_request_;
  cc::mojom::MojoCompositorFrameSinkAssociatedPtr compositor_frame_sink_;
  mojo::Binding<cc::mojom::MojoCompositorFrameSinkClient> client_binding_;
  cc::mojom::DisplayPrivateAssociatedPtr display_private_;
  std::unique_ptr<base::ThreadChecker> thread_checker_;
  std::unique_ptr<cc::ExternalBeginFrameSource> begin_frame_source_;
  const cc::FrameSinkId frame_sink_id_;

  DISALLOW_COPY_AND_ASSIGN(DisplayClientCompositorFrameSink);
};

}  // namspace ws
}  // namespace ui

#endif  // SERVICES_UI_PUBLIC_CPP_DISPLAY_CLIENT_COMPOSITOR_FRAME_SINK_H_
